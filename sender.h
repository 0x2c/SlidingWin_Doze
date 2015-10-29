#ifndef __SENDER_H__
#define __SENDER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>
#include "common.h"
#include "util.h"
#include "communicate.h"


// Contains 3 accounting buffers
// 1 ) sentAwaiting
// 2 ) timedout
// 3 ) sendMemory

void init_sender(Sender *, int);


/* ---------------------------------------------------
 * FUNCTION: next_expiring_timeval
 * ---------------------------------------------------
 * Put expiring frames into timedout buffer to be sent
 
 * PRE-CONDITION: <2>timedout buffer is cleared by
 * handle_timedout_frames which sends to recvr

 * POST-CONDITION: <1>sentAwaiting is clear
 * of any timedout frames for incoming
 * RETURN: salient frame's timeval
 *
 * ---------------------------------------------------
 */
struct timeval* next_expiring_timeval(Sender* s);


/* ---------------------------------------------------
 * FUNCTION: handle_incoming_acks
 * ---------------------------------------------------
 * Push off any frames that were acked by the receiver
 * from <1>sentAwaiting buffer and do sws protocol
 
 * PRE-CONDITION: <1>sentAwaiting clear of TMO
 
 * POST-CONDITION: <1>sentAwaiting cleared
 * frames that need to be acknowledged
 *
 * !!May need to removed ackd frame from timedout
 * ---------------------------------------------------
 */
void handle_incoming_acks(Sender * s, LLnode ** outgoing);


/* ---------------------------------------------------
 * FUNCTION: handle_input_cmds
 * ---------------------------------------------------
 * Communicate.h populates with frames that are either from the commandLine
 * or bufferd in memory from fragmentation since the limit is 8 for msg
 * that are too long. Gives <3>MemBuf priority over commandLine LL
 
 * PRE-CONDITION: <1>sentAwaiting cleared in handle_incoming_ack
 
 * POST-CONDITION:  new frames
 * filled <1>sentAwaiting.
 * If SWS is found to
 * be full, don't
 * do anythng
 *
 * ---------------------------------------------------
 */
void handle_input_cmds(Sender * s, LLnode ** outgoing);


/* ---------------------------------------------------
 * FUNCTION: handle_timeout_frames
 * ---------------------------------------------------
 * Sends out frames in <2>timedout, clearing out buffr
 
 * PRE-CONDITION: <2>timedout holds frames to send
 
 * POST-CONDITION: emptied <2>timedout frames,
 * moved them to <1>sentAwaiting buffr for    
 * next cycle of handle_incoming_acks.    
 *
 * ---------------------------------------------------
 */
void handle_timedout_frames(Sender * s, LLnode ** outgoing);



void * run_sender(void *);

#endif
