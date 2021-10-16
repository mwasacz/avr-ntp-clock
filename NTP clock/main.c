/*----------------------------------------------------------------------*
 *  NTP CLOCK                                                           *
 *  with ATtiny4313, ENC28J60 and AVT2632 kit                           *
 *                                                                      *
 *  by Mikolaj Wasacz                                                   *
 *                                                                      *
 *  Microcontroller: AVR ATmega16                                       *
 *  Fuse bits: Low:0x3F High:0xC1                                       *
 *  Clock: 16 MHz Crystal Oscillator                                    *
 *----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 *  INCLUDES                                                            *
 *----------------------------------------------------------------------*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "config.h"
#include "net.h"

#define SEC_OFFSET 3155673600
#define MAX_SEC 3155760000

/*----------------------------------------------------------------------*
 *  FONT DEFINITION                                                     *
 *----------------------------------------------------------------------*/

#ifndef __AVR_ATtiny4313__
const __flash uint8_t font[] = {
    0xC0,    // 0
    0xF9,    // 1
    0xA4,    // 2
    0xB0,    // 3
    0x99,    // 4
    0x92,    // 5
    0x82,    // 6
    0xF8,    // 7
    0x80,    // 8
    0x90,    // 9
    0xFF,    // Blank
    0xFF,    // Blank
    0xFF,    // Blank
    0xFF,    // Blank
    0xFF,    // Blank
    0xFF     // Blank
};
#endif

/*----------------------------------------------------------------------*
 *  VARIABLES                                                           *
 *----------------------------------------------------------------------*/

typedef struct
{
    uint8_t second[2];
    uint8_t minute[2];
    uint8_t hour[2];
    uint8_t dow;
    uint8_t blank0;
    uint8_t month[2];
    uint8_t day[2];
    uint8_t state;
    uint8_t blank1;
    uint8_t year[4];
    uint8_t menu[6];
    uint8_t page;
    uint8_t cnt;
} disp_t;

static disp_t disp = {
    .second = { 0, 0 },
    .minute = { 0, 0 },
    .hour = { 0, 0 },
    .dow = 6,
    .blank0 = 15,
    .month = { 1, 0 },
    .day = { 1, 0 },
    .state = 0,
    .blank1 = 15,
    .year = { 0, 0, 0, 2 },
    .menu = { 0, 0, 0, 0, 0, 10 },
    .page = 0,
    .cnt = 1
};

/*----------------------------------------------------------------------*
 *                                                                      *
 *  FUNCTIONS                                                           *
 *                                                                      *
 *----------------------------------------------------------------------*/

#define swapWord(a, b) \
do \
{ \
    uint16_t temp; \
    asm ( \
        "movw %2, %1 \n" \
        "movw %1, %0 \n" \
        "movw %0, %2 \n" \
        : "+r"(a), "+r"(b), "=r"(temp) \
    ); \
} while (0)

