/*
 * net.c
 *
 * Created: 01.07.2018 14:32:04
 *  Author: Mikolaj
 */ 

#include "net.h"

// ToDo: writeByte from PROGMEM

// ToDo: try using local array instead of global

net_t net __attribute__((section(".noinit")));

#define offsetof(type, member) __builtin_offsetof(type, member)
#define memberSize(type, member) sizeof(((type *)0)->member)
#define netOffset(member) (offsetof(net_t, member) + memberSize(net_t, member))

#ifdef __AVR_ATtiny4313__
#define enc28j60ReadByte() spiTransferZero()
#define enc28j60WriteByte(data) spiTransfer(data)
#define enc28j60WriteZero() spiTransferZero()
#else
static void tx(uint8_t data)
{
    while (!(UCSRA & (1<<UDRE)));
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

static void enc28j60WriteZero()
{
    spiTransferZero();
    txHex(0);
}
#endif

#ifdef __AVR_ATtiny4313__
#define writeZeros(length) readBytes(length)
#else
static void writeZeros(uint8_t len)
{
    do
    {
        enc28j60WriteZero();
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

static uint8_t __attribute__((noinline)) getAddrLength(uint8_t p)
{
    //if (p <= netOffset(syncTime))
    //    return 2;
    if (p <= netOffset(config.myMac))
        return 6;
    else
        return 4;
}

static void writeAddr(uint8_t p)
{
    uint8_t *ptr = (uint8_t *)&net + p;
    Y_REG(ptr);
    uint8_t len = getAddrLength(p);
    do
    {
        enc28j60WriteByte(*--ptr);
    } while (--len);
}

static uint16_t addrChecksum(uint16_t sum, uint8_t p)
{
    uint8_t* ptr = (uint8_t *)&net + p;
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
        : "+r" (sum), "+e" (ptr), "+d" (p), "=d" (c) : "M" (netOffset(config.myMac) + 1)
    );

    return sum;
}

static uint8_t readBytes(uint8_t len)
{
    uint8_t ret;
    do
    {
        ret = enc28j60ReadByte();
    } while (--len);
    return ret;
}

static void readAddr(uint8_t p)
{
    uint8_t *ptr = (uint8_t *)&net + p;
    Y_REG(ptr);
    uint8_t len = getAddrLength(p);
    do
    {
        *--ptr = enc28j60ReadByte();
    } while (--len);
}

static uint8_t checkAddr(uint8_t p)
{
    uint8_t *ptr = (uint8_t *)&net + p;
    Y_REG(ptr);
    uint8_t len = getAddrLength(p);
    do
    {
        if (*--ptr != enc28j60ReadByte()) return len;
    } while (--len);
    return len;
}

static void copyAddr(uint8_t d, uint8_t s)
{
    uint8_t *dst = (uint8_t *)&net + d;
    uint8_t *src = (uint8_t *)&net + s;
    uint8_t len = 4;
    R_REG(len);
    do
    {
        *--dst = *--src;
    } while (--len);
}

static uint16_t timeFrac(uint16_t tcnt)
{
    return DWORD(0, 0, L(tcnt), H(tcnt)) / TIMER_1S;
}

static void sendDhcpPacket(uint8_t state)
{
    // UDP checksum
    uint16_t sum;
    if (state == 1)
    {
        uint16_t* ptr = (uint16_t *)&net.xid;
        E_REG(ptr);
        *ptr = *ptr + 1; // ToDo

        sum = (uint24_t)IP_PROTO_UDP_V + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0137 + 0x0401 + 0x032A + 0x64FF - 0xFFFF;
    }
    else if (state == 2)
    {
        sum = (uint24_t)IP_PROTO_UDP_V + DHCP_REQUEST_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_REQUEST_LEN + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0332 + 0x0400 + 0x0036 + 0x0400 + 0x0037 + 0x0401 + 0x032A + 0x64FF - 0xFFFF;
        sum = sum << 8 | sum >> 8;
        
        sum = addrChecksum(sum, netOffset(myIp));
        sum = addrChecksum(sum, netOffset(serverId));

        sum = sum << 8 | sum >> 8;
    }
    else // state == 3
    {
        sum = (uint24_t)IP_PROTO_UDP_V + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0337 + 0x0401 + 0x032A + 0x64FF - 0xFFFF;

        sum = addrChecksum(sum, netOffset(myIp));
        sum = addrChecksum(sum, netOffset(myIp));
    }
    sum = addrChecksum(sum, netOffset(config.myMac));
    sum = addrChecksum(sum, netOffset(xid));
    enc28j60WriteByte(~H(sum));
    enc28j60WriteByte(~L(sum));

    enc28j60WriteByte(1); // OP
    enc28j60WriteByte(1); // HTYPE
    enc28j60WriteByte(6); // HLEN
    enc28j60WriteZero(); // HOPS
    writeAddr(netOffset(xid)); // Transaction ID

    if (state == 3)
    {
        writeZeros(4); // Secs, Flags
        writeAddr(netOffset(myIp)); // Client IP
        writeZeros(12); // Your IP, Server IP, Gateway IP
    }
    else
    {
        writeZeros(20); // Secs, Flags, Client IP, Your IP, Server IP, Gateway IP
    }

    // Client MAC, Server name, Boot file
    writeAddr(netOffset(config.myMac));
    writeZeros(202);

    // Magic cookie
    enc28j60WriteByte(0x63);
    enc28j60WriteByte(0x82);
    enc28j60WriteByte(0x53);
    enc28j60WriteByte(0x63);

    // Message type
    enc28j60WriteByte(53);
    enc28j60WriteByte(1);
    enc28j60WriteByte(state | 1);

    if (state == 2)
    {
        // Requested IP
        enc28j60WriteByte(50);
        enc28j60WriteByte(4);
        writeAddr(netOffset(myIp));

        // Server ID
        enc28j60WriteByte(54);
        enc28j60WriteByte(4);
        writeAddr(netOffset(serverId));
    }

    // Parameter request list
    enc28j60WriteByte(55);
    enc28j60WriteByte(4);
    enc28j60WriteByte(1);
    enc28j60WriteByte(3);
    enc28j60WriteByte(42);
    enc28j60WriteByte(100);

    // End option
    enc28j60WriteByte(0xFF);
}

static void sendNtpPacket(uint8_t state)
{
    uint8_t src = netOffset(time);
    uint8_t dst = netOffset(timestamp);
    R_REG(src);
    R_REG(dst);
    cli();
    uint16_t tcnt = TCNT1;
    uint8_t tifr = TIFR;
    copyAddr(dst, src);
    sei();
    
    if (tifr & (1<<OCF1A))
        tcnt = TIMER_1S - 1;
    uint16_t f = timeFrac(tcnt);
    net.timestamp[0] = L(f);
    net.timestamp[1] = H(f);

    // UDP checksum
    uint16_t sum = IP_PROTO_UDP_V + 48 + UDP_HEADER_LEN + 123 + 123 + 48 + UDP_HEADER_LEN + 0x2300;
    sum = addrChecksum(sum, netOffset(myIp));
    sum = addrChecksum(sum, netOffset(config.dstIp));
    sum = addrChecksum(sum, netOffset(timestamp));
    enc28j60WriteByte(~H(sum));
    enc28j60WriteByte(~L(sum));

    enc28j60WriteByte(0x23); // LI, VN, Mode - client
    writeZeros(39);

    // Transmit timestamp
    writeAddr(netOffset(timestamp));
    writeZeros(2);
}

static void sendUdpPacket(uint16_t len, uint8_t state)
{
    enc28j60WriteZero(); // Source port H // ToDo: consider variable Source port higher than 1024
    if (state == 5)
    {
        enc28j60WriteByte(123); // Source port L
        enc28j60WriteZero(); // Destination port H // ToDo: consider configurable Destination port
        enc28j60WriteByte(123); // L
    }
    else
    {
        enc28j60WriteByte(68); // Source port L
        enc28j60WriteZero(); // Destination port H
        enc28j60WriteByte(67); // L
    }

    // Length
    len -= IP_HEADER_LEN;
    enc28j60WriteByte(H(len));
    enc28j60WriteByte(L(len));

    if (state == 5)
        sendNtpPacket(state);
    else
        sendDhcpPacket(state);
}

static void sendIpPacket(uint16_t len, uint8_t state)
{
    enc28j60WriteByte(0x45); // Version, IHL
    enc28j60WriteZero(); // DSCP, ECN

    len -= TXSTART_INIT + ETH_HEADER_LEN;
    R_REG(len);
    enc28j60WriteByte(H(len)); // Length H
    enc28j60WriteByte(L(len)); // L
    
    uint16_t* ptr = &net.ipId;
    E_REG(ptr);
    uint16_t id = *ptr + 1;
    *ptr = id;
    enc28j60WriteByte(H(id)); // Id H
    enc28j60WriteByte(L(id)); // L
    
    enc28j60WriteByte(0x40); // Flags - DF, Fragment offset H
    enc28j60WriteZero(); // L
    enc28j60WriteByte(0x20); // TTL // ToDo: TTL 0x40 ???
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
        : "+r" (sum) : "r" (id)
    );
    
    if (state > 2)
        sum = addrChecksum(sum, netOffset(myIp));
    if (state == 5)
        sum = addrChecksum(sum, netOffset(config.dstIp));
    enc28j60WriteByte(~H(sum));
    enc28j60WriteByte(~L(sum));

    // Source IP
    if (state > 2)
        writeAddr(netOffset(myIp));
    else
        writeZeros(4);

    // Destination IP
    if (state == 5)
        writeAddr(netOffset(config.dstIp));
    else
        writeFFs(4);

    sendUdpPacket(len, state);
}

static void sendArpPacket(uint8_t state)
{
    enc28j60WriteZero(); // Hardware type H
    enc28j60WriteByte(1); // L
    enc28j60WriteByte(8); // Protocol type H
    enc28j60WriteZero(); // L
    enc28j60WriteByte(6); // Hardware address length
    enc28j60WriteByte(4); // Protocol address length

    // Operation
    enc28j60WriteZero();
    uint8_t operL = 1;
    R_REG(operL);
    if (flag & (1<<ARP_REPLY))
        operL = 2;
    enc28j60WriteByte(operL);

    writeAddr(netOffset(config.myMac)); // Sender hardware address
    writeAddr(netOffset(myIp)); // Sender protocol address

    if (flag & (1<<ARP_REPLY))
    {
        writeAddr(netOffset(arpReplyMac)); // Target hardware address
        writeAddr(netOffset(arpReplyIp)); // Target protocol address
    }
    else
    {
        writeZeros(6); // Target hardware address
        writeAddr(netOffset(arpIp)); // Target protocol address
    }
}

static void sendEthPacket(uint16_t len, uint8_t state)
{
    // Destination MAC
    if (flag & (1<<ARP_REPLY))
        writeAddr(netOffset(arpReplyMac));
    else if (state == 5)
        writeAddr(netOffset(dstMac));
    else
        writeFFs(6);

    // Source MAC
    writeAddr(netOffset(config.myMac));

    // EtherType
    enc28j60WriteByte(ETHTYPE_H_V);
    if (state == 4 || flag & (1<<ARP_REPLY))
    {
        enc28j60WriteByte(ETHTYPE_ARP_L_V);
        sendArpPacket(state);
    }
    else
    {
        enc28j60WriteZero(); // ETHTYPE_IP_L_V
        sendIpPacket(len, state);
    }
}

static void sendPacket(uint8_t state)
{
    uint16_t addr = TXSTART_INIT;
    
    if (state == 4 || flag & (1<<ARP_REPLY))
        addr += ARP_LEN + ETH_HEADER_LEN;
    else if (state == 2)
        addr += DHCP_REQUEST_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
    else if (state == 5)
        addr += NTP_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
    else // state == 1 || state == 3
        addr += DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
    
    enc28j60WritePacket(addr);
#ifndef __AVR_ATtiny4313__
    tx('\t');
#endif
    sendEthPacket(addr, state);
    enc28j60EndWrite();
#ifndef __AVR_ATtiny4313__
    tx('\r');
    tx('\n');
#endif
}

#define return1 do { asm (""); return; } while (0)
#define return2 do { asm (" "); return; } while (0)
#define return3 do { asm ("  "); return; } while (0)
#define return4 do { asm ("   "); return; } while (0)
#define return5 do { asm ("    "); return; } while (0)

static void receiveDhcpPacket(uint16_t len, netstate_t* netstate)
{
    if (len < DHCP_OFFER_ACK_LEN + UDP_HEADER_LEN) return;

    if (enc28j60ReadByte() != 2) return1; // OP
    if (enc28j60ReadByte() != 1) return1; // HTYPE
    if (enc28j60ReadByte() != 6) return1; // HLEN
    enc28j60ReadByte(); // HOPS // ToDo: check HOPS
    if (checkAddr(netOffset(xid))) return1; // Transaction ID

    readBytes(8); // Secs, Flags, Client IP // ToDo: check Secs and Flags

    uint8_t invalidAddr = 0;
    if (netstate->state == 1)
        readAddr(netOffset(myIp)); // Your IP
    else
        invalidAddr = checkAddr(netOffset(myIp)); // Your IP
    
    readBytes(8); // Server IP, Gateway IP

     // Client MAC
    if (checkAddr(netOffset(config.myMac))) return;
    for (uint8_t i = 0; i < 10; i++)
    {
        if (enc28j60ReadByte() != 0) return;
    }
    
    // Server name, Boot file, Magic cookie
    if (readBytes(193) != 0x63) return2;
    if (enc28j60ReadByte() != 0x82) return2;
    if (enc28j60ReadByte() != 0x53) return2;
    if (enc28j60ReadByte() != 0x63) return2;

    uint8_t found = 0;
    uint8_t type = 0;
    uint16_t readLen = 240 + UDP_HEADER_LEN;// + IP_HEADER_LEN + ETH_HEADER_LEN + CRC_LEN;
    while (1) // ToDo: get NTP server addr and timezone from DHCP
    {
        readLen++;
        if (readLen > len) return2;
        uint8_t code = enc28j60ReadByte();
        if (code == 0xFF)
            break;
        if (code)
        {
            readLen++;
            if (readLen > len) return2;
            uint8_t l = enc28j60ReadByte();
            readLen += l;
            if (readLen > len) return2;
            if (code == 53 && l == 1)
            {
                if (found & 0x04) return;
                type = enc28j60ReadByte();
                found |= 0x04;
            }
            else if (code == 54 && l == 4)
            {
                if (found & 0x10) return;
                if (netstate->state == 1)
                    readAddr(netOffset(serverId));
                else
                    invalidAddr |= checkAddr(netOffset(serverId));
                found |= 0x10;
            }
            else if (code == 51 && l == 4)
            {
                if (found & 0x08) return;
                uint8_t b3 = enc28j60ReadByte();
                uint8_t b2 = enc28j60ReadByte();
                uint8_t b1 = enc28j60ReadByte();
                uint8_t b0 = enc28j60ReadByte();
                asm volatile (
                    "lsr %2 \n"
                    "ror %1 \n"
                    "ror %0 \n"
                    "or %2, %3 \n"
                    "breq store_%= \n"
                    "ldi %0, 0xFF \n"
                    "ldi %1, 0xFF \n"
                    "store_%=:"
                    "cli \n"
                    "sts %4+1, %1 \n"
                    "sts %4, %0 \n"
                    "sei \n"
                    :
                    : "d"(b0), "d"(b1), "r"(b2), "r"(b3), "m"(net.leaseTime)
                );
                found |= 0x08;
            }
            else if (code == 1 && l == 4)
            {
                if (found & 0x01) return;
                readAddr(netOffset(netmask));
                found |= 0x01;
            }
            else
            {
                if (code == 3 && l >= 4)
                {
                    if (found & 0x02) return;
                    readAddr(netOffset(gwIp));
                    found |= 0x02;
                    l -= 4;
                }
                if (l)
                    readBytes(l);
                asm ("");
            }
        }
    }
    
    do
    {
        if (type == 6)
        {
            netstate->state = 1;
            break;
        }

        if (invalidAddr) return;
        if (found < 0x1C) return;

        uint8_t arpPtr = netOffset(config.dstIp);

        if (found & 0x01) // found netmask
        {
            uint8_t c = 4;
            R_REG(c);
            uint8_t* pd = net.config.dstIp + 4;
            uint8_t* pm = net.myIp + 4;
            uint8_t* pn = net.netmask + 4;
            do
            {
                uint8_t d = *--pd;
                uint8_t m = *--pm;
                uint8_t n = *--pn;
                if ((d & n) != (m & n)) // should route via gw
                {
                    if (!(found & 0x02)) return; // gw not found
                    arpPtr = netOffset(gwIp);
                    break;
                }
            } while (--c);
        }

        if (netstate->state == 1)
        {
            if (type != 2) return;
            netstate->state = 2;
        }
        else
        {
            if (type != 5) return;
            netstate->state = 6;
        }
        copyAddr(netOffset(arpIp), arpPtr);
    } while (0);

    retryTimeH = 0;
    retryTimeL = 0;
    netstate->retryCount = RETRY_COUNT;
}

static void receiveNtpPacket(netstate_t* netstate)
{
    // LI, VN, Mode
    uint8_t x = enc28j60ReadByte();
    if ((x & 0x07) != 0x04) return3;
    if ((x & 0x38) == 0x00) return3;
    if ((x & 0xC0) == 0xC0) return3; // ToDo: handle leap seconds

    // Stratum
    x = enc28j60ReadByte() - 1;
    if (x > 15 - 1) return3;

    readBytes(22); // ToDo: check if reference clock is synchronized
    
    // Originate timestamp
    if (checkAddr(netOffset(timestamp))) return;
    if (enc28j60ReadByte() != 0) return;
    if (enc28j60ReadByte() != 0) return;

    net_t *ptr = &net;
    E_REG(ptr);
    
    cli(); // ToDo: read timestamp immediately after receiving packet
    uint16_t tcnt = TCNT1;
    uint8_t tifr = TIFR;
    uint32_t timInt = ptr->time;
    sei();

    if (tifr & (1<<OCF1A))
        tcnt = TIMER_1S - 1;
    uint16_t timFrac = timeFrac(tcnt);

    uint8_t temp[6] = { ptr->timestamp[0], ptr->timestamp[1], ptr->timestamp[2], ptr->timestamp[3], ptr->timestamp[4], ptr->timestamp[5] };

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
    
    u32 resTcnt = {0};
    resTcnt.d = (uint32_t)timFrac * TIMER_1S;

    ptr = &net;
    uint8_t gtccr = (1<<PSR10);
    tifr = (1<<OCF1A) | (1<<OCF1B);
    uint16_t timeout = SYNC_TIMEOUT;
    E_REG(ptr);
    R_REG(gtccr);
    R_REG(tifr);
    R_REG(timeout);

    cli();
    GTCCR = gtccr;
    TCNT1 = resTcnt.w[1];
    TIFR = tifr;
    ptr->time = timInt;
    ptr->syncTime = timeout;
    flag |= (1<<TIME_OK);
    flag |= (1<<SYNC_OK);
    sei();

    netstate->state = 6;
#ifndef __AVR_ATtiny4313__
    tx('\r');
    tx('\n');
    tx(':');
#endif
}

static void receiveUdpPacket(uint16_t len, netstate_t* netstate)
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
    uint16_t udpLen = WORD(lo, hi);
    if (udpLen > len || udpLen < NTP_LEN + UDP_HEADER_LEN) return5;

    // Checksum
    enc28j60ReadByte();
    enc28j60ReadByte();

    if (netstate->state <= 3 && srcPort == 67 && dstPort == 68)
        receiveDhcpPacket(udpLen, netstate);
    else if (netstate->state == 5 && srcPort == 123 && dstPort == 123)
        receiveNtpPacket(netstate);
}

