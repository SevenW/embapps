//============================================================================
// Name        : costcontrol.cpp
// Author      : SevenWatt
// Version     :
// Copyright   : (c) 2015 sevenwatt.com
// Description : Receive Cost Control RT-110 or Energy Count 3000 packets
//             : and retransmit into JeeNode network, and log to console.
//             : mainly based on the work of Thomas Schulz:
//             : http://forum.jeelabs.net/comment/4972.html#comment-4972
//             : http://forum.jeelabs.net/comment/6577.html#comment-6577
//             : result formating by ohweh
//             : http://www.fhemwiki.de/wiki/JeeLink
//
//============================================================================

#include "chip.h"
#include "uart.h"
#include <stdio.h>
//#include <string.h> //memcpy

#define chThdYield() // FIXME still used in rf69.h

// TODO #define RF69_SPI_BULK 1
#include "spi.h"
#include "rf69.h"

const uint8_t bufSize = 66;
uint8_t rxSize = 0;
uint8_t rxBuf[bufSize];
//uint8_t rssi[bufSize+1];
//uint16_t rssi_i=0;

#include "rf69cc.h"
RF69CC<SpiDev0> rf;

#define VERBOSE 1
#define EC3KLEN   41    // length of Energy Count 3000 packet

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Energy Count 3000 utilities

uint16_t crc_ccitt_update(uint16_t crc, uint8_t data) {
	data ^= crc & 0xFF;
	data ^= data << 4;

	return ((((uint16_t) data << 8) | (crc >> 8)) ^ (uint8_t) (data >> 4)
			^ ((uint16_t) data << 3));
}

static uint16_t mem2word(uint8_t * data)  // convert 2 bytes to word
		{
	return data[0] << 8 | data[1];
}

// left shift block of bytes
static void lshift(unsigned char * buff, uint8_t blen, uint8_t shift) {
	uint8_t offs, bits, slen, i;
	uint16_t wbuf;

	if (shift == 0)
		return;
	offs = shift / 8;
	bits = 8 - shift % 8;
	slen = blen - offs - 1;
	wbuf = buff[offs];
	for (i = 0; i < slen; ++i) {
		wbuf = wbuf << 8 | buff[i + offs + 1];
		buff[i] = wbuf >> bits;
	}
	buff[slen] = wbuf << (uint8_t) (8 - bits);
}

//  delete 0-bits inserted after 5 consecutive 1-bits and reverse bit-sequence
//  EC3k packets are transmitted with HDLC bit-stuffing and LSBit first
//  decode EC3k packets needs these 0-bits deleted and bits reversed to
//  MSBit first
static void del0bitins_revbits(unsigned char * buff, uint8_t blen) {
	uint8_t sval, dval, bit;
	uint8_t si, sbi, di, dbi, n1bits;

	di = dval = dbi = n1bits = 0;
	for (si = 0; si < blen; ++si) {
		sval = buff[si];      // get source byte
		for (sbi = 0; sbi < 8; ++sbi) {
			bit = sval & 0x80;      // get source-bit
			sval <<= 1;       // process source MSBit to LSBit
			if (n1bits >= 5 && bit == 0) {  // 5 1-bits and 1 0-bit
				n1bits = 0;     //  reset counter
				continue;     //  and skip 0-bit
			}
			if (bit)
				n1bits++;   // count consecutive 1-bits
			else
				n1bits = 0;   // 0-bit: reset counter
			dval = dval >> 1 | bit;   // add source-bit to destination
			++dbi;        //  reversing destin bit-sequence
			if (dbi == 8) {     // destination byte complete
				buff[di++] = dval;    //  store it
				dval = dbi = 0;     //  reset for next destin byte
			}
		}
	}
	if (dbi)
		buff[di] = dval >> (uint8_t) (8 - dbi);
}

static uint8_t count1bits(uint32_t v) {
	uint8_t c; // c accumulates the total bits set in v
	for (c = 0; v; c++) {
		v &= v - 1; // clear the least significant bit set
	}
	return c;
}

