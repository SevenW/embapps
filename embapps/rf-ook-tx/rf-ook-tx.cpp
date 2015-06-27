//============================================================================
// Name        : rf-ook-tx.cpp
// Author      : SevenWatt
// Version     :
// Copyright   : sevenwatt.com (c) 2015
// Description : Send RFM69 packets. Report received data on the serial port
//
//============================================================================

#include "chip.h"
#include "uart.h"
#include <stdio.h>

// TODO #define RF69_SPI_BULK 1
#include "spi.h"
#include "rf69.h"
#include "rf69rc.h"

//configuration items
uint8_t DIO2 = 15; //GPIO pin DIO2(=DATA)

//#define FREQ_BAND 433
#define FREQ_BAND 868  // 868 or 434
#define PACKET false  //true or false

RF69RC<SpiDev0> rf;

volatile uint32_t ticks = 0;
volatile uint32_t pulse = 0;                //pulse duration counter
volatile uint32_t delay = 0;                //delay duration counter
extern "C" void SysTick_Handler(void) {
	ticks++;
	if (pulse)
		pulse--;
	if (delay)
		delay--;
}

bool parity_even_bit(uint16_t v) {
	bool parity = false;  // parity will be the parity of v

	while (v) {
		parity = !parity;
		v = v & (v - 1);
	}
	return parity;
}

//Continuous OOK enable transmitter
void enableOOK() {
	LPC_GPIO_PORT->DIR[0] |= 1 << DIO2;
	LPC_GPIO_PORT->CLR[0] |= 1 << DIO2;
	//enable transmit
	rf.transmitOOKOn();
}

//Continuous OOK disable transmitter
void disableOOK() {
	//disable transmit
	rf.transmitOOKOff();
	LPC_GPIO_PORT->DIR[0] &= ~(1 << DIO2);
}

//Continuous OOK send one ON/OFF-pulse
static void ookPulse(int on, int off) {
	pulse = on / 10;
	LPC_GPIO_PORT->SET[0] |= 1 << DIO2;
	while (pulse)
		;
	pulse = off / 10;
	LPC_GPIO_PORT->CLR[0] |= 1 << DIO2;
	while (pulse)
		;
}

//Packet mode OOK transmit buffer
uint8_t ookBuf[66];
uint8_t k = 0;
uint8_t l = 0;

//Packet mode OOK add one packet-bit to transmit buffer
void addOutBit(uint8_t v) {
	if (k <= 65) {
		ookBuf[k] |= (v & 1) << (7 - l);
		if (++l == 8) {
			l = 0;
			k++;
			if (k > 65) {
				printf("FIFO size exceeded. Partial transmission\n");
			} else {
				ookBuf[k] = 0;
			}
		}
	}
}

//Packet mode OOK send complete packet
void sendPacket() {
	if (PACKET) {
		//pad ookBuf with OFF (=0) to fill last byte.
		while (l)
			addOutBit(0);
//		for (uint8_t i = 0; i < k; ++i)
//			printf("%02x", ookBuf[i]);
//		printf("\n");
		rf.sendook(0, ookBuf, k);
	}
}

static void fs20sendBits(uint16_t data, uint8_t bits) {
	if (bits == 8) {
		++bits;
		data = (data << 1) | parity_even_bit(data);
	}
	for (uint16_t mask = 1 << (bits - 1); mask != 0; mask >>= 1) {
		if (PACKET) {
			uint8_t n = (data & mask ? 3 : 2);
			for (uint8_t on = n; on > 0; on--)
				addOutBit(1);
			for (uint8_t off = n; off > 0; off--)
				addOutBit(0);
		} else {
			int width = data & mask ? 600 : 400;
			ookPulse(width, width);
		}
	}
}

void startOOK(uint32_t br) {
	if (PACKET) {
		//clear the ookBuf
		k = 0;
		l = 0;
		ookBuf[0] = 0;
		rf.setBitrate(br);
	} else {
		enableOOK();
	}
}

void stopOOK() {
	if (PACKET) {
		sendPacket();
	} else {
		disableOOK();
	}
}

static void fs20cmd(uint16_t house, uint8_t addr, uint8_t cmd) {
	uint8_t sum = 6 + (house >> 8) + house + addr + cmd;
	for (uint8_t i = 0; i < 3; ++i) {
		startOOK(5000); //PACKET mode: 200us (5000bps) largest common divisor
		fs20sendBits(1, 13);
		fs20sendBits(house >> 8, 8);
		fs20sendBits(house & 0xFF, 8);
		fs20sendBits(addr, 8);
		fs20sendBits(cmd, 8);
		fs20sendBits(sum, 8);
		fs20sendBits(0, 1);
		stopOOK();
		delay = 880; //8.8ms = 1000 beats of 10us
		while (delay)
			;
	}
}

