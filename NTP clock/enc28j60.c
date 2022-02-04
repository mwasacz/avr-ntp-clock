/*
 * enc28j60.c
 *
 * Created: 01.07.2018 12:22:00
 *  Author: Mikolaj
 */

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

uint8_t spiTransferZero()
{
    return spiTransfer(0);
}

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

static void enc28j60WriteOpZero(uint8_t opAddr)
{
    enc28j60WriteOp(opAddr, 0);
}

#define enc28j60Read(address)               enc28j60ReadOp(ENC28J60_READ_CTRL_REG | (address))
#define enc28j60ReadMacMii(address)         enc28j60ReadOpMacMii(ENC28J60_READ_CTRL_REG | (address))

#define enc28j60Write(address, data)        enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG | (address), (data))
#define enc28j60BitFieldSet(address, data)  enc28j60WriteOp(ENC28J60_BIT_FIELD_SET | (address), (data))
#define enc28j60BitFieldClr(address, data)  enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR | (address), (data))
#define enc28j60WriteZero(address)          enc28j60WriteOpZero(ENC28J60_WRITE_CTRL_REG | (address))

static void enc28j60SetBank(uint8_t address)
{
    // Set the bank
    enc28j60BitFieldClr(ECON1, ECON1_BSEL1 | ECON1_BSEL0);
    enc28j60BitFieldSet(ECON1, address);
}

static uint8_t enc28j60PhyReadL(uint8_t address)
{
    enc28j60SetBank(2);
    // Set the right address and start the register read operation
    enc28j60Write(B2_MIREGADR, address);
    enc28j60Write(B2_MICMD, MICMD_MIIRD);

    enc28j60SetBank(3);
    // Wait until the PHY read completes
    while (enc28j60ReadMacMii(B3_MISTAT) & MISTAT_BUSY);

    enc28j60SetBank(2);
    // Quit reading
    enc28j60WriteZero(B2_MICMD);

    // Get low byte
    return enc28j60ReadMacMii(B2_MIRDL);
}

static void enc28j60PhyWrite(uint8_t address, uint16_t data)
{
    enc28j60SetBank(2);
    // Set the PHY register address
    enc28j60Write(B2_MIREGADR, address);

    // Write the PHY data
    enc28j60Write(B2_MIWRL, L(data));
    enc28j60Write(B2_MIWRH, H(data));

    enc28j60SetBank(3);
    // Wait until the PHY write completes
    while(enc28j60ReadMacMii(B3_MISTAT) & MISTAT_BUSY);
}

void enc28j60Init(uint8_t *myMac)
{
    // Perform system reset
    CS_LO;
    spiTransfer(ENC28J60_SOFT_RESET);
    CS_HI;
    // Wait 1 ms after issuing the reset command (rev. B7 Silicon Errata point 2)
    __builtin_avr_delay_cycles(F_CPU / 1000 + 1);

    // Do bank 0 stuff
    // ECON1 ECON1_BSEL1 and ECON1_BSEL0 are 0 by default
    // Initialize receive buffer
    // For 16-bit transfers we must write low byte first
    // Set receive buffer start address
    STATIC_ASSERT(L(RXSTART_INIT) == 0);
    enc28j60WriteZero(B0_ERXSTL);
    STATIC_ASSERT(H(RXSTART_INIT) == 0);
    enc28j60WriteZero(B0_ERXSTH);
    // Set receive pointer address
    STATIC_ASSERT(L(RXSTART_INIT) == 0);
    enc28j60WriteZero(B0_ERXRDPTL);
    STATIC_ASSERT(H(RXSTART_INIT) == 0);
    enc28j60WriteZero(B0_ERXRDPTH);
    // Set receive buffer end
    // ERXND defaults to 0x1FFF (end of ram)
    enc28j60Write(B0_ERXNDL, L(RXSTOP_INIT));
    enc28j60Write(B0_ERXNDH, H(RXSTOP_INIT));
    // Set transmit buffer start
    // ETXST defaults to 0x0000 (beginning of ram)
    STATIC_ASSERT(L(TXSTART_INIT) == 0);
    enc28j60WriteZero(B0_ETXSTL);
    enc28j60Write(B0_ETXSTH, H(TXSTART_INIT));

    // Do bank 1 stuff
    enc28j60SetBank(1);
    enc28j60Write(B1_ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN);

    // Do bank 2 stuff
    enc28j60SetBank(2);
    // Enable MAC receive
    enc28j60Write(B2_MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);
    // Bring MAC out of reset
    enc28j60WriteZero(B2_MACON2);
    // Enable automatic padding and CRC operations
    enc28j60Write(B2_MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN);
    // Set inter-frame gap (non-back-to-back)
    enc28j60Write(B2_MAIPGL, 0x12);
    enc28j60Write(B2_MAIPGH, 0x0C);
    // Set inter-frame gap (back-to-back)
    enc28j60Write(B2_MABBIPG, 0x12);
    // Set the maximum packet size which the controller will accept
    enc28j60Write(B2_MAMXFLL, L(MAX_FRAMELEN));
    enc28j60Write(B2_MAMXFLH, H(MAX_FRAMELEN));

    // Do bank 3 stuff
    enc28j60SetBank(3);
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

    // Half duplex mode
    enc28j60PhyWrite(PHCON1, 0);

    // No loopback of transmitted frames
    enc28j60PhyWrite(PHCON2, PHCON2_HDLDIS);

    // LED config
    enc28j60PhyWrite(PHLCON, PHLCON_LACFG2 | PHLCON_LBCFG2 | PHLCON_LBCFG1 | PHLCON_LBCFG0 | PHLCON_LFRQ0
        | PHLCON_STRCH);

    // Enable packet reception
    enc28j60BitFieldSet(ECON1, ECON1_RXEN);
}

