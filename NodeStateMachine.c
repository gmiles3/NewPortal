
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "wifibroadcast/pkt.h"
#include "wifibroadcast/rx.h"
#include "wifibroadcast/tx.h"

#define MAINHUB 0
#define LOCAL_TIMEOUT_LIMIT 25
#define MAX_CMDLINE_INPUT 100

typedef enum {
    START,
    SEND_REQUEST,
    WAIT_FOR_INFO,
    SEND_DATA,
    WAIT_FOR_PARENT
} State;

unsigned short maxID = 0;
char data[MAX_USER_PACKET_LENGTH];
State current_state = START;
unsigned short nodeID;
struct timeval local_timer;
FILE* ffmpeg = NULL;

packet_t* createDataMessage(char* data, size_t bytes) {
    packet_t* packet = malloc(sizeof(packet_t));
    if (packet != NULL) {
        packet->payload = data;
        packet->source = nodeID;
        packet->destination = MAINHUB;
        packet->payload_length = bytes;
    }
    return packet;
}

packet_t* createRequestMessage() {
    packet_t* packet = malloc(sizeof(packet_t));
    if (packet != NULL) {
        packet->source = -1;
        packet->destination = MAINHUB;
        packet->payload_length = 0;
    }
    return packet;
}

int localTimeout() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    if(current_time.tv_sec*1000 + current_time.tv_usec/1000 - local_timer.tv_sec*1000 - local_timer.tv_usec/1000 > LOCAL_TIMEOUT_LIMIT * (maxID + 1)) {
        gettimeofday(&local_timer, NULL);
        fprintf(stderr, "LOCAL TIMEOUT\n");
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

    rx_context_t *node_rx_context = rx_initialize(interface_name, channel);
    tx_context_t *node_tx_context = tx_initialize(interface_name, channel, node_rx_context);


    while(1) {

        switch(current_state) {
        case START:
            //WAIT FOR BEACON
            printf("case: START\n");
            packet_t* packetIn = NULL;
            while(1) {
                packetIn = receive_packet_now(node_rx_context);//NO parameters?
                if (packetIn != NULL) { //gets a packet
                    fprintf(stderr, "START made it: %hu %hu\n", packetIn->source, packetIn->destination);
                    if (packetIn->source == MAINHUB && packetIn->destination == ((unsigned short)-1)) { //check if BEACON message
                        current_state = SEND_REQUEST; //go to send a request
                        free(packetIn);
                        break;
                    }
                }
            }
            break;
        case SEND_REQUEST:
            printf("case: SEND_REQUEST\n");
            packet_t* request = createRequestMessage();//ready request mem
            if (request == NULL) {
                printf("memory could not be allocated!");
                exit(-1); // couldn't allocate memory
            }
            fprintf(stderr, "SEND_REQUEST sent with %d retries\n", send_packet_now(node_tx_context, request));
            free(request); //free memory of packet we just sent.
            current_state = WAIT_FOR_INFO;//lets wait to hear back
            break;
        case WAIT_FOR_INFO:
            printf("case: WAIT_FOR_INFO\n");
            packet_t* info_packet = NULL;
            refreshLocalTimeout();
            for (int i=0; current_state == WAIT_FOR_INFO; refreshLocalTimeout(), i++) while (!localTimeout()) {
                // printf("blocking on wait_for_info...\n");
                info_packet = receive_packet_now(node_rx_context);
                if (info_packet != NULL) {
                    fprintf(stderr, "WAIT_FOR_INFO got something: %hu %hu\n", info_packet->source, info_packet->destination);
                    fprintf(stderr, "I think destination is %d\n", (unsigned short)(info_packet->destination+1));
                    if (i > 2 && info_packet->source == MAINHUB && info_packet->destination == ((unsigned short)-1)) {
                        fprintf(stderr, "WAIT_FOR_INFO got a beacon: %hu %hu\n", info_packet->source, info_packet->destination);
                        current_state = SEND_REQUEST;
                        free(info_packet);
                        break;
                    }
                    if (info_packet->source == MAINHUB && (unsigned short)(info_packet->destination+1) > (MAINHUB+1) )
                    {
                        fprintf(stderr, "I THINK destination is %d\n", (unsigned short)(info_packet->destination+1));
                        fprintf(stderr, "WAIT_FOR_INFO made it: %hu %hu\n", info_packet->source, info_packet->destination);
                        nodeID = info_packet->destination;
                        maxID = nodeID;
                        current_state = SEND_DATA;
                        free(info_packet); //free becase going to next state
                        break;
                    }
                }
            }

            break;
        case SEND_DATA:
            printf("SEND_DATA\n");
            char data[MAX_USER_PACKET_LENGTH] = "HELLO WORLD";

            /* Open the command for reading video data. */
            if (ffmpeg == NULL) {
                //ffmpeg = popen("ffmpeg -f v4l2 -input_format mjpeg -video_size 160x120 -i /dev/video0 -vf scale=80:60 -qscale:v 15 -vframes 1 -f image2 -loglevel quiet -", "r");
                ffmpeg = popen("ffmpeg -f v4l2 -input_format mjpeg -video_size 160x120 -i /dev/video0 -r 1 -pix_fmt yuv420p -f rawvideo -vf \"scale=80x60\" -loglevel quiet -", "r");
                if (ffmpeg == NULL) {
                    printf("failed to run command\n" );
                    exit(1);
                }
            }
            packet_t* toSend = createDataMessage(data, fread(data, 1, MAX_USER_PACKET_LENGTH, ffmpeg));
            if (toSend == NULL) {
                printf("memory could not be allocated!");
                exit(-1);
            }
            fprintf(stderr, "SEND_DATA sent with %d retries\n", send_packet_now(node_tx_context, toSend));
            free(toSend);
            current_state = WAIT_FOR_PARENT;
            break;
        case WAIT_FOR_PARENT:
            printf("WAIT_FOR_PARENT\n");
            packet_t* dataIn = NULL;
            while (1) {
                dataIn = receive_packet_now(node_rx_context);
                if (dataIn != NULL) {
                    fprintf(stderr, "WAIT_FOR_PARENT got something: %hu %hu (%hu) (max=%hu)\n", dataIn->source, dataIn->destination, nodeID, maxID);
                    fprintf(stderr, "I think destination is %d\n", (unsigned short)(dataIn->destination+1));
                    if (dataIn->source == MAINHUB && (unsigned short)(dataIn->destination+1) > (MAINHUB+1)) {
                        // INFO packet
                        fprintf(stderr, "I THINK destination is %d\n", (unsigned short)(dataIn->destination+1));
                        maxID = dataIn->destination;
                        if (maxID == nodeID) {
                            fprintf(stderr, "(That's me. %d=%d.)", maxID, nodeID);
                            current_state = SEND_DATA;
                            free(dataIn);
                            break;
                        }
                    } else if (dataIn->source == nodeID - 1 && (unsigned short)(dataIn->destination+1) != ((unsigned short)-1)) {
                        fprintf(stderr, "WAIT_FOR_PARENT made it: %hu %hu\n", dataIn->source, dataIn->destination);
                        current_state = SEND_DATA;
                        free(dataIn);
                        break;
                    }
                }
            }
            break;
        default:
            printf("should not see\n");
            break;
        }
    }

    if (ffmpeg != NULL) {
        pclose(ffmpeg);
    }

    return 0;
}
