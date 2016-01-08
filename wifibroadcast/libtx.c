// (c)2015 befinitiv

/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <time.h>

#include "fec.h"

#include "lib.h"
#include "wifibroadcast.h"
#include "tx.h"

/* this is the template radiotap header we send packets out with */

static const u8 u8aRadiotapHeader[] = {

    0x00, 0x00, // <-- radiotap version
    0x0c, 0x00, // <- radiotap header lengt
    0x04, 0x80, 0x00, 0x00, // <-- bitmap
    0x22,
    0x0,
    0x18, 0x00
};

/* Penumbra IEEE80211 header */

//the last byte of the mac address is recycled as a port number
#define SRC_MAC_LASTBYTE 15
#define DST_MAC_LASTBYTE 21

static u8 u8aIeeeHeader[] = {
    0x08, 0x01, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x13, 0x22, 0x33, 0x44, 0x55, 0x66,
    0x13, 0x22, 0x33, 0x44, 0x55, 0x66,
    0x10, 0x86,
};

/*
 *void
 *usage(void)
 *{
 *    printf(
 *        "(c)2015 befinitiv. Based on packetspammer by Andy Green.  Licensed under GPL2\n"
 *        "\n"
 *        "Usage: tx [options] <interface>\n\nOptions\n"
 *        "-r <count> Number of FEC packets per block (default 4). Needs to match with rx.\n\n"
 *        "-f <bytes> Number of bytes per packet (default %d. max %d). This is also the FEC block size. Needs to match with rx\n"
 *        "-p <port> Port number 0-255 (default 0)\n"
 *        "-b <count> Number of data packets in a block (default 8). Needs to match with rx.\n"
 *        "-x <count> Number of transmissions of a block (default 1)\n"
 *        "-m <bytes> Minimum number of bytes per frame (default: 0)\n"
 *        "-s <stream> If <stream> is > 1 then the parameter changes \"tx\" input from stdin to named fifos. Each fifo transports a stream over a different port (starting at -p port and incrementing). Fifo names are \"%s\". (default 1)\n"
 *        "Example:\n"
 *        "  iwconfig wlan0 down\n"
 *        "  iw dev wlan0 set monitor otherbss fcsfail\n"
 *        "  ifconfig wlan0 up\n"
 *        "  iwconfig wlan0 channel 13\n"
 *        "  tx wlan0        Reads data over stdin and sends it out over wlan0\n"
 *        "\n", MAX_USER_PACKET_LENGTH, MAX_USER_PACKET_LENGTH, FIFO_NAME);
 *    exit(1);
 *}
 */

void set_port_no(uint8_t *pu, uint16_t src_port, uint16_t dest_port) {
    //dirty hack: the last byte of the mac address is the port number. this makes it easy to filter out specific ports via wireshark
    pu[sizeof(u8aRadiotapHeader) + SRC_MAC_LASTBYTE - 1] = src_port >> 8;
    pu[sizeof(u8aRadiotapHeader) + DST_MAC_LASTBYTE - 1] = dest_port >> 8;
    pu[sizeof(u8aRadiotapHeader) + SRC_MAC_LASTBYTE] = src_port & 0xff;
    pu[sizeof(u8aRadiotapHeader) + DST_MAC_LASTBYTE] = dest_port & 0xff;
}

/*
 *
 *typedef struct {
 *    int seq_nr;
 *    int fd;
 *    int curr_pb;
 *    packet_buffer_t *pbl;
 *} fifo_t;
 *
 */


int packet_header_init(uint8_t *packet_header) {
    u8 *pu8 = packet_header;
    memcpy(packet_header, u8aRadiotapHeader, sizeof(u8aRadiotapHeader));
    pu8 += sizeof(u8aRadiotapHeader);
    memcpy(pu8, u8aIeeeHeader, sizeof (u8aIeeeHeader));
    pu8 += sizeof (u8aIeeeHeader);

    //determine the length of the header
    return pu8 - packet_header;
}

