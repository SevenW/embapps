// Report received data on the serial port.
#define chThdYield() // FIXME still used in rf69.h

#include "LPC8xx.h"
#include "uart.h"
#include <stdio.h>
#include "spi.h"
#include "rf69.h"
#include "radio-ook.h"
#include "decodeOOK_TEST.h"

//configuration items
uint8_t DIO2 = 15; //GPIO pin DIO2(=DATA)

//#define FREQ_BAND 433
#define FREQ_BAND 868

uint8_t fixthd = 60;
//uint32_t frqkHz = 433920;
uint32_t frqkHz = 868400;
//uint32_t bitrate = 300000;
uint32_t bitrate = 32768;
uint8_t bw = 16; //0=250kHz, 8=200kHz, 16=167kHz, 1=125kHz, 9=100kHz, 17=83kHz 2=63kHz, 10=50kHz

const uint8_t tsample = 25; //us samples
uint32_t samplesSec = 1000000/tsample;


volatile uint32_t sampleTicks = 0;
extern "C" void SysTick_Handler(void) {
    sampleTicks++;
}

RF69A<SpiDev0> rfa;

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
//WH1080DecoderV2 WH1080;
WH1080DecoderV2a WH1080a;
//VisonicDecoder viso;
EMxDecoder emx;
KSxDecoder ksx;
FSxDecoder fsx;
FSxDecoderA fsxa;
//
#endif

void reportOOK (const char* s, class DecodeOOK& decoder) {
    uint8_t pos;
    const uint8_t* data = decoder.getData(pos);
    printf("%s ", s);
    for (uint8_t i = 0; i < pos; ++i) {
        printf("%02x", data[i]);
    }
    printf("\r\n");
    decoder.resetDecoder();
}

void processBit(uint16_t pulse_dur, uint8_t signal) {
//	if (WH1080a.nextPulse(pulse_dur, signal)) {
//		reportOOK("WH1080a", WH1080a);
//		//set scope trigger
//		//palWritePad(GPIOB, 4, 1);
//	}
    if (fsx.nextPulse(pulse_dur, signal)) {
        reportOOK("FS20   ", fsx);
        //set scope trigger
        //palWritePad(GPIOB, 4, 1);
    }
    if (fsxa.nextPulse(pulse_dur, signal)) {
        reportOOK("FS20A   ", fsxa);
        //set scope trigger
        //palWritePad(GPIOB, 4, 1);
    }
//    if (emx.nextPulse(pulse_dur, signal)) {
//        reportOOK("EMx   ", emx);
//        //set scope trigger
//        //palWritePad(GPIOB, 4, 1);
//    }
//    if (ksx.nextPulse(pulse_dur, signal)) {
//        reportOOK("KSx   ", ksx);
//        //set scope trigger
//        //palWritePad(GPIOB, 4, 1);
//    }
}

