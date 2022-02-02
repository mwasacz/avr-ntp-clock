/*
 * common.c
 *
 * Created: 02/02/2022 14:33:22
 *  Author: Mikolaj
 */ 

#include "common.h"

#define SEC_OFFSET 3155673600

net_t net __attribute__((section(".noinit")));

void resetTimer(uint32_t timInt, uint16_t timFrac)
{
    uint32_t *timePtr = &net.time;
    E_REG(timePtr);
    uint16_t *syncTimePtr = (uint16_t*)((uint8_t*)timePtr - offsetof(net_t, time) + offsetof(net_t, syncTime));
    
    uint8_t gtccr = (1<<PSR10);
    uint8_t tifr = (1<<OCF1A) | (1<<OCF1B);
    uint16_t timeout = SYNC_TIMEOUT;
    R_REG(gtccr);
    R_REG(tifr);
    R_REG(timeout);

    cli();
    GTCCR = gtccr;
    TCNT1 = timFrac;
    TIFR = tifr;
    *timePtr = timInt;
    *syncTimePtr = timeout;
    flag |= (1<<TIME_OK);
    flag |= (1<<SYNC_OK);
    sei();
}

static void swapPtr(timezone_t** tz, timezone_t** tzAlt)
{
    timezone_t* temp = *tz;
    *tz = *tzAlt;
    *tzAlt = temp;
    R_REG(*tz);
    R_REG(*tzAlt);
}

void displayTime(uint32_t time)
{
    time -= SEC_OFFSET;

    uint8_t second = time % 60;
    uint32_t tmm = time / 60;
    
    timezone_t *tzAlt = &net.config.timezones[1];
    timezone_t *tz = &net.config.timezones[0];
    uint8_t cmpCnt = 8;
    uint8_t cnt = 8;
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
        if (tzEnd > tzAltEnd)
        {
            swapPtr(&tz, &tzAlt);
            break;
        }
        cmpCnt <<= 1;
        if (cmpCnt == 0)
        {
            swapPtr(&tz, &tzAlt);
            break;
        }
    }
    
    uint8_t minute, hour, dow, year, month, day;
    while (1)
    {
        uint16_t offset = mulAdd8(tz->offsetMinute, tz->offsetHour, 60);
        uint32_t x = tmm;
        R_REG(x);
        if (tz->offsetAdd)
        x += offset;
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
        R_REG(td);
        
        uint32_t tm32 = tm;
        minute = tm32 % 60;
        hour = tm32 / 60;
        R_REG(minute);
        R_REG(hour);
        
        uint32_t td32 = td + 5;
        dow = td32 % 7;
        R_REG(dow);
        dow++;

        uint32_t t4d;
        asm (
            "movw %A0, %1 \n"
            "ldi %C0, 0 \n"
            "add %A0, %A0 \n"
            "adc %B0, %B0 \n"
            "adc %C0, %C0 \n"
            "add %A0, %A0 \n"
            "adc %B0, %B0 \n"
            "adc %C0, %C0 \n"
            "ldi %D0, 0 \n"
            : "=&d"(t4d)
            : "r"(td)
        );
        year = t4d / 1461;
        td = t4d % 1461;
        td >>= 2;
        
        month = 1;
        uint8_t temp;
        while (1)
        {
            asm (
                "ldi %0, 29 \n"
                "sbrs %1, 0 \n"
                "sbrc %1, 1 \n"
                "ldi %0, 28 \n"
                : "=&d"(temp)
                : "r"(year)
            );
            if (month != 2)
            {
                uint8_t m = month;
                if (m & 0x08)
                m = ~m;
                temp = (m & 1) + 30;
            }
            if (td < temp)
            break;
            td -= temp;
            month++;
        }
        day = td;
        
        uint8_t d = day + 7;
        uint8_t endWeek = tz->endWeek;
        uint8_t week = 5;
        R_REG(week);
        if (endWeek < 5 || d < temp)
        week = d / 7;
        
        uint8_t endMinute = tz->endMinute;
        uint8_t endHour = tz->endHour;
        uint8_t endDow = tz->endDow;
        uint8_t endMonth = tz->endMonth;
        asm (
            "cp %1, %6 \n"
            "cpc %2, %7 \n"
            "cpc %3, %8 \n"
            "cpc %4, %9 \n"
            "cpc %5, %10 \n"
            "sbc %0, __zero_reg__ \n"
            : "+r"(cnt)
            : "r"(endMinute), "r"(endHour), "r"(endDow), "r"(endWeek), "r"(endMonth),
            "r"(minute), "r"(hour), "r"(dow), "r"(week), "r"(month)
        );
        cnt >>= 1;
        if (cnt & 1)
        break;
        swapPtr(&tz, &tzAlt);
    }
    
    disp_t *dispPtr = &net.disp;
    E_REG(dispPtr);
    config_t *configPtr = (config_t*)((uint8_t*)dispPtr - offsetof(net_t, disp) + offsetof(net_t, config));
    
    // (dayTime >= nightTime && (dayTime <= w || w < nightTime)) || (dayTime <= w && w < nightTime)
    asm (
        "cp %2, %4 \n"
        "cpc %3, %5 \n"
        "brcs cmp1_%= \n"
        "cp %0, %2 \n"
        "cpc %1, %3 \n"
        "brcs cmp2_%= \n"
        "rjmp day_%= \n"
        "cmp1_%=:"
        "cp %0, %2 \n"
        "cpc %1, %3 \n"
        "brcs night_%= \n"
        "cmp2_%=:"
        "cp %0, %4 \n"
        "cpc %1, %5 \n"
        "brcc night_%= \n"
        "day_%=:"
        "cbi %6, %7 \n"
        "rjmp end_%= \n"
        "night_%=:"
        "sbi %6, %7 \n"
        "end_%=:"
        :
        : "r"(minute), "r"(hour), "r"(configPtr->dayTime.minute), "r"(configPtr->dayTime.hour),
        "r"(configPtr->nightTime.minute), "r"(configPtr->nightTime.hour), "I"(_SFR_IO_ADDR(flag)), "I"(DISP_NIGHT)
    );
    flag &= ~(1<<BLINK_LED);
    
    if (year >= 100)
    {
        dispPtr->year[0] = 15;
        dispPtr->year[1] = 15;
        dispPtr->year[2] = 15;
        dispPtr->year[3] = 15;
        flag &= ~(1<<DISP_OK);
    }
    else
    {
        dispPtr->second[0] = second % 10;
        dispPtr->second[1] = second / 10;
        dispPtr->minute[0] = minute % 10;
        dispPtr->minute[1] = minute / 10;
        dispPtr->hour[0] = hour % 10;
        dispPtr->hour[1] = hour / 10;
        dispPtr->dow = dow;
        
        dispPtr->month[0] = month % 10;
        dispPtr->month[1] = month / 10;
        day++;
        dispPtr->day[0] = day % 10;
        dispPtr->day[1] = day / 10;
        
        flag |= (1<<DISP_OK);
        dispPtr->year[0] = year % 10;
        dispPtr->year[1] = year / 10;
        dispPtr->year[2] = 0;
        dispPtr->year[3] = 2;
    }
}