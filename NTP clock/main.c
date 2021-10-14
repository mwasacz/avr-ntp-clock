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
//#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "net.h"
#include "config.h"

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
    0xFF,    // -
};
#endif

/*----------------------------------------------------------------------*
 *  VARIABLES                                                           *
 *----------------------------------------------------------------------*/

//                         SEC  MIN  HOUR WEEK  MON  DAY  STATE YEAR     MENU          PAGE CNT
static uint8_t disp[26] = {0,0, 0,0, 0,0, 6,10, 1,0, 1,0, 0,10, 0,0,0,2, 0,0,0,0,0,10, 0,1};
//                   CNT   1 2  3 4  5 6  1 2   3 4  5 6  1 2   3 4 5 6
//                   PAGE  0 0  0 0  0 0  1 1   1 1  1 1  2 2   2 2 2 2
#define state disp[12]
#define page disp[24]
#define dispPage 24
#define dispCnt 25
//static uint8_t page = 0;//12;

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

    disp[0] = second % 10;
    disp[1] = second / 10;
    
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

        //uint32_t y = (uint24_t)td << 2;
        //year = y / 1461;
        //td = y % 1461;
        //td >>= 2;
        
        year = 0;
        uint16_t temp16;
        while (1)
        {
            asm (
                "ldi %A0, lo8(366) \n"
                "ldi %B0, hi8(366) \n"
                "sbrs %1, 0 \n"
                "sbrc %1, 1 \n"
                "ldi %A0, lo8(365) \n"
                : "=d"(temp16)
                : "r"(year)
            );
            if (td < temp16)
                break;
            td -= temp16;
            year++;
        }
        
        month = 1;
        uint8_t temp8;
        while (1)
        {
            if (month == 2)
                temp8 = temp16 - 365 + 28;
            else
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
        "M"((1<<OCIE1A) | (1<<ICIE1) | (1<<TOIE0)), "M"((1<<OCIE1A) | (1<<ICIE1) | (1<<OCIE0A) | (1<<TOIE0))
    );
    
    uint8_t *dispPtr = disp;
    E_REG(dispPtr);
    
    //dispPtr[0] = second % 10; // ToDo
    //dispPtr[1] = second / 10;
    dispPtr[2] = minute % 10;
    dispPtr[3] = minute / 10;
    dispPtr[4] = hour % 10;
    dispPtr[5] = hour / 10;
    dispPtr[6] = dow;
    
    day++;
    dispPtr[8] = month % 10;
    dispPtr[9] = month / 10;
    dispPtr[10] = day % 10;
    dispPtr[11] = day / 10;
    
    dispPtr[14] = year % 10;
    dispPtr[15] = year / 10;
}