/*
 *void fifo_init(fifo_t *fifo, int fifo_count, int block_size) {
 *    int i;
 *
 *    for(i=0; i<fifo_count; ++i) {
 *        int j;
 *
 *        fifo[i].seq_nr = 0;
 *        fifo[i].fd = -1;
 *        fifo[i].curr_pb = 0;
 *        fifo[i].pbl = lib_alloc_packet_buffer_list(block_size, MAX_PACKET_LENGTH);
 *
 *        //prepare the buffers with headers
 *        for(j=0; j<block_size; ++j) {
 *            fifo[i].pbl[j].len = 0;
 *        }
 *    }
 *
 *}
 *
 *void fifo_open(fifo_t *fifo, int fifo_count) {
 *    int i;
 *    if(fifo_count > 1) {
 *        //new FIFO style
 *
 *        //first, create all required fifos
 *        for(i=0; i<fifo_count; ++i) {
 *            char fn[256];
 *            sprintf(fn, FIFO_NAME, i);
 *
 *            unlink(fn);
 *            if(mkfifo(fn, 0666) != 0) {
 *                printf("Error creating FIFO \"%s\"\n", fn);
 *                exit(1);
 *            }
 *        }
 *
 *        //second: wait for the data sources to connect
 *        for(i=0; i<fifo_count; ++i) {
 *            char fn[256];
 *            sprintf(fn, FIFO_NAME, i);
 *
 *            printf("Waiting for \"%s\" being opened from the data source... \n", fn);
 *            if((fifo[i].fd = open(fn, O_RDONLY)) < 0) {
 *                printf("Error opening FIFO \"%s\"\n", fn);
 *                exit(1);
 *            }
 *            printf("OK\n");
 *        }
 *    }
 *    else {
 *        //old style STDIN input
 *        fifo[0].fd = STDIN_FILENO;
 *    }
 *}
 *
 *
 *void fifo_create_select_set(fifo_t *fifo, int fifo_count, fd_set *fifo_set, int *max_fifo_fd) {
 *    int i;
 *
 *    FD_ZERO(fifo_set);
 *
 *    for(i=0; i<fifo_count; ++i) {
 *        FD_SET(fifo[i].fd, fifo_set);
 *
 *        if(fifo[i].fd > *max_fifo_fd) {
 *            *max_fifo_fd = fifo[i].fd;
 *        }
 *    }
 *}
 */


int pb_transmit_packet(pcap_t *ppcap, /* int seq_nr,*/ uint8_t *packet_transmit_buffer, int packet_length, int retry_count) {
    /*
     * //add header outside of FEC
     *wifi_packet_header_t *wph = (wifi_packet_header_t*)(packet_transmit_buffer + packet_header_len);
     *wph->sequence_number = seq_nr;
     */

    int r = pcap_inject(ppcap, packet_transmit_buffer, packet_length);
    if (r != packet_length) {
        pcap_perror(ppcap, "Trouble injecting packet");
        if (retry_count < 10) return pb_transmit_packet(ppcap, /*seq_nr,*/ packet_transmit_buffer, packet_length, retry_count + 1);
        pcap_perror(ppcap, "Giving up");
        return -1;
    }
    return retry_count;
}

