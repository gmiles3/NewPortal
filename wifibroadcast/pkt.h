#ifndef PKT_H
#define PKT_H

#include "wifibroadcast.h"
#include "lib.h"

#define MAX_PACKET_LENGTH 4192
#define MAX_USER_PACKET_LENGTH 1450
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32
//#define PCAP_ERRBUF_SIZE 80

typedef struct {
    unsigned short source;
    unsigned short destination; // ((unsigned T) -1) is valid and evaluates to the maximum value for integer type T
    size_t payload_length;
    uint8_t* payload;
} packet_t;

typedef struct _rx_context {
    struct bpf_program bpfprogram;
    char szProgram[512];
    char szErrbuf[PCAP_ERRBUF_SIZE];
    pcap_t *ppcap;
    uint8_t packet_recieve_buffer[MAX_PACKET_LENGTH];
    size_t packet_header_length;
    int fd;
    packet_buffer_t *pb;
    packet_t* queued_packet1;
    packet_t* queued_packet2;
} rx_context_t;

typedef struct _tx_context {
    char szErrbuf[PCAP_ERRBUF_SIZE];
    /*int i;*/
    pcap_t *ppcap;
    /*char fBrokenSocket;*/
    /*int pcnt;*/
    /*time_t start_time;*/
    uint8_t packet_transmit_buffer[MAX_PACKET_LENGTH];
    size_t packet_header_length;
    /*fd_set fifo_set;*/
    /*int max_fifo_fd;*/
    /*fifo_t fifo[MAX_FIFOS];*/
    packet_buffer_t *pb;
    rx_context_t *rx;
} tx_context_t;

void queue_packet(rx_context_t *);

#endif