static void displayTime(uint32_t ts)
{
    ts -= SEC_OFFSET;

    //uint32_t maxSec = MAX_SEC; // ToDo: MAX_SEC cap doesnt work because the timezone offset is applied later
    //R_REG(maxSec);
    //if (ts > maxSec)
    //    ts = maxSec;

    uint8_t second = ts % 60;
    uint32_t tmm = ts / 60;
    
    timezone_t *tzAlt = &net.config.timezones[1];
    timezone_t *tz = &net.config.timezones[0];
    uint8_t cmpCnt = 5;
    uint8_t cnt = 3;
    R_REG(tzAlt);
    E_REG(tz);
    R_REG(cmpCnt);
    R_REG(cnt);
    
    uint8_t *tzAltEndPtr = &net.config.timezones[1].endMinute + 5;
    uint8_t *tzEndPtr = &net.config.timezones[0].endMinute + 5;
    while (1)
    {
        uint8_t tzAltEnd = *--tzAltEndPtr;
        uint8_t tzEnd = *--tzEndPtr;
        if (tzEnd < tzAltEnd)
        {
            break;
        }
        if (tzEnd > tzAltEnd || --cmpCnt == 0)
        {
            swapWord(tz, tzAlt);
            break;
        }
    }
    
    uint8_t minute, hour, dow, month, day, year;
    while (1)
    {
        uint16_t offset = ((uint8_t)(tz->offsetHour * 15) << 2) + tz->offsetMinute;
        uint32_t x = tmm;
        if (tz->offsetAdd)
        {
            asm (
                "add %A0, %A1 \n"
                "adc %B0, %B1 \n"
                "adc %C0, __zero_reg__ \n"
                "adc %D0, __zero_reg__ \n"
                : "+r" (x)
                : "r" (offset)
            );
        }
        else
        {
            asm (
                "sub %A0, %A1 \n"
                "sbc %B0, %B1 \n"
                "sbc %C0, __zero_reg__ \n"
                "sbc %D0, __zero_reg__ \n"
                : "+r" (x)
                : "r" (offset)
            );
        }
        
        uint16_t tm = x % (24 * 60);
        uint16_t td = x / (24 * 60);
        dow = (td + 5) % 7 + 1;

        uint24_t y = DWORD(L(td), H(td), 0, 0);
        y <<= 2;
        uint32_t z = y;
        year = z / 1461;
        td = z % 1461;
        td >>= 2;
        
        //year = 0;
        //uint16_t temp16;
        //while (1)
        //{
        //    asm (
        //        "ldi %A0, lo8(366) \n"
        //        "ldi %B0, hi8(366) \n"
        //        "sbrs %1, 0 \n"
        //        "sbrc %1, 1 \n"
        //        "ldi %A0, lo8(365) \n"
        //        : "=d"(temp16)
        //        : "r"(year)
        //    );
        //    if (td < temp16)
        //        break;
        //    td -= temp16;
        //    year++;
        //}
        
        month = 1;
        uint8_t temp8;
        while (1)
        {
            //temp8 = temp16 - 365 + 28;
            asm (
                "ldi %0, 29 \n"
                "sbrs %1, 0 \n"
                "sbrc %1, 1 \n"
                "ldi %0, 28 \n"
                : "=d"(temp8)
                : "r"(year)
            );
            if (month != 2)
            {
                uint8_t m = month;
                if (m & 0x08)
                    m = ~m;
                temp8 = (m & 1) + 30;
            }
            if (td < temp8)
                break;
            td -= temp8;
            month++;
        }
        day = td;
        
        //uint8_t leap = !(year & 3);
        //if (days > 58 + leap)
        //    days = days + 2 - leap;
        //if (year & 3)
        //{
        //    if (days > 58)
        //        days += 2;
        //}
        //else
        //{
        //    if (days > 59)
        //        days += 1;
        //}
        //
        //uint8_t mon = (uint16_t)(days * 12) / 367;
        //uint8_t day = days - (uint16_t)(mon * 367) / 12;
        
        uint8_t d = day + 7;
        uint8_t endWeek = tz->endWeek;
        uint8_t week = 5;
        R_REG(week);
        if (endWeek < 5 || d < temp8)
            week = d / 7;
        
        minute = tm % 60;
        hour = tm / 60;
        
        if (month < tz->endMonth) break;
        if (month == tz->endMonth)
        {
            if (week < endWeek) break;
            if (week == endWeek)
            {
                if (dow < tz->endDow) break;
                if (dow == tz->endDow)
                {
                    if (hour < tz->endHour) break;
                    if (hour == tz->endHour)
                    {
                        if (minute < tz->endMinute) break;
                    }
                }
            }
        }
        if (--cnt == 0) break;
        swapWord(tz, tzAlt);
    }
    
    net_t *netPtr = &net;
    E_REG(netPtr);
    
    // (dayTime >= nightTime && (dayTime <= w || w < nightTime)) || (dayTime <= w && w < nightTime)
    uint8_t temp;
    asm volatile (
        "ldi %0, %9 \n"
        "cp %3, %5 \n"
        "cpc %4, %6 \n"
        "brcs cmp1_%= \n"
        "cp %1, %3 \n"
        "cpc %2, %4 \n"
        "brcc day_%= \n"
        "rjmp cmp2_%= \n"
        "cmp1_%=:"
        "cp %1, %3 \n"
        "cpc %2, %4 \n"
        "brcs night_%= \n"
        "cmp2_%=:"
        "cp %1, %5 \n"
        "cpc %2, %6 \n"
        "brcc night_%= \n"
        "day_%=:"
        "ldi %0, %8 \n"
        "night_%=:"
        "out %7, %0 \n"
        : "=&d"(temp)
        : "r"(minute), "r"(hour), "r"(netPtr->config.dayTime.minute), "r"(netPtr->config.dayTime.hour),
        "r"(netPtr->config.nightTime.minute), "r"(netPtr->config.nightTime.hour), "I"(_SFR_IO_ADDR(TIMSK)),
        "M"((1<<OCIE1A) | (1<<OCIE1B) | (1<<TOIE0)), "M"((1<<OCIE1A) | (1<<OCIE1B) | (1<<OCIE0A) | (1<<TOIE0))
    );
    
    disp_t *dispPtr = &disp;
    E_REG(dispPtr);
    
    dispPtr->second[0] = second % 10;
    dispPtr->second[1] = second / 10;
    dispPtr->minute[0] = minute % 10;
    dispPtr->minute[1] = minute / 10;
    dispPtr->hour[0] = hour % 10;
    dispPtr->hour[1] = hour / 10;
    dispPtr->dow = dow;
    
    day++;
    dispPtr->month[0] = month % 10;
    dispPtr->month[1] = month / 10;
    dispPtr->day[0] = day % 10;
    dispPtr->day[1] = day / 10;
    
    dispPtr->year[0] = year % 10;
    dispPtr->year[1] = year / 10;
}

