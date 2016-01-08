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
#include "rx.h"
// TODO alter the Makefile so I don't need to include a .c file
#include "libtx.c"
#include "librx.c"

int main(int argc, char *argv[]) {

    if (argc < 5) exit(1);
    unsigned short channel = atoi(argv[2]);
    tx_context_t *context = tx_initialize(argv[1], channel, rx_initialize(argv[1], channel));
    unsigned short source = atoi(argv[3]);
    unsigned short destination = atoi(argv[4]);

    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, stdin)) != -1) {
        if (!line) break;
        packet_t pkt = { .payload = (uint8_t*)line, .payload_length = strlen(line), .source = source, .destination = destination };
        if (pkt.payload_length) pkt.payload_length--;
        send_packet_now(context, &pkt);
        printf("%s", line);
        free(line); line = NULL; len = 0;
    }

    if (line) free(line);

    return 0;
}