// generic multiplicative (self-synchronizing) de-scrambler
//  descramble Energy Count 3000 packets needs only the polynomial
//  x^18+x^17+x^13+x^12+x+1 (0x31801) plus bit-inversion, equivalant
//  to Non-Return-to-Zero-Space (NRZS) and polynomial x^17+x^12+1 and
//  to Non-Return-to-Zero-Inverted (NRZI) and polynomial x^17+x^12+1
//  plus bit-inversion
//  scrambler polynomial hexadecimal representation with exponents
//  as 1-relative bit numbers and an implicit + 1 term
//  e.g. 0x8810 for CRC-16-CCITT polynomial x^16 + x^12 + x^5 + 1
//         x^16       + x^12                + x^5             + 1
//  0x8810  1  0  0  0   1  0  0  0   0  0  0  1   0  0  0  0
//         16 15 14 13  12 11 10  9   8  7  6  5   4  3  2  1
//  see http://www.hackipedia.org/Checksums/CRC/html/Cyclic_redundancy_check.htm
//      "Specification of CRC" and "Representations...(reverse of reciprocal)"
//  and http://www.ece.cmu.edu/%7Ekoopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
//      "hexadecimal representation" on page 2 in section "2.Background"
//  descramblers for Energy Count 3000 found with Berlekamp-Massey algorithm:
//    2111 1111 111             polynomial exponents 20..1
//          0987 6543 2109 8765 4321
//  0x10800    1 0000 1000 0000 0000  x^17+x^12+1
//  0x31801   11 0001 1000 0000 0001  x^18+x^17+x^13+x^12+x+1
//  0x52802  101 0010 1000 0000 0010  x^19+x^17+x^14+x^12+x^2+1
//  0xF7807 1111 0111 1000 0000 0111  20,19,18,17,15,14,13,12,3,2,1
//  ITU-T V.52 PN9 PseudoRandomBitSequence, see RFM22 and trc103 manuals:
//    0x110              1 0001 0000  x^9+x^5+1
//  IEEE 802.11b scrambler, see "4.4 Scrambler" page 16 (PDF page 17)
//  in http://epubl.ltu.se/1402-1617/2001/066/LTU-EX-01066-SE.pdf
//     0x48                 100 1000  x^7+x^4+1
//  descramb() takes 3328 .. 3524 usec per 70 byte block
//
#define SCRAMPOLY  0x31801  // polynomial x^18+x^17+x^13+x^12+x+1

static void descramb(uint8_t* buff, uint16_t len) {
	uint8_t ibit, obit;
	uint8_t bit;
	uint8_t inpbyte, outbyte;
	uint32_t scramshift;

	//scramshift = 0xFFFFFFFF;
	scramshift = 0xF185D3AC; //descrambler primed at end of preanble
	while (len--) {
		inpbyte = *buff;
		for (bit = 0; bit < 8; ++bit) {
			// RFM69 receives MSBit first into bytes:
			ibit = (inpbyte & 0x80) >> 7;
			obit = ibit ^ (count1bits(scramshift & SCRAMPOLY) & 0x01);
			scramshift = scramshift << 1 | ibit;
			inpbyte <<= 1;
			outbyte = outbyte << 1 | obit;
		}
		*buff++ = outbyte ^ 0xFF;
	}
}

// convert descrambled Energy Count 3000 packets to interpretable format
//  packet 'rblock' is expected descrambled with common sequence at 'offs'
//  delete 0-bits inserted after 5 consecutive 1-bits starting at ID byte
//  and reverse bits per byte, i.e. move bit-7,6..0 to bit-0,1..7
//  left-shift packet by 4 bits and delete common sequence before ID byte
//  takes 684..728 usec
//
static uint16_t ec3kcrc;