static void receiveIpPacket(uint16_t len, netstate_t* netstate)
{
    if (enc28j60ReadByte() != 0x45) return4; // Version, IHL
    enc28j60ReadByte(); // DSCP, ECN // ToDo: check DSCP, ECN ???

    // Length
    uint8_t hi = enc28j60ReadByte();
    uint8_t lo = enc28j60ReadByte();
    uint16_t ipLen = WORD(lo, hi);
    if (ipLen > len || ipLen < NTP_LEN + UDP_HEADER_LEN + IP_HEADER_LEN) return4;

    // Id, Flags, Fragment offset, TTL, Protocol // ToDo: check Flags, Fragment offset ???
    if (readBytes(6) != IP_PROTO_UDP_V) return4;

    if (netstate->state == 5)
    {
        enc28j60ReadByte(); // Checksum H
        enc28j60ReadByte(); // L
        if (checkAddr(netOffset(config.dstIp))) return4; // Source IP
    }
    else
    {
        readBytes(6); // Checksum, Source IP // ToDo: check Source IP ???
    }

    // Destination IP
    if (netstate->state > 3)
    {
        if (checkAddr(netOffset(myIp))) return;
    }
    else
    {
        readBytes(4); // ToDo: check Destination IP ???
    }

    receiveUdpPacket(ipLen - IP_HEADER_LEN, netstate);
}

