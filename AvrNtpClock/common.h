#ifndef COMMON_H
#define COMMON_H

#include <avr/io.h>
#include <stddef.h>
#include "config.h"
#include "arithmetic.h"

#define STATIC_ASSERT(x)    _Static_assert(x, #x)

// Force variable to be allocated to a specific register class
// This sometimes prevents optimizations that lead to higher overall code size
#define R_REG(x)            asm ("" : "+r" (x))
#define B_REG(x)            asm ("" : "+b" (x))
#define Y_REG(x)            asm ("" : "+y" (x))

// Data required by the net module, including the state variable
// State 0 - ENC28J60 not responding
// State 1 - Network cable disconnected
// State 2 - Waiting for DHCP offer
// State 3 - Waiting for DHCP ack
// State 4 - Waiting for DHCP ack (renew)
// State 5 - Waiting for ARP reply
// State 6 - Waiting for NTP reply
// State 7 - Synchronized
typedef struct
{
    uint8_t retryCount;
    uint8_t state;
    uint16_t nextPacketPtr;
} netstate_t;

// Brightness setting for daytime or nighttime (also stores when daytime or nighttime ends)
typedef struct
{
    uint8_t level;
    uint8_t endMinute;
    uint8_t endHour;
} brightness_t;

// Date and time
typedef struct
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} datetime_t;

// Timezone data as offset from UTC (also stores date and time when the timezone ends)
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

// Configuration data stored in EEPROM
typedef struct
{
    uint8_t myMac[6];
    uint8_t netmask[4];
    uint8_t gwIp[4];
    uint8_t myIp[4];
    uint8_t dstIp[4];
    brightness_t nightBrightness;
    brightness_t dayBrightness;
    timezone_t timezones[2];
} config_t;

// Display buffers, organized as four 6-byte pages
// Pages are aligned to 8-byte blocks and at least one byte after each page must be blank (set to 15)
typedef struct
{
    uint8_t menu[6];
    uint16_t unused1;
    uint8_t dow;
    uint8_t blank1;
    uint8_t month[2];
    uint8_t day[2];
    uint16_t unused2;
    uint8_t state;
    uint8_t blank2;
    uint8_t year[4];
    uint16_t unused3;
    uint8_t second[2];
    uint8_t minute[2];
    uint8_t hour[2];
    uint8_t unused4;
} disp_t;

// Struct that holds all global variables
// This allows using 1-byte offsets within mem_t instead of 2-byte pointers
// All 6-byte addresses must be placed before 4-byte addresses (including nested structs)
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
} mem_t;

// Size of struct member
#define memberSize(type, member)    sizeof(((type *)0)->member)
// Offset of member within mem (returns first byte past the member, should be used with pre-decrement operations)
#define memOffset(member)           (offsetof(mem_t, member) + memberSize(mem_t, member))

extern mem_t mem;

#ifdef __AVR_ATtiny4313__
// Store retryTime in unused IO register pair
#define retryTime   _SFR_IO16(_SFR_IO_ADDR(GPIOR1))
#define retryTimeH  GPIOR2
#define retryTimeL  GPIOR1
STATIC_ASSERT(((1 << OCF0A) | (1 << OCF0B) | (1 << TOV0)) == 7);
#else
#define retryTime   mem.retryTimeMem.u16
#define retryTimeH  mem.retryTimeMem.u8[1]
#define retryTimeL  mem.retryTimeMem.u8[0]
STATIC_ASSERT(((1 << TOV1) | (1 << OCF0) | (1 << TOV0)) == 7);
#endif
STATIC_ASSERT(offsetof(mem_t, disp) == dispOffset);

// Difference in seconds between 1900-01-01 (NTP epoch) and 2000-01-01
#define SEC_OFFSET  3155673600

// Timeout constants
#define RETRY_COUNT     5
#define RETRY_TIMEOUT_S 15
#define RETRY_TIMEOUT_L 900
#define SYNC_TIMEOUT    3600

extern void resetTimer(uint32_t timInt, uint16_t timFrac);
extern void displayTime(uint32_t time);

#endif // COMMON_H
