/*
 * net.c
 *
 * Created: 01.07.2018 14:32:04
 *  Author: Mikolaj
 */ 

#include "net.h"

//#define SEC_OFFSET 3155673600


// ToDo: writeByte from PROGMEM

// ToDo: merge into one array and pass uint8_t instead of uint8_t*

// ToDo: try using local array instead of global

uint8_t net[64]/* = {
0,0,0,0,0,0,
0,0,0,0,0,0,
0,0,0,0,0,0,
MAC5,MAC4,MAC3,MAC2,MAC1,MAC0,
104,5,168,192
}*/;

#define xstr(s) str(s)
#define str(s) #s

#define LEN(ptr, len, p) uint8_t (len); uint8_t* (ptr); asm ("mov %A0, %1 \n ldi %B0, 0 \n ldi %2, 4 \n cpi %1, lo8(net+18+6+1) \n brcc end_%= \n ldi %2, 6 \n end_%=: \n" : "=e" ((ptr)), "+d" ((p)), "=d" ((len)));

//#define netPtr		0x60

#define timestamp	net+0
#define arpReplyMac	net+6
#define dstMac		net+12
#define myMac		net+18
#define dstIp		net+24
#define myIpInit	net+28
#define gwIpInit	net+32
#define myIp		net+36
#define gwIp		net+40
#define netmask		net+44
#define serverId	net+48
#define arpIp		net+52
#define arpReplyIp	net+56
#define xid			net+60

//uint8_t myMac[6] = {MAC5,MAC4,MAC3,MAC2,MAC1,MAC0};
//static uint8_t dstIp[4] = {104,5,168,192};//{17,4,130,134}
//static uint8_t myIp[4];// = {192,168,5,105};
//static uint8_t gwIp[4];// = {192,168,5,1};
//static uint8_t netmask[4];// = {255,255,255,255};
//static volatile uint8_t serverId[4];// = {192,168,5,1};
//static uint8_t arpIp[4];
//static volatile uint8_t dstMac[6];// = {0,0,0,0,0,0};
//static uint8_t timestamp[5];
//static volatile uint8_t arpReplyAddr[10];
/*static uint8_t srcMac[6];
static uint8_t srcIp[4];
static uint8_t icmpData[133];
static uint8_t icmpLen;*/

//static volatile uint8_t xid[4] = { 0x12, 0x34, 0x56, 0x78 };// ToDo
uint32_t time;
uint16_t leaseTime;
uint16_t syncTime;

// ToDo: flag USE_GW instead of arpIp
#define flag TWAR
#define ARP_REPLY (1<<0)
#define TIME_OK (1<<1)

//uint8_t retryTime;

#define getLen(len) ({ uint8_t hi = enc28j60ReadByte(); uint8_t lo = enc28j60ReadByte(); WORD(lo, hi); })

//#define length(p) ({ uint8_t len = 4; R_VAR(len); if (p < 24) len = 6; len; })

// ToDo: writeFFs function ???

static void writeZeros(uint8_t len)
{
	do
	{
		enc28j60WriteByte(0);
	} while (--len);
}

#define writeFFs(len) for (uint8_t i = 0; i < (len); i++) enc28j60WriteByte(0xFF);

static void writeAddrNew(uint8_t p)
{
	LEN(ptr, len, p);
	Y_REG(ptr);
	do
	{
		enc28j60WriteByte(*--ptr);
	} while (--len);
}
//#define writeAddr(p, len) writeAddrNew(p)
#define writeAddr(p, len) ({ uint8_t x; asm volatile ("ldi %0, lo8("xstr(p)")" : "=d" (x)); writeAddrNew(x); })

