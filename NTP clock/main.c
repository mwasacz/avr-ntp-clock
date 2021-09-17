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

#define SEC_OFFSET 3155670000
#define MAX_SEC 3155760000

/*----------------------------------------------------------------------*
 *  FONT DEFINITION                                                     *
 *----------------------------------------------------------------------*/

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

/*----------------------------------------------------------------------*
 *  VARIABLES                                                           *
 *----------------------------------------------------------------------*/

//static uint32_t time = 0;

//                             SEC  MIN  HOUR  WEEK  MON  DAY   MODE  YEAR
static uint8_t disp[24] = {0,0, 0,0, 0,0,  6,10, 1,0, 1,0,  0,10, 0,0,0,2, 0,0,0,0,0,10};
//                       BLINK      X X  X X         X X  X X   X     X X
//                       CNT   1 2  3 4  5 6   1 2   3 4  5 6   1 2   3 4 5 6
//                       PAGE  0 0  0 0  0 0   1 1   1 1  1 1   2 2   2 2 2 2
#define state disp[12]

static uint8_t page = 0;//12;

//static volatile uint8_t dig[5] = {1,1,1,1,1};

#define flag TWAR
#define TIME_OK (1<<1)
//#define SUBMENU (1<<3)
//#define VALOVF (1<<4)

/*----------------------------------------------------------------------*
 *                                                                      *
 *  FUNCTIONS                                                           *
 *                                                                      *
 *----------------------------------------------------------------------*/

/*static uint8_t monthLen(uint8_t mon)
{
    if (mon == 2)
    {
        if ((datetime[14] & 3) != (datetime[15] & 2))
        return 28;
        else
        return 29;
    }
    else
    {
        uint8_t x;
        if (mon < 8)
        x = mon;
        else
        x = mon - 1;
        return 30 + (x & 1);
    }
}*/

/*static void test()
{
    uint8_t year = dig[4];
    uint16_t days = (uint16_t)year * 365;
    days += (uint8_t)(year + 3) >> 2;
    days += ((uint16_t)dig[3] * 367 - 362) / 12; // ToDo
    days += dig[2];
    uint32_t hours = (uint32_t)days * 24;
    hours += dig[1];
    uint32_t minutes = (uint32_t)hours * 60;
    minutes += dig[0];
    TCNT1 = minutes;
    TCNT1 = minutes >> 16;
}*/

static uint8_t isDst(uint8_t day, uint8_t monthL, uint8_t dow, uint8_t hour)
{
    if (monthL == 1) return 0;
    if (monthL == 2) return 0;
    if (monthL > 3) return 1;

    if (dow == 7 && day > 24 && hour > 1) return (monthL != 0);
    
    uint8_t previousSunday = day - dow;
    if (previousSunday > 24) return (monthL != 0);
    return (monthL == 0);
}

static uint8_t date(uint24_t x)
{
    uint32_t th = x;
    uint8_t hours = th % 24;
    disp[4] = hours % 10;
    disp[5] = hours / 10;

    uint16_t td = th / 24;

    uint8_t dow = (td + 5) % 7;
    dow++;
    disp[6] = dow;

    //uint32_t x = td << 2;
    //uint8_t year = x / 1461;
    //uint16_t days = (x % 1461) >> 2;
    
    uint16_t temp = 366;
    uint8_t year = 0;
    while (td >= temp)
    {
        td -= temp;
        year++;
        temp = 365 + !(year & 3);
    }
    disp[14] = year % 10;
    disp[15] = year / 10;
    
    uint8_t mon = 1;
    uint8_t tmp = 31;
    while (td >= tmp)
    {
        if (mon == 2)
            tmp = temp - 365 + 28;
        else
        {
            uint8_t x;
            if (mon < 8)
                x = mon;
            else
                x = ~mon;
            tmp = 30 + (x & 1);
        }
        td -= tmp;
        mon++;
    }
    uint8_t day = td + 1;
    
    //uint8_t leap = !(year & 3);
    //if (days > 58 + leap)
    //    days = days + 2 - leap;
    //
    //uint8_t mon = (uint16_t)(days * 12) / 367;
    //uint8_t day = days - (uint16_t)(mon * 367) / 12;

    disp[10] = day % 10;
    disp[11] = day / 10;
    uint8_t monthL = mon % 10;
    disp[8] = monthL;
    disp[9] = mon / 10;

    return isDst(day, monthL, dow, hours);
}

