/*
 * config.h
 *
 * Created: 01.07.2018 11:40:51
 *  Author: Mikolaj
 */ 


#ifndef CONFIG_H
#define CONFIG_H

// ToDo: decide to use UAA or LAA
//#define MAC0		'N'
//#define MAC1		'T'
//#define MAC2		'P'
//#define MAC3		'c'
//#define MAC4		'l'
//#define MAC5		'k'

//#define TRANS_NUM_GWMAC 1
//#define SOURCE_PORT_H 10//0xF0
//#define SOURCE_PORT_L 0x28//0x00

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
#define CONST8(val) ({ uint8_t x; asm volatile ("ldi %0, %1 \n" : "=d" (x) : "M" ((val))); x; })
#define CONST16(val) ({ uint16_t x; asm volatile ("ldi %A0, %1 \n ldi %B0, %2 \n" : "=d" (x) : "M" (L((val))), "M" (H((val)))); x; })
#define LD_Y(ptr) ({ uint8_t x; asm volatile ("ld %1, -%a0 \n" : "+y" ((ptr)), "=r" (x)); x; })
#define ST_Y(ptr, val) asm volatile ("st -%a0, %1 \n" : "+y" ((ptr)) : "r" ((val)))

//#define enc28j60ReadWordBE() ({ u16 x = {0}; x.b[1] = enc28j60ReadByte(); x.b[0] = enc28j60ReadByte(); x.w; })
typedef __uint24 uint24_t;
typedef union
{
	uint16_t w;
	uint8_t b[2];
} u16;

#endif /* CONFIG_H */