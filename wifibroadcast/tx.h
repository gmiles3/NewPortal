#ifndef TX_H
#define TX_H

#include "pkt.h"

int send_packet_now(tx_context_t*, packet_t*);
tx_context_t* tx_initialize(const char* interface_name, unsigned short channel, rx_context_t*);

#endif
