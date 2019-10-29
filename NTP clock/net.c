/*
 * net.c
 *
 * Created: 01.07.2018 14:32:04
 *  Author: Mikolaj
 */ 

#include "net.h"

//#define SEC_OFFSET 3155673600

//#define WORD(x,y) (((uint16_t)(x)<<8)|(y))
#define REPEAT(x,y) for (uint8_t i = 0; i < (x); i++) y
/*typedef union
{
	uint32_t d;
	uint16_t w[2];
	uint8_t b[4];
} u32;*/

#define checkLen(len) { u16 l = {0}; l.b[1] = enc28j60ReadByte(); l.b[0] = enc28j60ReadByte(); if ((len) < l.w) return; }

// ToDo: writeByte from PROGMEM

// ToDo: merge into one array and pass uint8_t instead of uint8_t*

uint8_t net[56]/* = {
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
#define myIp		net+28
#define gwIp		net+32
#define netmask		net+36
#define serverId	net+40
#define arpIp		net+44
#define arpReplyIp	net+48
#define xid			net+52

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
uint16_t cnt;

// ToDo: flag USE_GW instead of arpIp
#define flag TWAR
#define ARP_REPLY (1<<0)
#define TIME_OK (1<<1)

uint8_t tim;

// ToDo: writeFFs function ???

static void writeZeros(uint8_t len)
{
	do
	{
		enc28j60WriteByte(0);
	} while (--len);
}

static void writeAddrNew(uint8_t p)
{
	LEN(ptr, len, p);
	do
	{
		enc28j60WriteByte(LD_Y(ptr));
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
	do
	{
		uint8_t x = enc28j60ReadByte();
		ST_Y(ptr, x);
	} while (--len);
}
//#define readAddr(p, len) readAddrNew(p)
#define readAddr(p, len) ({ uint8_t x; asm volatile ("ldi %0, lo8("xstr(p)")" : "=d" (x)); readAddrNew(x); })

static uint8_t checkAddrNew(uint8_t p)
{
	LEN(ptr, len, p);
	do
	{
		if (LD_Y(ptr) != enc28j60ReadByte()) return len;
	} while (--len);
	return len;
}
//#define checkAddr(p, len) checkAddrNew(p)
#define checkAddr(p, len) ({ uint8_t x; asm volatile ("ldi %0, lo8("xstr(p)")" : "=d" (x)); checkAddrNew(x); })

// ToDo: __attribute__((noinline)) ???
static void copyAddr(uint8_t* dst, uint8_t* src)
{
	uint8_t c = 4;
	do
	{
		uint8_t x;
		asm volatile (
			"ld %2, -%a1 \n"
			"st -%a0, %2 \n"
			: "+e" (dst), "+e" (src), "=r" (x)
		);
	} while (--c);
}

/*static uint8_t routeViaGw()
{
	for (uint8_t i = 0; i < 4; i++)
	{
		if ((dstIp[i] & netmask[i]) != (myIp[i] & netmask[i]))
		{
			return 1;
		}
	}
	return 0;
}*/

