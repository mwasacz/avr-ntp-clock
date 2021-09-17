/*
 * net.c
 *
 * Created: 01.07.2018 14:32:04
 *  Author: Mikolaj
 */ 

#include "net.h"

//#define SEC_OFFSET 3155673600

// ToDo: writeByte from PROGMEM

// ToDo: try using local array instead of global

// ToDo: unify enc28j60ReadByte, enc28j60WriteByte, readBytes, writeZeros

net_t net;

#define offsetof(type, member) __builtin_offsetof(type, member)
#define memberSize(type, member) sizeof(((type *)0)->member)
#define netOffset(member) (offsetof(net_t, member) + memberSize(net_t, member))

// ToDo: flag USE_GW instead of arpIp
#define flag TWAR
#define ARP_REPLY (1<<0)
#define TIME_OK (1<<1)

static void writeZeros(uint8_t len)
{
	do
	{
		enc28j60WriteByte(0);
	} while (--len);
}

static void writeFFs(uint8_t len)
{
	do
	{
		enc28j60WriteByte(0xFF);
	} while (--len);
}

static __attribute__((noinline)) uint8_t getAddrLength(uint8_t p)
{
	//if (p <= netOffset(syncTime))
	//	return 2;
	if (p <= netOffset(myMac))
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
		"brcc checksumLoop \n"
		"ldi %3, 3 \n"
		"clc \n"
		"checksumLoop:"
		"ld %2, -%a1 \n"
		"adc %B0, %2 \n"
		"ld %2, -%a1 \n"
		"adc %A0, %2 \n"
		"dec %3 \n"
		"brne checksumLoop \n"
		"adc %B0, __zero_reg__ \n"
		"adc %A0, __zero_reg__ \n"
		: "+r" (sum), "+e" (ptr), "+d" (p), "=d" (c) : "M" (netOffset(myMac) + 1)
	);

	return sum;
}

/*static void writeConst(const uint8_t* ptr, uint8_t len)
{
	do
	{
		enc28j60WriteByte(pgm_read_byte(ptr++));
	} while (--len);
}*/

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
	E_REG(dst);
	E_REG(src);
	uint8_t len = 4;
	do
	{
		*--dst = *--src;
	} while (--len);
}

static uint16_t timeFrac(uint16_t tcnt)
{
	return DWORD(0, 0, L(tcnt), H(tcnt)) / 62500;
}