static uint16_t addrChecksumNew(uint16_t sum, uint8_t p)
{
	uint8_t c;
	uint8_t* ptr;

	asm (
		"mov %A1, %2 \n"
		"ldi %B1, 0 \n"

		"ldi %3, 2 \n"
		"cpi %2, lo8(net+18+6+1) \n"
		"brcc loop \n"
		"ldi %3, 3 \n"
		"clc \n"

		"loop:"
		"ld %2, -%a1 \n"
		"adc %B0, %2 \n"
		"ld %2, -%a1 \n"
		"adc %A0, %2 \n"
		"dec %3 \n"
		"brne loop \n"

		"adc %B0, __zero_reg__ \n"
		"adc %A0, __zero_reg__ \n"
		: "+r" (sum), "=e" (ptr), "+d" (p), "=d" (c)
	);

	/*asm (
		"mov %A1, %3 \n"
		"ldi %B1, 0 \n"

		"cpi %3, lo8(myMac+6) \n"
		"brcc short \n"

		"ld %2, -%a1 \n"
		"add %B0, %2 \n"
		"ld %2, -%a1 \n"
		"adc %A0, %2 \n"

		"short: \n"
		"ld %2, -%a1 \n"
		"adc %B0, %2 \n"
		"ld %2, -%a1 \n"
		"adc %A0, %2 \n"
		"ld %2, -%a1 \n"
		"adc %B0, %2 \n"
		"ld %2, -%a1 \n"
		"adc %A0, %2 \n"
		"adc %B0, __zero_reg__ \n"
		"adc %A0, __zero_reg__ \n"
		//"adc %B0, __zero_reg__ \n"
		: "+r" (sum), "=e" (ptr), "=r" (x)
		: "r"(p)
	);*/
	return sum;
}
//#define addrChecksum(sum, p) addrChecksumNew(sum, p)
#define addrChecksum(sum, p) ({ uint8_t x; asm volatile ("ldi %0, lo8("xstr(p)")" : "=d" (x)); addrChecksumNew(sum, x); })

/*static void writeConst(const uint8_t* ptr, uint8_t len)
{
	do
	{
		enc28j60WriteByte(pgm_read_byte(ptr++));
	} while (--len);
}*/

//#define skipBytes(len) writeZeros(len) // ToDo: use this
static void skipBytes(uint8_t len)
{
	do
	{
		enc28j60ReadByte();
	} while (--len);
}

static void readAddrNew(uint8_t p)
{
	LEN(ptr, len, p);
	Y_REG(ptr);
	do
	{
		*--ptr = enc28j60ReadByte();
	} while (--len);
}
//#define readAddr(p, len) readAddrNew(p)
#define readAddr(p, len) ({ uint8_t x; asm volatile ("ldi %0, lo8("xstr(p)")" : "=d" (x)); readAddrNew(x); })

static uint8_t checkAddrNew(uint8_t p)
{
	LEN(ptr, len, p);
	Y_REG(ptr);
	do
	{
		if (*--ptr != enc28j60ReadByte()) return len;
	} while (--len);
	return len;
}
//#define checkAddr(p, len) checkAddrNew(p)
#define checkAddr(p, len) ({ uint8_t x; asm volatile ("ldi %0, lo8("xstr(p)")" : "=d" (x)); checkAddrNew(x); })

static void copyAddr(uint8_t* dst, uint8_t* src)
{
	E_REG(dst);
	E_REG(src);
	uint8_t c = 4;
	R_REG(c);
	do
	{
		*--dst = *--src;
	} while (--c);
}

