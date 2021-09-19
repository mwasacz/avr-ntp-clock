/*
 * enc28j60.c
 *
 * Created: 01.07.2018 12:22:00
 *  Author: Mikolaj
 */ 

#include "enc28j60.h"

// ToDo:
// DEBUG -----------------
void tx(uint8_t data)
{
    while (!(UCSRA & (1<<UDRE)));
    UDR = data;
}
void txHex(uint8_t data)
{
    uint8_t hi = (data >> 4) + '0';
    if (hi > '9')
    hi += 'A' - '9' - 1;
    tx(hi);
    uint8_t lo = (data & 0xF) + '0';
    if (lo > '9')
    lo += 'A' - '9' - 1;
    tx(lo);
}
// /DEBUG ----------------

// set CS to 0 = active
#define CS_LO   CS_PORT &= ~(1<<CS)
// set CS to 1 = passive
#define CS_HI   CS_PORT |= (1<<CS)

#define SO_IN   SPI_PIN & (1<<SPI_SO)
#define SI_HI   SPI_PORT |= (1<<SPI_SI)
#define SI_LO   SPI_PORT &= ~(1<<SPI_SI)
#define SCK_HI  SPI_PORT |= (1<<SPI_SCK)
#define SCK_LO  SPI_PORT &= ~(1<<SPI_SCK)

static uint8_t spiTransfer(uint8_t data)
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

static uint8_t enc28j60ReadOp(uint8_t opAddr)
{
    // assert CS
    CS_LO;

    // issue read command
    spiTransfer(opAddr);
    // read data
    uint8_t data = spiTransfer(0x00);

    // release CS
    CS_HI;

    return data;
}

static uint8_t enc28j60ReadOpMacMii(uint8_t opAddr)
{
    // assert CS
    CS_LO;

    // issue read command
    spiTransfer(opAddr);
    // dummy read
    spiTransfer(0x00);
    // read data
    uint8_t data = spiTransfer(0x00);

    // release CS
    CS_HI;

    return data;
}

static void enc28j60WriteOp(uint8_t opAddr, uint8_t data)
{
    // assert CS
    CS_LO;
    
    // issue write command
    spiTransfer(opAddr);
    // write data
    spiTransfer(data);

    // release CS
    CS_HI;
}

#define enc28j60Read(address)               enc28j60ReadOp(ENC28J60_READ_CTRL_REG | (address))
#define enc28j60ReadMacMii(address)         enc28j60ReadOpMacMii(ENC28J60_READ_CTRL_REG | (address))

#define enc28j60Write(address, data)        enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG | (address), (data))
#define enc28j60BitFieldSet(address, data)  enc28j60WriteOp(ENC28J60_BIT_FIELD_SET | (address), (data))
#define enc28j60BitFieldClr(address, data)  enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR | (address), (data))

static void enc28j60SetBank(uint8_t address)
{
    // set the bank
    enc28j60BitFieldClr(ECON1, ECON1_BSEL1|ECON1_BSEL0);
    enc28j60BitFieldSet(ECON1, address);
}

static uint8_t enc28j60PhyReadL(uint8_t address)
{
    enc28j60SetBank(2);
    // set the right address and start the register read operation
    enc28j60Write(B2_MIREGADR, address);
    enc28j60Write(B2_MICMD, MICMD_MIIRD);
    
    enc28j60SetBank(3);
    // wait until the PHY read completes
    while (enc28j60ReadMacMii(B3_MISTAT) & MISTAT_BUSY);
    
    enc28j60SetBank(2);
    // quit reading
    enc28j60Write(B2_MICMD, 0x00);
    
    // get low byte
    return enc28j60ReadMacMii(B2_MIRDL);
}

static void enc28j60PhyWrite(uint8_t address, uint16_t data)
{
    enc28j60SetBank(2);
    // set the PHY register address
    enc28j60Write(B2_MIREGADR, address);
    
    // write the PHY data
    enc28j60Write(B2_MIWRL, data); 
    enc28j60Write(B2_MIWRH, data>>8);

    enc28j60SetBank(3);
    // wait until the PHY write completes
    while(enc28j60ReadMacMii(B3_MISTAT) & MISTAT_BUSY);
}

