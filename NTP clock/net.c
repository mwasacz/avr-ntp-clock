/*
 * net.c
 *
 * Created: 01.07.2018 14:32:04
 *  Author: Mikolaj
 */

#include "net.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include "config.h"
#include "common.h"
#include "arithmetic.h"
#include "enc28j60.h"

#define commonReturn1   do { asm (""); return; } while (0)
#define commonReturn2   do { asm (" "); return; } while (0)
#define commonReturn3   do { asm ("  "); return; } while (0)
#define commonReturn4   do { asm ("   "); return; } while (0)
#define commonReturn5   do { asm ("    "); return; } while (0)

#ifdef __AVR_ATtiny4313__

#define enc28j60ReadByte()      spiTransferZero()
#define enc28j60WriteByte(data) spiTransfer(data)
#define enc28j60WriteByteZero() spiTransferZero()

#else

static void tx(uint8_t data)
{
    while (!(UCSRA & (1 << UDRE)));
    UDR = data;
}

static void txHex(uint8_t data)
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

static uint8_t enc28j60ReadByte()
{
    uint8_t data = spiTransferZero();
    txHex(data);
    return data;
}

static void enc28j60WriteByte(uint8_t data)
{
    spiTransfer(data);
    txHex(data);
}

static void enc28j60WriteByteZero()
{
    spiTransferZero();
    txHex(0);
}

#endif

static uint8_t readBytes(uint8_t len)
{
    uint8_t ret;
    do
    {
        ret = enc28j60ReadByte();
    } while (--len);
    return ret;
}

#ifdef __AVR_ATtiny4313__
#define writeZeros(length)  readBytes(length)
#else
static void writeZeros(uint8_t len)
{
    do
    {
        enc28j60WriteByteZero();
    } while (--len);
}
#endif

static void writeFFs(uint8_t len)
{
    do
    {
        enc28j60WriteByte(0xFF);
    } while (--len);
}

static uint8_t __attribute__((noinline)) addrLength(uint8_t p)
{
    if (p <= memOffset(config.myMac))
        return 6;
    else
        return 4;
}

static void writeAddr(uint8_t p)
{
    uint8_t *ptr = (uint8_t *)&mem + p;
    Y_REG(ptr);
    uint8_t len = addrLength(p);
    do
    {
        enc28j60WriteByte(*--ptr);
    } while (--len);
}

static uint16_t addrChecksum(uint16_t sum, uint8_t p)
{
    uint8_t *ptr = (uint8_t *)&mem + p;
    uint8_t c;

    asm (
        "ldi %3, 2 \n"
        "cpi %2, %4 \n"
        "brcc loop_%= \n"
        "ldi %3, 3 \n"
        "clc \n"
        "loop_%=:"
        "ld %2, -%a1 \n"
        "adc %B0, %2 \n"
        "ld %2, -%a1 \n"
        "adc %A0, %2 \n"
        "dec %3 \n"
        "brne loop_%= \n"
        "adc %B0, __zero_reg__ \n"
        "adc %A0, __zero_reg__ \n"
        : "+r" (sum), "+e" (ptr), "+d" (p), "=&d" (c)
        : "M" (memOffset(config.myMac) + 1)
    );

    return sum;
}

static void readAddr(uint8_t p)
{
    uint8_t *ptr = (uint8_t *)&mem + p;
    Y_REG(ptr);
    uint8_t len = addrLength(p);
    do
    {
        *--ptr = enc28j60ReadByte();
    } while (--len);
}

static uint8_t checkAddr(uint8_t p)
{
    uint8_t *ptr = (uint8_t *)&mem + p;
    Y_REG(ptr);
    uint8_t len = addrLength(p);
    do
    {
        if (*--ptr != enc28j60ReadByte()) return len;
    } while (--len);
    return len;
}

static void copyAddr(uint8_t d, uint8_t s)
{
    uint8_t *dst = (uint8_t *)&mem + d;
    uint8_t *src = (uint8_t *)&mem + s;
    uint8_t len = 4;
    R_REG(len);
    do
    {
        *--dst = *--src;
    } while (--len);
}

static void __attribute__((noinline)) resetRetryTime()
{
    retryTimeH = 0;
    retryTimeL = 0;
}

static uint16_t __attribute__((noinline)) timeFrac(uint16_t tcnt)
{
    return UINT32(0, 0, L(tcnt), H(tcnt)) / TIMER_1S;
}

