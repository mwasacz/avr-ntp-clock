#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include "config.h"
#include "arithmetic.h"

mem_t mem __attribute__((section(".noinit")));

// Brightness correction for displays
// Note: 128 is a special value that disables the display
const __flash uint8_t brightnessVal[10] = { 128, 4, 12, 25, 45, 71, 104, 145, 195, 255 };

// Set time to timInt and load Timer1 with timFrac
void resetTimer(uint32_t timInt, uint16_t timFrac)
{
    uint32_t *timePtr = &mem.time;
    B_REG(timePtr);
    uint16_t *syncTimePtr = (uint16_t *)((uint8_t *)timePtr - offsetof(mem_t, time) + offsetof(mem_t, syncTime));

    uint8_t gtccr = (1 << PSR10);
    uint8_t tifr = (1 << OCF1A) | (1 << OCF1B);
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
    flag |= (1 << TIME_OK);
    flag |= (1 << SYNC_OK);
    sei();
}

// Swap timezone pointers
static void swapPtr(timezone_t **tz, timezone_t **tzAlt)
{
    timezone_t *temp = *tz;
    *tz = *tzAlt;
    *tzAlt = temp;
    R_REG(*tz);
    R_REG(*tzAlt);
}

// Update time display
void displayTime(uint32_t time)
{
    time -= SEC_OFFSET;

    // Split time into seconds and total minutes
    uint8_t second = time % 60;
    uint32_t tmm = time / 60;

    timezone_t *tzAlt = &mem.config.timezones[1];
    timezone_t *tz = &mem.config.timezones[0];
    uint8_t cmpCnt = 8;
    uint8_t cnt = 8;
    R_REG(tzAlt);
    B_REG(tz);
    R_REG(cmpCnt);
    R_REG(cnt);

    // Compare timezone end dates, swap so that tz ends before tzAlt
    uint8_t *tzAltEndPtr = &mem.config.timezones[1].endMinute + 5;
    uint8_t *tzEndPtr = &mem.config.timezones[0].endMinute + 5;
    while (1)
    {
        uint8_t tzAltEnd = *--tzAltEndPtr;
        uint8_t tzEnd = *--tzEndPtr;
        if (tzEnd < tzAltEnd)
            break;
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

    uint8_t minute, hour, dow, year, month, day, yearAlt;

    // Loop which is repeated at most three times
    // In iteration 0, we calculate time according to timezone tz, and check if it's before tz end time
    // In iteration 1, we calculate time according to timezone tzAlt, and check if it's before tzAlt end time
    // In iteration 2, we calculate time according to timezone tz again
    while (1)
    {
        uint16_t offset = mulAdd8(tz->offsetMinute, tz->offsetHour, 60);
        uint32_t x = tmm;
        R_REG(x);

        // Add or subtract offset
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

        // Split total minutes into total days and minutes within day
        uint16_t tm = x % (24 * 60);
        uint16_t td = x / (24 * 60);
        R_REG(td);

        // Split minutes within day into minutes and hours
        uint32_t tm32 = tm;
        minute = tm32 % 60;
        hour = tm32 / 60;
        R_REG(minute);
        R_REG(hour);

        // Calculate day of week
        uint32_t td32 = td + 5;
        dow = td32 % 7;
        R_REG(dow);
        dow++;

        // Split total days into year and days within year
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
            : "=&d" (t4d)
            : "r" (td)
        );
        year = t4d / 1461;
        td = t4d % 1461;
        td >>= 2;

        // Split days within year into months and days
        month = 1;
        uint8_t temp;
        while (1)
        {
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
            if (td < temp)
                break;
            td -= temp;
            month++;
        }
        day = td;

        // Calculate week within month, end week and end day of week
        uint8_t d = day;
        uint8_t endWeek = tz->endWeek;
        if (endWeek >= 5)
            d = d + 35 - temp;
        uint8_t endDow = tz->endDow;
        d = d + 7 + endDow - dow;
        uint8_t week = d / 7;

        // If current time is less than tz end time, decrement cnt
        uint8_t endMinute = tz->endMinute;
        uint8_t endHour = tz->endHour;
        uint8_t endMonth = tz->endMonth;
        asm (
            "cp %2, %8 \n"
            "cpc %3, %9 \n"
            "cpc %4, %10 \n"
            "cpc %5, %11 \n"
            "cpc %6, %12 \n"
            "cpse %7, %1 \n"
            "sbrs %0, 2 \n"
            "sbc %0, __zero_reg__ \n"
            "mov %1, %7 \n"
            : "+r" (cnt), "+r" (yearAlt)
            : "r" (minute), "r" (hour), "r" (dow), "r" (week), "r" (month), "r" (year), "r" (endMinute), "r" (endHour),
            "r" (endDow), "r" (endWeek), "r" (endMonth)
        );

        // If cnt has been decremented or this is the third iteration, break the loop
        cnt >>= 1;
        if (cnt & 1)
            break;

        swapPtr(&tz, &tzAlt);
    }

    disp_t *dispPtr = &mem.disp;
    B_REG(dispPtr);
    config_t *configPtr = (config_t *)((uint8_t *)dispPtr - offsetof(mem_t, disp) + offsetof(mem_t, config));

    if (year >= 100)
    {
        // Years beyond 2099 are not supported, so turn off time and date display
        dispPtr->year[0] = 15;
        dispPtr->year[1] = 15;
        dispPtr->year[2] = 15;
        dispPtr->year[3] = 15;
        OCR0A = 0;
    }
    else
    {
        // Check if it's daytime or nighttime and set brightness accordingly
        uint8_t level = configPtr->dayBrightness.level;
        asm (
            "cp %4, %6 \n"
            "cpc %5, %7 \n"
            "brcs cmp1_%= \n"
            "cp %2, %4 \n"
            "cpc %3, %5 \n"
            "brcs cmp2_%= \n"
            "rjmp end_%= \n"
            "cmp1_%=:"
            "cp %2, %4 \n"
            "cpc %3, %5 \n"
            "brcs night_%= \n"
            "cmp2_%=:"
            "cp %2, %6 \n"
            "cpc %3, %7 \n"
            "brcs end_%= \n"
            "night_%=:"
            "ldd %0, %a1 + %8 \n"
            "end_%=:"
            : "+r" (level)
            : "b" (dispPtr), "r" (minute), "r" (hour), "r" (configPtr->nightBrightness.endMinute),
            "r" (configPtr->nightBrightness.endHour), "r" (configPtr->dayBrightness.endMinute),
            "r" (configPtr->dayBrightness.endHour),
            "I" (offsetof(mem_t, config.nightBrightness.level) - offsetof(mem_t, disp))
        );
        OCR0A = brightnessVal[level];

        // Update display page 3
        dispPtr->second[0] = second % 10;
        dispPtr->second[1] = second / 10;
        dispPtr->minute[0] = minute % 10;
        dispPtr->minute[1] = minute / 10;
        dispPtr->hour[0] = hour % 10;
        uint8_t h = hour / 10;
        if (h == 0)
            h = 15;
        dispPtr->hour[1] = h;
        dispPtr->dow = dow;

        // Update display page 1
        dispPtr->month[0] = month % 10;
        dispPtr->month[1] = month / 10;
        day++;
        dispPtr->day[0] = day % 10;
        uint8_t d = day / 10;
        if (d == 0)
            d = 15;
        dispPtr->day[1] = d;

        // Update display page 2
        dispPtr->year[0] = year % 10;
        dispPtr->year[1] = year / 10;
        dispPtr->year[2] = 0;
        dispPtr->year[3] = 2;
    }
    flag |= (1 << BLINK_LED);
}