void enc28j60Init(uint8_t* myMac)
{
    // perform system reset
    CS_LO;
    spiTransfer(ENC28J60_SOFT_RESET);
    CS_HI;
    //enc28j60WriteOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
    // check CLKRDY bit to see if reset is complete
    //delay_us(50);
    //while(!(enc28j60Read(ESTAT) & ESTAT_CLKRDY));
    // ToDo: verify this delay
    __builtin_avr_delay_cycles(F_CPU / 1000 + 1);
    //_delay_ms(1);
    //_delay_ms(20);

    // do bank 0 stuff
    enc28j60SetBank(0); // ToDo: is it necessary ?
    // initialize receive buffer
    // 16-bit transfers, must write low byte first
    // set receive buffer start address
    //NextPacketPtr = RXSTART_INIT;
    enc28j60Write(B0_ERXSTL, RXSTART_INIT&0xFF);
    enc28j60Write(B0_ERXSTH, RXSTART_INIT>>8);
    // set receive pointer address
    enc28j60Write(B0_ERXRDPTL, RXSTART_INIT&0xFF);
    enc28j60Write(B0_ERXRDPTH, RXSTART_INIT>>8);
    // set receive buffer end
    // ERXND defaults to 0x1FFF (end of ram)
    enc28j60Write(B0_ERXNDL, RXSTOP_INIT&0xFF);
    enc28j60Write(B0_ERXNDH, RXSTOP_INIT>>8);
    // set transmit buffer start
    // ETXST defaults to 0x0000 (beginning of ram)
    enc28j60Write(B0_ETXSTL, TXSTART_INIT&0xFF);
    enc28j60Write(B0_ETXSTH, TXSTART_INIT>>8);
    
    // do bank 1 stuff
    enc28j60SetBank(1);
    enc28j60Write(B1_ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_BCEN);

    // do bank 2 stuff
    enc28j60SetBank(2);
    // enable MAC receive
    enc28j60Write(B2_MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
    // bring MAC out of reset
    enc28j60Write(B2_MACON2, 0x00);
    // enable automatic padding and CRC operations
    //enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
    enc28j60Write(B2_MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
    // set inter-frame gap (non-back-to-back)
    enc28j60Write(B2_MAIPGL, 0x12);
    enc28j60Write(B2_MAIPGH, 0x0C);
    // set inter-frame gap (back-to-back)
    enc28j60Write(B2_MABBIPG, 0x12);
    // Set the maximum packet size which the controller will accept
    enc28j60Write(B2_MAMXFLL, MAX_FRAMELEN&0xFF);  
    enc28j60Write(B2_MAMXFLH, MAX_FRAMELEN>>8);
    
    // do bank 3 stuff
    enc28j60SetBank(3);
    // write MAC address
    // NOTE: MAC address in ENC28J60 is byte-backward
    for (uint8_t c = ENC28J60_WRITE_CTRL_REG | (B3_MAADR1 + 5); c >= (ENC28J60_WRITE_CTRL_REG | B3_MAADR1); c--)
    {
        Y_REG(myMac);
        enc28j60WriteOp(c ^ 1, *--myMac);
    }

    // half duplex mode
    enc28j60PhyWrite(PHCON1, 0);

    // no loopback of transmitted frames
    enc28j60PhyWrite(PHCON2, PHCON2_HDLDIS);
    
    // LED config
    enc28j60PhyWrite(PHLCON, PHLCON_LACFG2|PHLCON_LBCFG2|PHLCON_LBCFG1|PHLCON_LBCFG0|PHLCON_LFRQ0|PHLCON_STRCH);

    // enable packet reception
    enc28j60BitFieldSet(ECON1, ECON1_RXEN);
}

uint8_t enc28j60ReadByte()
{
    uint8_t data = spiTransfer(0);
    txHex(data);
    return data;
}

uint8_t enc28j60PacketReceived()
{
    enc28j60SetBank(1);
    // check if a packet has been received and buffered
    //if( !(enc28j60Read(EIR) & EIR_PKTIF) )
    return enc28j60Read(B1_EPKTCNT);
}

uint16_t enc28j60ReadPacket(uint16_t* NextPacketPtr)//uint16_t maxlen, uint8_t* packet)
{
    uint8_t rxstat;
    uint16_t len;

    enc28j60SetBank(0);
    // Make absolutely certain that any previous packet was discarded
    //if( WasDiscarded == FALSE)
    //  MACDiscardRx();

    // Set the read pointer to the start of the received packet
    enc28j60Write(B0_ERDPTL, (*NextPacketPtr)&0xFF);
    enc28j60Write(B0_ERDPTH, (*NextPacketPtr)>>8);
    // ToDo: start reading buffer here - spiTransfer(ENC28J60_READ_BUF_MEM);
    
    // assert CS
    CS_LO;
    
    // issue read command
    spiTransfer(ENC28J60_READ_BUF_MEM);

    // read the next packet pointer
    uint8_t lo = spiTransfer(0);
    uint8_t hi = spiTransfer(0);
    *NextPacketPtr = WORD(lo, hi);
    //NextPacketPtr  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
    //NextPacketPtr |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;
    // read the packet length
    lo = spiTransfer(0);
    hi = spiTransfer(0);
    len = WORD(lo, hi);
    //len  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
    //len |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;
    // remove the CRC count
    //len -= 4;
    // read the receive status
    rxstat = spiTransfer(0);
    spiTransfer(0);
    //rxstat  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
    //rxstat |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;

    // limit retrieve length
    //if (len > maxlen)
    //{
    //len = maxlen;
    //}

    if (!(rxstat & 0x80))
    {
        return 0;
    }

    return len;
}

void enc28j60EndRead(uint16_t* NextPacketPtr)
{
    // release CS
    CS_HI;

    // Move the RX read pointer to the start of the next received packet
    // This frees the memory we just read out
    //enc28j60Write(ERXRDPTL, (NextPacketPtr));
    //enc28j60Write(ERXRDPTH, (NextPacketPtr)>>8);
    // However, compensate for the errata point 13, rev B4: never write an even address!
    // gNextPacketPtr is always an even address if RXSTOP_INIT is odd.
    // RXSTART_INIT is zero, no test for gNextPacketPtr less than RXSTART_INIT.
    //if (NextPacketPtr -1 > RXSTOP_INIT)
    if (*NextPacketPtr > RXSTOP_INIT + 1)
    {
        enc28j60Write(B0_ERXRDPTL, (RXSTOP_INIT)&0xFF);
        enc28j60Write(B0_ERXRDPTH, (RXSTOP_INIT)>>8);
    }
    else
    {
        enc28j60Write(B0_ERXRDPTL, (*NextPacketPtr-1)&0xFF);
        enc28j60Write(B0_ERXRDPTH, (*NextPacketPtr-1)>>8);
    }

    // decrement the packet counter indicate we are done with this packet
    enc28j60BitFieldSet(ECON2, ECON2_PKTDEC);

    tx('\r');
    tx('\n');
}

// ToDo: enc28j60WriteZero
void enc28j60WriteByte(uint8_t data)
{
    spiTransfer(data);
    txHex(data);
}

void enc28j60WritePacket(uint16_t txnd)//len)
{
    // Check no transmit in progress
    //while (enc28j60ReadOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_TXRTS)
    while (enc28j60Read(ECON1) & ECON1_TXRTS)
    {
        // Reset the transmit logic problem. See Rev. B4 Silicon Errata point 12.
        if ((enc28j60Read(EIR) & EIR_TXERIF))
        {
            enc28j60BitFieldSet(ECON1, ECON1_TXRST);
            enc28j60BitFieldClr(ECON1, ECON1_TXRST);
        }
    }

    enc28j60SetBank(0);

    // Set the write pointer to start of transmit buffer area
    enc28j60Write(B0_EWRPTL, TXSTART_INIT&0xFF);
    enc28j60Write(B0_EWRPTH, TXSTART_INIT>>8);
    // Set the TXND pointer to correspond to the packet size given
    //enc28j60Write(ETXNDL, (TXSTART_INIT+len)&0xFF);
    //enc28j60Write(ETXNDH, (TXSTART_INIT+len)>>8);
    enc28j60Write(B0_ETXNDL, txnd&0xFF);
    enc28j60Write(B0_ETXNDH, txnd>>8);
    
    // assert CS
    CS_LO;
    
    // issue write command
    spiTransfer(ENC28J60_WRITE_BUF_MEM);

    // write per-packet control byte
    //enc28j60WriteOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);
    spiTransfer(0); // ToDo
    //enc28j60WriteByte(0);

    tx('\t');
}

void enc28j60EndWrite()
{
    // release CS
    CS_HI;

    // send the contents of the transmit buffer onto the network
    enc28j60BitFieldSet(ECON1, ECON1_TXRTS);
    
    tx('\r');
    tx('\n');
}

uint8_t enc28j60LinkUp()
{
    // PHSTAT1 LLSTAT (= bit 2 in lower reg), PHSTAT1_LLSTAT
    // LLSTAT is latching, that is: if it was down since last
    // calling enc28j60linkup then we get first a down indication
    // and only at the next call to enc28j60linkup it will come up.
    // This way we can detect intermittent link failures and
    // that might be what we want.
    // The non latching version is LSTAT.
    // PHSTAT2 LSTAT (= bit 10 in upper reg)
    return enc28j60PhyReadL(PHSTAT1) & PHSTAT1_LLSTAT;
    //return enc28j60PhyReadH(PHSTAT2) & (1<<2);
}