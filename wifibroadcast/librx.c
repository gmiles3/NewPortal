// (c)2015 befinitiv
// Edited by Luke Stubbs

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
#include "fec.h"
#include "lib.h"
#include "wifibroadcast.h"
#include "radiotap.h"
#include "pkt.h"

#define MAX_USER_PACKET_LENGTH 1450
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32
#define DEBUG 1
#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

// this is where we store a summary of the
// information from the radiotap header
typedef struct  {
    int m_nChannel;
    int m_nChannelFlags;
    int m_nRate;
    int m_nAntenna;
    int m_nRadiotapFlags;
} __attribute__((packed)) PENUMBRA_RADIOTAP_DATA;

int flagHelp = 0;
int param_port = 0;

packet_t* receive_packet_now(rx_context_t *crx) {
    struct pcap_pkthdr * ppcapPacketHeader = NULL;
    struct ieee80211_radiotap_iterator rti;
    PENUMBRA_RADIOTAP_DATA prd = { 0, 0, 0, 0, 0 };
    u8 payloadBuffer[MAX_PACKET_LENGTH];
    u8 *pu8Payload = payloadBuffer;
    int bytes;
    int n;
    int retval;
    int u16HeaderLen;

    /*if (crx->queued_packet1) {*/
    /*    packet_t* ret = crx->queued_packet1;*/
    /*    crx->queued_packet1 = crx->queued_packet2;*/
    /*    crx->queued_packet2 = NULL;*/
    /*    return ret;*/
    /*}*/

    // receive
    retval = pcap_next_ex(crx->ppcap, &ppcapPacketHeader, (const u_char**)&pu8Payload);

    if (retval < 0) {
        fprintf(stderr, "Socket broken\n");
        fprintf(stderr, "%s\n", pcap_geterr(crx->ppcap));
        exit(1);
    }

    if (retval != 1)
        return NULL;

    u16HeaderLen = (pu8Payload[2] + (pu8Payload[3] << 8));

    if (ppcapPacketHeader->len <
            (u16HeaderLen + crx->packet_header_length))
        return NULL;

    bytes = ppcapPacketHeader->len -
            (u16HeaderLen + crx->packet_header_length);
    if (bytes < 0)
        return NULL;

    if (ieee80211_radiotap_iterator_init(&rti,
                                         (struct ieee80211_radiotap_header *)pu8Payload,
                                         ppcapPacketHeader->len) < 0)
        return NULL;

    unsigned short source, destination;

    while ((n = ieee80211_radiotap_iterator_next(&rti)) == 0) {

        switch (rti.this_arg_index) {
        case IEEE80211_RADIOTAP_RATE:
            prd.m_nRate = (*rti.this_arg);
            break;

        case IEEE80211_RADIOTAP_CHANNEL:
            prd.m_nChannel =
                le16_to_cpu(*((u16 *)rti.this_arg));
            prd.m_nChannelFlags =
                le16_to_cpu(*((u16 *)(rti.this_arg + 2)));
            break;

        case IEEE80211_RADIOTAP_ANTENNA:
            prd.m_nAntenna = (*rti.this_arg) + 1;
            break;

        case IEEE80211_RADIOTAP_FLAGS:
            prd.m_nRadiotapFlags = *rti.this_arg;
            break;
        }
    }
    pu8Payload += u16HeaderLen;

    int nLinkEncap = pcap_datalink(crx->ppcap);

    switch (nLinkEncap) {
    case DLT_PRISM_HEADER:
        //fprintf(stderr, "DLT_PRISM_HEADER Encap\n");
        //ret->packet_header_length = 0x20; // ieee80211 comes after this
        //sprintf(ret->szProgram, "radio[0x4a:4]==0x13223344");
        source = (pu8Payload[0x4e] << 8) + pu8Payload[0x4f];
        destination = (pu8Payload[0x54] << 8) + pu8Payload[0x55];
        break;

    case DLT_IEEE802_11_RADIO:
        //fprintf(stderr, "DLT_IEEE802_11_RADIO Encap\n");
        //ret->packet_header_length = 0x18; // ieee80211 comes after this
        //sprintf(ret->szProgram, "ether[0x0a:4]==0x13223344");
        source = (pu8Payload[0x0e] << 8) + pu8Payload[0x0f];
        destination = (pu8Payload[0x14] << 8) + pu8Payload[0x15];
        break;

    default:
        fprintf(stderr, "!!! unknown encapsulation !\n");
        exit(1);
    }

    pu8Payload += crx->packet_header_length;

    if (prd.m_nRadiotapFlags & IEEE80211_RADIOTAP_F_FCS) bytes -= 4;

    int checksum_correct = (prd.m_nRadiotapFlags & 0x40) == 0;
    //fprintf(stderr, "pkt rx--%s\n", (checksum_correct ? "g" : "NG"));

    packet_t* ret = calloc(1, sizeof(packet_t));
    if (!ret) return ret;

    ret->payload = malloc(bytes);
    if (!ret->payload) { free(ret); return NULL; }
    memcpy(ret->payload, pu8Payload, bytes);
    ret->payload_length = bytes;
    ret->source = source;
    ret->destination = destination;
    return ret;

    //process_payload(pu8Payload, bytes, checksum_correct, block_buffer_list,
    //                adapter_no);
}