static uint8_t debounce()
{
    uint8_t gifr = (1<<INTF0) | (1<<INTF1);
    uint8_t deb = DEBOUNCE_CYCLES;
    R_REG(gifr);
    R_REG(deb);
    cli();
    if (GIFR & ((1<<INTF0) | (1<<INTF1)))
    {
        GIFR = gifr;
        debounceCnt = deb;
    }
    sei();
    return debounceCnt;
}

static void eepromRead()
{
    while (EECR & (1<<EEPE));

    uint8_t c = sizeof(config_t) - 1;
    uint8_t* ptr = (uint8_t*)&net.config + sizeof(config_t);
    do
    {
        EEAR = c;
        EECR |= (1<<EERE);
        *--ptr = EEDR;
    } while (--c != 0xFF);
}

static void eepromWrite(uint8_t p, uint8_t val)
{
    while (EECR & (1<<EEPE));

#ifdef __AVR_ATtiny4313__
    EECR = 0;
#endif
    EEAR = p;
    EEDR = val;
    cli(); // ToDo
    EECR |= (1<<EEMPE);
    EECR |= (1<<EEPE);
    sei();
}

/*----------------------------------------------------------------------*
 *                                                                      *
 *  MAIN                                                                *
 *                                                                      *
 *----------------------------------------------------------------------*/