static void sendDhcpPacket(uint8_t state)
{
	// UDP checksum
	uint16_t sum;
	if (state == 1)
	{
		uint32_t* ptr = (uint32_t *)&net.xid;
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
	sum = addrChecksum(sum, netOffset(myMac));
	sum = addrChecksum(sum, netOffset(xid));
	enc28j60WriteByte(~H(sum));
	enc28j60WriteByte(~L(sum));

	enc28j60WriteByte(1); // OP
	enc28j60WriteByte(1); // HTYPE
	enc28j60WriteByte(6); // HLEN
	enc28j60WriteByte(0); // HOPS
	writeAddr(netOffset(xid)); // Transaction ID // ToDo: 16 xid ?

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
	writeAddr(netOffset(myMac));
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
	cli();
	uint16_t tcnt = TCNT1;
	uint8_t tifr = TIFR;
	copyAddr(netOffset(timestamp), netOffset(time));
	sei();
	retryTime = 2;
	
	if (tifr & (1<<ICF1))
		tcnt = 62499;
	uint16_t f = timeFrac(tcnt);
	net.timestamp[0] = L(f);
	net.timestamp[1] = H(f);

	// UDP checksum
	uint16_t sum = IP_PROTO_UDP_V + 48 + UDP_HEADER_LEN + 123 + 123 + 48 + UDP_HEADER_LEN + 0x2300;
	sum = addrChecksum(sum, netOffset(myIp));
	sum = addrChecksum(sum, netOffset(dstIp));
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
	enc28j60WriteByte(0); // Source port H // ToDo: consider variable Source port higher than 1024
	if (state == 5)
	{
		enc28j60WriteByte(123); // Source port L
		enc28j60WriteByte(0); // Destination port H // ToDo: consider configurable Destination port
		enc28j60WriteByte(123); // L
	}
	else
	{
		enc28j60WriteByte(68); // Source port L
		enc28j60WriteByte(0); // Destination port H
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
	static uint16_t ipId;

	enc28j60WriteByte(0x45); // Version, IHL
	enc28j60WriteByte(0x00); // DSCP, ECN

	len -= TXSTART_INIT + ETH_HEADER_LEN;
	R_REG(len);
	enc28j60WriteByte(H(len)); // Length H
	enc28j60WriteByte(L(len)); // L
	
	uint16_t* ptr = &ipId;
	E_REG(ptr);
	uint16_t id = *ptr + 1;
	*ptr = id;
	//uint16_t id = ipId + 1;
	//ipId = id;
	enc28j60WriteByte(H(id)); // Id H
	enc28j60WriteByte(L(id)); // L
	
	enc28j60WriteByte(0x40); // Flags - DF, Fragment offset H
	enc28j60WriteByte(0x00); // L
	enc28j60WriteByte(0x20); // TTL // ToDo: TTL 0x40 ???
	enc28j60WriteByte(IP_PROTO_UDP_V); // Protocol

	// Checksum
	//uint24_t s = len + id;//(uint24_t)0x4500 + len + 0x1200 + id + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
	//s += (uint16_t)0x4500 + 0x1200 + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
	//uint16_t sum = s + (s >> 16); // ToDo: optimize
	uint24_t s = 0x4500 + len + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
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
		sum = addrChecksum(sum, netOffset(dstIp));
	enc28j60WriteByte(~H(sum));
	enc28j60WriteByte(~L(sum));

	// Source IP
	if (state > 2)
		writeAddr(netOffset(myIp));
	else
		writeZeros(4);

	// Destination IP
	if (state == 5)
		writeAddr(netOffset(dstIp));
	else
		writeFFs(4);

	sendUdpPacket(len, state);
}

static void sendArpPacket(uint8_t state)
{
	enc28j60WriteByte(0); // Hardware type H
	enc28j60WriteByte(1); // L
	enc28j60WriteByte(8); // Protocol type H
	enc28j60WriteByte(0); // L
	enc28j60WriteByte(6); // Hardware address length
	enc28j60WriteByte(4); // Protocol address length

	// Operation
	enc28j60WriteByte(0);
	uint8_t operL = 1;
	R_REG(operL);
	if (flag & ARP_REPLY)
		operL = 2;
	enc28j60WriteByte(operL);

	writeAddr(netOffset(myMac)); // Sender hardware address
	writeAddr(netOffset(myIp)); // Sender protocol address

	if (flag & ARP_REPLY)
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
	if (flag & ARP_REPLY)
		writeAddr(netOffset(arpReplyMac));
	else if (state == 5)
		writeAddr(netOffset(dstMac));
	else
		writeFFs(6);

	// Source MAC
	writeAddr(netOffset(myMac));

	// EtherType
	enc28j60WriteByte(ETHTYPE_H_V);
	if (state == 4 || flag & ARP_REPLY)
	{
		enc28j60WriteByte(ETHTYPE_ARP_L_V);
		sendArpPacket(state);
	}
	else
	{
		enc28j60WriteByte(ETHTYPE_IP_L_V);
		sendIpPacket(len, state);
	}
}

static void sendPacket(uint8_t state)
{
	uint16_t addr = TXSTART_INIT;
	
	if (state == 4 || flag & ARP_REPLY)
		addr += ARP_LEN + ETH_HEADER_LEN;
	else if (state == 2)
		addr += DHCP_REQUEST_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
	else if (state == 5)
		addr += NTP_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
	else // state == 1 || state == 3
		addr += DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
	
	enc28j60WritePacket(addr);
	sendEthPacket(addr, state);
	enc28j60EndWrite();
}

static void receiveDhcpPacket(uint16_t len, uint8_t* state)
{
	if (len < DHCP_OFFER_ACK_LEN + UDP_HEADER_LEN) return;

	if (enc28j60ReadByte() != 2) return; // OP
	if (enc28j60ReadByte() != 1) return; // HTYPE
	if (enc28j60ReadByte() != 6) return; // HLEN
	enc28j60ReadByte(); // HOPS // ToDo: check HOPS
	if (checkAddr(netOffset(xid))) return; // Transaction ID

	readBytes(8); // Secs, Flags, Client IP // ToDo: check Secs and Flags

	// ToDo
	uint8_t invalidAddr = 0;
	if (*state == 1)
		readAddr(netOffset(myIp)); // Your IP
	else
		invalidAddr = checkAddr(netOffset(myIp)); // Your IP
	
	readBytes(8); // Server IP, Gateway IP

	 // Client MAC
	if (checkAddr(netOffset(myMac))) return;
	for (uint8_t i = 0; i < 10; i++)
	{
		if (enc28j60ReadByte() != 0) return;
	}
	
	// Server name, Boot file, Magic cookie
	if (readBytes(193) != 0x63) return;
	if (enc28j60ReadByte() != 0x82) return;
	if (enc28j60ReadByte() != 0x53) return;
	if (enc28j60ReadByte() != 0x63) return;

	uint8_t found = 0;
	uint8_t type = 0;
	uint16_t readLen = 240 + UDP_HEADER_LEN;// + IP_HEADER_LEN + ETH_HEADER_LEN + CRC_LEN;
	while (1) // ToDo: get NTP server addr and timezone from DHCP
	{
		readLen++;
		if (readLen > len) return;
		uint8_t code = enc28j60ReadByte();
		if (code == 0xFF)
			break;
		if (code)
		{
			readLen++;
			if (readLen > len) return;
			uint8_t l = enc28j60ReadByte();
			readLen += l;
			if (readLen > len) return;
			if (code == 53 && l == 1) // ToDo: check duplicate options
			{
				type = enc28j60ReadByte();
				found |= 0x04;
			}
			else if (code == 54 && l == 4)
			{
				if (*state == 1)
					readAddr(netOffset(serverId));
				else
					invalidAddr |= checkAddr(netOffset(serverId));
				found |= 0x10;
			}
			else if (code == 51 && l == 4)
			{
				uint8_t max = enc28j60ReadByte() | enc28j60ReadByte();
				uint8_t hi = enc28j60ReadByte();
				uint8_t lo = enc28j60ReadByte();
				uint16_t lease = 0xFFFF;
				R_REG(lease);
				if (max == 0)
					lease = WORD(lo, hi);
				net.leaseTime = lease >> 1;
				found |= 0x08;
			}
			else if (code == 1 && l == 4)
			{
				readAddr(netOffset(netmask));
				found |= 0x01;
			}
			else
			{
				if (code == 3 && l >= 4)
				{
					readAddr(netOffset(gwIp));
					found |= 0x02;
					l -= 4;
				}
				while (l) // ToDo: readBytes
				{
					enc28j60ReadByte();
					l--;
				}
			}
		}
	}

	if (type == 6)
	{
		*state = 1;
		return;
	}

	if (invalidAddr) return;
	if (found < 0x1C) return;

	uint8_t ptr = netOffset(dstIp);

	if (found & 0x01) // found netmask
	{
		uint8_t c = 4;
		R_REG(c);
		uint8_t* pd = net.dstIp;
		uint8_t* pm = net.myIp;
		uint8_t* pn = net.netmask;
		do
		{
			uint8_t d = *--pd;
			uint8_t m = *--pm;
			uint8_t n = *--pn;
			if ((d & n) != (m & n)) // should route via gw
			{
				if (!(found & 0x02)) return; // gw not found
				ptr = netOffset(gwIp);
				break;
			}
		} while (--c);
	}

	*state <<= 1;
	if (*state == 2)
	{
		if (type != 2) return;
	}
	else
	{
		if (type != 5) return;

		if (net.syncTime == 0)
			*state = 4;
	}
	retryTime = 0;
	copyAddr(netOffset(arpIp), ptr);
}

static void receiveNtpPacket(uint8_t* state)
{
	// LI, VN, Mode
	uint8_t x = enc28j60ReadByte();
	if ((x & 0x07) != 0x04) return;
	if ((x & 0x38) == 0x00) return;
	if ((x & 0xC0) == 0xC0) return; // ToDo: handle leap seconds

	// Stratum
	x = enc28j60ReadByte() - 1;
	if (x > 15 - 1) return;

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

	if (tifr & (1<<ICF1))
		tcnt = 62499;
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
	resTcnt.d = (uint32_t)timFrac * 62500;

	cli();// ToDo
	//TIMSK |= (1<<OCIE1A) | (1<<OCIE1B);
	
	SFIOR = (1<<PSR10);
	TCNT1 = resTcnt.w[1];

	ptr = &net;
	E_REG(ptr);
	ptr->time = timInt;

	flag |= TIME_OK;

	TIFR  = (1<<OCF1A) | (1<<OCF1B) | (1<<ICF1); // ToDo
	sei();

	retryTime = 0;
	ptr->syncTime = 15;//ToDo: should be 900
	*state = 6;
}

static void receiveUdpPacket(uint16_t len, uint8_t* state)
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
	if (udpLen > len || udpLen < NTP_LEN + UDP_HEADER_LEN) return;

	// Checksum
	enc28j60ReadByte();
	enc28j60ReadByte();

	if (*state <= 3 && srcPort == 67 && dstPort == 68)
		receiveDhcpPacket(udpLen, state);
	else if (*state == 5 && srcPort == 123 && dstPort == 123)
		receiveNtpPacket(state);
}

static void receiveIpPacket(uint16_t len, uint8_t* state)
{
	if (enc28j60ReadByte() != 0x45) return; // Version, IHL
	enc28j60ReadByte(); // DSCP, ECN // ToDo: check DSCP, ECN ???

	// Length
	uint8_t hi = enc28j60ReadByte();
	uint8_t lo = enc28j60ReadByte();
	uint16_t ipLen = WORD(lo, hi);
	if (ipLen > len || ipLen < NTP_LEN + UDP_HEADER_LEN + IP_HEADER_LEN) return;

	// Id, Flags, Fragment offset, TTL, Protocol // ToDo: check Flags, Fragment offset ???
	if (readBytes(6) != IP_PROTO_UDP_V) return;

	if (*state == 5)
	{
		enc28j60ReadByte(); // Checksum H
		enc28j60ReadByte(); // L
		if (checkAddr(netOffset(dstIp))) return; // Source IP
	}
	else
	{
		readBytes(6); // Checksum, Source IP // ToDo: check Source IP ???
	}

	// Destination IP
	if (*state > 3)
	{
		if (checkAddr(netOffset(myIp))) return;
	}
	else
	{
		readBytes(4); // ToDo: check Destination IP ???
	}

	receiveUdpPacket(ipLen - IP_HEADER_LEN, state);
}

static void receiveArpPacket(uint8_t* state)
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

		flag |= ARP_REPLY;
	}
	else if (operL == 2 && *state == 4) // ToDo: consider checking target IP, MAC
	{
		readAddr(netOffset(dstMac)); // Sender hardware address
		if (checkAddr(netOffset(arpIp))) return; // Sender protocol address
		
		retryTime = 0;
		*state = 5;
	}
}

