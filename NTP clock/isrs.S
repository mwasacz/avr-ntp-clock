#include <avr/io.h>
#include "config.h"

.extern mem
.extern netTick

#ifndef __AVR_ATtiny4313__
font:
;        0     1     2     3     4     5     6     7     8     9
.byte 0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
sel:
.byte (DISP_SEL & ~(1 << 0)) | (1 << SW_DISP_SEL)
.byte (DISP_SEL & ~(1 << 1)) | (1 << SW_DISP_SEL)
.byte (DISP_SEL & ~(1 << 2)) | (1 << SW_DISP_SEL)
.byte (DISP_SEL & ~(1 << 3)) | (1 << SW_DISP_SEL)
.byte DISP_SEL | (1 << SW_DISP_SEL)
.byte DISP_SEL | (1 << SW_DISP_SEL)
#endif

.global TIMER1_COMPB_vect
TIMER1_COMPB_vect:
    cbi _SFR_IO_ADDR(flag), BLINK_LED
    reti

.global TIMER0_OVF_vect
TIMER0_OVF_vect:
    push r24
    in r24, _SFR_IO_ADDR(SREG)
    push ZL
    push ZH
    sbi _SFR_IO_ADDR(flag), DISP_BLANK
    in ZL, _SFR_IO_ADDR(page)
    add ZL, ZL
    breq 1f
    in ZH, _SFR_IO_ADDR(OCR0A)
    subi ZH, 1
    brcc 2f
    ldi ZL, 16
1:  cbi _SFR_IO_ADDR(flag), DISP_BLANK
2:  ldi ZH, 7
    out _SFR_IO_ADDR(TIFR), ZH
    brvs 1f
    in ZH, _SFR_IO_ADDR(dispCnt)
#ifndef __AVR_ATtiny4313__
    sbis _SFR_IO_ADDR(DISP_SEL_PIN), SW_DISP_SEL
    subi ZL, -2
#endif
    cpi ZL, 24
    sbic _SFR_IO_ADDR(flag), SYNC_OK
    sbic _SFR_IO_ADDR(flag), BLINK_LED
    brcc 2f
1:  cbi _SFR_IO_ADDR(MAIN_PORT), LED
    rjmp 3f
2:  sbi _SFR_IO_ADDR(MAIN_PORT), LED
3:  subi ZH, 1
    brcc 1f
#ifdef __AVR_ATtiny4313__
    ldi ZH, 5
#else
    ldi ZH, 3
#endif
1:  out _SFR_IO_ADDR(dispCnt), ZH
    add ZL, ZH
    ldi ZH, 0
    subi ZL, lo8(-(mem + dispOffset))
    sbci ZH, hi8(-(mem + dispOffset))
    ld ZL, Z
#ifdef __AVR_ATtiny4313__
    swap ZL
    in ZH, _SFR_IO_ADDR(dispCnt)
    adc ZL, ZH
    sbic _SFR_IO_ADDR(DISP_CS_PORT), CS
    ori ZL, 1 << CS
    out _SFR_IO_ADDR(DISP_CS_PORT), ZL
#else
    ldi ZH, 0
    subi ZL, lo8(-(font))
    sbci ZH, hi8(-(font))
    lpm ZL, Z
    ldi ZH, DISP_SEL | (1 << SW_DISP_SEL)
    out _SFR_IO_ADDR(DISP_SEL_PORT), ZH
    out _SFR_IO_ADDR(DISP_SEG_PORT), ZL
    in ZL, _SFR_IO_ADDR(dispCnt)
    ldi ZH, 0
    subi ZL, lo8(-(sel))
    sbci ZH, hi8(-(sel))
    lpm ZL, Z
    out _SFR_IO_ADDR(DISP_SEL_PORT), ZL
#endif
    in ZL, _SFR_IO_ADDR(debounceCnt)
    subi ZL, 1
    brcs 1f
    out _SFR_IO_ADDR(debounceCnt), ZL
1:  pop ZH
    pop ZL
    out _SFR_IO_ADDR(SREG), r24
    rjmp isr_end

#ifdef __AVR_ATtiny4313__
.global TIMER0_COMPA_vect
TIMER0_COMPA_vect:
#else
.global TIMER0_COMP_vect
TIMER0_COMP_vect:
#endif
    sbis _SFR_IO_ADDR(flag), DISP_BLANK
    reti
    push r24
#ifdef __AVR_ATtiny4313__
    ldi r24, DISP_SEL | DISP_SEG
    sbic _SFR_IO_ADDR(DISP_CS_PORT), CS
    ldi r24, DISP_SEL | DISP_SEG | (1 << CS)
    out _SFR_IO_ADDR(DISP_CS_PORT), r24
#else
    ldi r24, DISP_SEL | (1 << SW_DISP_SEL)
    out _SFR_IO_ADDR(DISP_SEL_PORT), r24
    ldi r24, DISP_SEG
    out _SFR_IO_ADDR(DISP_SEG_PORT), r24
#endif
    cbi _SFR_IO_ADDR(MAIN_PORT), LED
    rjmp isr_end

.global TIMER1_COMPA_vect
TIMER1_COMPA_vect:
    push r24
    ldi r24, (1 << OCIE0A) | (1 << TOIE0)
    out _SFR_IO_ADDR(TIMSK), r24
    sei
    in r24, _SFR_IO_ADDR(SREG)
    push r0
    push r1
    push r18
    push r19
    push r20
    push r21
    push r22
    push r23
    push r24
    push r25
    push r26
    push r27
    push r30
    push r31
    eor r1, r1
    rcall netTick
    pop r31
    pop r30
    pop r27
    pop r26
    pop r25
    pop r24
    pop r23
    pop r22
    pop r21
    pop r20
    pop r19
    pop r18
    pop r1
    pop r0
    out _SFR_IO_ADDR(SREG), r24
    ldi r24, (1 << OCIE1A) | (1 << OCIE1B) | (1 << OCIE0A) | (1 << TOIE0)
    cli
    out _SFR_IO_ADDR(TIMSK), r24

isr_end:
    pop r24
    reti