static void sendDhcpPacket(uint8_t state)
{
    // UDP checksum
    uint16_t sum;
    if (state == 2)
    {
        uint32_t* ptr = (uint32_t*)&mem.xid;
        B_REG(ptr);
        *ptr = *ptr + 1;

        sum = (uint24_t)IP_PROTO_UDP_V + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_DISCOVER_RENEW_LEN
            + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0137 + 0x0201 + 0x03FF;
    }
    else if (state == 3)
    {
        sum = (uint24_t)IP_PROTO_UDP_V + DHCP_REQUEST_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_REQUEST_LEN
            + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0332 + 0x0400 + 0x0036 + 0x0400 + 0x0037
            + 0x0201 + 0x03FF - 0xFFFF;
        sum = UINT16(H(sum), L(sum));

        sum = addrChecksum(sum, memOffset(myIp));
        sum = addrChecksum(sum, memOffset(serverId));

        R_REG(sum);
        sum = UINT16(H(sum), L(sum));
    }
    else // state == 4
    {
        sum = (uint24_t)IP_PROTO_UDP_V + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_DISCOVER_RENEW_LEN
            + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0337 + 0x0201 + 0x03FF;

        sum = addrChecksum(sum, memOffset(myIp));
        sum = addrChecksum(sum, memOffset(myIp));
    }
    sum = addrChecksum(sum, memOffset(config.myMac));
    sum = addrChecksum(sum, memOffset(xid));
    enc28j60WriteByte(~H(sum));
    enc28j60WriteByte(~L(sum));

    enc28j60WriteByte(1); // OP
    enc28j60WriteByte(1); // HTYPE
    enc28j60WriteByte(6); // HLEN
    enc28j60WriteByteZero(); // HOPS
    writeAddr(memOffset(xid)); // Transaction ID

    if (state == 4)
    {
        writeZeros(4); // Secs, Flags
        writeAddr(memOffset(myIp)); // Client IP
        writeZeros(12); // Your IP, Server IP, Gateway IP
    }
    else
    {
        writeZeros(20); // Secs, Flags, Client IP, Your IP, Server IP, Gateway IP
    }

    // Client MAC, Server name, Boot file
    writeAddr(memOffset(config.myMac));
    writeZeros(202);

    // Magic cookie
    enc28j60WriteByte(0x63);
    enc28j60WriteByte(0x82);
    enc28j60WriteByte(0x53);
    enc28j60WriteByte(0x63);

    // Message type
    enc28j60WriteByte(53);
    enc28j60WriteByte(1);
    enc28j60WriteByte((state - 1) | 1);

    if (state == 3)
    {
        // Requested IP
        enc28j60WriteByte(50);
        enc28j60WriteByte(4);
        writeAddr(memOffset(myIp));

        // Server ID
        enc28j60WriteByte(54);
        enc28j60WriteByte(4);
        writeAddr(memOffset(serverId));
    }

    // Parameter request list
    enc28j60WriteByte(55);
    enc28j60WriteByte(2);
    enc28j60WriteByte(1);
    enc28j60WriteByte(3);

    // End option
    enc28j60WriteByte(0xFF);
}

static void sendNtpPacket(uint8_t state)
{
    uint8_t src = memOffset(time);
    uint8_t dst = memOffset(timestamp);
    R_REG(src);
    R_REG(dst);
    cli();
    uint16_t tcnt = TCNT1;
    uint8_t tifr = TIFR;
    copyAddr(dst, src);
    sei();

    if (tifr & (1 << OCF1A))
        tcnt = TIMER_1S - 1;
    uint16_t f = timeFrac(tcnt);
    mem.timestamp[0] = L(f);
    mem.timestamp[1] = H(f);

    // UDP checksum
    uint16_t sum = IP_PROTO_UDP_V + 48 + UDP_HEADER_LEN + 123 + 123 + 48 + UDP_HEADER_LEN + 0x2300;
    sum = addrChecksum(sum, memOffset(myIp));
    sum = addrChecksum(sum, memOffset(config.dstIp));
    sum = addrChecksum(sum, memOffset(timestamp));
    enc28j60WriteByte(~H(sum));
    enc28j60WriteByte(~L(sum));

    enc28j60WriteByte(0x23); // LI, VN, Mode - client
    writeZeros(39);

    // Transmit timestamp
    writeAddr(memOffset(timestamp));
    writeZeros(2);
}

