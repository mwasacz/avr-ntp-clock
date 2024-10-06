; This file is part of AVR NTP Clock
; Copyright (C) 2018-2022 Mikolaj Wasacz
; SPDX-License-Identifier: GPL-2.0-only

; Multiply and add 8-bit values to obtain 16-bit result
addL = 25
mul1L = 24
mul2L = 22
cnt = 18
.global mulAdd8Packed
mulAdd8Packed:
    ldi cnt, 9
    rjmp 3f
1:  brcc 2f
    add addL, mul2L
2:  ror addL
3:  ror mul1L
    dec cnt
    brne 1b
    ret

; Multiply and add 16-bit values to obtain 32-bit result
addL = 24
addH = 25
mul1L = 22
mul1H = 23
mul2L = 20
mul2H = 21
cnt = 18
.global mulAdd16Packed
mulAdd16Packed:
    ldi cnt, 17
    rjmp 3f
1:  brcc 2f
    add addL, mul2L
    adc addH, mul2H
2:  ror addH
    ror addL
3:  ror mul1H
    ror mul1L
    dec cnt
    brne 1b
    ret