static uint8_t ec3krevshift(uint8_t idoffs, uint8_t *rblock, uint16_t rblen) {
	uint16_t i, ec3klen;
	uint16_t crc;

	ec3klen = rblen - idoffs;
	del0bitins_revbits(rblock + idoffs, ec3klen);
	crc = 0xFFFF;
	if (ec3klen >= EC3KLEN)
		for (i = 0; i < EC3KLEN; ++i)
			crc = crc_ccitt_update(crc, rblock[idoffs + i]);
	ec3kcrc = crc;
	lshift(rblock, rblen, 4 + idoffs * 8); //remove 4 bits of start mark '9'
	return ec3klen;
}

// report Energy Count 3000 packet data
//      EC3k packet offsets (offs9) below start with "9" before ID
//                             1     1   1   1                   2   3   3   3   3 3 3   4 4
//-2-1 0 1 2   4   6   8       2     5   7   9                   9   1   3   5   7 8 9   1 2
// IbBfSID--TsecZeroOsecZero---Wslo---WattWmax?unknown-----------?ThiZero-WshiOhiRcOZCsumEfIb....
// 557E96EA19B1300009AAF0000000EB3FC88032E068F84946848B4853D884DFB14100000000314105802FB87E5555555
//  0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3 3 3 3 3 3 3 3 3 4 4 4 4 4
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4
//     ^^------v  EC3k packet offsets (offs) above start with ID
//     |       v
//     `offs9  offs len
//   -2      -   1   Ib   idle byte 0x55 preceding HDLC frame
//   -1      -   1   Bf   HDLC begin flag 0x7E
//    0      -   0.4 S    start mark, always 9
//    0.4    0   2   ID   Energy Count 3000 transmitter ID
//    2.4    2   2   Tsec time total in seconds lower 16 bits
//    4.4    4   2   Zero 00-bytes
//    6.4    6   2   Osec time ON in seconds lower 16 bits
//    8.4    8   3.4 Zero 00-bytes
//   12     11.4 3.4 Wslo energy in watt-seconds lower 28 bits
//   15.4   15   2   Watt current watt*10
//   17.4   17   2   Wmax maximum watt*10
//   19.4   19  10   ?unknown? (seems to be used by EC3k internal calculations)
//   29.4   29   1.4 Thi  time total in seconds upper 12 bits
//   31     30.4 2.4 Zero 00-bytes
//   33.4   33   2   Wshi energy in watt-seconds upper 16 (or 12?) bits
//   35.4   35   1.4 Ohi  time ON in seconds upper 12 bits
//   36     36.4 1   Rc   reset counter: number of EC3k transmitter resets
//   37     37.4 0.4 O    device on/off flag: 0xX8 = ON (watt!=0), 0xX0 = off (watt==0)
//   38.4   38   0.4 Z    0-nibble
//   39     38.4 2   Csum checksum CRC-CCITT starting with start mark 9
//   41     40.4 1   Ef   HDLC end flag 0x7E
//   42     41.4 1+  Ib   idle bytes 0x55 following HDLC frame
//  trailing ".4" means half byte (4 bits)
//
static uint8_t ec3kdecode(uint8_t *ec3kpkt, uint8_t *payl) {
	uint16_t id, lswsec, mswsec, Watt, Wmax;
	uint16_t Wshi, Wsmid, Wslo;
	uint8_t nres, onoff;
	uint32_t tsec, osec, Whrs;

	id = mem2word(ec3kpkt + 0);
	lswsec = mem2word(ec3kpkt + 2);
	mswsec = mem2word(ec3kpkt + 29) >> 4;
	tsec = (uint32_t) mswsec << 16 | lswsec;
	lswsec = mem2word(ec3kpkt + 6);
	mswsec = mem2word(ec3kpkt + 35) >> 4;
	osec = (uint32_t) mswsec << 16 | lswsec;
	Watt = mem2word(ec3kpkt + 15);
	Wmax = mem2word(ec3kpkt + 17);
	Wslo = mem2word(ec3kpkt + 13);
	Wsmid = mem2word(ec3kpkt + 11);
	Wshi = mem2word(ec3kpkt + 33);
	Wsmid = (Wsmid & 0x0FFF) | (Wshi << 12);
	Wshi >>= 4;
	nres = (uint8_t) (ec3kpkt[36] << 4) | (uint8_t) (ec3kpkt[37] >> 4);
	onoff = ec3kpkt[37] & 0x08;
	//generates much more code for 64bit (+4,2k), could be avoided by an own implementation of division without 64bit
	//Ws are 40bits, 3600 = 2^4 * 225 -> shift Ws 4bits right = 36Bits left to divide by 225
	//Option: use only 36Bits, means 32bits left after shifting which gets max 19088 kWh - could be enough ;-), but we have no problem with flash-size at the moment)
	//	Whrs = ((uint64_t) Wshi << 32 | (uint32_t) Wsmid << 16 | (uint32_t) Wslo)
	//			/ 3600; //with only 32bits: max 1193kWh
	Whrs = ((uint32_t)Wshi / 3600) << 32;
	Whrs += (((((uint32_t)Wshi % 3600) << 16) + Wsmid) / 3600) << 16;
	uint32_t remainder = ((((uint32_t)Wshi % 3600) << 16) + Wsmid) % 3600;
	Whrs += (((remainder << 16) + Wslo) / 3600);

	//  report EC3k packet contents
#if VERBOSE
	//TODO: Add timestamps
	printf("EC3K %d ID=%d %d sT %d sON", 0/*us timestamp*/, id, tsec, osec);
#endif

#if VERBOSE
	printf(" %d.%d W %d.%d Wmax %d resets\r\n", Watt / 10, Watt % 10, Wmax / 10, Wmax % 10, nres);
#endif
	//TODO: Update comment
	//TODO: Send Ws or kWs in stead of kWh (no loss of precision)
	// ---------------------------------------------------------------------------------------
	// - crc calc algorithm is unknown.
	//
	// output format:
	//   01 - static jeenode id 22 (defined in nodemap.local)
	//   02 - ec3k sender id
	//   03 - seconds total
	//   04 - seconds on
	//   05 - watt hours
	//   06 - actual consumption (shifted by 10)
	//   07 - max. consumption (shifted by 10)
	//   08 - number of resets
	// ---------------------------------------------------------------------------------------

	//uint8_t payl[20];
	//TODO: check len of payl is len of tXBuf is sufficient
	uint8_t plen = 0;
	payl[0] = id >> 8;       // ec3k sender id
	payl[1] = id;
	payl[2] = tsec >> 24;    // seconds total
	payl[3] = tsec >> 16;
	payl[4] = tsec >> 8;
	payl[5] = tsec;
	payl[6] = osec >> 24;    // seconds on
	payl[7] = osec >> 16;
	payl[8] = osec >> 8;
	payl[9] = osec;
	payl[10] = Whrs >> 24;    // watt hours
	payl[11] = Whrs >> 16;
	payl[12] = Whrs >> 8;
	payl[13] = Whrs;
	payl[14] = Watt >> 8;     // actual watt consumption
	payl[15] = Watt;
	payl[16] = Wmax >> 8;     // watt max
	payl[17] = Wmax;
	payl[18] = nres;          // resets
	plen = 19;

#if VERBOSE
	printf("$ 22 %d %d %d %d %d %d %d", id, tsec, osec, Whrs, Watt, Wmax, nres);
	printf("\r\n");
#endif

	return plen;
}

