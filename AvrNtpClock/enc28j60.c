// This file is part of AVR NTP Clock
// Copyright (C) 2018-2022 Mikolaj Wasacz
// SPDX-License-Identifier: GPL-2.0-only

// This file includes code from Tuxgraphics NTP clock by Guido Socher, covered by the following copyright notice:

// Author: Guido Socher 
// Copyright:LGPL V2
// See http://www.gnu.org/licenses/old-licenses/lgpl-2.0.html
//
// Based on the enc28j60.c file from the AVRlib library by Pascal Stang.
// For AVRlib See http://www.procyonengineering.com/
// Used with explicit permission of Pascal Stang.

// This file includes code from Procyon AVRlib by Pascal Stang, covered by the following copyright notice:

// File Name    : 'enc28j60.c'
// Title        : Microchip ENC28J60 Ethernet Interface Driver
// Author       : Pascal Stang (c)2005
// Created      : 9/22/2005
// Revised      : 9/22/2005
// Version      : 0.1
// Target MCU   : Atmel AVR series
// Editor Tabs  : 4

#include "enc28j60.h"

#include <avr/io.h>
#include "config.h"
#include "common.h"
#include "arithmetic.h"

#ifdef __AVR_ATtiny4313__
#define CS_LO   DISP_CS_PORT &= ~(1 << CS)
#define CS_HI   DISP_CS_PORT |= (1 << CS)
#else
#define CS_LO   MAIN_PORT &= ~(1 << CS)
#define CS_HI   MAIN_PORT |= (1 << CS)
#endif

#define SO_IN   MAIN_PIN & (1 << SPI_SO)
#define SI_HI   MAIN_PORT |= (1 << SPI_SI)
#define SI_LO   MAIN_PORT &= ~(1 << SPI_SI)
#define SCK_HI  MAIN_PORT |= (1 << SPI_SCK)
#define SCK_LO  MAIN_PORT &= ~(1 << SPI_SCK)

// Initialization values for ENC28J60 registers
const __flash uint8_t regInitVal[42] = {
    // Set receive buffer start address
    ENC28J60_WRITE_CTRL_REG | B0_ERXSTL, L(RXSTART_INIT),
    ENC28J60_WRITE_CTRL_REG | B0_ERXSTH, H(RXSTART_INIT),
    // Set receive pointer address
    ENC28J60_WRITE_CTRL_REG | B0_ERXRDPTL, L(RXSTOP_INIT),
    ENC28J60_WRITE_CTRL_REG | B0_ERXRDPTH, H(RXSTOP_INIT),
    // Set receive buffer end address
    ENC28J60_WRITE_CTRL_REG | B0_ERXNDL, L(RXSTOP_INIT),
    ENC28J60_WRITE_CTRL_REG | B0_ERXNDH, H(RXSTOP_INIT),
    // Set transmit buffer start address
    ENC28J60_WRITE_CTRL_REG | B0_ETXSTL, L(TXSTART_INIT),
    ENC28J60_WRITE_CTRL_REG | B0_ETXSTH, H(TXSTART_INIT),
    // Select bank 2
    ENC28J60_WRITE_CTRL_REG | ECON1, ECON1_BSEL1,
    // Enable MAC receive
    ENC28J60_WRITE_CTRL_REG | B2_MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS,
    // Bring MAC out of reset
    ENC28J60_WRITE_CTRL_REG | B2_MACON2, 0,
    // Enable automatic padding and CRC operations
    ENC28J60_WRITE_CTRL_REG | B2_MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN,
    // Allow infinite deferrals
    ENC28J60_WRITE_CTRL_REG | B2_MACON4, MACON4_DEFER,
    // Increase collision window to reduce the number of false late collision errors
    ENC28J60_WRITE_CTRL_REG | B2_MACLCON2, 63,
    // Set inter-frame gap (non-back-to-back)
    ENC28J60_WRITE_CTRL_REG | B2_MAIPGL, 0x12,
    ENC28J60_WRITE_CTRL_REG | B2_MAIPGH, 0x0C,
    // Set inter-frame gap (back-to-back)
    ENC28J60_WRITE_CTRL_REG | B2_MABBIPG, 0x12,
    // Set the maximum packet size which the controller will accept
    ENC28J60_WRITE_CTRL_REG | B2_MAMXFLL, L(MAX_FRAMELEN),
    ENC28J60_WRITE_CTRL_REG | B2_MAMXFLH, H(MAX_FRAMELEN),
    // Select bank 3
    ENC28J60_WRITE_CTRL_REG | ECON1, ECON1_BSEL1 | ECON1_BSEL0,
    // Disable clock output
    ENC28J60_WRITE_CTRL_REG | B3_ECOCON, 0
};