static void sendDhcpPacket(uint8_t state)
{
	uint16_t sum;
	if (state == 1)
	{
		/*uint8_t x;
		uint8_t* ptr = xid;
		asm volatile (
			"ld %1, %a0 \n"
			"subi %1, 0xFF \n"
			"st %a0+, %1 \n"
			"ld %1, %a0 \n"
			"sbci %1, 0xFF \n"
			"st %a0+, %1 \n"
			"ld %1, %a0 \n"
			"sbci %1, 0xFF \n"
			"st %a0+, %1 \n"
			"ld %1, %a0 \n"
			"sbci %1, 0xFF \n"
			"st %a0+, %1 \n"
			: "+e" (ptr), "=d" (x)
		);*/

// 		uint32_t x;
// 		uint8_t* ptr = xid;
// 		asm volatile (
// 			"ld %A1, %a0+ \n"
// 			"ld %B1, %a0+ \n"
// 			"ld %C1, %a0+ \n"
// 			"ld %D1, %a0+ \n"
// 			"adiw %A1, 1 \n"
// 			"adc %C1, __zero_reg__ \n"
// 			"adc %D1, __zero_reg__ \n"
// 			"st -%a0, %D1 \n"
// 			"st -%a0, %C1 \n"
// 			"st -%a0, %B1 \n"
// 			"st -%a0, %A1 \n"
// 			: "+e" (ptr), "=w" (x)
// 		);

		sum = (uint24_t)IP_PROTO_UDP_V + 248 + UDP_HEADER_LEN + 67 + 68 + 248 + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0137 + 0x0201 + 0x03FF - 0;
	}
	else
	{
		if (state == 2)
		{
			sum = (uint24_t)IP_PROTO_UDP_V + 260 + UDP_HEADER_LEN + 67 + 68 + 260 + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0332 + 0x0400 + 0x0036 + 0x0400 + 0x0037 + 0x0201 + 0x03FF - 0xFFFF;
			sum = sum << 8 | sum >> 8;
			
			sum = addrChecksum(sum, myIp+4);
			sum = addrChecksum(sum, serverId+4);

			sum = sum << 8 | sum >> 8;
		}
		else // state == 3
		{
			sum = (uint24_t)IP_PROTO_UDP_V + 248 + UDP_HEADER_LEN + 67 + 68 + 248 + UDP_HEADER_LEN + 0x0101 + 0x0600 + 0x6382 + 0x5363 + 0x3501 + 0x0337 + 0x0201 + 0x03FF - 0;

			sum = addrChecksum(sum, myIp+4);
			sum = addrChecksum(sum, myIp+4);
		}
	}
	sum = addrChecksum(sum, myMac+6);
	sum = addrChecksum(sum, xid+4);

	enc28j60WriteByte(~H(sum));
	enc28j60WriteByte(~L(sum));

	enc28j60WriteByte(0x01);
	enc28j60WriteByte(0x01);
	enc28j60WriteByte(0x06);
	enc28j60WriteByte(0x00);
	writeAddr(xid+4, 4);
	//enc28j60WriteByte(xid);
	//writeAddr(myMac+3, 3);
	if (state == 3)
	{
		writeZeros(4);
		writeAddr(myIp+4, 4);
		writeZeros(12);
	}
	else
	{
		writeZeros(20);
	}
	writeAddr(myMac+6, 6);
	writeZeros(202);
	enc28j60WriteByte(0x63);
	enc28j60WriteByte(0x82);
	enc28j60WriteByte(0x53);
	enc28j60WriteByte(0x63);
	enc28j60WriteByte(53);
	enc28j60WriteByte(1);
	enc28j60WriteByte(state | 1);
	//if (state == 1)
	//	enc28j60WriteByte(1);
	//else
	//	enc28j60WriteByte(3);

	if (state == 2)
	{
		enc28j60WriteByte(50);
		enc28j60WriteByte(4);
		writeAddr(myIp+4, 4);
		enc28j60WriteByte(54);
		enc28j60WriteByte(4);
		writeAddr(serverId+4, 4);
	}
	enc28j60WriteByte(55);
	enc28j60WriteByte(2);
	enc28j60WriteByte(1);
	enc28j60WriteByte(3);
	enc28j60WriteByte(0xFF);
}

static void sendNtpPacket(uint8_t state)
{
	//uint16_t sum = IP_PROTO_UDP_V + 48 + UDP_HEADER_LEN + 123 + 123 + 48 + UDP_HEADER_LEN + 0x2300;// + (leaseTime>>16) + (leaseTime&0xFFFF) + (leaseTime>>16) + (leaseTime&0xFFFF);
	
	cli();// ToDo
	copyAddr(timestamp+6, (uint8_t*)&time+4);
	uint16_t tcnt = TCNT1;
	sei();
	tim = 2;
	
	uint8_t f = tcnt / CONST16(245);
	net[1] = f;
	uint16_t sum = CONST16(IP_PROTO_UDP_V + 48 + UDP_HEADER_LEN + 123 + 123 + 48 + UDP_HEADER_LEN + 0x2300);

	//asm (
	//	"add %B0, %1 \n"
	//	"adc %A0, __zero_reg__ \n"
	//	: "+r" (sum)
	//	: "r" (f)
	//);

	sum = addrChecksum(sum, myIp+4);
	sum = addrChecksum(sum, dstIp+4);
	sum = addrChecksum(sum, timestamp+6);
	//sum = addrChecksum(sum, timestamp+5);

	enc28j60WriteByte(~H(sum));
	enc28j60WriteByte(~L(sum));

	enc28j60WriteByte(0x23);
	writeZeros(39);

	writeAddr(timestamp+6, 6);
	writeZeros(2);
	//writeAddr(timestamp+5, 5);
	//writeZeros(3);
}

