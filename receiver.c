#include "receiver.h"

void init_receiver(Receiver * r, int id) {
    r->recv_id = id;
    r->input_framelist_head = NULL;
    pthread_cond_init(&r->buffer_cv, NULL);
    pthread_mutex_init(&r->buffer_mutex, NULL);
    
    int N = glb_senders_array_length;
    allocArray(r->GRP, uchar8_t, N, 0);
    allocArray(r->LAF, uchar8_t, N, MAX_RWS - 1); // since max is 128, or 127 in 0idx.
    allocArray(r->LFR, uchar8_t, N, MAX_SEQ - 1);
    allocArray(r->buffer, LLnode *, N, 0);
    allocArray(r->memoryBuf, LLnode *, N, 0);
    
    while( --N >= 0 ) {
        ll_insert_node(&r->buffer[N], (LLnode *)malloc(sizeof(LLnode)), llt_head);
        ll_insert_node(&r->memoryBuf[N], (LLnode *)malloc(sizeof(LLnode)), llt_head);
    }
}

// Makes copy of whatever is being sent, original is preserved
// Does not print the <RECV_%d> message
// Does not modify the original frame
// Send as is.
void sendFrame(LLnode **outgoing, Frame *toSend) {
//    FrameBuf *wrap = (FrameBuf *)malloc(sizeof(FrameBuf));
  //  wrap->buf = convert_frame_to_char(toSend);
    
    ll_append_node(outgoing, convert_frame_to_char(toSend));
}

int doRWS( Receiver *r, LLnode **outgoing, Frame *frame ) {
    if( frame->dst != r->recv_id )
        return 0;
    
    // Is the receiver window full?
    if( frame->seq > r->LAF[frame->src] ) {
        fprintf(stderr, "receiver window is full. Try again next pass");
        //reuse to send hint for resending of packet with sequence number LFR + 1
        frame->ctr = SYN_MASK;
        frame->seq = r->LFR[frame->src] + 1;
        sendFrame(outgoing, frame);
        return 0;
    }
    
    int seq_LFR_diff = (frame->seq + MAX_SEQ - r->LFR[frame->src]) % MAX_SEQ;
    if( seq_LFR_diff >= 1 ) {
        LLnode *iter = r->buffer[frame->src];
        while( (iter = iter->next) != r->buffer[frame->src] ) {
            if( frame->seq < ((FrameBuf *)iter->value)->buf[2] )
                break;
        }
        frame->ctr |= ACK_MASK;
        FrameBuf *temp = (FrameBuf *)malloc( sizeof(FrameBuf));
        temp->buf = convert_frame_to_char(frame);
        ll_insert_node(&iter, temp, llt_framebuf);
        
        iter = r->buffer[frame->src];
        while( (iter = iter->next) != r->buffer[frame->src] ) {
            if( ((FrameBuf *)iter->value)->buf[2] == (r->LFR[frame->src] + 1) % MAX_SEQ) {
                LLnode *acknode = ll_pop_node(&iter);
                Frame *ackframe = convert_char_to_frame(((FrameBuf *)acknode->value)->buf);
                
                printf("<RECV_%d>:[%s]\n", r->recv_id, ackframe->data);
                frame->ctr |= ACK_MASK;
                sendFrame(outgoing, ackframe);
                
                r->LFR[frame->src] = ackframe->seq;
                r->LAF[frame->src] = (r->LFR[frame->src] + MAX_RWS) % MAX_SEQ;
                free(ackframe);
                ll_destroy_node(acknode);
            } else {
                break;
            }
        }
    } else { // seq_LFR_diff <= 0
        frame->ctr = ACK_MASK;
        sendFrame(outgoing, frame);
    }
    return 1;
}

void handle_incoming_msgs(Receiver * r, LLnode ** outgoing) {
    
    int ll_length = ll_get_length(r->input_framelist_head);
    while ( ll_length > 0) {
        LLnode* msg_node = ll_pop_node(&r->input_framelist_head);
        Frame *frame = convert_char_to_frame( (char *)msg_node->value );
        
        if( checkcrc32((void *)frame, frame->crc) ) {
            doRWS(r, outgoing, frame);
        }
        free(frame);
        ll_destroy_node(msg_node);
        ll_length = ll_get_length(r->input_framelist_head);
    }
    
}

void * run_receiver(void * input_receiver) {
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver * receiver = (Receiver *) input_receiver;
    LLnode * outgoing_frames_head;


    //This incomplete receiver thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up if there is nothing in the incoming queue(s)
    //2. Grab the mutex protecting the input_msg queue
    //3. Dequeues messages from the input_msg queue and prints them
    //4. Releases the lock
    //5. Sends out any outgoing messages

    
    while(1) {
        //NOTE: Add outgoing messages to the outgoing_frames_head pointer
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval, NULL);

        //Either timeout or get woken up because you've received a datagram
        //NOTE: You don't really need to do anything here, but it might be useful for debugging purposes to have the receivers periodically wakeup and print info
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames should go 
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        //Check whether anything arrived
        int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
        if (incoming_msgs_length == 0) {
            //Nothing has arrived, do a timed wait on the condition variable (which releases the mutex). Again, you don't really need to do the timed wait.
            //A signal on the condition variable will wake up the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv, &receiver->buffer_mutex, &time_spec);
        }

        handle_incoming_msgs(receiver, &outgoing_frames_head);

        pthread_mutex_unlock(&receiver->buffer_mutex);
        
        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while(ll_outgoing_frame_length > 0) {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *) ll_outframe_node->value;
            
            //The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);
            
            //Free up the ll_outframe_node
            free(ll_outframe_node);
            
            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);

}