// Wait for 12us
static void __attribute__((noinline)) delay12us()
{
    __builtin_avr_delay_cycles(F_CPU * 12 / 1000000);
}

// Wait for 1ms
static void __attribute__((noinline)) delay1ms()
{
    __builtin_avr_delay_cycles(F_CPU / 1000 + 1);
}

// Exchange byte with ENC28J60 via SPI
uint8_t spiTransfer(uint8_t data)
{
    uint8_t i = 8;
    do
    {
        SI_LO;
        if (data & 0x80)
            SI_HI;
        data <<= 1;
        SCK_HI;
        if (SO_IN)
            data |= 1;
        SCK_LO;
    } while (--i);
    return data;
}

// Exchange byte with ENC28J60 via SPI, sent byte is 0x00 (requires less overhead to call)
uint8_t spiTransferZero()
{
    return spiTransfer(0);
}

// Read ENC28J60 register (opAddr must be a combined operation code and register address)
static uint8_t enc28j60ReadOp(uint8_t opAddr)
{
    // Assert CS
    CS_LO;

    // Issue read command
    spiTransfer(opAddr);
    // Read data
    uint8_t data = spiTransferZero();

    // Release CS
    CS_HI;

    return data;
}

// Read ENC28J60 MAC or MII register (opAddr must be a combined operation code and register address)
static uint8_t enc28j60ReadOpMacMii(uint8_t opAddr)
{
    // Assert CS
    CS_LO;

    // Issue read command
    spiTransfer(opAddr);
    // Dummy read
    spiTransferZero();
    // Read data
    uint8_t data = spiTransferZero();

    // Release CS
    CS_HI;

    return data;
}

// Write data to ENC28J60 register (opAddr must be a combined operation code and register address)
static void enc28j60WriteOp(uint8_t opAddr, uint8_t data)
{
    // Assert CS
    CS_LO;

    // Issue write command
    spiTransfer(opAddr);
    // Write data
    spiTransfer(data);

    // Release CS
    CS_HI;
}

// Write data to a pair of ENC28J60 registers (opAddr must be a combined operation code and register address)
static void enc28j60WriteOpPair(uint8_t opAddr, uint16_t data)
{
    enc28j60WriteOp(opAddr, L(data));
    enc28j60WriteOp(opAddr + 1, H(data));
}

// Read ENC28J60 register
#define enc28j60Read(address)               enc28j60ReadOp(ENC28J60_READ_CTRL_REG | (address))
// Read ENC28J60 MAC or MII register
#define enc28j60ReadMacMii(address)         enc28j60ReadOpMacMii(ENC28J60_READ_CTRL_REG | (address))
// Write data to ENC28J60 register
#define enc28j60Write(address, data)        enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG | (address), (data))
// Set bits in ENC28J60 register
#define enc28j60BitFieldSet(address, data)  enc28j60WriteOp(ENC28J60_BIT_FIELD_SET | (address), (data))
// Clear bits in ENC28J60 register
#define enc28j60BitFieldClr(address, data)  enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR | (address), (data))
// Write data to a pair of ENC28J60 registers
#define enc28j60WritePair(address, data)    enc28j60WriteOpPair(ENC28J60_WRITE_CTRL_REG | (address), (data))

// Write data to ENC28J60 ECON1 register (requires less overhead to call)
static void enc28j60WriteECON1(uint8_t data)
{
    enc28j60Write(ECON1, data);
}