static void sendDhcpPacket(uint8_t state)
{
	// UDP checksum
	uint16_t sum;
	if (state == 1)
	{
		uint32_t* ptr = (void*)xid;
		E_REG(ptr);
		*ptr = *ptr + 1;

		sum = (uint24_t)IP_PROTO_UDP_V + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0137 + 0x0401 + 0x032A + 0x64FF - 0xFFFF;
	}
	else if (state == 2)
	{
		sum = (uint24_t)IP_PROTO_UDP_V + DHCP_REQUEST_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_REQUEST_LEN + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0332 + 0x0400 + 0x0036 + 0x0400 + 0x0037 + 0x0401 + 0x032A + 0x64FF - 0xFFFF;
		sum = sum << 8 | sum >> 8;
		
		sum = addrChecksum(sum, myIp+4);
		sum = addrChecksum(sum, serverId+4);

		sum = sum << 8 | sum >> 8;
	}
	else // state == 3
	{
		sum = (uint24_t)IP_PROTO_UDP_V + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 67 + 68 + DHCP_DISCOVER_RENEW_LEN + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0337 + 0x0401 + 0x032A + 0x64FF - 0xFFFF;

		sum = addrChecksum(sum, myIp+4);
		sum = addrChecksum(sum, myIp+4);
	}
	sum = addrChecksum(sum, myMac+6);
	sum = addrChecksum(sum, xid+4);
	enc28j60WriteByte(~H(sum));
	enc28j60WriteByte(~L(sum));

	enc28j60WriteByte(1); // OP
	enc28j60WriteByte(1); // HTYPE
	enc28j60WriteByte(6); // HLEN
	enc28j60WriteByte(0); // HOPS
	writeAddr(xid+4, 4); // Transaction ID

	if (state == 3)
	{
		writeZeros(4); // Secs, Flags
		writeAddr(myIp+4, 4); // Client IP
		writeZeros(12); // Your IP, Server IP, Gateway IP
	}
	else
	{
		writeZeros(20); // Secs, Flags, Client IP, Your IP, Server IP, Gateway IP
	}

	// Client MAC, Server name, Boot file
	writeAddr(myMac+6, 6);
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
		writeAddr(myIp+4, 4);

		// Server ID
		enc28j60WriteByte(54);
		enc28j60WriteByte(4);
		writeAddr(serverId+4, 4);
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
	if (TIFR & (1<<ICF1))
		tcnt = 62499;
	copyAddr(timestamp+6, (void*)&time+4);
	sei();
	retryTime = 2;
	
	uint16_t f = DWORD(0, tcnt) / 62500;
	net[0] = L(f);
	net[1] = H(f);

	// UDP checksum
	uint16_t sum = IP_PROTO_UDP_V + 48 + UDP_HEADER_LEN + 123 + 123 + 48 + UDP_HEADER_LEN + 0x2300;
	sum = addrChecksum(sum, myIp+4);
	sum = addrChecksum(sum, dstIp+4);
	sum = addrChecksum(sum, timestamp+6);
	enc28j60WriteByte(~H(sum));
	enc28j60WriteByte(~L(sum));

	enc28j60WriteByte(0x23); // LI, VN, Mode - client
	writeZeros(39);

	// Transmit timestamp
	writeAddr(timestamp+6, 6);
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
	static uint8_t id; // ToDo: 16bit Id

	enc28j60WriteByte(0x45); // Version, IHL
	enc28j60WriteByte(0x00); // DSCP, ECN

	len -= TXSTART_INIT + ETH_HEADER_LEN;
	R_REG(len);
	enc28j60WriteByte(H(len)); // Length H
	enc28j60WriteByte(L(len)); // L
	//uint16_t* ptr = &idx;
	//E_REG(ptr);
	//uint16_t x = *ptr + 1;
	//*ptr = x;
	//enc28j60WriteByte(H(x));
	//enc28j60WriteByte(L(x));

	enc28j60WriteByte(0x12); // Id H
	enc28j60WriteByte(++id); // L
	enc28j60WriteByte(0x40); // Flags - DF, Fragment offset H
	enc28j60WriteByte(0x00); // L
	enc28j60WriteByte(0x20); // TTL // ToDo: TTL 0x40 ???
	enc28j60WriteByte(IP_PROTO_UDP_V); // Protocol

	// Checksum
	uint16_t sum = len + id;//(uint24_t)0x4500 + len + 0x1200 + id + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
	sum += (uint16_t)0x4500 + 0x1200 + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
	if (state > 2)
		sum = addrChecksum(sum, myIp+4);
	if (state == 5)
		sum = addrChecksum(sum, dstIp+4);
	enc28j60WriteByte(~H(sum));
	enc28j60WriteByte(~L(sum));

	// Source IP
	if (state > 2)
		writeAddr(myIp+4, 4);
	else
		writeZeros(4);

	// Destination IP
	if (state == 5)
		writeAddr(dstIp+4, 4);
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

	writeAddr(myMac+6, 6); // Sender hardware address
	writeAddr(myIp+4, 4); // Sender protocol address

	if (flag & ARP_REPLY)
	{
		writeAddr(arpReplyMac+6, 6); // Target hardware address
		writeAddr(arpReplyIp+4, 4); // Target protocol address
	}
	else
	{
		writeZeros(6); // Target hardware address
		writeAddr(arpIp+4, 4); // Target protocol address
	}
}

