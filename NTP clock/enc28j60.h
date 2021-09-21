/*
 * enc28j60.h
 *
 * Created: 01.07.2018 12:22:14
 *  Author: Mikolaj
 */ 


#ifndef ENC28J60_H
#define ENC28J60_H

#include <avr/io.h>
#include <util/delay.h>
#include "config.h"

// ENC28J60 Control Registers

// All-bank registers
#define EIE         0x1B
#define EIR         0x1C
#define ESTAT       0x1D
#define ECON2       0x1E
#define ECON1       0x1F
// Bank 0 registers
#define B0_ERDPTL   0x00
#define B0_ERDPTH   0x01
#define B0_EWRPTL   0x02
#define B0_EWRPTH   0x03
#define B0_ETXSTL   0x04
#define B0_ETXSTH   0x05
#define B0_ETXNDL   0x06
#define B0_ETXNDH   0x07
#define B0_ERXSTL   0x08
#define B0_ERXSTH   0x09
#define B0_ERXNDL   0x0A
#define B0_ERXNDH   0x0B
#define B0_ERXRDPTL 0x0C
#define B0_ERXRDPTH 0x0D
#define B0_ERXWRPTL 0x0E
#define B0_ERXWRPTH 0x0F
#define B0_EDMASTL  0x10
#define B0_EDMASTH  0x11
#define B0_EDMANDL  0x12
#define B0_EDMANDH  0x13
#define B0_EDMADSTL 0x14
#define B0_EDMADSTH 0x15
#define B0_EDMACSL  0x16
#define B0_EDMACSH  0x17
// Bank 1 registers
#define B1_EHT0     0x00
#define B1_EHT1     0x01
#define B1_EHT2     0x02
#define B1_EHT3     0x03
#define B1_EHT4     0x04
#define B1_EHT5     0x05
#define B1_EHT6     0x06
#define B1_EHT7     0x07
#define B1_EPMM0    0x08
#define B1_EPMM1    0x09
#define B1_EPMM2    0x0A
#define B1_EPMM3    0x0B
#define B1_EPMM4    0x0C
#define B1_EPMM5    0x0D
#define B1_EPMM6    0x0E
#define B1_EPMM7    0x0F
#define B1_EPMCSL   0x10
#define B1_EPMCSH   0x11
#define B1_EPMOL    0x14
#define B1_EPMOH    0x15
#define B1_EWOLIE   0x16
#define B1_EWOLIR   0x17
#define B1_ERXFCON  0x18
#define B1_EPKTCNT  0x19
// Bank 2 registers
#define B2_MACON1   0x00
#define B2_MACON2   0x01
#define B2_MACON3   0x02
#define B2_MACON4   0x03
#define B2_MABBIPG  0x04
#define B2_MAIPGL   0x06
#define B2_MAIPGH   0x07
#define B2_MACLCON1 0x08
#define B2_MACLCON2 0x09
#define B2_MAMXFLL  0x0A
#define B2_MAMXFLH  0x0B
#define B2_MAPHSUP  0x0D
#define B2_MICON    0x11
#define B2_MICMD    0x12
#define B2_MIREGADR 0x14
#define B2_MIWRL    0x16
#define B2_MIWRH    0x17
#define B2_MIRDL    0x18
#define B2_MIRDH    0x19
// Bank 3 registers
#define B3_MAADR1   0x00
#define B3_MAADR0   0x01
#define B3_MAADR3   0x02
#define B3_MAADR2   0x03
#define B3_MAADR5   0x04
#define B3_MAADR4   0x05
#define B3_EBSTSD   0x06
#define B3_EBSTCON  0x07
#define B3_EBSTCSL  0x08
#define B3_EBSTCSH  0x09
#define B3_MISTAT   0x0A
#define B3_EREVID   0x12
#define B3_ECOCON   0x15
#define B3_EFLOCON  0x17
#define B3_EPAUSL   0x18
#define B3_EPAUSH   0x19
// PHY registers
#define PHCON1      0x00
#define PHSTAT1     0x01
#define PHHID1      0x02
#define PHHID2      0x03
#define PHCON2      0x10
#define PHSTAT2     0x11
#define PHIE        0x12
#define PHIR        0x13
#define PHLCON      0x14

