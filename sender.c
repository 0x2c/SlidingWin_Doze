#include "sender.h"

void init_sender(Sender * s, int id) {
    s->send_id = id;
    s->input_cmdlist_head = NULL;
    s->input_framelist_head = NULL;
    pthread_cond_init(&s->buffer_cv, NULL);
    pthread_mutex_init(&s->buffer_mutex, NULL);

    s->LFS = MAX_SEQ-1;
    s->LAR = MAX_SEQ-1;
    s->SWS = 0;
    int N = glb_receivers_array_length;
    s->buffer = (LLnode**) malloc( N * sizeof(LLnode*) );
    while( --N >= 0 ) {
        s->buffer[N] = NULL;
    }

    if( DEBUG ) {
        fprintf(stderr, "Running Sanity Checks\n");
        test_next_expiring_timeval(s);

    }
    
}

/***
 *  Sender keeps track of when frames were sent out in the current sliding window
    Sender will always wait 1 decisecond unless this function returns a timeval to 
    wake prematurely due to a frame that will timeout within that interval
 */
struct timeval* next_expiring_timeval(Sender* s) {
    struct timeval now;
    int N = glb_receivers_array_length;
    LLnode *iter = NULL;
    while( --N >= 0 ) {
        if( (iter = s->buffer[N]) ) {
            do {
                gettimeofday( &now, NULL ); // prevent dsync if loop takes too long
                FrameBuf *temp = (FrameBuf *)iter->value;
                time_t exsec = temp->expires.tv_sec;
                time_t exusec = temp->expires.tv_usec;
                if( now.tv_sec <= exsec && now.tv_usec < exusec ) {
                    if( temp->buf[3] & ACK_MASK ) 
                        continue; // check if frame has been acked.
                                  // removed later on.
                    return &temp->expires;
                }
            } while( (iter = iter->next) != s->buffer[N] );
        }
    }
    return NULL;
}

Frame* test_createFrame(int src, int dst, int seq, int ctr, char *msg, int msglen) {
    Frame *f = (Frame *)malloc( sizeof(Frame) );
    f->src = src;
    f->dst = dst;
    f->seq = seq;
    f->ctr = ctr;
    memcpy(f->data, msg, msglen);
/*
    char *whatsit = convert_frame_to_char(f);
    FrameBuf *bf = (FrameBuf*)malloc( sizeof(FrameBuf));
    bf->buf = whatsit;
    printf("%s\n", whatsit);
*/
    return f;
}

void test_next_expiring_timeval( Sender *s ) {
    struct timeval now;
    gettimeofday( &now, NULL );

    fprintf(stderr, "Testing next expiring timeval\n");
    fprintf(stderr, "Inserting 17 frames into buffer\n");
    int i = 0;
    for(; i < 17; ++i) {
        Frame *f = test_createFrame(0, 0, i, SYN_MASK, "Hello", 5);
         
        //ll_append_node(&s->buffer[0], b);

        // LLnode * testNode = (LLnode *)malloc( sizeof(LLnode) );
        // testNode->type = LLtype.llt_frame; 
        // FrameBuf *testBuf = (FrameBuf *)malloc( sizeof(FrameBuf) );
        
        
        // ll_append_node(&(s->buffer[0]), testNode);
    }
}

void handle_timedout_frames(Sender * sender, LLnode ** outgoing_frames_head_ptr) {
    struct timeval now;
    int N = glb_receivers_array_length;
    while( --N >= 0 ) {
    }
    //TODO: Suggested steps for handling timed out datagrams
    //    1) Iterate through the sliding window protocol information you maintain for each receiver
    //    2) Locate frames that are timed out and add them to the outgoing frames
    //    3) Update the next timeout field on the outgoing frames
}


void handle_incoming_acks(Sender * s, LLnode ** outgoing_frames) {
    //TODO: Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is corrupted
    //    4) Check whether the frame is for this sender
    //    5) Do sliding window protocol for sender/receiver pair   
}