// hexadecimal dump data bytes
//
static void hexdumpspc(uint8_t * data, uint16_t dlen, uint8_t ispc) {
	uint8_t spcnt;

	for (spcnt = 0; dlen != 0; --dlen, ++spcnt) {
		if (ispc && spcnt == ispc) {
			spcnt = 0;
			printf(" ");
		}
		printf(" %02X", *data++);
	}
	printf("\r\n");
}

static void hexdump(uint8_t * data, uint16_t dlen) {
	hexdumpspc(data, dlen, 0);
}

static int processrecv(bool do_EC3K, uint8_t descram, uint8_t *ec3kPacket) {
	uint8_t bitoffs = 0;
	uint8_t ec3klen;
	uint8_t ec3kvalidlen = 0;
	//uint8_t rxorig[rxSize];

	// save original receive data, because EC3k decode modifies rxbuf contents,
	// but on decode failure the original data shall be written to dataflash
	// memcpy((void*)rxorig, (const void*) rxBuf, rxSize);

	if (descram != 0x0C) {
		printf("data");
		hexdump((uint8_t*) rxBuf, rxSize);
	}
	bitoffs = 8; //because matched on last 5 bytes of preamble, 8 bits to skip 0x7E
	//can be done, but 0x7E is used in crc and bit-de-stuffing (should not?)
	//bitoffs = 0; //because matched on last 5 bytes of preamble + HDLC 0x7E

	if (descram) {
		descramb((uint8_t*) rxBuf, rxSize);
		if (descram != 0x0C) {
			printf("dscr");
			hexdump((uint8_t*) rxBuf, rxSize);
		}
		if (descram == 0x0C || descram == 0x0D) {
			ec3klen = ec3krevshift(bitoffs / 8, (uint8_t*) rxBuf, rxSize);
			if (descram != 0x0C) {
				printf("ec3k");
				hexdump((uint8_t*) rxBuf, ec3klen);
			}
			printf("ec3klen %d, ec3kcrc %04X\r\n", ec3klen, ec3kcrc);
			if (ec3klen >= EC3KLEN && ec3kcrc == 0xF0B8) { /* from ec3krevshift() */
				ec3kvalidlen = ec3kdecode((uint8_t*) rxBuf, ec3kPacket);
			}
		}
	}
	return ec3kvalidlen;
}