void receiveOOK() {
    //moving average buffer
    //const uint8_t avg_len = 7;
    const uint8_t avg_len = 1;
    uint32_t filter = 0;
    uint32_t sig_mask = (1 << avg_len) - 1;

    //Fixed threshold
    rfa.OOKthdMode(0x40); //0x00=fix, 0x40=peak, 0x80=avg
    rfa.setBitrate(bitrate);

    rfa.setFrequency(frqkHz);
    rfa.setBW(bw);
    rfa.setThd(fixthd);
    //rfa.readAllRegs();
    uint8_t t_step = 1;
    uint32_t soon = sampleTicks + t_step;

    uint32_t nrssi = 0;
    uint32_t sumrssi = 0;
    uint32_t sumsqrssi = 0;
    uint32_t rssivar = 0;
    uint32_t rssimax = 0;

    uint32_t new_edge = sampleTicks;
    uint32_t last_edge = new_edge;
    uint8_t last_data = LPC_GPIO_PORT->B0[DIO2];

    uint16_t flush_cnt = 0;
    uint16_t flush_max = 10000 / tsample; //10ms

    uint16_t flip_cnt=0;

    uint32_t ts_rssi = sampleTicks;
    uint8_t rssi = 48;
    uint32_t thdUpd = sampleTicks;
    uint32_t thdUpdCnt = 0;
    while (true) {
        //evaluate RSSI every millisecond
        rssi = ~rfa.readRSSI();
        if ((sampleTicks - ts_rssi) > (1000/tsample)) {
            //rssi = ~rfa.readRSSI();
            sumrssi += rssi;
            sumsqrssi += rssi*rssi;
            nrssi++;
            ts_rssi = sampleTicks;
            if (rssi > rssimax) rssimax=rssi;
        }

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

        uint32_t ts_thdUpdNow = sampleTicks;

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

        if (ts_thdUpdNow - thdUpd >=10*samplesSec) {
            //rssivar = ((sumsqrssi - sumrssi*sumrssi/nrssi) / (nrssi-1)); //64 bits
            rssivar = ((sumsqrssi - ((sumrssi>>8)*((sumrssi<<8)/nrssi))) / (nrssi-1)); //32bits n<65000
            uint32_t rssiavg = sumrssi/nrssi;
            //determine stddev (no sqrt avaialble?)
            uint8_t stddev = 1;
            while (stddev < 10) {
                //safe until sqr(15)
                if (stddev*stddev >= rssivar) break;
                stddev++;
            }
            //printf("n=%d, sum=%d, sumsqr=%d\n", nrssi, sumrssi, sumsqrssi);
            //adapt rssi thd to noise level if variance is low
            if (rssivar < 36) {
                uint8_t delta_thd = 3*stddev;
                if (delta_thd < 6)  delta_thd = 6;
                if (fixthd != rssiavg+delta_thd) {
                    fixthd = rssiavg+delta_thd;
                    //if (fixthd < 70) fixthd = 70;
                    rfa.setThd(fixthd);
                    //printf("THD:%3d\r\n", fixthd);
                } else {
                    //printf("THD:keep\r\n");
                }
            } else {
                //printf("THD:keep\r\n");
            }

            printf("RSSI: %3d(v%3d-s%2d-m%3d) THD:%3d\r\n", rssiavg, rssivar, stddev, rssimax, fixthd);
            printf("%d polls took %d systicks = %dus - flips = %d\r\n", thdUpdCnt, ts_thdUpdNow - thdUpd, (tsample*(ts_thdUpdNow - thdUpd))/thdUpdCnt, flip_cnt);

            nrssi = sumrssi = sumsqrssi = rssimax = 0;
            thdUpd = ts_thdUpdNow;
            thdUpdCnt = 0;

            flip_cnt=0;

            //clear scope trigger
            //palWritePad(GPIOB, 4, 0);
        }
        thdUpdCnt++;

        if (rssi > 120) {
            //set scope trigger
            //palWritePad(GPIOB, 4, 1);
        }

        //sleep at resolutions higher then system tick
        while((soon-sampleTicks > 0) && !((soon-sampleTicks) & 0x80000000)) {
            //nop
        }
        soon = sampleTicks + t_step;
    }
    return;
}

static void setMaxSpeed () {
    LPC_SYSCON->SYSPLLCLKSEL = SYSPLLCLKSEL_Val;    // select PLL input
    LPC_SYSCON->SYSPLLCLKUEN = 0x01;                // update clock source
    while (!(LPC_SYSCON->SYSPLLCLKUEN & 0x1)) ;     // wait until updated

    LPC_SYSCON->SYSPLLCTRL = SYSPLLCTRL_Val;        // main clock is PLL out
    LPC_SYSCON->PDRUNCFG &= ~(1<<7);                // power-up SYSPLL
    while (!(LPC_SYSCON->SYSPLLSTAT & 0x1)) ;       // wait until PLL locked

    LPC_SYSCON->MAINCLKSEL = MAINCLKSEL_Val;        // select PLL clock output
    LPC_SYSCON->MAINCLKUEN = 0x01;                  // update MCLK clock source
    while (!(LPC_SYSCON->MAINCLKUEN & 0x1)) ;       // wait until updated

    LPC_SYSCON->SYSAHBCLKDIV = SYSAHBCLKDIV_Val;
}


int main () {
    for (int i = 0; i <3000000; ++i) __ASM("");
    //setMaxSpeed();
    //SystemCoreClockUpdate();

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
        LPC_SWM->PINENABLE0 = 0xffffffff;
        // lpc810: sck=0p8, ssel=1p5, miso=2p4, mosi=5p1, dio2=3p3, tx=4p2
        //SPI0
		LPC_SWM->PINASSIGN3 = 0x00FFFFFF;   // sck  -    -    -
        LPC_SWM->PINASSIGN4 = 0xFF010205;   // -    nss  miso mosi
        //USART0
        LPC_SWM->PINASSIGN0 = 0xffffff04;
        DIO2 = 3;
        break;
    case 0x8120:
        nodeId = 12;
        LPC_SWM->PINASSIGN0 = 0xFFFF0004;
        // jnp v2: sck 6, ssel 8, miso 11, mosi 9, irq 10, dio2 1p9
        LPC_SWM->PINASSIGN3 = 0x06FFFFFF;
        LPC_SWM->PINASSIGN4 = 0xFF080B09;
        DIO2 = 1;
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
        // jnp v3: sck 17, ssel 23, miso 9, mosi 8, irq 1, dio2 15p11
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
    printf("CoreClk %d, MainClk %d\n\n", SystemCoreClock, SystemCoreClock*LPC_SYSCON->SYSAHBCLKDIV);

    rfa.init(nodeId, 42, 868400);

    while(true) {
        receiveOOK();
    }
}