// Read low byte of ENC28J60 PHY register
static uint8_t enc28j60PhyReadL(uint8_t address)
{
    // Select bank 2
    enc28j60WriteECON1(ECON1_RXEN | ECON1_BSEL1);

    // Set the right address and start the register read operation
    enc28j60Write(B2_MIREGADR, address);
    enc28j60Write(B2_MICMD, MICMD_MIIRD);

    // Reading a PHY register takes 10.24 us
    delay12us();

    // Quit reading
    enc28j60Write(B2_MICMD, 0);

    // Get low byte
    return enc28j60ReadMacMii(B2_MIRDL);
}

// Initialize ENC28J60
uint8_t enc28j60Init(uint8_t *myMac)
{
    // Exit Power Save mode (rev. B7 Silicon Errata point 19)
    enc28j60BitFieldClr(ECON2, ECON2_PWRSV);

    // Wait 1 ms after exiting Power Save mode
    delay1ms();

    // Perform system reset
    CS_LO;
    spiTransfer(ENC28J60_SOFT_RESET);
    CS_HI;

    // Wait 1 ms after issuing the reset command (rev. B7 Silicon Errata point 2)
    delay1ms();

    // Verify clock is ready and unimplemented bit reads as 0
    if ((enc28j60Read(ESTAT) & (0x08 | ESTAT_CLKRDY)) != ESTAT_CLKRDY)
        return 0;

    // Copy register values from flash to ENC28J60
    uint8_t c = 42 / 2;
    const __flash uint8_t *ptr = regInitVal;
    R_REG(c);
    Y_REG(ptr);
    do
    {
        uint8_t opAddr = *ptr;
        uint8_t data = *(ptr + 1);
        enc28j60WriteOp(opAddr, data);
        ptr += 2;
    } while (--c);

    // Write MAC address
    // Note: MAC address in ENC28J60 is byte-backward
    for (uint8_t c = ENC28J60_WRITE_CTRL_REG | (B3_MAADR1 + 5); c >= (ENC28J60_WRITE_CTRL_REG | B3_MAADR1); c--)
    {
        Y_REG(myMac);
        uint8_t addr;
        asm (
            "ldi %0, 1 \n"
            "eor %0, %1 \n"
            : "=&d" (addr)
            : "r" (c)
        );
        enc28j60WriteOp(addr, *--myMac);
    }

    // Select bank 2
    enc28j60WriteECON1(ECON1_BSEL1);

    // Half duplex mode
    enc28j60Write(B2_MIREGADR, PHCON1);
    enc28j60WritePair(B2_MIWRL, 0);
    delay12us();

    // No loopback of transmitted frames (rev. B7 Silicon Errata point 9)
    enc28j60Write(B2_MIREGADR, PHCON2);
    enc28j60WritePair(B2_MIWRL, PHCON2_HDLDIS);
    delay12us();

    // LED config
    enc28j60Write(B2_MIREGADR, PHLCON);
    enc28j60WritePair(B2_MIWRL, 0x3000 | PHLCON_LACFG2 | PHLCON_LBCFG2 | PHLCON_LBCFG1 | PHLCON_LBCFG0 | PHLCON_LFRQ0
        | PHLCON_STRCH);
    delay12us();

    // Enable packet reception
    enc28j60WriteECON1(ECON1_RXEN);

    return 1;
}

// Check for a received packet (returns a positive value if a packet has been received)
uint8_t enc28j60PacketReceived()
{
    // Select bank 1
    enc28j60WriteECON1(ECON1_RXEN | ECON1_BSEL0);

    // Check if a packet has been received and buffered
    // Use EPKTCNT instead if EIR PKTIF (rev. B7 Silicon Errata point 6)
    return enc28j60Read(B1_EPKTCNT);
}

