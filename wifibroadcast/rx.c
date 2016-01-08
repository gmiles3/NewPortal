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


#include "fec.h"

#include "pkt.h"
#include "lib.h"
#include "wifibroadcast.h"
#include "radiotap.h"
#include "rx.h"
#include "librx.c"

#define MAX_PACKET_LENGTH 4192
#define MAX_USER_PACKET_LENGTH 1450
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32

#define DEBUG 1
#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

int main(int argc, char *argv[]) {
    if (argc < 2) exit(1);
    unsigned short channel = atoi(argv[2]);
    rx_context_t *context = rx_initialize(argv[1], channel);

    packet_t* recv = NULL;
    while (1) {
        while (!(recv = receive_packet_now(context)));
        printf("(%d->%d) %lu:%s,\n", recv->source, recv->destination, recv->payload_length, strndup((const char*)recv->payload, recv->payload_length));
        free(recv);
    }

    return 0;
}
