#ifndef __COMMON_H__
#define __COMMON_H__

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

#define DEBUG 1
#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
#define MAX_SEQ 256 //actually 255, +1 out of convenience
#define MAX_ACK 256

#define MAX_SWS (MAX_SEQ / 2)
#define MAX_RWS (MAX_ACK / 2)
#define MAX_WAIT 100000   // microseconds

// Control Flag Masks
#define SYN_MASK 0x01     
#define ACK_MASK 0x02
#define TMO_MASK 0x04

#define MAX_FRAME_SIZE 64
#define FRAME_PAYLOAD_SIZE 56


/*......................... FRAME DEF .........................*/
struct Frame_t {
    unsigned char src;  // sender ID        1 byte
    unsigned char dst;  // receiver ID      1 byte
    unsigned char seq;  // sequence number  1 byte
    unsigned char ctr;  // control flag     1 byte
    char data[FRAME_PAYLOAD_SIZE]; // body 56 bytes
    uint32_t crc;                  // CRC   4 bytes
};
struct FrameBuf_t {
    char *buf;
    struct timeval expires;
};

typedef struct Frame_t    Frame;
typedef struct FrameBuf_t FrameBuf;



/*..................... SYSTEM CONFIG DEF .................... */
struct SysConfig_t {
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};

struct Cmd_t {
    uint16_t src_id;
    uint16_t dst_id;
    char * message;
};

typedef struct SysConfig_t SysConfig;
typedef struct Cmd_t Cmd;



/*...................... LINKEDLIST DEF ...................... */
enum LLtype {
    llt_string,
    llt_frame,
    llt_integer,
    llt_head
} LLtype;

struct LLnode_t {
    struct LLnode_t * prev;
    struct LLnode_t * next;
    enum LLtype type;
    void * value;
};

typedef struct LLnode_t LLnode;



/*...................... SENDR/RCVR DEF ...................... */
struct Sender_t {
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;    
    LLnode * input_cmdlist_head;
    LLnode * input_framelist_head;

    int send_id;
    unsigned char LFS; // Last Frame Sent
    unsigned char LAR; // Last ACK Received
    unsigned char SWS; // Sender Window Size ( LFS<seq#> - LAR<seq#> )

    LLnode **buffer;   // Buffer for each rcvr, stores frames of window
};

struct Receiver_t {
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode * input_framelist_head;
    
    int recv_id;
    unsigned char *LAF;     // Largest Acceptable Frame
    unsigned char *LFR;     // Last Frame Received >ACK Sent
    unsigned char *RWS;     // Receiver Window Size ( LFR<seq#> - LAF<seq#> )
    unsigned char **buffer; // Buffer for each sndr, stores frames of window
                            // also for out of order frames
};

enum SendFrame_DstType {
    ReceiverDst,
    SenderDst
} SendFrame_DstType ;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;




//Declare global variables here
//DO NOT CHANGE: 
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender * glb_senders_array;
Receiver * glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;
#endif 