static void receiveArpPacket(netstate_t* netstate)
{
    if (enc28j60ReadByte() != 0) return; // Hardware type H
    if (enc28j60ReadByte() != 1) return; // L
    if (enc28j60ReadByte() != 8) return; // Protocol type H
    if (enc28j60ReadByte() != 0) return; // L
    if (enc28j60ReadByte() != 6) return; // Hardware address length
    if (enc28j60ReadByte() != 4) return; // Protocol address length

    // Operation
    if (enc28j60ReadByte() != 0) return;
    uint8_t operL = enc28j60ReadByte();
    if (operL == 1)
    {
        readAddr(netOffset(arpReplyMac)); // Sender hardware address
        readAddr(netOffset(arpReplyIp)); // Sender protocol address

        readBytes(6); // Target hardware address
        if (checkAddr(netOffset(myIp))) return; // Target protocol address

        flag |= (1<<ARP_REPLY);
    }
    else if (operL == 2 && netstate->state == 4 && netstate->retryCount) // ToDo: consider checking target IP, MAC
    {
        readAddr(netOffset(dstMac)); // Sender hardware address
        if (checkAddr(netOffset(arpIp))) return; // Sender protocol address
        
        netstate->state = 5;
        retryTimeH = 0;
        retryTimeL = 0;
        netstate->retryCount = RETRY_COUNT;
    }
}

