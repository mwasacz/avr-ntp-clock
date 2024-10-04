#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include <stdint.h>

// Useful type conversion helpers and arithmetic functions

typedef __uint24 uint24_t;

typedef union
{
    uint16_t u16;
    uint8_t u8[2];
} u16_t;

typedef union
{
    uint24_t u24;
    uint8_t u8[3];
} u24_t;

typedef union
{
    uint32_t u32;
    uint16_t u16[2];
    uint8_t u8[4];
} u32_t;

#define H(x)                    ((x) >> 8)
#define L(x)                    ((x) & 0xFF)

#define UINT16(b0, b1)          (((u16_t){ .u8 = { (b0), (b1) } }).u16)
#define UINT24(b0, b1, b2)      (((u24_t){ .u8 = { (b0), (b1), (b2) } }).u24)
#define UINT32(b0, b1, b2, b3)  (((u32_t){ .u8 = { (b0), (b1), (b2), (b3) } }).u32)

extern uint16_t mulAdd8Packed(uint16_t addMul1, uint8_t mul2);
extern uint32_t mulAdd16Packed(uint32_t addMul1, uint16_t mul2);

// Multiply and add 8-bit values to obtain 16-bit result
#define mulAdd8(add, mul1, mul2)    mulAdd8Packed(UINT16(mul1, add), mul2)
// Multiply and add 16-bit values to obtain 32-bit result
#define mulAdd16(add, mul1, mul2)   mulAdd16Packed(UINT32(L(mul1), H(mul1), L(add), H(add)), mul2)

#endif // ARITHMETIC_H