static void sendUdpPacket(uint16_t len, uint8_t state)
{
    enc28j60WriteByteZero(); // Source port H // ToDo: consider variable Source port higher than 1024
    if (state == 6)
    {
        enc28j60WriteByte(123); // Source port L
        enc28j60WriteByteZero(); // Destination port H // ToDo: consider configurable Destination port
        enc28j60WriteByte(123); // L
    }
    else
    {
        enc28j60WriteByte(68); // Source port L
        enc28j60WriteByteZero(); // Destination port H
        enc28j60WriteByte(67); // L
    }

    // Length
    len -= IP_HEADER_LEN;
    enc28j60WriteByte(H(len));
    enc28j60WriteByte(L(len));

    if (state == 6)
        sendNtpPacket(state);
    else
        sendDhcpPacket(state);
}

static void sendIpPacket(uint16_t len, uint8_t state)
{
    enc28j60WriteByte(0x45); // Version, IHL
    enc28j60WriteByteZero(); // DSCP, ECN

    len -= TXSTART_INIT + ETH_HEADER_LEN;
    R_REG(len);
    enc28j60WriteByte(H(len)); // Length H
    enc28j60WriteByte(L(len)); // L

    uint16_t *ptr = &mem.ipId;
    B_REG(ptr);
    uint16_t id = *ptr + 1;
    *ptr = id;
    enc28j60WriteByte(H(id)); // Id H
    enc28j60WriteByte(L(id)); // L

    enc28j60WriteByte(0x40); // Flags - DF, Fragment offset H
    enc28j60WriteByteZero(); // L
    enc28j60WriteByte(0x20); // TTL // ToDo: change TTL to 0x40 ???
    enc28j60WriteByte(IP_PROTO_UDP_V); // Protocol

    // Checksum
    uint16_t s = 0x4500 + len + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
    R_REG(s);
    uint16_t sum = s;
    asm (
        "add %B0, %B1 \n"
        "adc %A0, %A1 \n"
        "adc %B0, __zero_reg__ \n"
        "adc %A0, __zero_reg__ \n"
        : "+r" (sum)
        : "r" (id)
    );

    if (state > 3)
        sum = addrChecksum(sum, memOffset(myIp));
    if (state == 6)
        sum = addrChecksum(sum, memOffset(config.dstIp));
    enc28j60WriteByte(~H(sum));
    enc28j60WriteByte(~L(sum));

    // Source IP
    if (state > 3)
        writeAddr(memOffset(myIp));
    else
        writeZeros(4);

    // Destination IP
    if (state == 6)
        writeAddr(memOffset(config.dstIp));
    else
        writeFFs(4);

    sendUdpPacket(len, state);
}

static void sendArpPacket(uint8_t state)
{
    enc28j60WriteByteZero(); // Hardware type H
    enc28j60WriteByte(1); // L
    enc28j60WriteByte(8); // Protocol type H
    enc28j60WriteByteZero(); // L
    enc28j60WriteByte(6); // Hardware address length
    enc28j60WriteByte(4); // Protocol address length

    // Operation
    enc28j60WriteByteZero();
    uint8_t operL = 1;
    R_REG(operL);
    if (flag & (1 << ARP_REPLY))
        operL = 2;
    enc28j60WriteByte(operL);

    writeAddr(memOffset(config.myMac)); // Sender hardware address
    writeAddr(memOffset(myIp)); // Sender protocol address

    if (flag & (1 << ARP_REPLY))
    {
        writeAddr(memOffset(arpReplyMac)); // Target hardware address
        writeAddr(memOffset(arpReplyIp)); // Target protocol address
    }
    else
    {
        writeZeros(6); // Target hardware address
        writeAddr(memOffset(arpIp)); // Target protocol address
    }
}

static void sendEthPacket(uint16_t len, uint8_t state)
{
    // Destination MAC
    if (flag & (1 << ARP_REPLY))
        writeAddr(memOffset(arpReplyMac));
    else if (state == 6)
        writeAddr(memOffset(dstMac));
    else
        writeFFs(6);

    // Source MAC
    writeAddr(memOffset(config.myMac));

    // EtherType
    enc28j60WriteByte(ETH_TYPE_H_V);
    if (state == 5 || flag & (1 << ARP_REPLY))
    {
        enc28j60WriteByte(ETH_TYPE_ARP_L_V);
        sendArpPacket(state);
    }
    else
    {
        STATIC_ASSERT(ETH_TYPE_IP_L_V == 0);
        enc28j60WriteByteZero();
        sendIpPacket(len, state);
    }
}