static void receiveEthPacket(uint16_t len, netstate_t* netstate)
{
    if (len < ARP_LEN + ETH_HEADER_LEN + CRC_LEN) return4;

    // Destination MAC (checked by ENC28J60), Source MAC (checked by ENC28J60), EtherType
    if (readBytes(13) != ETHTYPE_H_V) return4;
    uint8_t typeL = enc28j60ReadByte();
    if (netstate->retryCount && typeL == ETHTYPE_IP_L_V)
        receiveIpPacket(len - ETH_HEADER_LEN - CRC_LEN, netstate);
    else if (netstate->state > 2 && typeL == ETHTYPE_ARP_L_V)
        receiveArpPacket(netstate);
}

static void receivePacket(netstate_t* netstate)
{
    uint16_t len = enc28j60ReadPacket(&netstate->nextPacketPtr);
    receiveEthPacket(len, netstate);
    enc28j60EndRead(&netstate->nextPacketPtr);
#ifndef __AVR_ATtiny4313__
    tx('\r');
    tx('\n');
#endif
}

uint8_t netTick(uint32_t* time)
{
    net_t *ptr = &net;
    E_REG(ptr);
    
    if (ptr->leaseTime)
        ptr->leaseTime--;

    if (ptr->syncTime)
        ptr->syncTime--;
    
    uint16_t temp = retryTime;
    if (temp)
        retryTime = temp - 1;
    
    if (flag & (1<<TIME_OK))
    {
        *time = ptr->time + 1;
        ptr->time = *time;
        R_REG(*time);
        return 1;
    }
    return 0;
}