// Start reading the packet and return packet length
uint16_t enc28j60ReadPacket(uint16_t *nextPacketPtr)
{
    // Select bank 0
    enc28j60WriteECON1(ECON1_RXEN);

    // Set the read pointer to the start of the received packet
    enc28j60WritePair(B0_ERDPTL, *nextPacketPtr);

    // Assert CS
    CS_LO;

    // Issue read command
    spiTransfer(ENC28J60_READ_BUF_MEM);

    // Read the next packet pointer
    uint8_t lo = spiTransferZero();
    uint8_t hi = spiTransferZero();
    *nextPacketPtr = UINT16(lo, hi);

    // Read the packet length
    lo = spiTransferZero();
    hi = spiTransferZero();
    return UINT16(lo, hi);
}

// Finish reading the packet
void enc28j60EndRead(uint16_t *nextPacketPtr)
{
    // Release CS
    CS_HI;

    // Decrement the packet counter indicate we are done with this packet
    enc28j60Write(ECON2, ECON2_AUTOINC | ECON2_PKTDEC);

    // Move the RX read pointer to the start of the next received packet
    // This frees the memory we just read out
    // Note: never write an even address (rev. B7 Silicon Errata point 14)
    // The value of nextPacketPtr is always even if RXSTOP_INIT is odd
    // RXSTART_INIT is zero, no test for nextPacketPtr less than RXSTART_INIT
    uint16_t rxPtr = RXSTOP_INIT;
    R_REG(rxPtr);
    if (*nextPacketPtr - 1 < rxPtr)
        rxPtr = *nextPacketPtr - 1;
    enc28j60WritePair(B0_ERXRDPTL, rxPtr);
}

// Start writing the packet
void enc28j60WritePacket(uint16_t txnd)
{
    // Select bank 0
    enc28j60WriteECON1(ECON1_RXEN);

    // Set the write pointer to start of transmit buffer area
    enc28j60WritePair(B0_EWRPTL, TXSTART_INIT);
    // Set the TXND pointer
    enc28j60WritePair(B0_ETXNDL, txnd);

    // Assert CS
    CS_LO;

    // Issue write command
    spiTransfer(ENC28J60_WRITE_BUF_MEM);

    // Write per-packet control byte
    spiTransferZero();
}

// Finish writing the packet and send it
void enc28j60EndWrite(uint16_t txnd)
{
    // Release CS
    CS_HI;

    // Try transmitting up to 16 times to mitigate false late collision errors (rev. B7 Silicon Errata point 13)
    // Use bit 5 of byte 3 of Transmit Status Vector instead of LATECOL (rev. B7 Silicon Errata point 15)
    uint8_t attempts = 16;
    uint8_t eir;
    uint8_t status;
    do
    {
        // Reset the transmit logic problem (rev. B7 Silicon Errata point 12)
        enc28j60WriteECON1(ECON1_TXRST | ECON1_RXEN);
        enc28j60WriteECON1(ECON1_RXEN);
        enc28j60BitFieldClr(EIR, EIR_TXERIF | EIR_TXIF);

        // Send the contents of the transmit buffer onto the network
        enc28j60WriteECON1(ECON1_TXRTS | ECON1_RXEN);

        // Wait while transmission is in progress
        uint16_t c = 1000;
        do
        {
            eir = enc28j60Read(EIR);
            if (eir & (EIR_TXERIF | EIR_TXIF))
                break;
        } while (--c);

        // Cancel previous transmission if stuck
        enc28j60WriteECON1(ECON1_RXEN);

        // Set read pointer to byte 3 of Transmit Status Vector
        enc28j60WritePair(B0_ERDPTL, txnd + 4);

        // Read byte 3 of Transmit Status Vector
        status = enc28j60ReadOp(ENC28J60_READ_BUF_MEM);
    } while (eir & EIR_TXERIF && status & 0x20 && --attempts);
}

// Return positive value if ENC28J60 is ready
uint8_t enc28j60Ready()
{
    // Verify that packet reception is enabled
    return enc28j60Read(ECON1) & ECON1_RXEN;
}

// Return positive value if network cable is connected
uint8_t enc28j60LinkUp()
{
    // Check if the link is up and has been up since the last call to this function
    return enc28j60PhyReadL(PHSTAT1) & PHSTAT1_LLSTAT;
}
