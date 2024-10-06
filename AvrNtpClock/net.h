// This file is part of AVR NTP Clock
// Copyright (C) 2018-2022 Mikolaj Wasacz
// SPDX-License-Identifier: GPL-2.0-only

// This file includes code from Tuxgraphics NTP clock by Guido Socher, covered by the following copyright notice:

// Author: Guido Socher 
// Copyright:LGPL V2
// See http://www.gnu.org/licenses/old-licenses/lgpl-2.0.html
//
// Based on the net.h file from the AVRlib library by Pascal Stang.
// For AVRlib See http://www.procyonengineering.com/
// Used with explicit permission of Pascal Stang.

// This file includes code from Procyon AVRlib by Pascal Stang, covered by the following copyright notice:

// File Name    : 'net.h'
// Title        : Network support library
// Author       : Pascal Stang
// Created      : 8/30/2004
// Revised      : 7/3/2005
// Version      : 0.1
// Target MCU   : Atmel AVR series
// Editor Tabs  : 4
//
//  This code is distributed under the GNU Public License
//      which can be found at http://www.gnu.org/licenses/gpl.txt

#ifndef NET_H
#define NET_H

#include "common.h"

extern void netTick();
extern void netLoop(netstate_t *netstate);
extern void netInit(netstate_t *netstate);

// Protocol constants

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

#endif // NET_H