void netLoop(netstate_t* netstate)
{
    if (!enc28j60LinkUp())
    {
        netstate->state = 0;
        return;
    }

    if (netstate->state == 0)
    {
        *((uint8_t*)&net.leaseTime + 1) = 0;
        *((uint8_t*)&net.leaseTime) = 0;
        netstate->state = 1;
        R_REG(netstate->state);
        if (flag & (1<<CUSTOM_IP))
            netstate->state = 4;
        retryTimeH = 0;
        retryTimeL = 0;
        netstate->retryCount = RETRY_COUNT;
    }
    else if (enc28j60PacketReceived()) // ToDo: deal with buffer overflow !!!
        receivePacket(netstate);
    
    net_t* netPtr = &net;
    E_REG(netPtr);
    uint16_t sync;
    uint16_t lease;
    asm volatile (
        "ldd %B0, %a2+%3+1 \n"
        "ldd %A0, %a2+%3 \n"
        "ldd %B1, %a2+%4+1 \n"
        "ldd %A1, %a2+%4 \n"
        : "=r"(sync), "=r"(lease)
        : "b"(netPtr), "I"(offsetof(net_t, syncTime)), "I"(offsetof(net_t, leaseTime))
    );
    if (lease == 0 && netstate->state > 3 && !(flag & (1<<CUSTOM_IP)))
    {
#ifndef __AVR_ATtiny4313__
        tx('L');
        tx('\r');
        tx('\n');
#endif
        netstate->state = 3;
        retryTimeH = 0;
        retryTimeL = 0;
        netstate->retryCount = RETRY_COUNT;
    }
    else if (sync == 0 && netstate->state > 5)
    {
#ifndef __AVR_ATtiny4313__
        tx('S');
        tx('\r');
        tx('\n');
#endif
        netstate->state = 4;
        retryTimeH = 0;
        retryTimeL = 0;
        netstate->retryCount = RETRY_COUNT;
    }
    
    if (sync == 0 && netstate->retryCount == 0)
    {
#ifndef __AVR_ATtiny4313__
        if (flag & (1<<SYNC_OK))
        {
            tx('.');
            tx('\r');
            tx('\n');
        }
#endif
        flag &= ~(1<<SYNC_OK);
    }
    
    if (!(flag & (1<<ARP_REPLY)))
    {
        uint8_t retryH = retryTimeH;
        uint8_t retryL = retryTimeL;
        if ((retryL | retryH) || netstate->state > 5)
            return;
        if (netstate->retryCount)
        {
            (netstate->retryCount)--;
            if (netstate->retryCount == 0)
            {
                uint16_t temp = RETRY_TIMEOUT_LONG;
                R_REG(temp);
                cli();
                retryTime = temp;
                sei();
#ifndef __AVR_ATtiny4313__
                tx('W');
                tx('\r');
                tx('\n');
#endif
                return;
            }
#ifndef __AVR_ATtiny4313__
            tx('R');
#endif
        }
        else
        {
            netstate->state &= 4;
            if (netstate->state == 0)
                netstate->state = 1;
            netstate->retryCount = RETRY_COUNT - 1;
#ifndef __AVR_ATtiny4313__
            tx('F');
#endif
        }
        retryTimeL = RETRY_TIMEOUT_SHORT;
#ifndef __AVR_ATtiny4313__
        tx(' ');
        txHex(netstate->state);
        tx(' ');
        txHex(netstate->retryCount);
        tx('\r');
        tx('\n');
#endif
    }

    sendPacket(netstate->state); // ToDo: deal with buffer overflow !!!
    flag &= ~(1<<ARP_REPLY);
}

void netInit(netstate_t* netstate)
{
    flag = 0;
    
    uint8_t arpPtr = netOffset(config.dstIp);
    uint8_t* pt = (uint8_t*)&net.leaseTime + 4;
    uint8_t* pd = net.config.dstIp + 4;
    uint8_t* pm = net.config.myIp + 4;
    uint8_t* pn = net.config.netmask + 4;
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
            flag |= (1<<CUSTOM_IP);
        }
        if ((d & n) != (m & n))
        {
            arpPtr = netOffset(config.gwIp);
        }
    } while (--c);
    
    copyAddr(netOffset(arpIp), arpPtr);
    copyAddr(netOffset(myIp), netOffset(config.myIp));
    copyAddr(netOffset(xid), netOffset(config.myMac));
    
    enc28j60Init(&net.config.myMac[6]);
    netstate->nextPacketPtr = RXSTART_INIT;
    netstate->state = 0;
}