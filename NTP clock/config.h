/*
 * config.h
 *
 * Created: 01.07.2018 11:40:51
 *  Author: Mikolaj
 */ 


#ifndef CONFIG_H
#define CONFIG_H

#include <inttypes.h>

#ifndef __AVR_ATtiny4313__
#ifdef __AVR_ATmega16__
#define EEPE EEWE
#define EEMPE EEMWE
#define OCIE0A OCIE0
#define GTCCR SFIOR
#else
#error "Unsupported device"
#endif
#endif

#define MAIN_DDR        DDRD
#define MAIN_PORT       PORTD
#define MAIN_PIN        PIND

#define LED             0
#define SW_1            2
#define SW_2            3
#define SPI_SI          4
#define SPI_SO          5
#define SPI_SCK         6

#ifdef __AVR_ATtiny4313__

#define F_CPU           12000000UL
#define TIMER_1S        46875
#define DEBOUNCE_CYCLES 7

#define PWR_SENSE       1

#define DISP_CS_DDR     DDRB
#define DISP_CS_PORT    PORTB
#define CS              3
#define DISP_SEL        0x07
#define DISP_SEG        0xF0

#else

#define F_CPU           16000000UL
#define TIMER_1S        62500
#define DEBOUNCE_CYCLES 10

#define TX              1
#define CS              7

#define DISP_SEG_DDR    DDRA
#define DISP_SEG_PORT   PORTA
#define DISP_SEG        0xFF

#define DISP_SEL_DDR    DDRB
#define DISP_SEL_PORT   PORTB
#define DISP_SEL_PIN    PINB
#define DISP_SEL        0x0F
#define SW_DISP_SEL     4

#endif

#define BRIGHTNESS_N    127

#define H(x) ((x)>>8)
#define L(x) ((x)&0xFF)

#define R_REG(x) asm ("" : "+r" ((x)));
#define E_REG(x) asm ("" : "+e" ((x)));
#define Y_REG(x) asm ("" : "+y" ((x)));

typedef __uint24 uint24_t;
typedef union
{
    uint16_t w;
    uint8_t b[2];
} u16;
typedef union
{
    uint32_t d;
    uint16_t w[2];
    uint8_t b[4];
} u32;

// ToDo: simplify these
#define WORD(lo, hi) ({ u16 x = {0}; x.b[0] = lo; x.b[1] = hi; x.w; })
#define DWORD(b0, b1, b2, b3) ({ u32 x = {0}; x.b[0] = b0; x.b[1] = b1; x.b[2] = b2; x.b[3] = b3; x.d; })

#endif /* CONFIG_H */