int assemble_and_transmit(packet_buffer_t *pbl, pcap_t *ppcap, short src_port, short dest_port, uint8_t *packet_transmit_buffer, int packet_header_len) {
    //int data_packets_per_block = 1, fec_packets_per_block = 0, transmission_count = 1;
    //int i;
    /*
     *uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
     *uint8_t fec_pool[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][MAX_USER_PACKET_LENGTH];
     *uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
     */

    /*
     *for(i=0; i<data_packets_per_block; ++i) {
     *    data_blocks[i] = pbl[i].data;
     *}
     */


    /*
     *if(fec_packets_per_block) {
     *    for(i=0; i<fec_packets_per_block; ++i) {
     *        fec_blocks[i] = fec_pool[i];
     *    }
     *    fec_encode(packet_length, data_blocks, data_packets_per_block, (unsigned char **)fec_blocks, fec_packets_per_block);
     *}
     */

    //uint8_t *pb = packet_transmit_buffer;
    set_port_no(packet_transmit_buffer, src_port, dest_port);
    //pb += packet_header_len;

    //copy data
    memcpy(packet_transmit_buffer + packet_header_len /* + sizeof(wifi_packet_header_t) */, pbl->data, pbl->len);
    int plen = pbl->len + packet_header_len /* + sizeof(wifi_packet_header_t) */;

    int ret = pb_transmit_packet(ppcap, packet_transmit_buffer, plen, 0);

/*
 *    int x;
 *    for(x=0; x<transmission_count; ++x) {
 *        //send data and FEC packets interleaved
 *        int di = 0;
 *        int fi = 0;
 *        int seq_nr_tmp = *seq_nr;
 *        while(di < data_packets_per_block || fi < fec_packets_per_block) {
 *            if(di < data_packets_per_block) {
 *                pb_transmit_packet(ppcap, seq_nr_tmp, packet_transmit_buffer, packet_header_len, data_blocks[di], packet_length, 0);
 *                seq_nr_tmp++;
 *                di++;
 *            }
 *
 *            if(fi < fec_packets_per_block) {
 *                pb_transmit_packet(ppcap, seq_nr_tmp, packet_transmit_buffer, packet_header_len, fec_blocks[fi], packet_length, 0);
 *                seq_nr_tmp++;
 *                fi++;
 *            }
 *        }
 *    }
 */

    //*seq_nr += data_packets_per_block + fec_packets_per_block;



    //reset the length back
    /*
     *for(i=0; i< data_packets_per_block; ++i) {
     *    pbl[i].len = 0;
     *}
     */

    pbl->len = 0;

    return ret>=0 ? 0 : ret;
}

tx_context_t* tx_initialize(const char* interface_name, unsigned short channel, rx_context_t* rx) {
    tx_context_t *ret = calloc(1, sizeof(tx_context_t));
    if (ret == NULL) return ret;
    char* schannel = strdup("     ");
    sprintf(schannel, "%d", channel);

    char* cmdline = malloc(strlen(interface_name)+strlen(schannel)+strlen("./setup.sh  ")+4);
    sprintf(cmdline, "./setup.sh %s %s", interface_name, schannel);

    if (system(cmdline)) exit(1);
    free(cmdline); free(schannel);

    //printf("Raw data transmitter (c) 2015 befinitiv  GPL2\n");
    ret->rx = rx;
    ret->packet_header_length = packet_header_init(ret->packet_transmit_buffer);
    ret->szErrbuf[0] = '\0';
    ret->ppcap = pcap_open_live(interface_name, 800, 1, 20, ret->szErrbuf);
    if (ret->ppcap == NULL) {
        printf("Unable to open interface %s in pcap: %s\n",
               interface_name, ret->szErrbuf);
        return NULL;
    }
    pcap_setnonblock(ret->ppcap, 1, ret->szErrbuf);

    ret->pb = lib_alloc_packet_buffer_list(1, MAX_PACKET_LENGTH);

    if (ret->pb == NULL) { free(ret); ret = NULL; }

    return ret;
}

int send_packet_now(tx_context_t *ctx, packet_t* packet) {
    //uint8_t* data; size_t data_length; short port_in; short port_out;
    int dlen = MIN(MAX_PACKET_LENGTH - ctx->pb->len, packet->payload_length);
    //memcpy(ctx->pb->data + ctx->pb->len, data, dlen);
    ctx->pb->data = packet->payload;
    ctx->pb->len = dlen;
    int ret = assemble_and_transmit(ctx->pb, ctx->ppcap, packet->source, packet->destination, ctx->packet_transmit_buffer, ctx->packet_header_length);
    ctx->pb-> data = NULL;
    //queue_packet(ctx->rx);
    //queue_packet(ctx->rx);
    return ret;
}

