// Report received data on the serial port.

#include "LPC8xx.h"
#include "uart.h"
#include <stdio.h>

#define chThdYield() // FIXME still used in rf69.h

volatile uint32_t sampleTicks = 0;

extern "C" void SysTick_Handler(void) {
    sampleTicks++;
}

#include "spi.h"
#include "rf69.h"
#include "radio-ook.h"
#include "decodeOOK_TEST.h"

uint8_t DIO2 = 15; //GPIO pin DIO2(=DATA)
RF69A<SpiDevice> rfa;

//#define FREQ_BAND 433
#define FREQ_BAND 868

const uint8_t tsample = 25; //25 us samples
uint32_t samplesSec = 1000000/tsample;
//uint32_t samplesSec = 1000000/tsample;
static volatile uint8_t fixthd = 80;
//uint32_t frqkHz = 433920;
uint32_t frqkHz = 868400;
uint32_t bitrate = 2000;
uint8_t bw = 16; //0=250kHz, 8=200kHz, 16=167kHz, 1=125kHz, 9=100kHz, 17=83kHz 2=63kHz, 10=50kHz


#if FREQ_BAND == 433
//433MHz
#include "decoders433.h"
WS249 ws249;
Philips phi;
OregonDecoderV1 orscV1;
//OregonDecoderV2 orscV2;
//OregonDecoderV3 orscV3;
//CrestaDecoder cres;
KakuDecoder kaku;
//KakuADecoder kakuA;
//XrfDecoder xrf;
//HezDecoder hez;
#else
//868MHz
#include "decoders868.h"
WH1080DecoderV2 WH1080;
WH1080DecoderV2a WH1080a;
//VisonicDecoder viso;
//EMxDecoder emx;
//KSxDecoder ksx;
FSxDecoder fsx;
//
#endif

void reportOOK (const char* s, class DecodeOOK& decoder) {
    uint8_t pos;
    const uint8_t* data = decoder.getData(pos);
    //Serial.println("");
    //Serial.print(hour());
    //printDigits(minute());
    //printDigits(second());
    //Serial.print(" ");
    //int8_t textbuf[25];
    //rf12_settings_text(textbuf);
    //Serial.print(textbuf);
    //Serial.print(' ');
    printf("%s ", s);
    //Serial.print(' ');
    for (uint8_t i = 0; i < pos; ++i) {
        printf("%02x", data[i]);
    }
    printf("\r\n");

    decoder.resetDecoder();
}

void processBit(uint16_t pulse_dur, uint8_t signal) {
//    if (WH1080.nextPulse(pulse_dur, signal)) {
//        reportOOK("WH1080 ", WH1080);
//        //set scope trigger
//        //palWritePad(GPIOB, 4, 1);
//    }
//    if (WH1080a.nextPulse(pulse_dur, signal)) {
//        reportOOK("WH1080a", WH1080a);
//        //set scope trigger
//        //palWritePad(GPIOB, 4, 1);
//    }
    if (fsx.nextPulse(pulse_dur, signal)) {
        reportOOK("FS20   ", fsx);
        //set scope trigger
        //palWritePad(GPIOB, 4, 1);
    }
}