static void sendPacket(uint8_t state)
{
    uint16_t addr = TXSTART_INIT;

    if (state == 5 || flag & (1 << ARP_REPLY))
        addr += ARP_LEN + ETH_HEADER_LEN;
    else if (state == 3)
        addr += DHCP_REQUEST_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
    else if (state == 6)
        addr += NTP_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
    else // state == 2 || state == 4
        addr += DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;

    enc28j60WritePacket(addr);
#ifndef __AVR_ATtiny4313__
    tx('\t');
#endif
    uint16_t a = addr;
    R_REG(a);
    sendEthPacket(addr, state);
    enc28j60EndWrite(a);
#ifndef __AVR_ATtiny4313__
    tx('\r');
    tx('\n');
#endif
}

static void receiveDhcpPacket(uint16_t len, netstate_t *netstate)
{
    if (len < DHCP_OFFER_ACK_LEN + UDP_HEADER_LEN) commonReturn1;

    if (enc28j60ReadByte() != 2) commonReturn1; // OP
    if (enc28j60ReadByte() != 1) commonReturn1; // HTYPE
    if (enc28j60ReadByte() != 6) commonReturn1; // HLEN
    enc28j60ReadByte(); // HOPS // ToDo: check HOPS ???
    if (checkAddr(memOffset(xid))) commonReturn1; // Transaction ID

    readBytes(8); // Secs, Flags, Client IP // ToDo: check Secs and Flags ???

    uint8_t invalidAddr = 0;
    if (netstate->state == 2)
        readAddr(memOffset(myIp)); // Your IP
    else
        invalidAddr = checkAddr(memOffset(myIp)); // Your IP

    readBytes(8); // Server IP, Gateway IP

     // Client MAC
    if (checkAddr(memOffset(config.myMac))) return;
    for (uint8_t i = 0; i < 10; i++)
    {
        if (enc28j60ReadByte() != 0) return;
    }

    // Server name, Boot file, Magic cookie
    if (readBytes(193) != 0x63) commonReturn2;
    if (enc28j60ReadByte() != 0x82) commonReturn2;
    if (enc28j60ReadByte() != 0x53) commonReturn2;
    if (enc28j60ReadByte() != 0x63) commonReturn2;

    uint8_t found = 0;
    uint8_t type = 0;
    uint16_t readLen = 240 + UDP_HEADER_LEN;
    while (1) // ToDo: get NTP server addr and timezone from DHCP
    {
        uint8_t one = 1;
        R_REG(one);
        readLen += one;
        if (readLen > len) commonReturn2;
        uint8_t code = enc28j60ReadByte();
        if (code == 0xFF)
            break;
        if (code)
        {
            readLen += one;
            if (readLen > len) commonReturn2;
            uint8_t l = enc28j60ReadByte();
            readLen += l;
            if (readLen > len) commonReturn2;
            if (l == 4)
            {
                if (code == 1)
                {
                    if (found & 0x01) return;
                    readAddr(memOffset(netmask));
                    found |= 0x01;
                    continue;
                }
                if (code == 51)
                {
                    if (found & 0x08) return;
                    uint8_t b3 = enc28j60ReadByte();
                    uint8_t b2 = enc28j60ReadByte();
                    uint8_t b1 = enc28j60ReadByte();
                    uint8_t b0 = enc28j60ReadByte();
                    asm (
                        "lsr %2 \n"
                        "ror %1 \n"
                        "ror %0 \n"
                        "or %2, %3 \n"
                        "breq store_%= \n"
                        "ldi %0, 0xFF \n"
                        "ldi %1, 0xFF \n"
                        "store_%=:"
                        "cli \n"
                        "sts %4 + 1, %1 \n"
                        "sts %4, %0 \n"
                        "sei \n"
                        :
                        : "d" (b0), "d" (b1), "r" (b2), "r" (b3), "m" (mem.leaseTime)
                    );
                    found |= 0x08;
                    continue;
                }
                if (code == 54)
                {
                    if (found & 0x10) return;
                    if (netstate->state == 2)
                        readAddr(memOffset(serverId));
                    else
                        invalidAddr |= checkAddr(memOffset(serverId));
                    found |= 0x10;
                    continue;
                }
            }
            if (l >= 4 && code == 3)
            {
                if (found & 0x02) return;
                readAddr(memOffset(gwIp));
                found |= 0x02;
                l -= 4;
            }
            if (l == 1 && code == 53)
            {
                if (found & 0x04) return;
                type = enc28j60ReadByte();
                found |= 0x04;
                continue;
            }
            if (l)
                readBytes(l);
            asm ("");
        }
    }

    do
    {
        if (type == 6)
        {
            netstate->state = 2;
            break;
        }

        if (invalidAddr) return;

        uint8_t arpPtr = memOffset(config.dstIp);
        uint8_t c = 4;
        R_REG(arpPtr);
        R_REG(c);

        if (found < 0x1C) commonReturn3;
        if (found & 0x01)
        {
            uint8_t *pd = mem.config.dstIp + 4;
            uint8_t *pm = mem.myIp + 4;
            uint8_t *pn = mem.netmask + 4;
            do
            {
                uint8_t d = *--pd;
                uint8_t m = *--pm;
                uint8_t n = *--pn;
                if ((d & n) != (m & n))
                {
                    if (!(found & 0x02)) return;
                    arpPtr = memOffset(gwIp);
                    break;
                }
            } while (--c);
        }

        if (netstate->state == 2)
        {
            if (type != 2) return;
            netstate->state = 3;
        }
        else
        {
            if (type != 5) return;
            netstate->state = 7;
        }

        copyAddr(memOffset(arpIp), arpPtr);
    } while (0);

    resetRetryTime();
    netstate->retryCount = RETRY_COUNT;
}