static void kakuSend(uint8_t addr, uint8_t device, uint8_t on) {
    int cmd = 0x600 | ((device - 1) << 4) | ((addr - 1) & 0xF);
    if (on)
        cmd |= 0x800;
    for (uint8_t i = 0; i < 4; ++i) {
		startOOK(2667); //PACKET mode: 375us (2667bps) largest common divisor
    	int sr = cmd;

    	if (PACKET) {
			for (uint8_t bit = 0; bit < 12; ++bit) {
				addOutBit(1);
				addOutBit(0);
				addOutBit(0);
				addOutBit(0);
				//int on = bitRead(cmd, bit) ? 1125 : 375;
				int on = sr & 1 ? 1125 : 375;
				sr >>= 1;
				ookPulse(on, 1500 - on);
				uint8_t n = (sr & 1 ? 3 : 1);
				for (uint8_t on = n; on > 0; on--)
					addOutBit(1);
				for (uint8_t off = 4-n; off > 0; off--)
					addOutBit(0);
			}
			addOutBit(1);
			addOutBit(0);
    	} else {
			for (uint8_t bit = 0; bit < 12; ++bit) {
				ookPulse(375, 1125);
				//int on = bitRead(cmd, bit) ? 1125 : 375;
				int on = sr & 1 ? 1125 : 375;
				sr >>= 1;
				ookPulse(on, 1500 - on);
			}
		    ookPulse(375, 375);
    	}
		stopOOK();
        delay = 1100; //11ms = 1100 beats of 10us
		while (delay)
			;
    }
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
		LPC_SWM->PINENABLE0 = 0xffffffff;
		// lpc810: sck=0p8, ssel=1p5, miso=2p4, mosi=5p1, dio2=3p3, tx=4p2
		//SPI0
		LPC_SWM->PINASSIGN[3] = 0x00FFFFFF;   // sck  -    -    -
		LPC_SWM->PINASSIGN[4] = 0xFF010205;   // -    nss  miso mosi
		//USART0
		LPC_SWM->PINASSIGN[0] = 0xffffff04;
		DIO2 = 3;
		break;
	case 0x8120:
		nodeId = 12;
		LPC_SWM->PINASSIGN[0] = 0xFFFF0004;
		// jnp v2: sck 6, ssel 8, miso 11, mosi 9, irq 10, dio2 1p9
		LPC_SWM->PINASSIGN[3] = 0x06FFFFFF;
		LPC_SWM->PINASSIGN[4] = 0xFF080B09;
		DIO2 = 1;
		break;
	case 0x8121:
		nodeId = 13;
		LPC_SWM->PINASSIGN[0] = 0xFFFF0004;
		// A not working, but B is fine
		// eb20soic A: sck 14, ssel 15, miso 12, mosi 13, irq 8
		//LPC_SWM->PINASSIGN[3 = 0x0EFFFFFF;
		//LPC_SWM->PINASSIGN[4 = 0xFF0F0C0D;
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
		// jnp v3: sck 17, ssel 23, miso 9, mosi 8, irq 1, dio2 15p11
		LPC_SWM->PINASSIGN[0] = 0xFFFF0004;
		LPC_SWM->PINASSIGN[3] = 0x11FFFFFF;
		LPC_SWM->PINASSIGN[4] = 0xFF170908;
		DIO2 = 15;
		break;
	}

	const uint16_t tsample = 10; //us samples
	SysTick_Config(SystemCoreClock / (1000000 / tsample));

	uart0Init(115200);
	for (int i = 0; i < 10000; ++i)
		__ASM("");
	printf("\n[rf-ook-tx] dev %x node %d\n", devId, nodeId);
	printf("OOK TX\n");

	rf.init(nodeId, 42, 868270);
	if (PACKET) {
		rf.initOOKpckt(nodeId, 42, 868270);
	} else {
		rf.initOOKcont(nodeId, 42, 868270);
	}
	rf.txPower(5); // 0 = min .. 31 = max

	while (true) {
		if (ticks > 500000) {
			printf("Send one at %d\n", ticks);
			fs20cmd(0x1000, 0x01, 0x12);
			ticks = 0;
		}
	chThdYield()
}
}