// Initial configuration data (all addresses are least significant byte first)
volatile config_t config EEMEM = {
    .myMac = { 0x6B, 0x6C, 0x63, 0x50, 0x54, 0x4E },
    .netmask = { 0, 0, 0, 0 }, // Obtain from DHCP
    .gwIp = { 0, 0, 0, 0 }, // Obtain from DHCP
    .myIp = { 0, 0, 0, 0 }, // Obtain from DHCP
    .dstIp = { 17, 4, 130, 134 }, // timeserver.rwth-aachen.de
    .nightTime = {
        .minute = 0,
        .hour = 22
    },
    .dayTime = {
        .minute = 0,
        .hour = 7
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

int main()
{
#ifdef __AVR_ATtiny4313__
    DISP_CS_PORT = (1<<CS) | DISP_SEL | DISP_SEG;
    DISP_CS_DDR = (1<<CS) | DISP_SEL | DISP_SEG;

    MAIN_PORT = (1<<SW_1) | (1<<SW_2) | (1<<PWR_SENSE);
    MAIN_DDR = (1<<LED) | (1<<SPI_SI) | (1<<SPI_SCK);
    
    ACSR |= (1<<ACD);
    PRR = (1<<PRUSI) | (1<<PRUSART);
    
    MCUCR = (1<<ISC10) | (1<<ISC00);

    OCR0A = BRIGHTNESS_N;
    TCCR0B = (1<<CS01) | (1<<CS00);
    //TCCR0A = (1<<WGM01);
    //OCR0A = 207;
    //OCR0B = 103;
#else
    MAIN_PORT = (1<<TX) | (1<<SW_1) | (1<<SW_2) | (1<<CS);
    MAIN_DDR = (1<<LED) | (1<<TX) | (1<<SPI_SI) | (1<<SPI_SCK) | (1<<CS);
    
    DISP_SEL_PORT = (1<<SW_DISP_SEL) | DISP_SEL;
    DISP_SEL_DDR = DISP_SEL;
    
    DISP_SEG_PORT = DISP_SEG;
    DISP_SEG_DDR = DISP_SEG;
    
    ACSR |= (1<<ACD);
    
    UCSRB = (1<<TXEN);
    UBRRL = 20;
    
    MCUCR = (1<<ISC10) | (1<<ISC00);

    OCR0 = BRIGHTNESS_N;
    TCCR0 = (1<<CS01) | (1<<CS00);
#endif
    
    OCR1A = TIMER_1S - 1;
    OCR1B = TIMER_1S / 2 - 1;
    TCCR1B = (1<<CS12) | (1<<WGM12);
    
    TIMSK = (1<<TOIE0);
    
    eepromRead();

    sei();

    while (1)
    {
        // ToDo: set default display values

        netstate_t netstate;
        netInit(&netstate);
        
        MAIN_PORT &= ~(1<<LED);
        TIMSK = (1<<OCIE1A) | (1<<OCIE1B) | (1<<TOIE0);
        disp.page = 0;

        while (1)
        {
            netLoop(&netstate);
            disp.state = netstate.state;
            
            if (debounce() == 0)
            {
                uint8_t btn = MAIN_PIN;

                uint8_t p = 12;
                R_REG(p);
                if (!(btn & (1<<SW_1)))
                {
                    if (!(btn & (1<<SW_2)))
                        break;
                    else
                        p = 6;
                }
                else if (btn & (1<<SW_2))
                    p = 0;
                disp.page = p;
            }
        }

        disp.page = 18;
        TIMSK = (1<<TOIE0);
        MAIN_PORT |= (1<<LED);

        uint8_t addr = 0;
        uint8_t dig1 = 0, dig2 = 0, dig3 = 0;
        uint8_t pos = 0;
        uint8_t old = 0;
        uint8_t btn = (1<<SW_1);

        while (1)
        {
            if (btn & (1<<SW_1))
            {
                if (pos == 0)
                {
                    uint8_t* ptr = (uint8_t*)&net.config + sizeof(config_t) - 1;
                    E_REG(ptr);
                    ptr -= addr;
                    
                    if (addr)
                    {
                        uint8_t newVal = dig1;
                        newVal *= 10;
                        newVal += dig2;
                        newVal *= 10;
                        newVal += dig3;
                        uint8_t oldVal = *(ptr + 1);
                        if (oldVal != newVal)
                        {
                            *(ptr + 1) = newVal;
                            eepromWrite(addr, newVal);
                        }
                    }
                    
                    uint8_t val = *ptr;
                    dig1 = val / 100;
                    val %= 100;
                    dig2 = val / 10;
                    dig3 = val % 10;
                    
                    disp.menu[0] = addr % 10;
                    disp.menu[1] = addr / 10;
                    addr++;
                    if (addr == sizeof(config_t))
                        break;
                }
                else if (pos == 1)
                {
                    dig1++;
                    if (dig1 > 2)
                        dig1 = 0;
                    else if ((uint24_t)DWORD(dig3, dig2, dig1, 0) > 0x020505)
                    {
                        dig2 = 0;
                        dig3 = 0;
                    }
                }
                else
                {
                    if (pos == 2)
                        dig2++;
                    else
                        dig3++;

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
            }
            disp_t *dispPtr = &disp;
            E_REG(dispPtr);
            if (btn & (1<<SW_2))
            {
                pos++;
                pos &= 3;
                uint8_t p = 10;
                R_REG(p);
                if (pos != 0)
                    p = pos;
                dispPtr->menu[5] = p;
            }
            dispPtr->menu[2] = dig3;
            dispPtr->menu[3] = dig2;
            dispPtr->menu[4] = dig1;
            
            while (debounce());
            
            uint8_t new = MAIN_PIN;
            btn = old & ~new;
            old = new;
        }
    }
}

/*----------------------------------------------------------------------*
 *                                                                      *
 *  INTERRUPT SERVICE ROUTINES                                          *
 *                                                                      *
 *----------------------------------------------------------------------*/

// ToDo: disable OCIE1A before sei() to avoid potential recursive entry
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
{
    MAIN_PORT &= ~(1<<LED);
    
    uint32_t time;
    if (netTick(&time))
        displayTime(time);
}

ISR(TIMER1_COMPB_vect, ISR_NAKED)
{
    asm volatile (
        "sbic %0, %1 \n"
        "sbi %2, %3 \n"
        "reti \n"
        :
        : "I"(_SFR_IO_ADDR(flag)), "I"(SYNC_OK), "I"(_SFR_IO_ADDR(MAIN_PORT)), "I"(LED)
    );
}

ISR(TIMER0_OVF_vect)
{    
    uint8_t* ptr = (uint8_t*)&disp - 1;
    E_REG(ptr);
    
    uint8_t c = ptr[offsetof(disp_t, cnt) + 1];
    c--;
    if (c == 0)
#ifdef __AVR_ATtiny4313__
        c = 6;
#else
        c = 4;
#endif
    ptr[offsetof(disp_t, cnt) + 1] = c;
    
    uint8_t p = ptr[offsetof(disp_t, page) + 1];
    p += c;

#ifdef __AVR_ATtiny4313__
    uint8_t d = ptr[p];
    c |= (d << 4) | (d >> 4); // ToDo: ensure d is never greater than 15
    if (DISP_CS_PORT & (1<<CS))
        c |= (1<<CS);
    DISP_CS_PORT = c; 
#else
    if (!(DISP_SEL_PIN & (1<<SW_DISP_SEL)))
        p += 2;

    DISP_SEL_PORT = (~(1 << (c - 1)) & DISP_SEL) | (1<<SW_DISP_SEL);
    DISP_SEG_PORT = font[ptr[p]];
#endif

    uint8_t cnt = debounceCnt;
    if (cnt)
        cnt--;
    debounceCnt = cnt;
}

#ifdef __AVR_ATtiny4313__
ISR(TIMER0_COMPA_vect, ISR_NAKED)
{
    uint8_t x;
    asm volatile (
        "push %0 \n"
        "ldi %0, %3 \n"
        "sbic %1, %2 \n"
        "ldi %0, %3 | (1 << %2) \n"
        "out %1, %0 \n"
        "pop %0 \n"
        "reti \n"
        : "=d"(x)
        : "I"(_SFR_IO_ADDR(DISP_CS_PORT)), "I"(CS), "M"(DISP_SEL | DISP_SEG)
    );
}
#else
ISR(TIMER0_COMP_vect)
{
    DISP_SEL_PORT = (1<<SW_DISP_SEL) | DISP_SEL;
    DISP_SEG_PORT = DISP_SEG;
}
#endif