int main() {
	// the device pin mapping is configured at run time based on its id
	uint16_t devId = LPC_SYSCON->DEVICEID;
	// choose different node id's, depending on the chip type
	uint8_t nodeId = 60;

	// SPI pin assignment:
	//  3: SPI0_SCK x         x         x
	//  4: x        SPI0_SSEL SPI0_MISO SPI0_MOSI

	switch (devId) {
	case 0x8100:
		nodeId = 10;
		// disable SWCLK/SWDIO and RESET
		LPC_SWM->PINENABLE0 |= (3 << 2) | (1 << 6);
		// lpc810 coin: sck=0p8, ssel=1p5, miso=2p4, mosi=5p1, tx=4p2
		// SPI0
		LPC_SWM->PINASSIGN[3] = 0x00FFFFFF;   // sck  -    -    -
		LPC_SWM->PINASSIGN[4] = 0xFF010205;   // -    nss  miso mosi
		// USART0
		LPC_SWM->PINASSIGN[0] = 0xFFFFFF04;
		break;
	case 0x8120:
		nodeId = 12;
		LPC_SWM->PINASSIGN[0] = 0xFFFF0004;
		// jnp v2: sck 6, ssel 8, miso 11, mosi 9, irq 10
		LPC_SWM->PINASSIGN[3] = 0x06FFFFFF;
		LPC_SWM->PINASSIGN[4] = 0xFF080B09;
		break;
	case 0x8121:
		nodeId = 13;
		LPC_SWM->PINASSIGN[0] = 0xFFFF0004;
		// A not working, but B is fine
		// eb20soic A: sck 14, ssel 15, miso 12, mosi 13, irq 8
		//LPC_SWM->PINASSIGN[3] = 0x0EFFFFFF;
		//LPC_SWM->PINASSIGN[4] = 0xFF0F0C0D;
		// eb20soic B: sck 12, ssel 13, miso 15, mosi 14, irq 8
		LPC_SWM->PINASSIGN[3] = 0x0CFFFFFF;
		LPC_SWM->PINASSIGN[4] = 0xFF0D0F0E;
		break;
	case 0x8122:
		nodeId = 14;
		LPC_SWM->PINASSIGN[0] = 0xFFFF0106;
		// ea812: sck 12, ssel 13, miso 15, mosi 14
		LPC_SWM->PINASSIGN[3] = 0x0CFFFFFF;
		LPC_SWM->PINASSIGN[4] = 0xFF0D0F0E;
		break;
	case 0x8241:
		nodeId = 23;
		// ea824: sck 24, ssel 15, miso 25, mosi 26
		LPC_SWM->PINASSIGN[0] = 0xFFFF1207;
		LPC_SWM->PINASSIGN[3] = 0x18FFFFFF;
		LPC_SWM->PINASSIGN[4] = 0xFF0F191A;
		break;
	case 0x8242:
		nodeId = 24;
		// jnp v3: sck 17, ssel 23, miso 9, mosi 8, irq 1
		LPC_SWM->PINASSIGN[0] = 0xFFFF0004;
		LPC_SWM->PINASSIGN[3] = 0x11FFFFFF;
		LPC_SWM->PINASSIGN[4] = 0xFF170908;
		break;
	}

	uart0Init(115200);
	for (int i = 0; i < 10000; ++i)
		__ASM("");
	printf("\n[costcontrol] dev %x node %d\n", devId, nodeId);
	printf("Cost Control RT-110 / Energy Count 3000 receiver\n");

	rf.init(nodeId, 42, 868300);
	//rf.encrypt("mysecret");
	rf.txPower(0); // 0 = min .. 31 = max

	rf.initCCreceive(868299); //63 = catch all packets, 0xAA is EC3000 syncword
	rf.setBitrate(20000);
	rf.setPayloadLen(56);

	uint16_t cnt = 0;

	while (true) {
//        if (++cnt == 0) {
//            const int TXLEN = 1; // can be set to anything from 1 to 65
//            static uint8_t txBuf[TXLEN];
//            printf(" > %d\n", ++txBuf[0]);
//            rf.send(0, txBuf, sizeof txBuf);
//        }

		int len = rf.receive_fixed(rxBuf, sizeof rxBuf);
		uint8_t rssi = rf.rssi;
		if (len >= 0) {
//			printf("OK %02X ", len);
//			for (int i = 0; i < len; i++)
//				printf("%02X ", rxBuf[i]);
//			printf(" (%d%s%d:%d)\r\n", rf.rssi, rf.afc < 0 ? "" : "+", rf.afc,
//					rf.lna);

//			printf("RSSI L%d ", rssi_i);
//			for (int i = 0; i < rssi_i; i++)
//				printf("%02X ", rssi[i]);
//			printf("\r\n");

			//rssi_i=0;

			uint8_t descram = 0x0C;
			//0x03:    // 0x31801 descrambler 1,12,13,17,18 + bit-inversion
			//0x0C:    // descramble and decode EC3k packets, silent
			//0x0D:    // descramble and decode EC3k packets, verbose

			bool do_EC3K = true;
			rxSize = len;

			const int TXLEN = 1; // can be set to anything from 1 to 65
			static uint8_t txBuf[TXLEN];

			uint8_t cnt = processrecv(do_EC3K, descram, txBuf);  //  process+report received block
			if (cnt>0) {
				printf("transmitting %d bytes\r\n", cnt);
				//Switch to JeeNode transmit mode
				rf.exitCCreceive();
				rf.init(nodeId, 42, 868300);
				rf.send(0, txBuf, cnt);
				//Switch back to Cost Control / EC3K receive mode
				rf.initCCreceive(868299); //868299kHz is optimal according to AFC.
				rf.setBitrate(20000);
				rf.setPayloadLen(56);

				//dump to console for "JeeLink mode"
				printf("OK %02X%02X", cnt + 2, nodeId);
				for (int ix = 0; ix < cnt; ix++)
					printf("%02X", txBuf[ix]);
				printf(" (%02X)\r\n", rssi);
			}
		}

	chThdYield()
	}
}
