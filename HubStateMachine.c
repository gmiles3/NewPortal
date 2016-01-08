#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include "wifibroadcast/pkt.h"
#include "wifibroadcast/rx.h"
#include "wifibroadcast/tx.h"

#define NODE_LIMIT 5
#define MAINHUB 0
#define CYCLES_TO_BEACON 50
#define LOCAL_TIMEOUT_LIMIT 25
#define GLOBAL_TIMEOUT_LIMIT (LOCAL_TIMEOUT_LIMIT*3)
#define MAX_CMDLINE_INPUT 100

typedef enum {
    START,
    SEND_BEACON,
    SEND_PING,
    WAIT_FOR_REQUEST,
    SEND_INFO,
    WAIT_FOR_PARENT
} State;

char next_ping_is_info = 0;
unsigned short maxID = 0;
unsigned short attPredID = 0;
unsigned short beaconCycles = 0;
State current_state = START;
struct timeval global_timer;
struct timeval local_timer;
FILE* pipe1;
FILE* pipe2;
FILE* pipe3;
FILE* pipe4;

packet_t* createBeaconMessage() {
    packet_t* packet = malloc(sizeof(packet_t));
    if (packet != NULL) {
        packet->source = MAINHUB;
        packet->destination = -1;
        packet->payload_length = 0;
    }
    fprintf(stderr, "%d: sending beacon; ", current_state);
    return packet;
}

packet_t* createInfoMessage();

packet_t* createPingMessage() {
    if (next_ping_is_info) { fprintf(stderr, "(converted from ping) "); return createInfoMessage(); }
    packet_t* packet = malloc(sizeof(packet_t));
    if (packet != NULL) {
        packet->source = attPredID; //MAINHUB;
        packet->destination = MAINHUB; ///maxID; //MAINHUB; // rejuvinate
        packet->payload_length = 0;
    }
    fprintf(stderr, "%d: sending ping appearing as from %hu; ", current_state, attPredID);
    return packet;
}

packet_t* createInfoMessage() {
    packet_t* packet = malloc(sizeof(packet_t));
    if (packet != NULL) {
        packet->source = MAINHUB;
        packet->destination = maxID;
        packet->payload_length = 0;
    }
    fprintf(stderr, "%d: sending info; ", current_state);
    return packet;
}

int globalTimeout() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    if(current_time.tv_sec*1000 + current_time.tv_usec/1000 - global_timer.tv_sec*1000 - global_timer.tv_usec/1000 > GLOBAL_TIMEOUT_LIMIT * (maxID + 1)) {
        fprintf(stderr, "GLOBAL TIMEOUT in state %d\n", current_state);
        gettimeofday(&global_timer, NULL);
        return 1;
    } else {
        return 0;
    }
}

static inline void refreshGlobalTimeout() {
    gettimeofday(&global_timer, NULL);
}

int localTimeout() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    if(current_time.tv_sec*1000 + current_time.tv_usec/1000 - local_timer.tv_sec*1000 - local_timer.tv_usec/1000 > LOCAL_TIMEOUT_LIMIT * (maxID + 1)) {
        fprintf(stderr, "LOCAL TIMEOUT in state %d\n", current_state);
        gettimeofday(&local_timer, NULL);
        return 1;
    } else {
        return 0;
    }
}

static inline void refreshLocalTimeout() {
    gettimeofday(&local_timer, NULL);
}



