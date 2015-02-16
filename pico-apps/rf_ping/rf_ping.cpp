// Report received data on the serial port.

#include "LPC8xx.h"
#include "uart.h"
#include <stdio.h>

#define chThdYield() // FIXME still used in rf69.h

// TODO #define RF69_SPI_BULK 1
#include "spi.h"
#include "rf69.h"

RF69<SpiDev0> rf;

uint8_t rxBuf[66];

int main () {
    // the device pin mapping is configured at run time based on its id
    uint16_t devId = LPC_SYSCON->DEVICE_ID;
    // choose different node id's, depending on the chip type
    uint8_t nodeId = 60;

    // SPI pin assignment:
    //  3: SPI0_SCK x         x         x        
    //  4: x        SPI0_SSEL SPI0_MISO SPI0_MOSI

    switch (devId) {
        case 0x8100:
            nodeId = 10;
            // disable SWCLK/SWDIO and RESET
            LPC_SWM->PINENABLE0 |= (3<<2) | (1<<6);
            // lpc810 coin: sck=0p8, ssel=1p5, miso=2p4, mosi=5p1, tx=4p2
            // SPI0
            LPC_SWM->PINASSIGN3 = 0x00FFFFFF;   // sck  -    -    -
            LPC_SWM->PINASSIGN4 = 0xFF010205;   // -    nss  miso mosi
            // USART0
            LPC_SWM->PINASSIGN0 = 0xFFFFFF04;
            break;
        case 0x8120:
            nodeId = 12;
            LPC_SWM->PINASSIGN0 = 0xFFFF0004;
            // jnp v2: sck 6, ssel 8, miso 11, mosi 9, irq 10
            LPC_SWM->PINASSIGN3 = 0x06FFFFFF; 
            LPC_SWM->PINASSIGN4 = 0xFF080B09;
            break;
        case 0x8121:
            nodeId = 13;
            LPC_SWM->PINASSIGN0 = 0xFFFF0004;
            // A not working, but B is fine
            // eb20soic A: sck 14, ssel 15, miso 12, mosi 13, irq 8
            //LPC_SWM->PINASSIGN3 = 0x0EFFFFFF; 
            //LPC_SWM->PINASSIGN4 = 0xFF0F0C0D;
            // eb20soic B: sck 12, ssel 13, miso 15, mosi 14, irq 8
            LPC_SWM->PINASSIGN3 = 0x0CFFFFFF; 
            LPC_SWM->PINASSIGN4 = 0xFF0D0F0E;
            break;
        case 0x8122:
            nodeId = 14;
            LPC_SWM->PINASSIGN0 = 0xFFFF0106;
            // ea812: sck 12, ssel 13, miso 15, mosi 14
            LPC_SWM->PINASSIGN3 = 0x0CFFFFFF; 
            LPC_SWM->PINASSIGN4 = 0xFF0D0F0E;
            break;
        case 0x8241:
            nodeId = 23;
            // ea824: sck 24, ssel 15, miso 25, mosi 26
            LPC_SWM->PINASSIGN0 = 0xFFFF1207;
            LPC_SWM->PINASSIGN3 = 0x18FFFFFF; 
            LPC_SWM->PINASSIGN4 = 0xFF0F191A;
            break;
        case 0x8242:
            nodeId = 24;
            // jnp v3: sck 17, ssel 23, miso 9, mosi 8, irq 1
            LPC_SWM->PINASSIGN0 = 0xFFFF0004;
            LPC_SWM->PINASSIGN3 = 0x11FFFFFF; 
            LPC_SWM->PINASSIGN4 = 0xFF170908;
            break;
    }

    uart0Init(115200);
    for (int i = 0; i < 10000; ++i) __ASM("");
    printf("\n[rf_ping] dev %x node %d\n", devId, nodeId);

    rf.init(nodeId, 42, 8683);
    //rf.encrypt("mysecret");
    rf.txPower(0); // 0 = min .. 31 = max

    uint16_t seq = 0, cnt = 0;

    while (true) {
        if (++cnt == 0) {
            const int TXLEN = 1; // can be set to anything from 1 to 65
            static uint8_t txBuf[TXLEN];
            printf(" > %d\n", ++txBuf[0]);
            rf.send(0, txBuf, sizeof txBuf);
        }

        int len = rf.receive(rxBuf, sizeof rxBuf);
        if (len >= 0) {
            printf("OK ");
            for (int i = 0; i < len; ++i)
                printf("%02x", rxBuf[i]);
            printf(" (%d%s%d:%d)\n",
                    rf.rssi, rf.afc < 0 ? "" : "+", rf.afc, rf.lna);
        }

        chThdYield() 
    }
}