static void receiveEthPacket(uint16_t len, uint8_t* state)
{
	if (len < ARP_LEN + ETH_HEADER_LEN + CRC_LEN) return;

	// Destination MAC (checked by ENC28J60), Source MAC (checked by ENC28J60), EtherType
	if (readBytes(13) != ETHTYPE_H_V) return;
	uint8_t typeL = enc28j60ReadByte();
	if (typeL == ETHTYPE_IP_L_V)
		receiveIpPacket(len - ETH_HEADER_LEN - CRC_LEN, state);
	else if (*state > 2 && typeL == ETHTYPE_ARP_L_V)
		receiveArpPacket(state);
}

static void receivePacket(uint8_t* state, uint16_t* NextPacketPtr)
{
	uint16_t len = enc28j60ReadPacket(NextPacketPtr);
	receiveEthPacket(len, state);
	enc28j60EndRead(NextPacketPtr);
}

void loop(uint8_t* state, uint16_t* NextPacketPtr)
{
	if (enc28j60PacketReceived()) // ToDo: deal with buffer overflow !!!
		receivePacket(state, NextPacketPtr);
	if (retryTime == 0 && *state < 6)
		retryTime = 5;
	else if (!(flag & ARP_REPLY))
		return;

	sendPacket(*state); // ToDo: deal with buffer overflow !!!
	flag &= ~ARP_REPLY;
}

void init()
{
	copyAddr(netOffset(myIp), netOffset(myIpInit));
	copyAddr(netOffset(gwIp), netOffset(gwIpInit));
	copyAddr(netOffset(xid), netOffset(myMac));
}

/*void tick()
{
	if (tim)
	{
		tim--;
	}
	//if (net.leaseTime > 0 && net.leaseTime != 0x7FFFFFFF)
	//{
	//	net.leaseTime--;
	//}
}*/