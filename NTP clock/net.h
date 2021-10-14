/*
 * net.h
 *
 * Created: 01.07.2018 12:22:28
 *  Author: Mikolaj
 */ 


#ifndef NET_H
#define NET_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "config.h"
#include "enc28j60.h"

typedef struct
{
    uint8_t minute;
    uint8_t hour;
} hmtime_t;

typedef struct
{
    uint8_t endMinute;
    uint8_t endHour;
    uint8_t endDow;
    uint8_t endWeek;
    uint8_t endMonth;
    uint8_t offsetMinute;
    uint8_t offsetHour;
    uint8_t offsetAdd;
} timezone_t;

typedef struct
{
    uint8_t myMac[6];
    uint8_t netmask[4];
    uint8_t gwIp[4];
    uint8_t myIp[4];
    uint8_t dstIp[4];
    hmtime_t nightTime;
    hmtime_t dayTime;
    timezone_t timezones[2];
} config_t;

typedef struct
{
    uint16_t leaseTime;
    uint16_t syncTime;
    uint16_t retryTime;
    uint32_t time;
    uint8_t timestamp[6];
    uint8_t arpReplyMac[6];
    uint8_t dstMac[6];
    config_t config;
    uint8_t myIp[4];
    uint8_t gwIp[4];
    uint8_t netmask[4];
    uint8_t serverId[4];
    uint8_t arpIp[4];
    uint8_t arpReplyIp[4];
    uint8_t xid[4];
    uint16_t ipId;
    //uint8_t retryCount;
    //uint8_t flag;
} net_t;

extern net_t net;
extern void netLoop(uint8_t* state, uint16_t* NextPacketPtr);
extern void netInit();

#ifdef __AVR_ATtiny4313__
//#define retryTime GPIOR2
#define retryCount GPIOR1
#define flag GPIOR0
#else
//#define retryTime OCR2
#define retryCount TWBR
#define flag TWAR
#endif

#define ARP_REPLY   0
#define TIME_OK     1
#define SYNC_ERROR  2
#define USE_DHCP    3

#define CRC_LEN 4

// notation: _P = position of a field
//           _V = value of a field

// ******* ETH *******
#define ETH_HEADER_LEN  14
#define ETHTYPE_H_V     0x08
#define ETHTYPE_ARP_L_V 0x06
#define ETHTYPE_IP_L_V  0x00


// ******* ARP *******
#define ARP_LEN 28
#define ETH_ARP_OPCODE_REPLY_H_V 0x0
#define ETH_ARP_OPCODE_REPLY_L_V 0x02
#define ETH_ARP_OPCODE_REQ_H_V 0x0
#define ETH_ARP_OPCODE_REQ_L_V 0x01
// start of arp header:
#define ETH_ARP_P 0xe
// arp.dst.ip
#define ETH_ARP_DST_IP_P 0x26
// arp.opcode
#define ETH_ARP_OPCODE_H_P 0x14
#define ETH_ARP_OPCODE_L_P 0x15
// arp.src.mac
#define ETH_ARP_SRC_MAC_P 0x16
#define ETH_ARP_SRC_IP_P 0x1c
#define ETH_ARP_DST_MAC_P 0x20
#define ETH_ARP_DST_IP_P 0x26

// ******* IP *******
#define IP_HEADER_LEN   20
// ip.src
#define IP_SRC_P 0x1a
#define IP_DST_P 0x1e
#define IP_HEADER_LEN_VER_P 0xe
#define IP_CHECKSUM_P 0x18
#define IP_TTL_P 0x16
#define IP_FLAGS_P 0x14
#define IP_P 0xe
#define IP_TOTLEN_H_P 0x10
#define IP_TOTLEN_L_P 0x11
#define IP_ID_H_P 0x12
#define IP_ID_L_P 0x13

#define IP_PROTO_P 0x17

#define IP_PROTO_ICMP_V 1
#define IP_PROTO_TCP_V 6
// 17=0x11
#define IP_PROTO_UDP_V 17
// ******* ICMP *******
#define ICMP_TYPE_ECHOREPLY_V 0
#define ICMP_TYPE_ECHOREQUEST_V 8
//
#define ICMP_TYPE_P 0x22
#define ICMP_CHECKSUM_P 0x24
#define ICMP_CHECKSUM_H_P 0x24
#define ICMP_CHECKSUM_L_P 0x25
#define ICMP_IDENT_H_P 0x26
#define ICMP_IDENT_L_P 0x27
#define ICMP_DATA_P 0x2a

// ******* UDP *******
#define UDP_HEADER_LEN  8
//
#define UDP_SRC_PORT_H_P 0x22
#define UDP_SRC_PORT_L_P 0x23
#define UDP_DST_PORT_H_P 0x24
#define UDP_DST_PORT_L_P 0x25
//
#define UDP_LEN_H_P 0x26
#define UDP_LEN_L_P 0x27
#define UDP_CHECKSUM_H_P 0x28
#define UDP_CHECKSUM_L_P 0x29
#define UDP_DATA_P 0x2a

// ******* NTP *******
#define NTP_LEN 48

// ******* DHCP *******
#define DHCP_DISCOVER_RENEW_LEN 250
#define DHCP_REQUEST_LEN    262
#define DHCP_OFFER_ACK_LEN  255

#endif /* NET_H */