/*
 * net.h
 *
 * Created: 01.07.2018 12:22:28
 *  Author: Mikolaj
 */

#ifndef NET_H
#define NET_H

#include "common.h"

extern void netTick();
extern void netLoop(netstate_t *netstate);
extern void netInit(netstate_t *netstate);

#define CRC_LEN                 4

#define ETH_HEADER_LEN          14
#define ETH_TYPE_H_V            0x08
#define ETH_TYPE_ARP_L_V        0x06
#define ETH_TYPE_IP_L_V         0x00

#define ARP_LEN                 28
#define ARP_OP_H_V              0x00
#define ARP_OP_REQUEST_L_V      0x01
#define ARP_OP_REPLY_L_V        0x02

#define IP_HEADER_LEN           20
#define IP_PROTO_UDP_V          0x11

#define UDP_HEADER_LEN          8

#define NTP_LEN                 48

#define DHCP_DISCOVER_RENEW_LEN 248
#define DHCP_REQUEST_LEN        260
#define DHCP_OFFER_ACK_LEN      255

#endif /* NET_H */