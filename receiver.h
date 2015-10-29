#ifndef __RECEIVER_H__
#define __RECEIVER_H__

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

void init_receiver(Receiver *, int);
int doRWS( Receiver * r, LLnode **outgoing, Frame *frame );
void sendACK( LLnode **outgoing, Frame *toSend );
void handle_incoming_msgs( Receiver* r, LLnode ** outgoing );
void * run_receiver(void *);

#endif
