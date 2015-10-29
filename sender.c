#include "sender.h"

void init_sender(Sender * s, int id) {
    s->send_id = id;
    s->input_cmdlist_head = NULL;
    s->input_framelist_head = NULL;
    pthread_cond_init(&s->buffer_cv, NULL);
    pthread_mutex_init(&s->buffer_mutex, NULL);
    
    int N = glb_receivers_array_length;
    allocArray(s->GRP, uchar8_t, N, 0);
    allocArray(s->LFS, uchar8_t, N, 0);
    allocArray(s->LAR, uchar8_t, N, 0);
    allocArray(s->SWS, uchar8_t, N, 0);
    
    allocArray(s->sentAwait, LLnode *, N, 0);
    allocArray(s->memoryBuf, LLnode *, N, 0);
    while( --N >= 0 ) {
        ll_insert_node(&s->sentAwait[N], (LLnode *)malloc(sizeof(LLnode)), llt_head);
        ll_insert_node(&s->memoryBuf[N], (LLnode *)malloc(sizeof(LLnode)), llt_head);
    }
}

struct timeval* next_expiring_timeval(Sender* s) {
    struct timeval now;
    gettimeofday( &now, NULL );
    int N = glb_receivers_array_length;
    struct timeval *next_expiring = NULL;
    
    while( --N >= 0 ) {
        LLnode *iter = s->sentAwait[N];
        while( (iter = iter->next ) != s->sentAwait[N] ) {
            struct timeval ts = ((FrameBuf *)iter->value)->expires;
            if( next_expiring == NULL )  {
                next_expiring = (struct timeval *)malloc(sizeof(struct timeval));
                next_expiring->tv_sec = ts.tv_sec;
                next_expiring->tv_usec = ts.tv_usec;
            } else if( ts.tv_sec <= next_expiring->tv_sec && ts.tv_usec < next_expiring->tv_usec ) {
                next_expiring->tv_sec = ts.tv_sec;
                next_expiring->tv_usec = ts.tv_usec;
            }

            if( now.tv_sec >= ts.tv_sec && now.tv_usec > ts.tv_usec ) {
                ((FrameBuf *)iter->value)->buf[3] |= TMO_MASK;
            }
        }
    }
    return next_expiring;
}

// For a frame:
// [0] = src
// [1] = dst
// [2] = seq
// [3] = ctr

// Note: If receiver sends back node without ACK flag set,
// its seq is larger than LAF, so resend sender's last ACK
// frame to keep both windows synchronized.
void handle_incoming_acks(Sender * s, LLnode ** outgoing) {
    
    while( ll_get_length(s->input_framelist_head) > 0 ) {
        LLnode *acknode = ll_pop_node(&s->input_framelist_head);
        char *_rawframe = ((FrameBuf*)acknode->value)->buf;
        LLnode *sent = s->sentAwait[_rawframe[1]];
        
        if( _rawframe[0] != s->send_id ) {
            ll_destroy_node(acknode); // drop the frame
            continue;
        }

        // Was receiver's window full?
        if( !(_rawframe[3] & ACK_MASK) ) {
            
            while( (sent = sent->next) != s->sentAwait[_rawframe[1]] ) {
                if( ((FrameBuf *)sent->value)->buf[2] == (s->LAR[_rawframe[1]] + 1) % MAX_SEQ ) {
                    FrameBuf *resend = (FrameBuf *)sent->value;
                    FrameBuf *fb = (FrameBuf *)malloc(sizeof(FrameBuf));
                    fb->buf = (char *) malloc(MAX_FRAME_SIZE);
                    memcpy(fb->buf, resend->buf, MAX_FRAME_SIZE);
                    ll_insert_node(outgoing, fb, llt_framebuf);
                    ll_insert_node(outgoing, acknode, llt_node);
                    break;
                }
            }
        } else {
            
            while( (sent = sent->next) != s->sentAwait[_rawframe[1]] ) {
                char *stored = ((FrameBuf *)sent->value)->buf;
                // found the frame in the buffer
                if( _rawframe[2] == stored[2] ) {
                    if( stored[2] == (s->LAR[_rawframe[1]]+1) % MAX_SEQ ) { // desired ack to move window
                        uchar8_t recv_id = _rawframe[1];
                        fprintf(stderr, "NOTICE <SND_%d> : Got an ACK from <RECV_%d>\n",s->send_id, recv_id);
                        // increase sliding window
                        s->LAR[recv_id] = (s->LAR[recv_id]+1) % MAX_SEQ;
                        s->SWS[recv_id] = (s->LFS[recv_id] + MAX_SEQ - s->LAR[recv_id]) % MAX_SEQ;
                        LLnode* LAR = ll_pop_node(&sent);
                        ll_destroy_node(LAR);
                        sent = sent->prev;
                    }
                    break;
                }
            }
            // Dropped if duplicate (ctr::ACK set), frame wasn't found, or ACK was out of order
            ll_destroy_node(acknode);
        }
        
    }
    
}

void handle_input_cmds(Sender * s, LLnode ** outgoing) {
    
}

void handle_timedout_frames(Sender * s, LLnode ** outgoing) {

}

void * run_sender(void * input_sender) {
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender * sender = (Sender *) input_sender;    
    LLnode * outgoing_frames_head;
    struct timeval *expiring_timeval;
    long sleep_usec_time, sleep_sec_time;
    
    //This incomplete sender thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up
    //2. Grab the mutex protecting the input_cmd/inframe queues
    //3. Dequeues messages from the input queue and adds them to the outgoing_frames list
    //4. Releases the lock
    //5. Sends out the messages


    while( 1 ) {
        outgoing_frames_head = NULL;

        //Get the current time
        gettimeofday(&curr_timeval, NULL);

        //time_spec is a data structure used to specify when the thread should wake up
        //The time is specified as an ABSOLUTE (meaning, conceptually, you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        //Check for the next event we should handle
        expiring_timeval = next_expiring_timeval(sender);

        //Perform full on timeout
        if ( expiring_timeval == NULL ) {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        } else {
            //Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval, expiring_timeval);
            
            //Sleep if the difference is positive
            if (sleep_usec_time > 0) {
                sleep_sec_time = sleep_usec_time/1000000;
                sleep_usec_time = sleep_usec_time % 1000000;   
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time*1000;
            }   
        }
        free(expiring_timeval);

        //Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        
        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames or input commands should go 
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        //Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);
        
        // Nothing (cmd nor incoming frame) has arrived, so do a timed wait on the sender's
        // condition variable (releases lock)
        // A signal on the condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 && inframe_queue_length == 0) {
            pthread_cond_timedwait(&sender->buffer_cv, &sender->buffer_mutex, &time_spec);
        }

        handle_incoming_acks(sender, &outgoing_frames_head);

        handle_input_cmds(sender, &outgoing_frames_head);

        handle_timedout_frames(sender, &outgoing_frames_head);
        
        pthread_mutex_unlock(&sender->buffer_mutex);

        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        
        while(ll_outgoing_frame_length > 0) {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *)  ll_outframe_node->value;

            //Don't worry about freeing the char_buf, the following function does that
            send_msg_to_receivers(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