static void receiveNtpPacket(netstate_t *netstate)
{
    // LI, VN, Mode
    uint8_t x = enc28j60ReadByte();
    if ((x & 0x07) != 0x04) commonReturn3;
    if ((x & 0x38) == 0x00) commonReturn3;
    if ((x & 0xC0) == 0xC0) commonReturn3; // ToDo: handle leap seconds

    // Stratum
    x = enc28j60ReadByte() - 1;
    if (x > 15 - 1) commonReturn3;

    readBytes(22); // ToDo: check if reference clock is synchronized

    // Originate timestamp
    if (checkAddr(memOffset(timestamp))) return;
    if (enc28j60ReadByte() != 0) return;
    if (enc28j60ReadByte() != 0) return;

    uint8_t *timestampPtr = mem.timestamp;
    B_REG(timestampPtr);
    uint32_t *timePtr = (uint32_t *)(timestampPtr - offsetof(mem_t, timestamp) + offsetof(mem_t, time));

    cli(); // ToDo: read timestamp immediately after receiving packet
    uint16_t tcnt = TCNT1;
    uint8_t tifr = TIFR;
    uint32_t timInt = *timePtr;
    sei();

    if (tifr & (1 << OCF1A))
        tcnt = TIMER_1S - 1;
    uint16_t timFrac = timeFrac(tcnt);

    uint8_t temp[6] = { timestampPtr[0], timestampPtr[1], timestampPtr[2], timestampPtr[3], timestampPtr[4],
        timestampPtr[5] };

    // (timInt, timFrac) -= (temp[5], temp[4], temp[3], temp[2], temp[1], temp[0]);
    asm (
        "sub %A0, %2 \n"
        "sbc %B0, %3 \n"
        "sbc %A1, %4 \n"
        "sbc %B1, %5 \n"
        "sbc %C1, %6 \n"
        "sbc %D1, %7 \n"
        : "+r" (timFrac), "+r" (timInt)
        : "r" (temp[0]), "r" (temp[1]), "r" (temp[2]), "r" (temp[3]), "r" (temp[4]), "r" (temp[5])
    );

    // Receive timestamp
    temp[5] = enc28j60ReadByte();
    temp[4] = enc28j60ReadByte();
    temp[3] = enc28j60ReadByte();
    temp[2] = enc28j60ReadByte();
    temp[1] = enc28j60ReadByte();
    temp[0] = enc28j60ReadByte();

    // (timInt, timFrac) += (temp[5], temp[4], temp[3], temp[2], temp[1], temp[0]);
    asm (
        "add %A0, %2 \n"
        "adc %B0, %3 \n"
        "adc %A1, %4 \n"
        "adc %B1, %5 \n"
        "adc %C1, %6 \n"
        "adc %D1, %7 \n"
        : "+r" (timFrac), "+r" (timInt)
        : "r" (temp[0]), "r" (temp[1]), "r" (temp[2]), "r" (temp[3]), "r" (temp[4]), "r" (temp[5])
    );

    // Transmit timestamp
    temp[5] = readBytes(3);
    temp[4] = enc28j60ReadByte();
    temp[3] = enc28j60ReadByte();
    temp[2] = enc28j60ReadByte();
    temp[1] = enc28j60ReadByte();
    temp[0] = enc28j60ReadByte();

    // (timInt, timFrac) += (temp[5], temp[4], temp[3], temp[2], temp[1], temp[0]);
    // (timInt, timFrac) >>= 1;
    asm (
        "add %A0, %2 \n"
        "adc %B0, %3 \n"
        "adc %A1, %4 \n"
        "adc %B1, %5 \n"
        "adc %C1, %6 \n"
        "adc %D1, %7 \n"
        "ror %D1 \n"
        "ror %C1 \n"
        "ror %B1 \n"
        "ror %A1 \n"
        "ror %B0 \n"
        "ror %A0 \n"
        : "+r" (timFrac), "+r" (timInt)
        : "r" (temp[0]), "r" (temp[1]), "r" (temp[2]), "r" (temp[3]), "r" (temp[4]), "r" (temp[5])
    );

    u32_t resTcnt = { .u32 = mulAdd16(0, timFrac, TIMER_1S) };

    resetTimer(timInt, resTcnt.u16[1]);

    netstate->state = 7;
#ifndef __AVR_ATtiny4313__
    tx('\r');
    tx('\n');
    tx(':');
#endif
}