void handle_input_cmds(Sender * sender, LLnode ** outgoing_frames_head_ptr) {
    //TODO: Suggested steps for handling input cmd
    //    1) Dequeue the Cmd from sender->input_cmdlist_head
    //    2) Convert to Frame
    //    3) Set up the frame according to the sliding window protocol
    //    4) Compute CRC and add CRC to Frame

    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    
        
    //Recheck the command queue length to see if stdin_thread dumped a command on us
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    while (input_cmd_length > 0)
    {
        //Pop a node off and update the input_cmd_length
        LLnode * ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        //Cast to Cmd type and free up the memory for the node
        Cmd * outgoing_cmd = (Cmd *) ll_input_cmd_node->value;
        free(ll_input_cmd_node);
            

        //DUMMY CODE: Add the raw char buf to the outgoing_frames list
        //NOTE: You should not blindly send this message out!
        //      Ask yourself: Is this message actually going to the right receiver (recall that default behavior of send is to broadcast to all receivers)?
        //                    Does the receiver have enough space in in it's input queue to handle this message?
        //                    Were the previous messages sent to this receiver ACTUALLY delivered to the receiver?
        int msg_length = strlen(outgoing_cmd->message);
        if (msg_length > MAX_FRAME_SIZE)
        {
            //Do something about messages that exceed the frame size
            printf("<SEND_%d>: sending messages of length greater than %d is not implemented\n", sender->send_id, MAX_FRAME_SIZE);
        }
        else
        {
            //This is probably ONLY one step you want
            Frame * outgoing_frame = (Frame *) malloc (sizeof(Frame));
            strcpy(outgoing_frame->data, outgoing_cmd->message);

            //At this point, we don't need the outgoing_cmd
            free(outgoing_cmd->message);
            free(outgoing_cmd);

            //Convert the message to the outgoing_charbuf
            char * outgoing_charbuf = convert_frame_to_char(outgoing_frame);
            ll_append_node(outgoing_frames_head_ptr,
                           outgoing_charbuf);
            free(outgoing_frame);
        }
    }   
}




void * run_sender(void * input_sender)
{    
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender * sender = (Sender *) input_sender;    
    LLnode * outgoing_frames_head;
    struct timeval * expiring_timeval;
    long sleep_usec_time, sleep_sec_time;
    
    //This incomplete sender thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up
    //2. Grab the mutex protecting the input_cmd/inframe queues
    //3. Dequeues messages from the input queue and adds them to the outgoing_frames list
    //4. Releases the lock
    //5. Sends out the messages


    while(1)
    {    
        outgoing_frames_head = NULL;

        //Get the current time
        gettimeofday(&curr_timeval, 
                     NULL);

        //time_spec is a data structure used to specify when the thread should wake up
        //The time is specified as an ABSOLUTE (meaning, conceptually, you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        //Check for the next event we should handle
        expiring_timeval = next_expiring_timeval(sender);

        //Perform full on timeout
        if (expiring_timeval == NULL) {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        }
        else
        {
            //Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval,
                                               expiring_timeval);

            //Sleep if the difference is positive
            if (sleep_usec_time > 0)
            {
                sleep_sec_time = sleep_usec_time/1000000;
                sleep_usec_time = sleep_usec_time % 1000000;   
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time*1000;
            }   
        }

        //Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000)
        {
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
        
        //Nothing (cmd nor incoming frame) has arrived, so do a timed wait on the sender's condition variable (releases lock)
        //A signal on the condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 &&
            inframe_queue_length == 0)
        {
            
            pthread_cond_timedwait(&sender->buffer_cv, 
                                   &sender->buffer_mutex,
                                   &time_spec);
        }
        //Implement this
        handle_incoming_acks(sender,
                             &outgoing_frames_head);

        //Implement this
        handle_input_cmds(sender,
                          &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);


        //Implement this
        handle_timedout_frames(sender,
                               &outgoing_frames_head);

        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        
        while(ll_outgoing_frame_length > 0)
        {
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