rx_context_t* rx_initialize(const char* interface_name, unsigned short channel) {
    rx_context_t *ret = calloc(1, sizeof(rx_context_t));
    if (ret == NULL) return ret;
    char* schannel = strdup("  ");
    sprintf(schannel, "%d", channel);

    char* cmdline = malloc(strlen(interface_name)+strlen(schannel)+strlen("./setup.sh  ")+4);
    sprintf(cmdline, "./setup.sh %s %s", interface_name, schannel);

    if (system(cmdline)) exit(1);
    free(cmdline); free(schannel);

    struct bpf_program bpfprogram;
    // open the interface in pcap

    ret->szErrbuf[0] = '\0';
    ret->ppcap = pcap_open_live(interface_name, 2048, 1, -1, ret->szErrbuf);
    if (ret->ppcap == NULL) {
        fprintf(stderr, "Unable to open interface %s in pcap: %s\n",
                interface_name, ret->szErrbuf);
        exit(1);
    }

    if(pcap_setnonblock(ret->ppcap, 1, ret->szErrbuf) < 0)
        fprintf(stderr, "Error setting %s to nonblocking mode: %s\n", interface_name, ret->szErrbuf);

    int nLinkEncap = pcap_datalink(ret->ppcap);

    switch (nLinkEncap) {

    case DLT_PRISM_HEADER:
        fprintf(stderr, "DLT_PRISM_HEADER Encap\n");
        ret->packet_header_length = 0x20; // ieee80211 comes after this
        sprintf(ret->szProgram, "radio[0x4a:4]==0x13223344");
        break;

    case DLT_IEEE802_11_RADIO:
        fprintf(stderr, "DLT_IEEE802_11_RADIO Encap\n");
        ret->packet_header_length = 0x18; // ieee80211 comes after this
        sprintf(ret->szProgram, "ether[0x0a:4]==0x13223344");
        break;

    default:
        fprintf(stderr, "!!! unknown encapsulation on %s !\n", interface_name);
        exit(1);
    }

    if (pcap_compile(ret->ppcap, &bpfprogram, ret->szProgram, 1, 0) == -1) {
        puts(ret->szProgram);
        puts(pcap_geterr(ret->ppcap));
        exit(1);
    } else {
        if (pcap_setfilter(ret->ppcap, &bpfprogram) == -1) {
            fprintf(stderr, "%s\n", ret->szProgram);
            fprintf(stderr, "%s\n", pcap_geterr(ret->ppcap));
        }
        pcap_freecode(&bpfprogram);
    }

    //ret->fd = pcap_get_selectable_fd(ret->ppcap);
    return ret;
}

/*packet_t* receive_packet_now(rx_context_t *crx) {*/
    /*fd_set readset;*/
    /*struct timeval to;*/

    /*to.tv_sec = 0;*/
    /*to.tv_usec = 1e5;*/

    /*FD_ZERO(&readset);*/
    /*FD_SET(crx->fd, &readset);*/

    /*int n = select(30, &readset, NULL, NULL, &to);*/

    /*if(FD_ISSET(crx->fd, &readset)) {*/
        /*return process_packet(crx);*/
    /*}*/
/*}*/

void queue_packet(rx_context_t *crx) {
    if (!(crx->queued_packet1)) { while (!((crx->queued_packet1) = receive_packet_now(crx))); }
    else if (!(crx->queued_packet2)) { while (!((crx->queued_packet2) = receive_packet_now(crx))); }
}