static void receiveUdpPacket(uint16_t len, netstate_t *netstate)
{
    // Source port
    if (enc28j60ReadByte() != 0) return;
    uint8_t srcPort = enc28j60ReadByte();

    // Destination port
    if (enc28j60ReadByte() != 0) return;
    uint8_t dstPort = enc28j60ReadByte();

    // Length
    uint8_t hi = enc28j60ReadByte();
    uint8_t lo = enc28j60ReadByte();
    uint16_t udpLen = UINT16(lo, hi);
    if (udpLen > len || udpLen < NTP_LEN + UDP_HEADER_LEN) commonReturn1;

    // Checksum
    enc28j60ReadByte();
    enc28j60ReadByte();

    if (netstate->state < 5)
    {
        if (srcPort != 67) return;
        if (dstPort != 68) commonReturn1;
        receiveDhcpPacket(udpLen, netstate);
    }
    else
    {
        if (netstate->state != 6) commonReturn3;
        if (srcPort != 123) return;
        if (dstPort != 123) commonReturn3;
        receiveNtpPacket(netstate);
    }
}

static void receiveIpPacket(uint16_t len, netstate_t *netstate)
{
    if (enc28j60ReadByte() != 0x45) commonReturn4; // Version, IHL
    enc28j60ReadByte(); // DSCP, ECN // ToDo: check DSCP, ECN ???

    // Length
    uint8_t hi = enc28j60ReadByte();
    uint8_t lo = enc28j60ReadByte();
    uint16_t ipLen = UINT16(lo, hi);
    if (ipLen > len || ipLen < NTP_LEN + UDP_HEADER_LEN + IP_HEADER_LEN) commonReturn4;

    // Id, Flags, Fragment offset, TTL, Protocol // ToDo: check Flags, Fragment offset ???
    if (readBytes(6) != IP_PROTO_UDP_V) commonReturn4;

    if (netstate->state == 6)
    {
        enc28j60ReadByte(); // Checksum H
        enc28j60ReadByte(); // L
        if (checkAddr(memOffset(config.dstIp))) commonReturn4; // Source IP
    }
    else
        readBytes(6); // Checksum, Source IP // ToDo: check Source IP ???

    // Destination IP
    if (netstate->state > 4)
    {
        if (checkAddr(memOffset(myIp))) return;
    }
    else
        readBytes(4); // ToDo: check Destination IP ???

    receiveUdpPacket(ipLen - IP_HEADER_LEN, netstate);
}