static void displayTime(uint32_t tim)
{
    uint32_t ts = tim - SEC_OFFSET;//= (time + UTC_OFFSET * 3600);

    if (ts > MAX_SEC)
        ts = MAX_SEC;

    uint16_t s = ts % 3600;
    uint24_t th = ts / 3600;

    // SECONDS
    uint8_t seconds = s % 60;
    disp[0] = seconds % 10;
    disp[1] = seconds / 10;
    // MINUTES
    uint8_t minutes = s / 60;
    disp[2] = minutes % 10;
    disp[3] = minutes / 10;

    //uint8_t minutes = tm % 60;
    //uint32_t th = tm / 60;

    //th += 1;//offset;

    //uint8_t y = th / 8766;
    //uint16_t z = th % 8766;
    //
    //uint16_t start = (uint16_t)START_DST + DSTlookup[(uint8_t)(y + 8) % 28];
    //uint16_t end = (uint16_t)END_DST + DSTlookup[(uint8_t)y % 28];
    //
    //if (z >= start && z < end)
    //    th++;
    if(date(th))
        date(th + 1);
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
    while (EECR & (1<<EEWE));

    uint16_t c = 18;
    uint8_t* ptr = &net.myMac[18];
    do
    {
        EEAR = --c;
        EECR |= (1<<EERE);
        *--ptr = EEDR;
    } while (c);
}

static void eepromWrite(uint8_t p, uint8_t val)
{
    while (EECR & (1<<EEWE));

    EEAR = p;
    EEDR = val;
    cli(); // ToDo
    EECR |= (1<<EEMWE);
    EECR |= (1<<EEWE);
    sei();
}

/*----------------------------------------------------------------------*
 *                                                                      *
 *  MAIN                                                                *
 *                                                                      *
 *----------------------------------------------------------------------*/

//uint8_t ee[10] EEMEM = { MAC5,MAC4,MAC3,MAC2,MAC1,MAC0, 104,5,168,192 };

