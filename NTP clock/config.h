/*
 * config.h
 *
 * Created: 01.07.2018 11:40:51
 *  Author: Mikolaj
 */ 


#ifndef CONFIG_H
#define CONFIG_H

#include <inttypes.h>

// ToDo: decide to use UAA or LAA

#define SPI_PORT	PORTD
#define SPI_PIN		PIND
#define SPI_SO		5
#define SPI_SI		4
#define SPI_SCK		6

#define CS_PORT		PORTD//PORTB
#define CS			7//3

#define DISP_PORT	PORTB
#define DISP_SEL	0x07
#define DISP_SEG	0xF0

#define LED_PORT	PORTD
#define LED			0
#define LED_ON		LED_PORT &= ~(1<<LED)
#define LED_OFF		LED_PORT |= (1<<LED)
#define LED_ISOFF	LED_PORT & (1<<LED)

#define SW_PORT		PORTD
#define SW_PIN		PIND
#define SW_1		2
#define SW_2		3
#define SW_1_ISON	!(SW_PIN & (1<<SW_1))
#define SW_2_ISON	!(SW_PIN & (1<<SW_2))

#define VOL_PORT	PORTD
#define VOL_PIN		PIND
#define VOL			1
#define VOL_ISOFF	PIND & (1<<VOL)

#define H(x) ((x)>>8)
#define L(x) ((x)&0xFF)

#define R_REG(x) asm ("" : "+r" ((x)));
#define E_REG(x) asm ("" : "+e" ((x)));
#define Y_REG(x) asm ("" : "+y" ((x)));

//#define enc28j60ReadWordBE() ({ u16 x = {0}; x.b[1] = enc28j60ReadByte(); x.b[0] = enc28j60ReadByte(); x.w; })
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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif /* CONFIG_H */