static void sendEthPacket(uint16_t len, uint8_t state)
{
	// Destination MAC
	if (flag & ARP_REPLY)
		writeAddr(arpReplyMac+6, 6);
	else if (state == 5)
		writeAddr(dstMac+6, 6);
	else
		writeFFs(6);

	// Source MAC
	writeAddr(myMac+6, 6);

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

//static uint8_t parseTimezone(uint8_t l)
//{
//	uint8_t x;
//	do
//	{
//		x = enc28j60ReadByte();
//	} while (x < '+' || x > ':' || x == '/');
//}

static void receiveDhcpPacket(uint16_t len, uint8_t* state)
{
	if (len < DHCP_OFFER_ACK_LEN + UDP_HEADER_LEN) return;

	if (enc28j60ReadByte() != 2) return; // OP
	if (enc28j60ReadByte() != 1) return; // HTYPE
	if (enc28j60ReadByte() != 6) return; // HLEN
	enc28j60ReadByte(); // HOPS // ToDo: check HOPS
	if (checkAddr(xid+4, 4)) return; // Transaction ID

	skipBytes(8); // Secs, Flags, Client IP // ToDo: check flags

	// ToDo
	uint8_t invalidAddr = 0;
	if (*state == 1)
	{
		readAddr(myIp+4, 4); // Your IP
	} 
	else
	{
		invalidAddr = checkAddr(myIp+4, 4); // Your IP
	}
	
	skipBytes(8); // Server IP, Gateway IP

	 // Client MAC
	if (checkAddr(myMac+6, 6)) return;
	for (uint8_t i = 0; i < 10; i++)
	{
		if (enc28j60ReadByte() != 0) return;
	}
	
	// Server name, Boot file
	skipBytes(192);

	// Magic cookie
	if (enc28j60ReadByte() != 0x63) return;
	if (enc28j60ReadByte() != 0x82) return;
	if (enc28j60ReadByte() != 0x53) return;
	if (enc28j60ReadByte() != 0x63) return;

	uint8_t found = 0;
	uint8_t type = 0;
	uint16_t readLen = 240 + UDP_HEADER_LEN;// + IP_HEADER_LEN + ETH_HEADER_LEN + CRC_LEN;
	while (1) // ToDo: get NTP server addr and timezone from DHCP
	{
		readLen++; // ToDo: readLen += 1 when code == 0
		if (readLen > len) return; // ToDo: break ???
		uint8_t code = enc28j60ReadByte();
		if (code == 0xFF)
		{
			break;
		}
		if (code)
		{
			readLen++;
			if (readLen > len) return; // ToDo: break ???
			uint8_t l = enc28j60ReadByte();
			readLen += l;
			if (readLen > len) return; // ToDo: break ???
			if (code == 53 && l == 1) // ToDo: check duplicate options
			{
				type = enc28j60ReadByte();
				found |= 0x04;
			}
			else if (code == 54 && l == 4)
			{
				if (*state == 1)
					readAddr(serverId+4, 4);
				else
					invalidAddr |= checkAddr(serverId+4, 4);
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
				leaseTime = lease >> 1;
				found |= 0x08;
			}
			else if (code == 1 && l == 4)
			{
				readAddr(netmask+4, 4);
				found |= 0x01;
			}
			else
			{
				if (code == 3 && l >= 4)
				{
					readAddr(gwIp+4, 4);
					found |= 0x02;
					l -= 4;
				}
				//else if (code == 42 && l >= 4)
				//{
				//	readAddr(dstIp+4, 4);
				//	found |= 0x20;
				//	l -= 4;
				//}
				//else if (code == 100)
				//{
				//	found |= 0x40;
				//	l -= parseTimezone(l);
				//}
				while (l) // ToDo: skipBytes
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

	uint8_t* ptr = dstIp+4;

	if (found & 0x01) // found netmask
	{
		uint8_t c = 4;
		R_REG(c);
		uint8_t* pd = dstIp+4;
		uint8_t* pm = myIp+4;
		uint8_t* pn = netmask+4;
		do
		{
			uint8_t d = *--pd;
			uint8_t m = *--pm;
			uint8_t n = *--pn;
			if ((d & n) != (m & n)) // should route via gw
			{
				if (!(found & 0x02)) return; // gw not found
				ptr = gwIp+4;
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

		if (syncTime == 0)
			*state = 4;
	}
	retryTime = 0;
	copyAddr(arpIp+4, ptr);
}

static void receiveNtpPacket(uint8_t* state)
{
	// LI, VN, Mode
	uint8_t x = enc28j60ReadByte();
	if ((x & 0x07) != 0x04) return;
	if ((x & 0x38) == 0x00) return;
	if ((x & 0xC0) == 0xC0) return; // ToDo: handle leap seconds

	// Stratum
	x = enc28j60ReadByte();
	if (x == 0) return;
	if (x > 15) return;

	skipBytes(22); // ToDo: check if reference clock is synchronized
	
	// Originate timestamp
	if (checkAddr(timestamp+6, 6)) return;
	if (enc28j60ReadByte() != 0) return;
	if (enc28j60ReadByte() != 0) return;

	uint32_t* timPtr = &time;
	E_REG(timPtr);
	
	cli(); // ToDo: read timestamp immediately after receiving packet
	uint16_t tcnt = TCNT1;
	if (TIFR & (1<<ICF1))
		tcnt = 62499;
	uint32_t timInt = *timPtr;
	sei();

	uint16_t timFrac = DWORD(0, tcnt) / 62500;

	uint8_t* ptr = timestamp;
	E_REG(ptr);
	uint8_t temp[6] = { ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5] };

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
	
	enc28j60ReadByte();
	enc28j60ReadByte();
	
	// Transmit timestamp
	temp[5] = enc28j60ReadByte();
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

	asm volatile ( // ToDo
		"ldi %A0, lo8(time) \n"
		"ldi %B0, hi8(time) \n"
		: "=d"(timPtr)
	);
	*timPtr = timInt;

	flag |= TIME_OK;

	TIFR  = (1<<OCF1A) | (1<<OCF1B) | (1<<ICF1); // ToDo
	sei();

	retryTime = 0;
	syncTime = 15;//ToDo: should be 900
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
	uint16_t udpLen = getLen();
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
	uint16_t ipLen = getLen();
	if (ipLen > len || ipLen < NTP_LEN + UDP_HEADER_LEN + IP_HEADER_LEN) return;

	skipBytes(5); // Id, Flags, Fragment offset, TTL // ToDo: check Flags, Fragment offset ???
	if (enc28j60ReadByte() != IP_PROTO_UDP_V) return; // Protocol

	if (*state == 5)
	{
		skipBytes(2); // Checksum
		if (checkAddr(dstIp+4, 4)) return; // Source IP
	}
	else
	{
		skipBytes(6); // Checksum, Source IP // ToDo: check Source IP ???
	}

	// Destination IP
	if (*state > 3)
	{
		if (checkAddr(myIp+4, 4)) return;
	}
	else
	{
		skipBytes(4); // ToDo: check Destination IP ???
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
		readAddr(arpReplyMac+6, 6); // Sender hardware address
		readAddr(arpReplyIp+4, 4); // Sender protocol address

		skipBytes(6); // Target hardware address
		if (checkAddr(myIp+4, 4)) return; // Target protocol address

		flag |= ARP_REPLY;
	}
	else if (operL == 2 && *state == 4) // ToDo: consider checking target IP, MAC
	{
		readAddr(dstMac+6, 6); // Sender hardware address
		if (checkAddr(arpIp+4, 4)) return; // Sender protocol address
		
		retryTime = 0;
		*state = 5;
	}
}

static void receiveEthPacket(uint16_t len, uint8_t* state)
{
	if (len < ARP_LEN + ETH_HEADER_LEN + CRC_LEN) return;

	// Destination MAC, Source MAC - checked by ENC28J60
	skipBytes(12);

	// EtherType
	if (enc28j60ReadByte() != ETHTYPE_H_V) return;
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
	copyAddr(myIp+4, myIpInit+4);
	copyAddr(gwIp+4, gwIpInit+4);
	copyAddr(xid+4, myMac+6);
}

/*void tick()
{
	if (tim)
	{
		tim--;
	}
	//if (leaseTime > 0 && leaseTime != 0x7FFFFFFF)
	//{
	//	leaseTime--;
	//}
}*/