// This file is part of AVR NTP Clock
// Copyright (C) 2018-2022 Mikolaj Wasacz
// SPDX-License-Identifier: GPL-2.0-only

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "config.h"
#include "common.h"
#include "arithmetic.h"
#include "net.h"

// If commonBranch is used multiple times, the compiler recognizes it as common code
// This way, multiple relative branch instructions can point to a single relative jump
// Inserted based on analyzing the disassembly
#define commonBranch    asm ("")

// Fuse bits for ATtiny4313: Low: 0xDF, High: 0x9B, Ext: 0xFF
FUSES = {
    .low = LFUSE,
    .high = HFUSE
#ifdef __AVR_ATtiny4313__
    ,
    .extended = EFUSE
#endif
};

// Handle button debouncing, return 0 when debouncing is complete
static uint8_t debounce()
{
    uint8_t gifr = (1 << INTF0) | (1 << INTF1);
    uint8_t deb = DEBOUNCE_CYCLES;
    R_REG(gifr);
    R_REG(deb);
    cli();
    if (GIFR & ((1 << INTF0) | (1 << INTF1)))
    {
        GIFR = gifr;
        debounceCnt = deb;
    }
    sei();
    return debounceCnt;
}

// Wait while EEPROM write is in progress
static void eepromWait()
{
    while (EECR & (1 << EEPE));
}

// Read byte from EEPROM at address p
static uint8_t eepromRead(uint8_t p)
{
    EEAR = p;
    EECR |= (1 << EERE);
    return EEDR;
}

// Write byte to EEPROM at address p
static void eepromWrite(uint8_t p, uint8_t val)
{
    eepromWait();
#ifdef __AVR_ATtiny4313__
    EECR = 0;
#endif
    EEAR = p;
    EEDR = val;
    cli();
    EECR |= (1 << EEMPE);
    EECR |= (1 << EEPE);
    sei();
}

// Initial configuration data (all addresses are least significant byte first)
volatile config_t config EEMEM = {
    .myMac = { 0x6B, 0x6C, 0x63, 0x50, 0x54, 0x4E },
    .netmask = { 0, 0, 0, 0 }, // Obtain from DHCP
    .gwIp = { 0, 0, 0, 0 }, // Obtain from DHCP
    .myIp = { 0, 0, 0, 0 }, // Obtain from DHCP
    .dstIp = { 17, 4, 130, 134 }, // timeserver.rwth-aachen.de
    .nightBrightness = {
        .level = 4,
        .endMinute = 0,
        .endHour = 7
    },
    .dayBrightness = {
        .level = 9,
        .endMinute = 0,
        .endHour = 22
    },
    .timezones = {
        {
            .endMinute = 0,
            .endHour = 3,
            .endDow = 7,
            .endWeek = 5,
            .endMonth = 10,
            .offsetMinute = 0,
            .offsetHour = 2,
            .offsetAdd = 1
        }, // CEST
        {
            .endMinute = 0,
            .endHour = 2,
            .endDow = 7,
            .endWeek = 5,
            .endMonth = 3,
            .offsetMinute = 0,
            .offsetHour = 1,
            .offsetAdd = 1
        } // CET
    }
};

