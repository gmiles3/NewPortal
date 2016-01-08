#ifndef RX_H
#define RX_H

#include "pkt.h"

packet_t* receive_packet_now(rx_context_t*);
rx_context_t* rx_initialize(const char* interface_name, unsigned short channel);

#endif
