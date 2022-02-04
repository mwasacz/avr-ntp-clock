#include <avr/io.h>
#include "config.h"

.extern mem
.extern netTick

#ifndef __AVR_ATtiny4313__
font:
;        0     1     2     3     4     5     6     7     8     9
.byte 0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
#endif

.global TIMER1_COMPB_vect
TIMER1_COMPB_vect:
    sbi _SFR_IO_ADDR(flag), BLINK_LED
    reti

.global TIMER0_OVF_vect
TIMER0_OVF_vect:
    push r0
    in r0, _SFR_IO_ADDR(SREG)
    push ZL
    push ZH
    in ZL, _SFR_IO_ADDR(page)
    sbis _SFR_IO_ADDR(flag), DISP_OK
    andi ZL, ~(1 << SW_2)
    mov ZH, ZL
    lsr ZH
    add ZL, ZH
    sbic _SFR_IO_ADDR(flag), SYNC_OK
    sbis _SFR_IO_ADDR(flag), BLINK_LED
    brhs 1f
    sbi _SFR_IO_ADDR(MAIN_PORT), LED
    rjmp 2f
1:  cbi _SFR_IO_ADDR(MAIN_PORT), LED
2:  in ZH, _SFR_IO_ADDR(dispCnt)
    subi ZH, 1
    brcc 1f
    ldi ZH, 5
1:  out _SFR_IO_ADDR(dispCnt), ZH
    add ZL, ZH
#ifndef __AVR_ATtiny4313__
    sbis _SFR_IO_ADDR(DISP_SEL_PIN), SW_DISP_SEL
    subi ZL, -2
#endif
    ldi ZH, 0
    subi ZL, lo8(-(mem))
    sbci ZH, hi8(-(mem))
    ld ZL, Z
#ifdef __AVR_ATtiny4313__
    swap ZL
    in ZH, _SFR_IO_ADDR(dispCnt)
    or ZL, ZH
    sbic _SFR_IO_ADDR(DISP_CS_PORT), CS
    ori ZL, 1 << CS
    out _SFR_IO_ADDR(DISP_CS_PORT), ZL
#else
    ldi ZH, 0
    subi ZL, lo8(-(font))
    sbci ZH, hi8(-(font))
    lpm ZL, Z
    ldi ZH, 1 << SW_DISP_SEL
    out _SFR_IO_ADDR(DISP_SEL_PORT), ZH
    out _SFR_IO_ADDR(DISP_SEG_PORT), ZL
    in ZL, _SFR_IO_ADDR(dispCnt)
    cpi ZL, 3
    brcs 1f
    add ZL, ZL
    andi ZL, (1 << 3) | (1 << 2)
1:  com ZL
    andi ZL, DISP_SEL | (1 << SW_DISP_SEL)
    out _SFR_IO_ADDR(DISP_SEL_PORT), ZL
#endif
    in ZL, _SFR_IO_ADDR(debounceCnt)
    subi ZL, 1
    brcs end
    out _SFR_IO_ADDR(debounceCnt), ZL
end:
    pop ZH
    pop ZL
    out _SFR_IO_ADDR(SREG), r0
    pop r0
    reti

#ifdef __AVR_ATtiny4313__
.global TIMER0_COMPA_vect
TIMER0_COMPA_vect:
#else
.global TIMER0_COMP_vect
TIMER0_COMP_vect:
#endif
    sbis _SFR_IO_ADDR(flag), DISP_NIGHT
    reti
    sbis _SFR_IO_ADDR(page), SW_2
    sbic _SFR_IO_ADDR(page), SW_1
    reti
#ifdef __AVR_ATtiny4313__
#if TIMER_DISP_D < 256
.global TIMER0_COMPB_vect
TIMER0_COMPB_vect:
#endif
#endif
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
    sbi _SFR_IO_ADDR(MAIN_PORT), LED
    pop r24
    reti

.global TIMER1_COMPA_vect
TIMER1_COMPA_vect:
    push r24
#ifndef __AVR_ATtiny4313__
    ldi r24, (1 << OCIE0) | (1 << TOIE0)
#elif TIMER_DISP_D < 256
    ldi r24, (1 << OCIE0A) | (1 << OCIE0B) | (1 << TOIE0)
#else
    ldi r24, (1 << OCIE0A) | (1 << TOIE0)
#endif
    out _SFR_IO_ADDR(TIMSK), r24
    sei
    push r1
    push r0
    in r0, _SFR_IO_ADDR(SREG)
    push r0
    eor r1, r1
    push r18
    push r19
    push r20
    push r21
    push r22
    push r23
    push r25
    push r26
    push r27
    push r30
    push r31
    rcall netTick
    pop r31
    pop r30
    pop r27
    pop r26
    pop r25
    pop r23
    pop r22
    pop r21
    pop r20
    pop r19
    pop r18
    pop r0
    out _SFR_IO_ADDR(SREG), r0
    pop r0
    pop r1
#ifndef __AVR_ATtiny4313__
    ldi r24, (1 << OCIE1A) | (1 << OCIE1B) | (1 << OCIE0) | (1 << TOIE0)
#elif TIMER_DISP_D < 256
    ldi r24, (1 << OCIE1A) | (1 << OCIE1B) | (1 << OCIE0A) | (1 << OCIE0B) | (1 << TOIE0)
#else
    ldi r24, (1 << OCIE1A) | (1 << OCIE1B) | (1 << OCIE0A) | (1 << TOIE0)
#endif
    cli
    out _SFR_IO_ADDR(TIMSK), r24
    pop r24
    reti