static void receiveArpPacket(netstate_t *netstate)
{
    if (enc28j60ReadByte() != 0) return; // Hardware type H
    if (enc28j60ReadByte() != 1) return; // L
    if (enc28j60ReadByte() != 8) return; // Protocol type H
    if (enc28j60ReadByte() != 0) return; // L
    if (enc28j60ReadByte() != 6) return; // Hardware address length
    if (enc28j60ReadByte() != 4) return; // Protocol address length

    // Operation
    if (enc28j60ReadByte() != ARP_OP_H_V) return;
    uint8_t operL = enc28j60ReadByte();
    if (operL == ARP_OP_REQUEST_L_V)
    {
        readAddr(memOffset(arpReplyMac)); // Sender hardware address
        readAddr(memOffset(arpReplyIp)); // Sender protocol address

        readBytes(6); // Target hardware address
        if (checkAddr(memOffset(myIp))) return; // Target protocol address

        flag |= (1 << ARP_REPLY);
    }
    else if (operL == ARP_OP_REPLY_L_V && netstate->state == 5 && netstate->retryCount)
    {
        readAddr(memOffset(dstMac)); // Sender hardware address
        if (checkAddr(memOffset(arpIp))) return; // Sender protocol address

        netstate->state = 6; // ToDo: check target IP, MAC ???
        resetRetryTime();
        netstate->retryCount = RETRY_COUNT;
    }
}

static void receiveEthPacket(uint16_t len, netstate_t *netstate)
{
    if (len < ARP_LEN + ETH_HEADER_LEN + CRC_LEN) commonReturn4;

    // Receive status, Destination MAC (checked by ENC28J60), Source MAC (checked by ENC28J60), EtherType
    if (!(spiTransferZero() & 0x80)) return;
#ifdef __AVR_ATtiny4313__
    uint8_t typeH = readBytes(14);
#else
    spiTransferZero();
    uint8_t typeH = readBytes(13);
#endif
    if (typeH != ETH_TYPE_H_V) commonReturn4;
    uint8_t typeL = enc28j60ReadByte();
    if (typeL == ETH_TYPE_IP_L_V)
    {
        if (netstate->retryCount == 0) commonReturn4;
        receiveIpPacket(len - ETH_HEADER_LEN - CRC_LEN, netstate);
    }
    else if (typeL == ETH_TYPE_ARP_L_V && netstate->state > 3)
        receiveArpPacket(netstate);
}

static void receivePacket(netstate_t *netstate)
{
    uint16_t len = enc28j60ReadPacket(&netstate->nextPacketPtr);
    receiveEthPacket(len, netstate);
    enc28j60EndRead(&netstate->nextPacketPtr);
#ifndef __AVR_ATtiny4313__
    tx('\r');
    tx('\n');
#endif
}

void netTick()
{
    uint32_t *timePtr = &mem.time;
    B_REG(timePtr);
    uint16_t *leaseTimePtr = (uint16_t *)((uint8_t *)timePtr - offsetof(mem_t, time) + offsetof(mem_t, leaseTime));
    uint16_t *syncTimePtr = (uint16_t *)((uint8_t *)timePtr - offsetof(mem_t, time) + offsetof(mem_t, syncTime));

    if (*leaseTimePtr)
        (*leaseTimePtr)--;

    if (*syncTimePtr)
        (*syncTimePtr)--;

    uint16_t temp = retryTime;
    if (temp)
        retryTime = temp - 1;

    uint32_t time = *timePtr + 1;
    *timePtr = time;
    R_REG(time);

    if (flag & (1 << TIME_OK))
        displayTime(time);
}