// ENC28J60 EIE Register Bit Definitions
#define EIE_INTIE       0x80
#define EIE_PKTIE       0x40
#define EIE_DMAIE       0x20
#define EIE_LINKIE      0x10
#define EIE_TXIE        0x08
#define EIE_WOLIE       0x04
#define EIE_TXERIE      0x02
#define EIE_RXERIE      0x01
// ENC28J60 EIR Register Bit Definitions
#define EIR_PKTIF       0x40
#define EIR_DMAIF       0x20
#define EIR_LINKIF      0x10
#define EIR_TXIF        0x08
#define EIR_WOLIF       0x04
#define EIR_TXERIF      0x02
#define EIR_RXERIF      0x01
// ENC28J60 ESTAT Register Bit Definitions
#define ESTAT_INT       0x80
#define ESTAT_LATECOL   0x10
#define ESTAT_RXBUSY    0x04
#define ESTAT_TXABRT    0x02
#define ESTAT_CLKRDY    0x01
// ENC28J60 ECON2 Register Bit Definitions
#define ECON2_AUTOINC   0x80
#define ECON2_PKTDEC    0x40
#define ECON2_PWRSV     0x20
#define ECON2_VRPS      0x08
// ENC28J60 ECON1 Register Bit Definitions
#define ECON1_TXRST     0x80
#define ECON1_RXRST     0x40
#define ECON1_DMAST     0x20
#define ECON1_CSUMEN    0x10
#define ECON1_TXRTS     0x08
#define ECON1_RXEN      0x04
#define ECON1_BSEL1     0x02
#define ECON1_BSEL0     0x01
// ENC28J60 ERXFCON Register Bit Definitions
#define ERXFCON_UCEN    0x80
#define ERXFCON_ANDOR   0x40
#define ERXFCON_CRCEN   0x20
#define ERXFCON_PMEN    0x10
#define ERXFCON_MPEN    0x08
#define ERXFCON_HTEN    0x04
#define ERXFCON_MCEN    0x02
#define ERXFCON_BCEN    0x01
// ENC28J60 MACON1 Register Bit Definitions
#define MACON1_LOOPBK   0x10
#define MACON1_TXPAUS   0x08
#define MACON1_RXPAUS   0x04
#define MACON1_PASSALL  0x02
#define MACON1_MARXEN   0x01
// ENC28J60 MACON2 Register Bit Definitions
#define MACON2_MARST    0x80
#define MACON2_RNDRST   0x40
#define MACON2_MARXRST  0x08
#define MACON2_RFUNRST  0x04
#define MACON2_MATXRST  0x02
#define MACON2_TFUNRST  0x01
// ENC28J60 MACON3 Register Bit Definitions
#define MACON3_PADCFG2  0x80
#define MACON3_PADCFG1  0x40
#define MACON3_PADCFG0  0x20
#define MACON3_TXCRCEN  0x10
#define MACON3_PHDRLEN  0x08
#define MACON3_HFRMLEN  0x04
#define MACON3_FRMLNEN  0x02
#define MACON3_FULDPX   0x01
// ENC28J60 MICMD Register Bit Definitions
#define MICMD_MIISCAN   0x02
#define MICMD_MIIRD     0x01
// ENC28J60 MISTAT Register Bit Definitions
#define MISTAT_NVALID   0x04
#define MISTAT_SCAN     0x02
#define MISTAT_BUSY     0x01
// ENC28J60 PHY PHCON1 Register Bit Definitions
#define PHCON1_PRST     0x8000
#define PHCON1_PLOOPBK  0x4000
#define PHCON1_PPWRSV   0x0800
#define PHCON1_PDPXMD   0x0100
// ENC28J60 PHY PHSTAT1 Register Bit Definitions
#define PHSTAT1_PFDPX   0x1000
#define PHSTAT1_PHDPX   0x0800
#define PHSTAT1_LLSTAT  0x0004
#define PHSTAT1_JBSTAT  0x0002
// ENC28J60 PHY PHCON2 Register Bit Definitions
#define PHCON2_FRCLINK  0x4000
#define PHCON2_TXDIS    0x2000
#define PHCON2_JABBER   0x0400
#define PHCON2_HDLDIS   0x0100
// ENC28J60 PHY PHLCON Register Bit Definitions
#define PHLCON_LACFG3   0x0800
#define PHLCON_LACFG2   0x0400
#define PHLCON_LACFG1   0x0200
#define PHLCON_LACFG0   0x0100
#define PHLCON_LBCFG3   0x0080
#define PHLCON_LBCFG2   0x0040
#define PHLCON_LBCFG1   0x0020
#define PHLCON_LBCFG0   0x0010
#define PHLCON_LFRQ1    0x0008
#define PHLCON_LFRQ0    0x0004
#define PHLCON_STRCH    0x0002

// ENC28J60 Packet Control Byte Bit Definitions
#define PKTCTRL_PHUGEEN     0x08
#define PKTCTRL_PPADEN      0x04
#define PKTCTRL_PCRCEN      0x02
#define PKTCTRL_POVERRIDE   0x01

// SPI operation codes
#define ENC28J60_READ_CTRL_REG  0x00
#define ENC28J60_READ_BUF_MEM   0x3A
#define ENC28J60_WRITE_CTRL_REG 0x40
#define ENC28J60_WRITE_BUF_MEM  0x7A
#define ENC28J60_BIT_FIELD_SET  0x80
#define ENC28J60_BIT_FIELD_CLR  0xA0
#define ENC28J60_SOFT_RESET     0xFF

// ToDo: verify buffer boundaries (max transmit length) and max frame length
// The RXSTART_INIT must be zero. See Rev. B4 Silicon Errata point 5.
// Buffer boundaries applied to internal 8K ram
// the entire available packet buffer space is allocated
//
// start with recbuf at 0 (must be zero! assumed in code)
#define RXSTART_INIT     0x0
// receive buffer end, must be odd number:
#define RXSTOP_INIT      (0x1FFF-0x0600)
// start TX buffer after RXSTOP_INIT with space for one full ethernet frame (~1500 bytes)
#define TXSTART_INIT     (0x1FFF-0x0600+1)
// stp TX buffer at end of mem
#define TXSTOP_INIT      0x1FFF
//
// max frame length which the controller will accept:
// (note: maximum ethernet frame length would be 1518)
#define        MAX_FRAMELEN        1500

extern void enc28j60Init(uint8_t* myMac);
extern uint8_t enc28j60LinkUp();
extern uint8_t enc28j60PacketReceived();
extern uint16_t enc28j60ReadPacket(uint16_t* NextPacketPtr);
extern uint8_t enc28j60ReadByte();
extern void enc28j60EndRead(uint16_t* NextPacketPtr);
extern void enc28j60WritePacket(uint16_t txnd);
extern void enc28j60WriteByte(uint8_t data);
extern void enc28j60WriteZero();
extern void enc28j60EndWrite();
#ifndef __AVR_ATtiny4313__
extern void tx(uint8_t data);
extern void txHex(uint8_t data);
#endif

#endif /* ENC28J60_H */