static void sendUdpPacket(uint16_t len, uint8_t state)
{
	enc28j60WriteByte(0);
	if (state == 5)
	{
		// ToDo: consider variable srcPort
		enc28j60WriteByte(123);
		enc28j60WriteByte(0);
		enc28j60WriteByte(123);
	}
	else
	{
		enc28j60WriteByte(68);
		enc28j60WriteByte(0);
		enc28j60WriteByte(67);
	}
	len -= IP_HEADER_LEN;

	enc28j60WriteByte(H(len));
	enc28j60WriteByte(L(len));

	if (state == 5)
		sendNtpPacket(state);
	else
		sendDhcpPacket(state);
}

/*static void sendIcmpPacket(uint8_t state)
{
	enc28j60WriteByte(0);
	enc28j60WriteByte(0);
	uint24_t sum = 0;
	for (uint8_t i = 0; i < icmpLen; i += 2)
		sum += WORD(icmpData[i], icmpData[i+1]);
	uint16_t csum = checksum(sum);
	enc28j60WriteByte(H(csum));
	enc28j60WriteByte(L(csum));
	REPEAT(icmpLen, enc28j60WriteByte(icmpData[i]));
}*/

static void sendIpPacket(uint16_t len, uint8_t state)
{
	static uint8_t id;
	enc28j60WriteByte(0x45);
	enc28j60WriteByte(0x00);
	len -= ETH_HEADER_LEN;
	enc28j60WriteByte(H(len));
	enc28j60WriteByte(L(len));
	enc28j60WriteByte(0x12);// ToDo: consider 16bit id
	enc28j60WriteByte(++id);
	enc28j60WriteByte(0x40);// dont fragment ???
	enc28j60WriteByte(0x00);
	enc28j60WriteByte(0x20);
	/*uint24_t sum;
	if (state > 15)
	{
		enc28j60WriteByte(IP_PROTO_ICMP_V);
		sum = (uint24_t)0x4500 + len + 0x1200 + id + 0x4000 + 0x2000 + IP_PROTO_ICMP_V;
	}
	else
	{
		enc28j60WriteByte(IP_PROTO_UDP_V);
		sum = (uint24_t)0x4500 + len + 0x1200 + id + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
	}*/
	enc28j60WriteByte(IP_PROTO_UDP_V);
	uint16_t sum = len + id;//(uint24_t)0x4500 + len + 0x1200 + id + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
	sum += (uint16_t)0x4500 + 0x1200 + 0x4000 + 0x2000 + IP_PROTO_UDP_V;
	if (state > 2)
	{
		sum = addrChecksum(sum, myIp+4);
	}
	if (state == 5)
	{
		sum = addrChecksum(sum, dstIp+4);
	}
	enc28j60WriteByte(~H(sum));
	enc28j60WriteByte(~L(sum));

	if (state > 2)
		writeAddr(myIp+4, 4);
	else
		writeZeros(4);

	if (state == 5)
		writeAddr(dstIp+4, 4);
	else
		REPEAT(4, enc28j60WriteByte(0xFF));

	sendUdpPacket(len, state);
}

static void sendArpPacket(uint8_t state)
{
	enc28j60WriteByte(0);
	enc28j60WriteByte(1);
	enc28j60WriteByte(8);
	enc28j60WriteByte(0);
	enc28j60WriteByte(6);
	enc28j60WriteByte(4);
	enc28j60WriteByte(0);
	if (flag & ARP_REPLY)
		enc28j60WriteByte(2);
	else
		enc28j60WriteByte(1);
	writeAddr(myMac+6, 6);
	writeAddr(myIp+4, 4);
	if (flag & ARP_REPLY)
	{
		writeAddr(arpReplyMac+6, 6);
		writeAddr(arpReplyIp+4, 4);
		//writeAddr(arpReplyAddr+10, 10);
	}
	else
	{
		writeZeros(6);
		writeAddr(arpIp+4, 4);
	}
}