void netLoop(netstate_t *netstate)
{
    if (netstate->state == 0)
    {
        netstate->nextPacketPtr = RXSTART_INIT;
        R_REG(netstate->nextPacketPtr);
        if (enc28j60Init(&mem.config.myMac[6]))
            netstate->state = 1;
        netstate->retryCount = 0;
    }
    else if (!enc28j60Ready())
    {
        netstate->state = 0;
        netstate->retryCount = 0;
    }
    else if (!enc28j60LinkUp())
    {
        netstate->state = 1;
        netstate->retryCount = 0;
    }
    else if (netstate->state == 1)
    {
        *((uint8_t *)&mem.leaseTime + 1) = 0;
        *((uint8_t *)&mem.leaseTime) = 0;
        netstate->state = 2;
        R_REG(netstate->state);
        if (flag & (1 << CUSTOM_IP))
            netstate->state = 5;
        resetRetryTime();
        netstate->retryCount = RETRY_COUNT;
    }
    else if (enc28j60PacketReceived()) // ToDo: deal with buffer overflow
        receivePacket(netstate);

    uint16_t *leaseTimePtr = &mem.leaseTime;
    B_REG(leaseTimePtr);
    uint16_t sync;
    uint16_t lease;
    asm (
        "ldd %B0, %a2 + %3 + 1 \n"
        "ldd %A0, %a2 + %3 \n"
        "ldd %B1, %a2 + 1 \n"
        "ld %A1, %a2 \n"
        : "=&r" (sync), "=&r" (lease)
        : "b" (leaseTimePtr), "I" (offsetof(mem_t, syncTime) - offsetof(mem_t, leaseTime))
    );

    if (lease == 0 && netstate->state > 4 && !(flag & (1 << CUSTOM_IP)))
    {
#ifndef __AVR_ATtiny4313__
        tx('L');
        tx('\r');
        tx('\n');
#endif
        netstate->state = 4;
        R_REG(netstate->state);
        resetRetryTime();
        netstate->retryCount = RETRY_COUNT;
    }
    else if (sync == 0 && netstate->state > 6)
    {
#ifndef __AVR_ATtiny4313__
        tx('S');
        tx('\r');
        tx('\n');
#endif
        netstate->state = 5;
        R_REG(netstate->state);
        resetRetryTime();
        netstate->retryCount = RETRY_COUNT;
    }

    if (sync == 0 && netstate->retryCount == 0)
    {
#ifndef __AVR_ATtiny4313__
        if (flag & (1 << SYNC_OK))
        {
            tx('.');
            tx('\r');
            tx('\n');
        }
#endif
        flag &= ~(1 << SYNC_OK);
    }

    if (netstate->state < 2)
        commonReturn5;

    if (!(flag & (1 << ARP_REPLY)))
    {
        uint8_t retryH = retryTimeH;
        uint8_t retryL = retryTimeL;
        if ((retryL | retryH) || netstate->state > 6)
            commonReturn5;
        if (netstate->retryCount)
        {
            (netstate->retryCount)--;
            if (netstate->retryCount == 0)
            {
                uint16_t temp = RETRY_TIMEOUT_L;
                R_REG(temp);
                cli();
                retryTime = temp;
                sei();
#ifndef __AVR_ATtiny4313__
                tx('W');
                tx('\r');
                tx('\n');
#endif
                commonReturn5;
            }
#ifndef __AVR_ATtiny4313__
            tx('R');
#endif
        }
        else
        {
            if (!(netstate->state & 1))
                netstate->state--;
            if (!(netstate->state & 4))
                netstate->state = 2;
            netstate->retryCount = RETRY_COUNT - 1;
#ifndef __AVR_ATtiny4313__
            tx('F');
#endif
        }
        retryTimeL = RETRY_TIMEOUT_S;
#ifndef __AVR_ATtiny4313__
        tx(' ');
        txHex(netstate->state);
        tx(' ');
        txHex(netstate->retryCount);
        tx('\r');
        tx('\n');
#endif
    }

    sendPacket(netstate->state); // ToDo: deal with buffer overflow
    flag &= ~(1 << ARP_REPLY);
}

void netInit(netstate_t *netstate)
{
    uint8_t arpPtr = memOffset(config.dstIp);
    STATIC_ASSERT(offsetof(mem_t, leaseTime) + sizeof(uint16_t) == offsetof(mem_t, syncTime));
    uint8_t *pt = (uint8_t *)&mem.leaseTime + 4;
    uint8_t *pd = mem.config.dstIp + 4;
    uint8_t *pm = mem.config.myIp + 4;
    uint8_t *pn = mem.config.netmask + 4;
    uint8_t c = 4;
    R_REG(c);
    do
    {
        *--pt = 0;
        uint8_t d = *--pd;
        uint8_t m = *--pm;
        uint8_t n = *--pn;
        if (m != 0)
        {
            flag |= (1 << CUSTOM_IP);
        }
        if ((d & n) != (m & n))
        {
            arpPtr = memOffset(config.gwIp);
        }
    } while (--c);

    copyAddr(memOffset(arpIp), arpPtr);
    copyAddr(memOffset(myIp), memOffset(config.myIp));
    copyAddr(memOffset(xid), memOffset(config.myMac));

    netstate->state = 0;
}