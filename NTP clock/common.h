/*
 * arithmetic.h
 *
 * Created: 28.01.2022 21:22:38
 *  Author: Mikolaj
 */ 


#ifndef COMMON_H
#define COMMON_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include "config.h"
#include "arithmetic.h"

#define STATIC_ASSERT(x)    _Static_assert(x, #x)

#define R_REG(x)            asm ("" : "+r" (x))
#define E_REG(x)            asm ("" : "+e" (x))
#define Y_REG(x)            asm ("" : "+y" (x))

typedef struct
{
    uint8_t minute;
    uint8_t hour;
} hmtime_t;

typedef struct
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} datetime_t;

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
    uint8_t menu[6];
    uint8_t state;
    uint8_t blank1;
    uint8_t year[4];
    uint8_t dow;
    uint8_t blank2;
    uint8_t month[2];
    uint8_t day[2];
    uint8_t second[2];
    uint8_t minute[2];
    uint8_t hour[2];
} disp_t;

typedef struct
{
    uint8_t retryCount;
    uint8_t state;
    uint16_t nextPacketPtr;
} netstate_t;

typedef struct
{
    uint8_t timestamp[6];
    uint8_t arpReplyMac[6];
    uint8_t dstMac[6];
    uint32_t time;
    uint16_t leaseTime;
    uint16_t syncTime;
    disp_t disp;
    config_t config;
    datetime_t initTime;
    uint8_t myIp[4];
    uint8_t gwIp[4];
    uint8_t netmask[4];
    uint8_t serverId[4];
    uint8_t arpIp[4];
    uint8_t arpReplyIp[4];
    uint8_t xid[4];
    uint16_t ipId;
#ifndef __AVR_ATtiny4313__
    u16_t retryTimeMem;
#endif
} net_t;

#define memberSize(type, member) sizeof(((type *)0)->member)
#define netOffset(member) (offsetof(net_t, member) + memberSize(net_t, member))

extern net_t net;

#ifdef __AVR_ATtiny4313__
#define retryTime   _SFR_IO16(_SFR_IO_ADDR(GPIOR1))
#define retryTimeH  GPIOR2
#define retryTimeL  GPIOR1
#else
#define retryTime   net.retryTimeMem.u16
#define retryTimeH  net.retryTimeMem.u8[1]
#define retryTimeL  net.retryTimeMem.u8[0]
#endif

#define RETRY_COUNT     5
#define RETRY_TIMEOUT_S 3 // ToDo: 15
#define RETRY_TIMEOUT_L 15 // ToDo: 900
#define SYNC_TIMEOUT    20 // ToDo: 3600

extern void resetTimer(uint32_t timInt, uint16_t timFrac);
extern void displayTime(uint32_t time);

#endif /* COMMON_H */