// Main function
int main()
{
#ifdef __AVR_ATtiny4313__

    DISP_CS_PORT = (1 << CS) | DISP_SEL | DISP_SEG;
    DISP_CS_DDR = (1 << CS) | DISP_SEL | DISP_SEG;

    MAIN_PORT = (1 << SW_1) | (1 << SW_2) | (1 << PWR_SENSE);
    MAIN_DDR = (1 << LED) | (1 << SPI_SI) | (1 << SPI_SCK);

    ACSR |= (1 << ACD);
    PRR = (1 << PRUSI) | (1 << PRUSART);

    MCUCR = (1 << ISC10) | (1 << ISC00);

    TCCR0A = (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01) | (1 << CS00);

#else

    MAIN_PORT = (1 << TX) | (1 << SW_1) | (1 << SW_2) | (1 << CS);
    MAIN_DDR = (1 << LED) | (1 << TX) | (1 << SPI_SI) | (1 << SPI_SCK) | (1 << CS);

    DISP_SEL_PORT = (1 << SW_DISP_SEL) | DISP_SEL;
    DISP_SEL_DDR = DISP_SEL;

    DISP_SEG_PORT = DISP_SEG;
    DISP_SEG_DDR = DISP_SEG;

    ACSR |= (1 << ACD);

    UCSRB = (1 << TXEN);
    UBRRL = 19;

    MCUCR = (1 << ISC10) | (1 << ISC00);

    TCCR0 = (1 << WGM01) | (1 << WGM00) | (1 << CS01) | (1 << CS00);

#endif

    OCR1A = TIMER_1S - 1;
    OCR1B = TIMER_1S / 2 - 1;
    TCCR1B = (1 << CS12) | (1 << WGM12);

    TIMSK = (1 << OCIE1A) | (1 << OCIE1B) | (1 << OCIE0A) | (1 << TOIE0);

    uint8_t p = (1 << SW_1) | (1 << SW_2);
    R_REG(p);

    // Copy configuration data from EEPROM and clear display
    // Also write some bytes past disp boundary but it's not important as memory is uninitialized anyway
    eepromWait();
    uint8_t c = sizeof(config_t) - 1;
    STATIC_ASSERT(sizeof(disp_t) <= sizeof(config_t));
    STATIC_ASSERT(offsetof(mem_t, disp) + sizeof(config_t) <= sizeof(mem_t));
    uint8_t *dispPtr = (uint8_t *)&mem.disp + sizeof(config_t);
    uint8_t *configPtr = (uint8_t *)&mem.config + sizeof(config_t);
    uint8_t blank = 15;
    R_REG(blank);
    do
    {
        *--dispPtr = blank;
        *--configPtr = eepromRead(c);
    } while (--c != 0xFF);

    page = p;

    // Main loop
    while (1)
    {
        mem.disp.state = 0;

        sei();

        // Initialize net module
        netstate_t netstate;
        netInit(&netstate);

        // Loop until settings menu is entered
        while (1)
        {
            do
            {
                // Handle network communications
                netLoop(&netstate);
                mem.disp.state = netstate.state;
            } while (debounce());

            // Read button states after debouncing
            uint8_t b = MAIN_PIN;
            b &= (1 << SW_1) | (1 << SW_2);
            if (b == 0)
                break;

            // Update display page
            page = b;
        }

        // Reset manual time input
        datetime_t *initTimePtr = &mem.initTime;
        B_REG(initTimePtr);
        initTimePtr->second = 0;
        initTimePtr->minute = 0;
        initTimePtr->hour = 0;
        initTimePtr->day = 1;
        initTimePtr->month = 1;
        initTimePtr->year = 0;

        uint8_t addr = 0;
        uint8_t dig1 = 0, dig2 = 0, dig3 = 0;
        uint8_t pos = 0;
        uint8_t old = 0;
        uint8_t btn = 0;

        // Loop while in the settings menu
        while (1)
        {
            if (btn & (1 << SW_2))
            {
                // Dummy loop just to allow breaking out of it (this construction results in the smallest code size)
                do
                {
                    // Increment appropriate digit and ensure the value is at most 255
                    if (pos == 1)
                    {
                        dig1++;
                        if (dig1 > 2)
                            dig1 = 0;
                        else if (UINT24(dig3, dig2, dig1) > 0x020505)
                        {
                            dig2 = 0;
                            dig3 = 0;
                        }
                    }
                    else
                    {
                        if (pos == 2)
                            dig2++;
                        else if (pos == 3)
                            dig3++;
                        else
                            break;

                        uint8_t x = 9;
                        R_REG(x);
                        if (dig1 == 2)
                            x = 5;

                        if (dig2 > x)
                            dig2 = 0;
                        if (dig2 != 5)
                            x = 9;

                        if (dig3 > x)
                            dig3 = 0;
                    }
                } while (0);

                // Calculate value from digits
                uint8_t newVal = mulAdd8(dig2, dig1, 10);
                newVal = mulAdd8(dig3, newVal, 10);

                STATIC_ASSERT(offsetof(mem_t, config) + sizeof(config_t) == offsetof(mem_t, initTime));
                uint8_t *ptr = (uint8_t *)&mem.config + sizeof(config_t) + sizeof(datetime_t) - 2;
                B_REG(ptr);
                ptr -= addr;

                if (pos == 0)
                {
                    // Update value in RAM and EEPROM if it's a configuration byte and it's different from current value
                    if (addr >= sizeof(datetime_t) && newVal != *(ptr + 1))
                    {
                        *(ptr + 1) = newVal;
                        eepromWrite(sizeof(config_t) + sizeof(datetime_t) - 1 - addr, newVal);
                    }

                    // Calculate digits for next byte
                    uint8_t val = *ptr;
                    dig3 = val % 10;
                    uint8_t v = val / 10;
                    R_REG(v);
                    dig2 = v % 10;
                    dig1 = v / 10;

                    addr++;
                    if (addr >= sizeof(config_t) + sizeof(datetime_t))
                        break;

                    commonBranch;
                }
                else if (addr >= sizeof(datetime_t))
                    commonBranch;
                else
                {
                    // This is a manual time input byte, so update value in RAM immediately
                    *(ptr + 1) = newVal;

                    datetime_t *initTimePtr = &mem.initTime;
                    Y_REG(initTimePtr);

                    // Get total days from previous years, including leap days
                    // Also add days within this month
                    uint8_t year = initTimePtr->year;
                    uint8_t d = year;
                    d -= 1;
                    *((int8_t *)&d) >>= 2;
                    d += initTimePtr->day;
                    uint16_t td = mulAdd16(d, year, 365);

                    // Add days from previous months within this year
                    uint8_t month = initTimePtr->month;
                    while (1)
                    {
                        month--;
                        if (month == 0)
                            break;
                        R_REG(month);
                        uint8_t temp;
                        asm (
                            "ldi %0, 29 \n"
                            "sbrs %1, 0 \n"
                            "sbrc %1, 1 \n"
                            "ldi %0, 28 \n"
                            : "=&d" (temp)
                            : "r" (year)
                        );
                        if (month != 2)
                        {
                            uint8_t m = month;
                            if (m & 0x08)
                                m = ~m;
                            temp = (m & 1) + 30;
                        }
                        td += temp;
                    }

                    // Calculate total seconds from days, hours, minutes, and seconds
                    uint16_t tm = mulAdd8(initTimePtr->minute, initTimePtr->hour, 60);
                    uint8_t s = initTimePtr->second;
                    uint16_t t2s = mulAdd16(s >> 1, tm, 30);
                    uint32_t time = mulAdd16(t2s, td, (uint16_t)24 * 60 * 30);
                    STATIC_ASSERT(L(-SEC_OFFSET) == 0);
                    asm (
                        "lsr %1 \n"
                        "adc %A0, %A0 \n"
                        "adc %B0, %B0 \n"
                        "adc %C0, %C0 \n"
                        "adc %D0, %D0 \n"
                        "subi %B0, %2 \n"
                        "sbci %C0, %3 \n"
                        "sbci %D0, %4 \n"
                        : "+d" (time), "+r" (s)
                        : "M" (L(-SEC_OFFSET >> 8)), "M" (L(-SEC_OFFSET >> 16)), "M" (L(-SEC_OFFSET >> 24))
                    );

                    // Set time
                    resetTimer(time, 0);
                }
            }

            uint8_t p = 15;
            R_REG(p);
            if (btn & (1 << SW_1))
            {
                // Update current digit indicator
                pos++;
                pos &= 3;
            }
            if (pos != 0)
                p = pos;

            // Update display page 0
            disp_t *dispPtr = &mem.disp;
            B_REG(dispPtr);
            dispPtr->menu[0] = addr % 10;
            dispPtr->menu[1] = addr / 10;
            dispPtr->menu[2] = dig3;
            dispPtr->menu[3] = dig2;
            dispPtr->menu[4] = dig1;
            dispPtr->menu[5] = p;
            page = 0;

            // Debounce and get button states
            while (debounce());
            uint8_t new = MAIN_PIN;
            btn = old & ~new;
            old = new;
        }

        flag &= ~(1 << SYNC_OK);
        flag &= ~(1 << CUSTOM_IP);
        page = old & ((1 << SW_1) | (1 << SW_2));
    }
}