static void sendEthPacket(uint16_t len, uint8_t state)
{
	//enc28j60WritePacket(len);
	if (flag & ARP_REPLY)
		writeAddr(arpReplyMac+6, 6);
		//writeAddr(arpReplyAddr+10, 6);
	else if (state == 5)
		writeAddr(dstMac+6, 6);
	else
		REPEAT(6, enc28j60WriteByte(0xFF));
	writeAddr(myMac+6, 6);
	enc28j60WriteByte(ETHTYPE_IP_H_V);
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
	uint16_t len;
	
	if (state == 4 || flag & ARP_REPLY) len = 28 + ETH_HEADER_LEN;
	//if (state == 1 || state == 3) len = 248 + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
	else if (state == 2) len = 260 + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
	else if (state == 5) len = 48 + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
	else len = 248 + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN;
	//else len = 28 + ETH_HEADER_LEN;

	enc28j60WritePacket(len);
	sendEthPacket(len, state);
	//flag &= ~ARP_REPLY;
	enc28j60EndWrite();
}

static void receiveDhcpPacket(uint16_t len, uint8_t* state)
{
	if (len < 255 + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN + CRC_LEN) return;

	if (enc28j60ReadByte() != 2) return;// ToDo???
	if (enc28j60ReadByte() != 1) return;
	if (enc28j60ReadByte() != 6) return;
	enc28j60ReadByte();

	if (checkAddr(xid+4, 4)) return;
	//if (enc28j60ReadByte() != xid) return state;
	//if (checkAddr(myMac+3, 3)) return state;

	skipBytes(8);

	// ToDo
	uint8_t invalidAddr = 0;
	if (*state == 1)
	{
		//uint8_t z = 0xFF;
		//uint8_t* ptr = netmask+4;
		//asm volatile (
		//	"st -%a0, __zero_reg__ \n"
		//	"st -%a0, __zero_reg__ \n"
		//	"st -%a0, __zero_reg__ \n"
		//	"st -%a0, __zero_reg__ \n"
		//	: "+e" (ptr)//, "=r" (z)
		//);
		//REPEAT(4, netmask[i] = 0);
		//REPEAT(4, netmask[i] = 0xFF);
		readAddr(myIp+4, 4);
	} 
	else
	{
		invalidAddr = checkAddr(myIp+4, 4);
	}
	
	skipBytes(8);

	if (checkAddr(myMac+6, 6)) return;
	REPEAT(10, if (enc28j60ReadByte() != 0)) return;
	
	skipBytes(192);
	if (enc28j60ReadByte() != 0x63) return;// Magic cookie
	if (enc28j60ReadByte() != 0x82) return;
	if (enc28j60ReadByte() != 0x53) return;
	if (enc28j60ReadByte() != 0x63) return;

	uint8_t found = 0;
	uint8_t type = 0;
	uint16_t readLen = 240 + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN + CRC_LEN;
	while (1)
	{
		readLen += 2;
		if (readLen > len)
		{
			break;
		}
		uint8_t code = enc28j60ReadByte();
		if (code == 0xFF)
		{
			break;
		}
		if (code)
		{
			uint8_t l = enc28j60ReadByte();
			readLen += l;
			if (readLen > len)
			{
				break;
			}
			if (code == 53 && l == 1)
			{
				type = enc28j60ReadByte();
				found |= 0x04;
			}
			else if (code == 54 && l == 4)
			{
				readAddr(serverId+4, 4);
				found |= 0x10;
			}
			else if (code == 51 && l == 4)
			{
				enc28j60ReadByte();
				enc28j60ReadByte();
				enc28j60ReadByte();
				enc28j60ReadByte();
				leaseTime = CONST16(30);
				// ToDo
				/*//leaseTime = ((uint32_t)enc28j60ReadByte()<<24 | (uint32_t)enc28j60ReadByte()<<16 | enc28j60ReadByte()<<8 | enc28j60ReadByte())>>1;*/
				//if (enc28j60ReadByte() | enc28j60ReadByte())
				//{
				//	// ToDo: set max leaseTime
				//	leaseTime = 0xFFFF;
				//}
				//else
				//{
				//	u16 l = {0};
				//	l.b[1] = enc28j60ReadByte();
				//	l.b[0] = enc28j60ReadByte();
				//	leaseTime = l.w >> 1;//40;//should be leaseTime = l.w;
				//}
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
				while (l)
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

	uint8_t* ptr = dstIp+4;

	if (found & 0x01) // found netmask
	{
		uint8_t c = 4;
		uint8_t* px = dstIp+4;
		uint8_t* py = myIp+4;
		uint8_t* pz = netmask+4;
		do
		{
			uint8_t x;
			uint8_t y;
			uint8_t z;
			asm (
				"ld %3, -%a0 \n"
				"ld %4, -%a1 \n"
				"ld %5, -%a2 \n"
				: "+x" (px), "+y" (py), "+z" (pz), "=r" (x), "=r" (y), "=r" (z)
			);
			if ((x & z) != (y & z)) // should route via gw
			{
				if (!(found & 0x02)) return; // gw not found
				ptr = gwIp+4;
				break;
			}
		} while (--c);
	}

	if (*state == 1)
	{
		if (type != 2) return;
		if (found < 0x1C) return;
		*state = 2;
	}
	else
	{
		if (type != 5) return;
		if (found < 0xC) return;
		if (*state == 2 || !cnt)
			*state = 4;
		else
			*state = 6;
	}
	tim = 0;
	//REPEAT(4, if ((dstIp[i] & netmask[i]) != (myIp[i] & netmask[i])) return state);
	//uint8_t* arp = arpIp+4;
	//py = dstIp+4;
	copyAddr(arpIp+4, ptr);
	//REPEAT(4, gwIp[i] = dstIp[i]);
	return;
}

static void receiveNtpPacket(uint16_t len, uint8_t* state)
{
	// ToDo: check if reference clock is synchronized
	if ((enc28j60ReadByte() & 0x7) != 0x4) return; // ToDo ???
	//if ((enc28j60ReadByte() & 0x3F) != 0x24) return state; // ToDo ???
	if (enc28j60ReadByte() > 15) return;
	skipBytes(22);

	//uint8_t c = 0;
	//uint8_t f = tcnt / 245;
	//
	//uint8_t x4 = enc28j60ReadByte();
	//if (x4 != timestamp[4]) return state;
	//uint8_t x3 = enc28j60ReadByte();
	//if (x3 != timestamp[3]) return state;
	//uint8_t x2 = enc28j60ReadByte();
	//if (x2 != timestamp[2]) return state;
	//uint8_t x1 = enc28j60ReadByte();
	//if (x1 != timestamp[1]) return state;
	//uint8_t x0 = enc28j60ReadByte();
	//if (x0 != timestamp[0]) return state;
	//
	//// (c,t,f) -= (x4,x3,x2,x1,x0);
	//asm (
	//	"sub %0, %3 \n"
	//	"sbc %A1, %4 \n"
	//	"sbc %B1, %5 \n"
	//	"sbc %C1, %6 \n"
	//	"sbc %D1, %7 \n"
	//	"sbc %2, __zero_reg__ \n"
	//	: "+r" (f), "+r" (t), "+r" (c)
	//	: "r" (x0), "r" (x1), "r" (x2), "r" (x3), "r" (x4)
	//);

	
	if (checkAddr(timestamp+6, 6)) return;
	REPEAT(2, if (enc28j60ReadByte() != 0) return);

	//if (checkAddr(timestamp+5, 5)) return state;
	
	//REPEAT(3, if (enc28j60ReadByte() != 0) return state);
	

	cli();
	uint8_t f = TCNT1 / CONST16(245);

	uint8_t x4;
	uint8_t x3;
	uint8_t x2;
	uint8_t x1;
	uint8_t x0 = f;
	
	uint8_t* ptr;// = (uint8_t*)&time;
	asm volatile (
		"ldi %A0, lo8(time) \n"
		"ldi %B0, hi8(time) \n"
		: "=d"(ptr)
	);
	asm (
		"ld %0, %a4+ \n"
		"ld %1, %a4+ \n"
		"ld %2, %a4+ \n"
		"ld %3, %a4+ \n"
		: "=r" (x1), "=r" (x2), "=r" (x3), "=r" (x4), "+e" (ptr)
	);
	sei();

	//ptr = timestamp;
	asm volatile (
		"ldi %A0, lo8(net+1) \n"
		"ldi %B0, hi8(net+1) \n"
		: "=d"(ptr)
	);
	uint8_t temp;
	
	// (x4,x3,x2,x1,x0) -= (timestamp[4], timestamp[3], timestamp[2], timestamp[1], timestamp[0]);
	asm (
		"ld %6, %a5+ \n"
		"sub %0, %6 \n"
		"ld %6, %a5+ \n"
		"sbc %1, %6 \n"
		"ld %6, %a5+ \n"
		"sbc %2, %6 \n"
		"ld %6, %a5+ \n"
		"sbc %3, %6 \n"
		"ld %6, %a5+ \n"
		"sbc %4, %6 \n"
		: "+r" (x0), "+r" (x1), "+r" (x2), "+r" (x3), "+r" (x4), "+e" (ptr), "=r" (temp)
	);

	uint8_t y4 = enc28j60ReadByte();
	uint8_t y3 = enc28j60ReadByte();
	uint8_t y2 = enc28j60ReadByte();
	uint8_t y1 = enc28j60ReadByte();
	uint8_t y0 = enc28j60ReadByte();
	
	// (c,t,f) += (x4,x3,x2,x1,x0);
	asm (
		"add %0, %5 \n"
		"adc %1, %6 \n"
		"adc %2, %7 \n"
		"adc %3, %8 \n"
		"adc %4, %9 \n"
		: "+r" (x0), "+r" (x1), "+r" (x2), "+r" (x3), "+r" (x4)
		: "r" (y0), "r" (y1), "r" (y2), "r" (y3), "r" (y4)
	);
	
	skipBytes(3);
	
	y4 = enc28j60ReadByte();
	y3 = enc28j60ReadByte();
	y2 = enc28j60ReadByte();
	y1 = enc28j60ReadByte();
	y0 = enc28j60ReadByte();
	skipBytes(3); // ToDo: remove
	
	// (c,t,f) += (x4,x3,x2,x1,x0);
	// (c,t,f) >>= 1;
	asm (
		"add %0, %5 \n"
		"adc %1, %6 \n"
		"adc %2, %7 \n"
		"adc %3, %8 \n"
		"adc %4, %9 \n"
		"ror %4 \n"
		"ror %3 \n"
		"ror %2 \n"
		"ror %1 \n"
		"ror %0 \n"
		: "+r" (x0), "+r" (x1), "+r" (x2), "+r" (x3), "+r" (x4)
		: "r" (y0), "r" (y1), "r" (y2), "r" (y3), "r" (y4)
	);
	
	uint16_t mres = CONST16(245);
	uint8_t mcnt = 8;

	// mres *= f;
	asm (
		"lsr %A0 \n"
		"m1: \n"
		"brcc m2 \n"
		"add %B0, %1 \n"
		"m2: \n"
		"ror %B0 \n"
		"ror %A0 \n"
		"dec %2 \n"
		"brne m1 \n"
		: "+r" (mres)
		: "r" (x0), "r" (mcnt)
	);
	
	cli();// ToDo
	//TIMSK |= (1<<OCIE1A) | (1<<OCIE1B);
	
	SFIOR = (1<<PSR10);
	TCNT1 = mres;
	//TCNT1 = f * 245;//((uint32_t)fraction * 62500)>>16;

	//ptr = (uint8_t*)&time;
	asm volatile (
		"ldi %A0, lo8(time) \n"
		"ldi %B0, hi8(time) \n"
		: "=d"(ptr)
	);
	asm volatile (
		"st %a0+, %1 \n"
		"st %a0+, %2 \n"
		"st %a0+, %3 \n"
		"st %a0+, %4 \n"
		: "+e" (ptr)
		: "r" (x1), "r" (x2), "r" (x3), "r" (x4)
	);

	flag |= TIME_OK;

	
	TIFR  = (1<<OCF1A) | (1<<OCF1B);// ToDo: asm const
	sei();

	tim = 0;
	cnt = CONST16(15);//ToDo: should be 900
	*state = 6;
}

static void receiveUdpPacket(uint16_t len, uint8_t* state)
{
	if (len < 48 + UDP_HEADER_LEN + IP_HEADER_LEN + ETH_HEADER_LEN + CRC_LEN) return;

	if (enc28j60ReadByte() != 0) return;
	uint8_t srcPort = enc28j60ReadByte();

	if (enc28j60ReadByte() != 0) return;
	uint8_t dstPort = enc28j60ReadByte();

	checkLen(len - IP_HEADER_LEN - ETH_HEADER_LEN - CRC_LEN);

	enc28j60ReadByte();
	enc28j60ReadByte();

	if (*state <= 3 && srcPort == 67 && dstPort == 68)
		receiveDhcpPacket(len, state);
	else if (*state == 5 && srcPort == 123)
		receiveNtpPacket(len, state); // ToDo: consider checking dstPort
}

static void receiveIpPacket(uint16_t len, uint8_t* state)
{
	if (enc28j60ReadByte() != 0x45) return;
	enc28j60ReadByte(); // ToDo: if (enc28j60ReadByte() != 0x00) return state; ???

	checkLen(len - ETH_HEADER_LEN - CRC_LEN);

	skipBytes(5);

	if (enc28j60ReadByte() != IP_PROTO_UDP_V) return;

	// ToDo: check source and target ip
	/*enc28j60ReadByte();
	enc28j60ReadByte();

	REPEAT(4, srcIp[i] = enc28j60ReadByte());*/
	if (*state == 5)
	{
		skipBytes(2);
		if (checkAddr(dstIp+4, 4)) return;
	}
	else
	{
		skipBytes(6);
	}

	//if (state > 2)
	//{
	//	uint8_t data = enc28j60ReadByte();
	//	if (state == 3 && data == 0xFF)
	//	{
	//		REPEAT(3, if (enc28j60ReadByte() != 0xFF) return state);
	//	}
	//	else if (data == myIp[3])
	//	{
	//		if (checkAddr(myIp+3, 3)) return state;
	//	}
	//	else
	//		return state;
	//}
	//else
	//	skipBytes(4);

	if (*state > 3)
	{
		if (checkAddr(myIp+4, 4)) return;
	}
	else
	{
		skipBytes(4);
	}

	//skipBytes(10);

	receiveUdpPacket(len, state);
}

static void receiveArpPacket(uint16_t len, uint8_t* state)
{
	if (enc28j60ReadByte() != 0) return;
	if (enc28j60ReadByte() != 1) return;
	if (enc28j60ReadByte() != 8) return;
	if (enc28j60ReadByte() != 0) return;
	if (enc28j60ReadByte() != 6) return;
	if (enc28j60ReadByte() != 4) return;
	if (enc28j60ReadByte() != 0) return;
	uint8_t data = enc28j60ReadByte();
	if (data == 1)
	{
		readAddr(arpReplyMac+6, 6);
		readAddr(arpReplyIp+4, 4);
		//readAddr(arpReplyAddr+10, 10);
		skipBytes(6);
		if (checkAddr(myIp+4, 4)) return;
		flag |= ARP_REPLY;
	}
	else if (data == 2 && *state == 4)
	{
		readAddr(dstMac+6, 6);
		if (checkAddr(arpIp+4, 4)) return;// ToDo: consider checking target IP, MAC
		tim = 0;
		*state = 5;
	}
}

static void receiveEthPacket(uint16_t len, uint8_t* state)
{
	if (len < 28 + ETH_HEADER_LEN + CRC_LEN) return;

	//uint8_t data = enc28j60ReadByte();
	//if (data == 0xFF)
	//{
	//	REPEAT(5, if (enc28j60ReadByte() != 0xFF) return state);
	//}
	//else if (data == MAC0)
	//{
	//	if (enc28j60ReadByte() != MAC1) return state;
	//	if (enc28j60ReadByte() != MAC2) return state;
	//	if (enc28j60ReadByte() != MAC3) return state;
	//	if (enc28j60ReadByte() != MAC4) return state;
	//	if (enc28j60ReadByte() != MAC5) return state;
	//}
	//else return state;
	skipBytes(12);

	//readAddr(srcMac+6, 6);

	if (enc28j60ReadByte() != ETHTYPE_IP_H_V) return;
	uint8_t data = enc28j60ReadByte();
	if (data == ETHTYPE_IP_L_V) return receiveIpPacket(len, state);
	if (*state > 2 && data == ETHTYPE_ARP_L_V) return receiveArpPacket(len, state);
}

void loop(uint8_t* state, uint16_t* NextPacketPtr)
{
	// RECEIVE
	if (enc28j60PacketReceived()) // ToDo: deal with buffer overflow !!!
	{
		uint16_t len = enc28j60ReadPacket(NextPacketPtr);
		receiveEthPacket(len, state);
		enc28j60EndRead(NextPacketPtr);
	}
	if (tim == 0 && *state < 6)
		tim = CONST8(5);
	else if (!(flag & ARP_REPLY))
		return;

	sendPacket(*state); // ToDo: deal with buffer overflow !!!
	flag &= ~ARP_REPLY;
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