void receiveOOK() {
    //printf("Enter receiveOOK at %d\n", sampleTicks);

    //moving average buffer
    const uint8_t avg_len = 5;
    uint32_t filter = 0;
    uint32_t sig_mask = (1 << avg_len) - 1;

    //Experimental: Fixed threshold
    rfa.OOKthdMode(0x00); //0x00=fix, 0x40=peak, 0x80=avg

    rfa.setFrequency(frqkHz);
    rfa.setBW(bw);
    rfa.setThd(fixthd);
    //rfa.readAllRegs();
    uint8_t t_step = 1;
    //uint32_t now = sampleTicks;
    uint32_t soon = sampleTicks + t_step;

    uint32_t nrssi = 0;
    //uint64_t sumrssi = 0LL;
    //uint64_t sumsqrssi = 0LL;
    uint32_t sumrssi = 0;
    uint32_t sumsqrssi = 0;
    uint32_t rssivar = 0;

    uint32_t new_edge = sampleTicks;
    uint32_t last_edge = new_edge;
    uint8_t last_data = LPC_GPIO_PORT->B0[DIO2];

    uint16_t flush_cnt = 0;
    uint16_t flush_max = 10000 / tsample; //10ms

    uint16_t flip_cnt=0;

    //assume system tick = 1ms (#define CH_CFG_ST_FREQUENCY 1000)
    uint32_t ts_print = sampleTicks;
    while (true) {
        //printf(" > %d\n", now);
        uint8_t rssi = ~rfa.readRSSI();
        uint8_t data_in = LPC_GPIO_PORT->B0[DIO2];
        filter = (filter << 1) | (data_in & 0x01);
        //efficient bit count, from literature
        uint8_t c = 0;
        uint32_t v = filter & sig_mask;
        for (c = 0; v; c++)
        {
            v &= v - 1; // clear the least significant bit set
        }
        uint8_t data_out = (c > (avg_len >> 1)); //bit count moving average
        //filtered DATA to scope
        //palWritePad(GPIOB, 5, (data_out & 0x01));

        uint32_t ts_printnow = sampleTicks;

        if (data_out != last_data) {
            new_edge = sampleTicks;
            processBit(tsample*(new_edge - last_edge), last_data);
            last_edge = new_edge;
            last_data = data_out;
            flush_cnt = 0;
            flip_cnt++;
        } else if (flush_cnt == flush_max /* && !data_out */) {
            //send fake pulse to notify end of transmission to decoders
            processBit(tsample*(sampleTicks - last_edge), last_data);
            processBit(1, !last_data);
        }
        flush_cnt++;

        sumrssi += rssi;
        sumsqrssi += rssi*rssi;
        nrssi++;
        if (ts_printnow - ts_print >=1*samplesSec) {
            //rssivar = ((sumsqrssi - sumrssi*sumrssi/nrssi) / (nrssi-1)); //64 bits
            //rssivar = ((sumsqrssi - sumrssi*(sumrssi/nrssi)) / (nrssi-1)); //32bits n<65000
            rssivar = ((sumsqrssi - ((sumrssi>>8)*((sumrssi<<8)/nrssi))) / (nrssi-1)); //32bits n<65000 more accurate
            uint32_t rssiavg = sumrssi/nrssi;
            //determine stddev (no sqrt avaialble?)
            uint8_t stddev = 1;
            while (stddev < 10) {
                //safe until sqr(15)
                if (stddev*stddev >= rssivar) break;
                stddev++;
            }
            //adapt rssi thd to noise level if variance is low
//            if (rssivar < 36) {
//                uint8_t delta_thd = 2*stddev;
//                if (delta_thd < 6)  delta_thd = 6;
//                //if (delta_thd < 10)  delta_thd = 10;
//                //const uint8_t delta_thd = 4;
//                if (fixthd != rssiavg+delta_thd) {
//                    fixthd = rssiavg+delta_thd;
//                    //if (fixthd < 70) fixthd = 70;
//                    rfa.setThd(fixthd);
//                    //printf("THD:%3d\r\n", fixthd);
//                } else {
//                    //printf("THD:keep\r\n");
//                }
//            } else {
//                //printf("THD:keep\r\n");
//                //printf("RSSI: %3d, %5d\r\n", rssiavg, rssivar);
//            }

            printf("RSSI: %3d(v%3d-s%2d) THD:%3d\r\n", rssiavg, rssivar, stddev, fixthd);
            printf("%d polls took %d systicks = %dus - flips = %d\r\n", nrssi, ts_printnow - ts_print, (tsample*(ts_printnow - ts_print))/nrssi, flip_cnt);

            nrssi = sumrssi = sumsqrssi = 0;
            ts_print = ts_printnow;

            flip_cnt=0;

            //clear scope trigger
            //palWritePad(GPIOB, 4, 0);
        }

        if (rssi > 120) {
            //set scope trigger
            //palWritePad(GPIOB, 4, 1);
        }

        //sleep at resolutions higher then system tick
        while((soon-sampleTicks > 0) && (soon-sampleTicks <= t_step)) {
            //nop
        }
        soon = sampleTicks + t_step;
    }
    return;
}


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
        // lpc810 coin: sck=0, ssel=1, miso=2, mosi=5
        LPC_SWM->PINASSIGN3 = 0x00FFFFFF;   // sck  -    -    -
        LPC_SWM->PINASSIGN4 = 0xFF010205;   // -    nss  miso mosi
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
        // jnp v3: sck 17, ssel 23, miso 9, mosi 8, irq 1, dio2 15
        LPC_SWM->PINASSIGN0 = 0xFFFF0004;
        LPC_SWM->PINASSIGN3 = 0x11FFFFFF;
        LPC_SWM->PINASSIGN4 = 0xFF170908;
        DIO2 = 15;
        break;
    }

    SysTick_Config(SystemCoreClock / (1000000/tsample)); //tsample=25us

    uart0Init(115200);
    for (int i = 0; i < 10000; ++i) __ASM("");
    printf("\n[rf_ook] dev %x node %d\n", devId, nodeId);

    rfa.init(nodeId, 42, 868400);

    while(true) {
        receiveOOK();
    }
}