static void checkPinChange()
{
    // ToDo: consider alternative debouncing using 1ms TIMER0 and count to 10
    cli();
    if (GIFR & ((1<<INTF0) | (1<<INTF1)))
    {
        GIFR = (1<<INTF0) | (1<<INTF1);
        //sei(); // ToDo
        uint16_t cnt = TCNT1 + 625;
        if (cnt >= 62500)
        cnt -= 62500;
        OCR1B = cnt;
        TIFR = (1<<OCF1B);
    }
    sei();
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

    EECR = 0; // ToDo: is this necessary?
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
    DISP_CS_PORT |= (1<<CS);
    DISP_CS_DDR = (1<<CS) | DISP_SEL | DISP_SEG;

    MAIN_PORT = (1<<SW_1) | (1<<SW_2) | (1<<PWR_SENSE);
    MAIN_DDR = (1<<LED) | (1<<SPI_SI) | (1<<SPI_SCK);
    
    ACSR |= (1<<ACD);
    PRR = (1<<PRUSI) | (1<<PRUSART);

    TCCR0A = (1<<CS01) | (1<<CS00);
    OCR0A = 127;
    //TCCR0A = (1<<WGM01);
    //OCR0A = 207;
    //OCR0B = 103;
    
    TCCR1B = (1<<CS12) | (1<<WGM13) | (1<<WGM12);
    ICR1 = 46874;
    OCR1A = 23436;
    //TCCR1B = (1<<CS12) | (1<<WGM12);
    //OCR1A = 46874;
    //OCR1B = 23436;
    
    MCUCR = (1<<ISC10) | (1<<ISC00);
    
    TIMSK = (1<<OCIE1A) | (1<<ICIE1) | (1<<TOIE0);
    //TIMSK = (1<<OCIE1A) | (1<<OCIE1B) | (1<<OCIE0A) | (1<<OCIE0B);
#else
    MAIN_PORT = (1<<TX) | (1<<SW_1) | (1<<SW_2) | (1<<CS);
    MAIN_DDR = (1<<LED) | (1<<TX) | (1<<SPI_SI) | (1<<SPI_SCK) | (1<<CS);
    
    DISP_SEL_PORT = (1<<SW_DISP_SEL) | DISP_SEL;
    DISP_SEL_DDR = DISP_SEL;
    
    DISP_SEG_PORT = DISP_SEG;
    DISP_SEG_DDR = DISP_SEG;
    
    ACSR = (1<<ACD);

    // Timer config
    TCCR0 = (1<<CS01) | (1<<CS00);
    OCR0 = 127;
    
    TCCR1B = (1<<CS12) | (1<<WGM13) | (1<<WGM12);
    ICR1 = 62499;
    OCR1A = 31249;
    //TCCR1B = (1<<CS12) | (1<<WGM12);
    //OCR1A = 62499;
    //OCR1B = 31249;
    
    TIMSK = (1<<OCIE1A) | (1<<TICIE1) | (1<<TOIE0);
    
    MCUCR = (1<<ISC10) | (1<<ISC00);
    
    UCSRB = (1<<TXEN);
    UBRRL = 20;
#endif

    // ToDo: check SW1,2_ISON and set page
    
    eepromRead();

    sei();

    while (1)
    {
        uint8_t *ptr = (uint8_t*)&net + 6;
        uint8_t c = 6;
        R_REG(c);
        do
        {
            *--ptr = 0;
        } while (--c);

        netInit();

        uint16_t NextPacketPtr = RXSTART_INIT;
        uint8_t st = 1;
        R_REG(st);
        if (!(flag & (1<<USE_DHCP)))
            st = 4;
        while (1)
        {
            if (enc28j60LinkUp())
            {
                netLoop(&st, &NextPacketPtr);
                state = st;
            }
            else
            {
                st = 1;
                R_REG(st);
                if (!(flag & (1<<USE_DHCP)))
                    st = 4;
                state = 0;
                net.retryTime = 0;
                retryCount = 5;
            }

            checkPinChange();

            if (TIFR & (1<<OCF1B))
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
                page = p;
            }
        }
        page = 18;

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
                    
                    disp[19] = addr / 10;
                    disp[18] = addr % 10;
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
            uint8_t *dispPtr = disp;
            E_REG(dispPtr);
            if (btn & (1<<SW_2))
            {
                pos++;
                pos &= 3;
                uint8_t p = 10;
                R_REG(p);
                if (pos != 0)
                    p = pos;
                dispPtr[23] = p;
            }
            dispPtr[20] = dig3;
            dispPtr[21] = dig2;
            dispPtr[22] = dig1;
            
            do
            {
                checkPinChange();
            } while (!(TIFR & (1<<OCF1B)));
            
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

// ToDo: disable ICIE1 before sei() to avoid potential recursive entry
//ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
ISR(TIMER1_CAPT_vect, ISR_NOBLOCK)
{
    MAIN_PORT &= ~(1<<LED);
    
    net_t *ptr = &net;
    E_REG(ptr);
    
    if (ptr->leaseTime)
        ptr->leaseTime--;

    if (ptr->syncTime)
        ptr->syncTime--;
    
    if (ptr->retryTime)
        ptr->retryTime--;
    
    if (flag & (1<<TIME_OK))
    {
        uint32_t t = ptr->time + 1;
        ptr->time = t;
        R_REG(t);
        displayTime(t);
    }
}

//ISR(TIMER1_COMPB_vect, ISR_NAKED)
ISR(TIMER1_COMPA_vect, ISR_NAKED)
{
    asm volatile (
        "sbis %0, %1 \n"
        "sbi %2, %3 \n"
        "reti \n"
        :
        : "I"(_SFR_IO_ADDR(flag)), "I"(SYNC_ERROR), "I"(_SFR_IO_ADDR(MAIN_PORT)), "I"(LED)
    );
}

ISR(TIMER0_OVF_vect)
{
    uint8_t* ptr = disp - 1;
    E_REG(ptr);
    
    uint8_t c = ptr[dispCnt + 1];
    c--;
    if (c == 0)
#ifdef __AVR_ATtiny4313__
        c = 6;
#else
        c = 4;
#endif
    ptr[dispCnt + 1] = c;
    
    uint8_t p = ptr[dispPage + 1];
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
        : "I"(_SFR_IO_ADDR(DISP_CS_PORT)), "I"(CS), "M"(10 << 4)
    );
}
#else
ISR(TIMER0_COMP_vect)
{
    DISP_SEL_PORT = (1<<SW_DISP_SEL) | DISP_SEL;
    DISP_SEG_PORT = DISP_SEG;
}
#endif