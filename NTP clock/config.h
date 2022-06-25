/*
 * config.h
 *
 * Created: 01.07.2018 11:40:51
 *  Author: Mikolaj
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <avr/io.h>

#ifndef __AVR_ATtiny4313__
#ifdef __AVR_ATmega16__
#define EEPE            EEWE
#define EEMPE           EEMWE
#define OCIE0A          OCIE0
#define OCR0A           OCR0
#define GTCCR           SFIOR
#else
#error "Unsupported device"
#endif
#endif

#ifdef __AVR_ATtiny4313__
#define LFUSE           FUSE_SUT1
#define HFUSE           (FUSE_EESAVE & FUSE_SPIEN & FUSE_BODLEVEL1 & FUSE_BODLEVEL0)
#define EFUSE           0xFF
#else
#define LFUSE           (FUSE_BODLEVEL & FUSE_BODEN & FUSE_SUT1)
#define HFUSE           (FUSE_SPIEN & FUSE_CKOPT & FUSE_EESAVE & FUSE_BOOTSZ1 & FUSE_BOOTSZ0)
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

#define F_CPU           12000000
#define TIMER_1S        46875
#define DEBOUNCE_CYCLES 10

#define PWR_SENSE       1

#define DISP_CS_DDR     DDRB
#define DISP_CS_PORT    PORTB
#define CS              3
#define DISP_SEL        0x07
#define DISP_SEG        0xF0

#else

#define F_CPU           16000000
#define TIMER_1S        62500
#define DEBOUNCE_CYCLES 13

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

#ifdef __AVR_ATtiny4313__
#define flag            PCMSK2
#define page            GPIOR0
#define dispCnt         PCMSK1
#define debounceCnt     PCMSK
#else
#define flag            TWBR
#define page            TWAR
#define dispCnt         TCNT2
#define debounceCnt     OCR2
#endif
#define dispOffset      26

#define TIME_OK         0
#define SYNC_OK         1
#define BLINK_LED       2
#define DISP_BLANK      3
#define CUSTOM_IP       4
#define ARP_REPLY       5

#endif /* CONFIG_H */