int main(int argc, char** argv) {

    char interface_name[MAX_CMDLINE_INPUT];
    unsigned short channel = -1;

    if (argc != 3) exit(1);

    sscanf(argv[1], "%s", interface_name);
    sscanf(argv[2], "%hu", &channel);

    rx_context_t *hub_rx_context = rx_initialize(interface_name, channel);
    tx_context_t *hub_tx_contxt = tx_initialize(interface_name, channel, hub_rx_context);

    while(1) {
        switch(current_state) {
        case START:
            printf("case: START\n");
            gettimeofday(&global_timer, NULL);
            current_state = SEND_BEACON;
            break;
        case SEND_BEACON:
            beaconCycles++;
            if (beaconCycles == CYCLES_TO_BEACON || maxID == 0) {
                printf("case: SEND_BEACON\n");
                packet_t* beacon = createBeaconMessage();
                if (beacon == NULL) {
                    fprintf(stderr, "memory could not be allocated!\n");
                    exit(-1);
                }
                fprintf(stderr, "SEND_BEACON sent with %d retries\n", send_packet_now(hub_tx_contxt, beacon));
                beaconCycles = 0;
                free(beacon);
                current_state = WAIT_FOR_REQUEST;
            } else {
                current_state = SEND_PING;
            }
            break;
        case SEND_PING:
            printf("case: SEND_PING\n");
            packet_t* ping = createPingMessage();
            if (ping == NULL) {
                printf("memory could not be allocated!\n");
                exit(-1);
            }
            fprintf(stderr, "SEND_PING sent with %d retries\n", send_packet_now(hub_tx_contxt, ping));
            free(ping);
            current_state = WAIT_FOR_PARENT;
            break;
        case WAIT_FOR_REQUEST:
            printf("case: WAIT_FOR_REQUEST\n");
            packet_t* requestDataIn = NULL;
            refreshLocalTimeout();
            while (!localTimeout()) {
                requestDataIn = receive_packet_now(hub_rx_context);
                if (requestDataIn != NULL) {
                    fprintf(stderr, "WAIT_FOR_REQUEST got something: %hu %hu len=%lu (max=%hu)\n", requestDataIn->source, requestDataIn->destination, requestDataIn->payload_length, maxID);
                    if (requestDataIn->source == ((unsigned short)-1) && requestDataIn->destination == MAINHUB) { //is a request packet
                        fprintf(stderr, "WAIT_FOR_REQUEST made it: %hu %hu\n", requestDataIn->source, requestDataIn->destination);
                        current_state = SEND_INFO;
                        free(requestDataIn);
                        break;
                    }
                }
            }
            if (current_state != SEND_INFO) {
                if (maxID == 0) {
                    fprintf(stderr, "WAIT_FOR_REQUEST: max=%d", maxID);
                    current_state = SEND_BEACON;
                } else {
                    fprintf(stderr, "WAIT_FOR_REQUEST: max=%d", maxID);
                    current_state = SEND_PING;
                }
            }
            break;
        case SEND_INFO:
            printf("SEND_INFO\n");
            attPredID = maxID;
            maxID = maxID + 1;
            packet_t* info = createInfoMessage();
            if (info == NULL) {
                printf("memory could not be allocated!\n");
                exit(-1);
            }
            fprintf(stderr, "SEND_INFO sent with %d retries\n", send_packet_now(hub_tx_contxt, info));
            free(info);
            char string[125];
            sprintf(string, "ffplay -x 640 -y 480 -pix_fmt yuv420p -s 80x60 -f rawvideo -framerate 1 -window_title \"Node %d\" -loglevel quiet -", maxID);
            FILE* temp = popen(string, "w");
            if (maxID == 1) {
                pipe1 = temp;
            } else if (maxID == 2) {
                pipe2 = temp;
            } else if (maxID == 3) {
                pipe3 = temp;
            } else if (maxID == 4) {
                pipe4 = temp;
            }
            next_ping_is_info = 1;
            current_state = WAIT_FOR_PARENT;
            break;
        case WAIT_FOR_PARENT:
            printf("WAIT_FOR_PARENT\n");
            packet_t* parentDataIn = NULL;
            refreshGlobalTimeout();
            while (!globalTimeout()) {
                parentDataIn = receive_packet_now(hub_rx_context);
                if (parentDataIn != NULL) {
                    if (parentDataIn->source != MAINHUB && parentDataIn->destination == MAINHUB) {//is a data packet
                        fprintf(stderr, "WAIT_FOR_PARENT made it: %hu %hu\n", parentDataIn->source, parentDataIn->destination);
                        refreshGlobalTimeout();

                        attPredID = parentDataIn->source % maxID;
                        if (parentDataIn->source == 1) {
                            fwrite(parentDataIn->payload, 1, parentDataIn->payload_length, pipe1);
                            fflush(pipe1);
                        } else if (parentDataIn->source == 2) {
                            fwrite(parentDataIn->payload, 1, parentDataIn->payload_length, pipe2);
                            fflush(pipe2);
                        } else if (parentDataIn->source == 3) {
                            fwrite(parentDataIn->payload, 1, parentDataIn->payload_length, pipe3);
                            fflush(pipe3);
                        } else if (parentDataIn->source == 4) {
                            fwrite(parentDataIn->payload, 1, parentDataIn->payload_length, pipe4);
                            fflush(pipe4);
                        }
                        if (parentDataIn->source == maxID) {
                            next_ping_is_info = 0;
                            current_state = SEND_BEACON;
                            free(parentDataIn);
                            break;
                        }
                        free(parentDataIn);
                    }
                }
            }
            if (current_state != SEND_BEACON) {
                current_state = SEND_PING; //pre-set state in case of global timeout
            }
            break;
        default:
            printf("should not see\n");
            break;
        }
    }

    if (pipe1)
        pclose(pipe1);
    if (pipe2)
        pclose(pipe2);
    if (pipe3)
        pclose(pipe3);
    if (pipe4)
        pclose(pipe4);
    return 0;
}