uint8_t enc28j60PacketReceived()
{
    enc28j60SetBank(1);
    // Check if a packet has been received and buffered
    // Use EPKTCNT instead if EIR PKTIF (rev. B7 Silicon Errata point 6)
    return enc28j60Read(B1_EPKTCNT);
}

uint16_t enc28j60ReadPacket(uint16_t *nextPacketPtr)
{
    enc28j60SetBank(0);

    // Set the read pointer to the start of the received packet
    enc28j60Write(B0_ERDPTL, L(*nextPacketPtr));
    enc28j60Write(B0_ERDPTH, H(*nextPacketPtr));

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

void enc28j60EndRead(uint16_t *nextPacketPtr)
{
    // Release CS
    CS_HI;

    // Move the RX read pointer to the start of the next received packet
    // This frees the memory we just read out
    // Note: never write an even address (rev. B7 Silicon Errata point 14)
    // The value of nextPacketPtr is always even if RXSTOP_INIT is odd
    // RXSTART_INIT is zero, no test for nextPacketPtr less than RXSTART_INIT
    uint16_t rxPtr = RXSTOP_INIT;
    R_REG(rxPtr);
    if (*nextPacketPtr - 1 < rxPtr)
    {
        rxPtr = *nextPacketPtr - 1;
    }
    enc28j60Write(B0_ERXRDPTL, L(rxPtr));
    enc28j60Write(B0_ERXRDPTH, H(rxPtr));

    // Decrement the packet counter indicate we are done with this packet
    enc28j60BitFieldSet(ECON2, ECON2_PKTDEC);
}

void enc28j60WritePacket(uint16_t txnd)
{
    // Check no transmit in progress
    while (enc28j60Read(ECON1) & ECON1_TXRTS)
    {
        // Reset the transmit logic problem (rev. B7 Silicon Errata point 12)
        if ((enc28j60Read(EIR) & EIR_TXERIF))
        {
            enc28j60BitFieldSet(ECON1, ECON1_TXRST);
            enc28j60BitFieldClr(ECON1, ECON1_TXRST);
        }
    }

    enc28j60SetBank(0);

    // Set the write pointer to start of transmit buffer area
    STATIC_ASSERT(L(TXSTART_INIT) == 0);
    enc28j60WriteZero(B0_EWRPTL);
    enc28j60Write(B0_EWRPTH, H(TXSTART_INIT));
    // Set the TXND pointer
    enc28j60Write(B0_ETXNDL, L(txnd));
    enc28j60Write(B0_ETXNDH, H(txnd));

    // Assert CS
    CS_LO;

    // Issue write command
    spiTransfer(ENC28J60_WRITE_BUF_MEM);

    // Write per-packet control byte
    spiTransferZero();
}

void enc28j60EndWrite()
{
    // Release CS
    CS_HI;

    // Send the contents of the transmit buffer onto the network
    enc28j60BitFieldSet(ECON1, ECON1_TXRTS);
}

uint8_t enc28j60LinkUp()
{
    // Check if the link is up and has been up since the last call to this function
    return enc28j60PhyReadL(PHSTAT1) & PHSTAT1_LLSTAT;
}