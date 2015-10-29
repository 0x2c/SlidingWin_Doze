#include "util.h"

//Linked list functions
int ll_get_length(LLnode * head) {
	LLnode * tmp;
	int count = 1;
	if (head == NULL)
		return 0;
	else {
		tmp = head->next;
		while (tmp != head){
			count++;
			tmp = tmp->next;
		}
		return count;
	}
}

// PostConfiguration: ..<-[newNode]<->[head]->..
void ll_append_node(LLnode ** headPtr, void * value) {
    LLnode * newNode;
    LLnode * head;
    if( headPtr == NULL )
        return;
    head = *headPtr;
    newNode = (LLnode *) malloc(sizeof(LLnode));
    newNode->value = value;
    if( head == NULL ) {
        (*headPtr) = newNode;
        newNode->prev = newNode->next = newNode;
    } else {
        newNode->prev = head->prev;
        newNode->next = head;
        newNode->prev->next = newNode;
        head->prev = newNode;
    }
}

// PostConfiguration ..<-[newNode]<->[head]->..
void ll_insert_node(LLnode ** headPtr, void * value, LLtype t) {
    if( headPtr == NULL )
        return;
    LLnode *head = *headPtr;
    LLnode *newNode = NULL;
    if( t != llt_node && t != llt_head ) {
        newNode = (LLnode *) malloc(sizeof(LLnode));
        newNode->value = value;
    } else {
        newNode = (LLnode *)value;
    }
    newNode->type = t;
    
    if( head == NULL ) {
        (*headPtr) = newNode;
        newNode->prev = newNode->next = newNode;
    } else {
        newNode->prev = head->prev;
        newNode->next = head;
        newNode->prev->next = newNode;
        head->prev = newNode;
    }
}

// Removes the head and returns it
// Headptr set to next node or NULL if there isn't any
LLnode * ll_pop_node(LLnode ** headPtr) {
    LLnode *toPop = *headPtr ;
    if( toPop == NULL) {
        return NULL;
    } else if( toPop->prev == toPop ) {
        *headPtr = NULL;
    } else {
        *headPtr = toPop->next;
        toPop->prev->next = *headPtr;
        toPop->next->prev = toPop->prev;
    }
    toPop->next = toPop->prev = NULL;
    return toPop;
}

void ll_destroy_node(LLnode * node) {
    if( node->type != llt_integer ) {
        if( node->type == llt_framebuf ) {
            free( ((FrameBuf *)node->value)->buf );
            free( node->value );
        } else {
            if(node->value != NULL)
                free(node->value);
        }
    }
	free(node);
}

//Compute the difference in usec for two timeval objects
long timeval_usecdiff(struct timeval *start_time, struct timeval *finish_time) {
  long usec;
  usec=(finish_time->tv_sec - start_time->tv_sec)*1000000;
  usec+=(finish_time->tv_usec- start_time->tv_usec);
  return usec;
}


//Print out messages entered by the user
void print_cmd(Cmd * cmd) {
	fprintf(stderr, "src=%d, dst=%d, message=%s\n", cmd->src_id, cmd->dst_id, cmd->message);
}


void print_frame(Frame *f) {
  fprintf( stderr, "\nFRAME :: src=%02x, dst=%02x, seq=%02x, ctr=%02x,\n    buff=%s\n    crc=%04x\n\n",
    f->src, f->dst, f->seq, f->ctr, f->data, f->crc );
}

char * convert_frame_to_char(Frame * frame) {
  char *buffer = (char *) malloc(MAX_FRAME_SIZE);
  char *offset = buffer;
  memset(buffer, 0, MAX_FRAME_SIZE);
  memcpy(offset, frame, 5);
  offset += 5;
  memcpy(offset, frame->data, FRAME_PAYLOAD_SIZE);
  offset += FRAME_PAYLOAD_SIZE;
  memcpy(offset, &frame->crc, 4);
 
  return buffer;
}

Frame * convert_char_to_frame(char * buffer) {
  Frame * frame = (Frame *) malloc(sizeof(Frame));
  frame->src = buffer[0];
  frame->dst = buffer[1];
  frame->seq = buffer[2];
  frame->ctr = buffer[3];
  frame->grp_id = buffer[4];
  memset(frame->data, 0, FRAME_PAYLOAD_SIZE);
  memcpy(frame->data, buffer + 5, FRAME_PAYLOAD_SIZE);
  memcpy(&frame->crc, buffer + 5 + FRAME_PAYLOAD_SIZE, 4);
  return frame;
}

uint32_t crc32(const void *buf, int len) {
    const uint8_t *p = (unsigned char*) buf;
    uint32_t crc = 0;
    while (len--)
        crc = (crc >> 8) ^ precomp32[(crc ^ *p++) & 0xFF];
    return crc;
}

int checkcrc32(const void *frame, int32_t crc) {
    return 1;
}