int main()
{
    //PORTB = (1<<CS);

    //DDRB  = (1<<CS) | DISP_SEL | DISP_SEG;
    DDRD  = /*(1<<LED) |*/ (1<<SPI_SI) | (1<<SPI_SCK) | (1<<1) | (1<<CS);

    PORTA = 0xFF;
    DDRA  = 0xFF;
    PORTB = 0x0F;
    DDRB  = 0x0F;
    //PORTC = 0xFF;
    //DDRC  = 0xFF;

    PORTD = /*(1<<SPI_SO) |*/ (1<<SW_1) | (1<<SW_2) | (1<<VOL) | (1<<CS) | (1<<LED);
    
    //ACSR  = (1<<ACBG) | (1<<ACIE);
    ACSR  = (1<<ACD);
    //PRR   = (1<<PRUSI) | (1<<PRUSART);

    // Timer config
    //TCCR0A= (1<<WGM01);// | (1<<CS02);
    TCCR0 = (1<<CS01) | (1<<CS00);//(1<<CS02);
    OCR0  = 127;
    //OCR0A  = 103;//207;
    
    TCCR1B= (1<<CS12) | (1<<WGM13) | (1<<WGM12);
    ICR1  = 62499;//46874
    OCR1A = 31249;//23436
    //TCCR1B= (1<<CS12) | (1<<WGM12);
    //OCR1A = 62499;//46874
    //OCR1B = 31249;//23436
    
    TIMSK = (1<<OCIE1A) | (1<<TICIE1)/* | (1<<OCIE1B)*/ | (1<<TOIE0);//(1<<OCIE0A);

    MCUCR = (1<<ISC10) | (1<<ISC00);// | (1<<SE);
    //PCMSK2= (1<<PCINT12);
    //GICR = (1<<INT1) | (1<<INT0);// | (1<<PCIE2);

    UCSRB = (1<<TXEN);
    UBRRL = 20;

    // ToDo: check SW1,2_ISON and set page
    
    
    eepromRead();

    sei();

    while (1)
    {
    init();
    enc28j60Init();
    enc28j60WriteMac(&net.myMac[6]); // Do in enc28j60Init

    uint8_t old;
    uint16_t NextPacketPtr = RXSTART_INIT;
    uint8_t st = 1;
    while (1)
    {
        //if (!enc28j60LinkUp())
        //{
        //    state = 0;
        //    tim = 0;
        //    while (!enc28j60LinkUp());
        //    C8(x, 1);
        //    state = x;
        //}

        //uint8_t test0 = PINA;
        //uint8_t test1 = PINA;
        //uint8_t test2 = PINA;
        //uint8_t test3 = PINA;
        //uint8_t test4 = PINA;
        //uint8_t test5 = PINA;
        //uint8_t test6 = PINA;
        //uint8_t test7 = PINA;


        if (enc28j60LinkUp())
        {
            loop(&st, &NextPacketPtr);
            state = st;
        }
        else
        {
            st = 1;
            state = 0;
            retryTime = 0;
        }

        //while (PIND & (1<<LED)); // ToDo: test buffer overflow
        //PINA = test0;
        //PINA = test1;
        //PINA = test2;
        //PINA = test3;
        //PINA = test4;
        //PINA = test5;
        //PINA = test6;
        //PINA = test7;

        checkPinChange();
        
        if (TIFR & (1<<OCF1B))
        {
            old = SW_PIN;

            if (!(old & (1<<SW_1)))
            {
                if (!(old & (1<<SW_2)))
                    break;
                else
                    page = 6;
            }
            else if (!(old & (1<<SW_2)))
                page = 12;
            else
                page = 0;
        }
    }
    page = 18;

    uint8_t addr = 10;
    uint8_t dig1 = 0, dig2 = 0, dig3 = 0;//, val = 0;
    uint8_t pos = 0;

    while (1)
    {
        checkPinChange();

        if (TIFR & (1<<OCF1B))
        {
            uint8_t new = SW_PIN;
            uint8_t btn = old & ~new;
            old = new;

            if (btn & (1<<SW_1))
            {
                if (pos == 0)
                {
                    uint8_t newVal = dig1;
                    newVal *= 10;
                    newVal += dig2;
                    newVal *= 10;
                    newVal += dig3;

                    uint8_t* ptr = &net.myMac[0] + addr;
                    uint8_t val = *ptr;
                    if (val != newVal)
                    {
                        *ptr = newVal;
                        eepromWrite(addr, newVal);
                    }

                    val = *--ptr;// ToDo: ld -Z
                    dig1 = val / 100;
                    val %= 100;
                    dig2 = val / 10;
                    dig3 = val % 10;
                    
                    addr--;
                    if (addr == 255) // ToDo
                        break;
                    disp[19] = 0;//addr >> 2;
                    disp[18] = 9 - addr;// & 3;
                }
                else if (pos == 1)
                {
                    dig1++;
                    if (dig1 > 2)
                        dig1 = 0;
                    else if (dig1 == 2 && (uint8_t)((dig2 << 4) | (dig2 >> 4) | dig3) > 0x55)//val > 55)
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
            if (btn & (1<<SW_2))
            {
                pos++;
                pos &= 3;
                uint8_t p;
                if (pos == 0)
                    p = 10;
                else
                    p = pos;

                disp[23] = p;
                    
            }
            disp[20] = dig3;
            disp[21] = dig2;
            disp[22] = dig1;
        }
    }
    }
}

/*----------------------------------------------------------------------*
 *                                                                      *
 *  INTERRUPT SERVICE ROUTINES                                          *
 *                                                                      *
 *----------------------------------------------------------------------*/

// ToDo: ISR_NOBLOCK
//ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
ISR(TIMER1_CAPT_vect, ISR_NOBLOCK)
{
    //LED_ON;
    
    if (retryTime)
        retryTime--;
    
    net_t *ptr = &net;
    E_REG(ptr);

    if (ptr->syncTime)
        ptr->syncTime--;
    else if (state == 6) // ToDo: set flag instead
        state = 4;

    if (ptr->leaseTime)
        ptr->leaseTime--;
    else if (state > 3) // ToDo: set flag instead
        state = 3;

    if (flag & TIME_OK)
    {
        //test();
        uint32_t t = ptr->time + 1;
        ptr->time = t;
        R_REG(t);
        displayTime(t);
    }
}

//ISR(TIMER1_COMPB_vect, ISR_NAKED)
ISR(TIMER1_COMPA_vect, ISR_NAKED)
{
    //LED_OFF;
    //reti();
    asm (
        //"sbi %0, %1 \n"
        "reti \n"
        :
        : "I"(_SFR_IO_ADDR(LED_PORT)), "I"(LED)
    );
}

ISR(TIMER0_OVF_vect)
{
    static uint8_t cnt;

    //PORTD = ~(1<<cnt) & 0x3F;
    //PORTC = data[cnt];
    uint8_t x = cnt + page;

    if (!(PIND & (1<<LED)))
        x += 2;

    //cnt++;

    //if (blink && blink == x && LED_ISOFF)
    //    DISP_PORT = (DISP_PORT & ~(DISP_SEL | DISP_SEG));
    //else
    //    DISP_PORT = (DISP_PORT & ~(DISP_SEL | DISP_SEG)) | (datetime[x] << 4) | cnt;
    PORTB = (~(1<<cnt)) & 0x0F;
    PORTA = font[disp[x]];
    cnt++;
    if (cnt > 5)
    {
        cnt = 0;
    }
}

/*
// ToDo: use ISR_NAKED, consider situation when both pressed
ISR(INT0_vect)
{
        if (SW_1_ISON && SW_2_ISON)
            page = 14;
        else if (SW_1_ISON)
            page = 4;
        else if (SW_2_ISON)
            page = 8;
        else
            page = 0;
}

ISR(INT1_vect, ISR_ALIASOF(INT0_vect));*/