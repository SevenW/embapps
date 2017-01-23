// Simple RF69 demo application.

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>

// defined in main.cpp
extern int serial_getc ();
extern uint32_t millis();

#include "spi.h"
#include "rf69.h"

RF69<SpiDev> rf;

uint8_t rxBuf[64];
uint8_t txBuf[62];
uint16_t txCnt = 0;

const int rf_freq = 8686;
const int rf_group = 6;
const int rf_nodeid = 61;

const bool verbose = true;

void setup () {
//    // LED on HyTiny F103 is PA1, LED on BluePill F103 is PC13
//    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
//            GPIO_CNF_OUTPUT_PUSHPULL, GPIO1);
//    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
//            GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

    //gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);   // rev1
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);    // rev3
    //gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8);    // rev4

    printf("\n[radio]\n");

    rf.init(rf_nodeid, rf_group, rf_freq);
    //rf.encrypt("mysecret");
    rf.txPower(16); // 0 = min .. 31 = max

    for (int i = 0; i < (int) sizeof txBuf; ++i)
        txBuf[i] = i;

    printf("  Enter 't' to broadcast a test packet as node %d.\n", rf_nodeid);
    printf("  Listening for packets on %.1f MHz, group %d ...\n\n",
            rf_freq * 0.1, rf_group);
}

void loop () {
    if (serial_getc() == 't') {
        printf("  Broadcasting %d-byte test packet\n", txCnt);
        rf.send(0, txBuf, txCnt);
        txCnt = (txCnt + 1) % sizeof txBuf;
    }

    int len = rf.receive(rxBuf, sizeof rxBuf);
    if (len >= 0) {
        printf("rf69 %04x%02x%02x%02x%04x%02x%02x%02x ",
                rf_freq, rf_group, rf.rssi, rf.lna, rf.afc,
                rxBuf[0], rxBuf[1], len - 2);
        for (int i = 2; i < len; ++i)
            printf("%02x", rxBuf[i]);
        const char* sep = rf.afc < 0 ? "" : "+";
        if (verbose)
            printf("  (%g%s%d:%d)", rf.rssi * 0.5, sep, rf.afc, rf.lna);
        putchar('\n');
    }
}
