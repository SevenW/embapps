#define FREQ_BAND 433
//#define FREQ_BAND 868

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

BaseSequentialStream *serial = (BaseSequentialStream*)&SD2;

static volatile int icnt = 0;
static volatile uint8_t fixthd = 0x10;
static volatile int maxdur = 0;
static volatile bool data_on = false;

#include "spi.h"
#include "../common/radio.h"
#include "radio-ook.h"

RF69A<SpiDevice> rf;

//TEMP for test
uint32_t frqkHz = 433920;
uint32_t bitrate = 1000;
uint8_t bw = 0; //0=250kHz, 9=100kHz
uint16_t cycle = 0;



#include "decodeOOK.h"

#if FREQ_BAND == 433
    //433MHz
    #include "decoders433.h"
    //WS249 ws249;
    Philips phi;
    OregonDecoderV1 orscV1;
    OregonDecoderV1 orscV1avg;
    //OregonDecoderV2 orscV2;
    //OregonDecoderV3 orscV3;
    //CrestaDecoder cres;
    //KakuDecoder kaku;
    //KakuADecoder kakuA;
    //XrfDecoder xrf;
    //HezDecoder hez;
#else
    //868MHz
    #include "decoders868.h"
    //VisonicDecoder viso;
    //EMxDecoder emx;
    //KSxDecoder ksx;
    //FSxDecoder fsx;
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
  chprintf(serial, "%s ", s);
  //Serial.print(' ');
  for (uint8_t i = 0; i < pos; ++i) {
    chprintf(serial, "%02x", data[i]);
  }
  chprintf(serial, "\r\n");

  decoder.resetDecoder();
}

bool processBit(uint16_t dur) {
    bool decoded = false;
    if (orscV1.nextPulse(dur)) {
        decoded = true;
        chprintf(serial, "\r\nf=%3d.%3dkHz BW-idx=%2d, OOKTHd = %d, run %d - ", frqkHz/1000, frqkHz%1000, bw, fixthd, cycle);
        reportOOK("ORSCV1 ", orscV1);
    }
    if (orscV1avg.nextPulse(dur)) {
        decoded = true;
        chprintf(serial, "\r\nf=%3d.%3dkHz BW-idx=%2d, OOKTHd = %d, run %d - ", frqkHz/1000, frqkHz%1000, bw, fixthd, cycle);
        reportOOK("ORSCV1a", orscV1avg);
    }
	//if (orscV2.nextPulse(rssi_diff))
	//reportOOK("orscV2", orscV2);
//    if (kaku.nextPulse(rssi_diff))
//        reportOOK("kaku", kaku);
//    if (kakuA.nextPulse(rssi_diff))
//        reportOOK("kaku-A", kakuA);
//	if (cres.nextPulse(rssi_diff))
//	reportOOK("cres", cres);
	//if (wh1080v4.nextPulse(rssi_diff, prev_rssi))
	//  reportOOK("WH1080V4", wh1080v4);
	// if (ws249.nextPulse(rssi_diff, prev_rssi))
	// reportOOK("WS249", ws249);
//    if (phi.nextPulse(rssi_diff, prev_rssi))
//        reportOOK("phi", phi);
    return decoded;
}


static bool clear = false;
bool vtarmed = false;

//circular buffer
const uint16_t cbufsize = 15000;
static uint8_t cbuf[cbufsize];
//static int16_t cbuf2[cbufsize];
static uint16_t cbufin = 0;
static uint16_t cbufout = 0;

//moving average buffer
static uint32_t filter = 0;
static uint8_t avg_len = 11;
static uint32_t sig_mask = (1 << avg_len) - 1;


static THD_WORKING_AREA(waRadio, 512);

bool pollRSSI(uint16_t dur, uint16_t tsample, uint16_t lead) {
    rtcnt_t t_step = US2RTC(STM32_SYSCLK, tsample);
    rtcnt_t now = chSysGetRealtimeCounterX();
    rtcnt_t soon = now + t_step;
    uint32_t nsamples = 1000 * dur / tsample;
    uint32_t nlead = 1000 * lead / tsample;
    uint32_t nstop = nsamples;
    bool trig = false;
    uint8_t rssiThd = 120;
    bool poll = true;

    uint64_t sumrssi = 0;
    uint64_t sumsqrssi = 0;
    uint32_t rssivar = 0;
    uint8_t prevthd = fixthd;

    rtcnt_t new_edge = now;
    rtcnt_t last_edge = now;
    rtcnt_t last_edge1 = now;
    rtcnt_t last_edge2 = now;
    uint8_t last_data = palReadPad(GPIOA, 9);
    uint8_t last_data1 = palReadPad(GPIOA, 9);
    uint8_t last_data2 = palReadPad(GPIOA, 9);

//    if (cycle & 2) {
//        avg_len = 7;
//        sig_mask = 0x7F;
//    } else {
//        avg_len = 3;
//        sig_mask = 0x07;
//    }

//    avg_len = ((cycle & 7) << 1) + 1; //1, 3, 5, 7, 9, 11, 13, 15
//    sig_mask = (1 << avg_len) - 1;


    uint32_t pcnt = 0;
    //assume system tick = 1ms (#define CH_CFG_ST_FREQUENCY 1000)
    systime_t nowms = chVTGetSystemTime();
    systime_t ts_print = nowms;
    while (poll) {
        if (clear) {
            break;
        }
        
        uint8_t rssi = ~rf.readRSSI();
//        //only read rssi in odd cycles to compare impact of SPI noise
//        uint8_t rssi = 60;
//        if (cycle & 1) rssi = ~rf.readRSSI();

        uint8_t data = palReadPad(GPIOA, 9);

        filter = (filter << 1) | (last_data & 0x01);
        uint8_t c = 0;
        uint32_t v = filter & sig_mask;
//        for (c = 0; v; c++)
//        {
//          v &= v - 1; // clear the least significant bit set
//        }
        for (c = 0; v; c++)
        {
          v &= v - 1; // clear the least significant bit set
        }
        uint8_t data1 = (c > (avg_len >> 1)); //bit count moving average
        //uint8_t data2 = ((signal & (1 << (avg_len -1))) ? 1 : 0); //n bits delayed

        systime_t ts_printnow = chVTGetSystemTime();

        bool decoded = false;
        if (data != last_data) {
            new_edge = chSysGetRealtimeCounterX();
            //decoded = processBit(RTC2US(STM32_SYSCLK, new_edge - last_edge));
            last_edge = new_edge;
            last_data = data;
        }
        if (data1 != last_data1) {
            new_edge = chSysGetRealtimeCounterX();
            if (orscV1avg.nextPulse(RTC2US(STM32_SYSCLK, new_edge - last_edge1))) {
                decoded = true;
                chprintf(serial, "ts=%d, f=%3d.%3dkHz BW-idx=%2d, BR= %d, OOKTHd = %d, Av= %2d, run %d - ", ts_printnow, frqkHz/1000, frqkHz%1000, bw, bitrate, fixthd, avg_len, cycle);
                reportOOK("ORSCV1avg", orscV1avg);
            }
            last_edge1 = new_edge;
            last_data1 = data1;
        }
//        if (data2 != last_data2) {
//            new_edge = chSysGetRealtimeCounterX();
//            if (orscV1.nextPulse(RTC2US(STM32_SYSCLK, new_edge - last_edge2))) {
//                decoded = true;
//                chprintf(serial, "ts=%d, f=%3d.%3dkHz BW-idx=%2d, BR= %d, OOKTHd = %d, run %d - ", ts_printnow, frqkHz/1000, frqkHz%1000, bw, bitrate, fixthd, cycle);
//                reportOOK("ORSCV1del", orscV1);
//            }
//            last_edge2 = new_edge;
//            last_data2 = data2;
//        }


        //use bit 0 to encode DATA signal
        (data ? rssi |= 0x01 : rssi &= 0xFE);
        ////use bit 0 as time counter 256ms period
        //(chVTGetSystemTime() & 0x80 ? rssi &= 0xFE : rssi |= 0x01);

        //int16_t fei = (cbufin & 1) ? rf.readFEI(false) : rf.readAFC();
        //cbuf2[cbufin] = fei;

        cbuf[cbufin++] = rssi;
        sumrssi += rssi;
        sumsqrssi += rssi*rssi;
        if (cbufin >= cbufsize) cbufin=0;
        //cbuf overflow
        if (cbufin == cbufout) {
            sumrssi -= cbuf[cbufout];
            sumsqrssi -= cbuf[cbufout]*cbuf[cbufout];
            cbufout++;
            if (cbufout >= cbufsize) cbufout=0;
        }

        if (cbufin == 0) {
            uint16_t n = cbufsize-1;
            /*uint32_t*/ rssivar = ((sumsqrssi - sumrssi*sumrssi/n) / (n-1));
            uint32_t rssiavg = sumrssi/n;
            //determine stddev (no sqrt avaialble?)
            uint8_t stddev = 1;
            while (stddev < 10) {
                //safe until sqr(15)
                if (stddev*stddev >= rssivar) break;
                stddev++;
            }
            //adapt rssi thd to noise level if variance is low
//            if (rssivar < 36) {
//                uint8_t delta_thd = 3*stddev;
//                if (delta_thd < 6)  delta_thd = 5;
//                //const uint8_t delta_thd = 4;
//                if (fixthd != rssiavg+delta_thd) {
//                    fixthd = rssiavg+delta_thd;
//                    //if (fixthd < 70) fixthd = 70;
//                    rf.setThd(fixthd);
//                    //chprintf(serial, "THD:%3d\r\n", fixthd);
//                } else {
//                    //chprintf(serial, "THD:keep\r\n");
//                }
//            } else {
//                //chprintf(serial, "THD:keep\r\n");
//                //chprintf(serial, "RSSI: %3d, %5d\r\n", rssiavg, rssivar);
//            }
            //systime_t ts_printnow = chVTGetSystemTime();
            if (ts_printnow - ts_print >5000) {
                ts_print = ts_printnow;
                chprintf(serial, "RSSI: %3d(v%3d) THD:%3d\r\n", rssiavg, rssivar, fixthd);
            }
            prevthd = fixthd;
        }

        //if (!trig && rssivar > 700) {
        //if (!trig && rssi > rssiThd) {
        if (!trig && decoded) {
            trig = true;
            nstop = pcnt + cbufsize - nlead;
            //rf.readFEI(true);
        }
        pcnt++;

        //1-byte print: tsample>=100us
        //print binary (one byte)
        //chSequentialStreamPut(serial, rssi);
        //1-byte printable only (clip very strong signals) (0xFF = EOF)
        //chSequentialStreamPut(serial, rssi < 0xDF ? 0x20+rssi : 0xFE);
        //print HEX
        //chprintf(serial, "%02x", rssi);
        //print DEC
        //chprintf(serial, "%3d\r\n", rssi);
        
        //sleep at resolutions higher then system tick
        now = chSysGetRealtimeCounterX();
        rtcnt_t sleep = soon - now;
        if (sleep<t_step && sleep > 0) {
            chSysPolledDelayX(sleep);
        }
        soon += t_step;

        poll = (pcnt<nsamples && pcnt<nstop);

    }
    //chprintf(serial, "%d polls took %d ms (systicks)\r\n", pcnt, chVTGetSystemTime() - nowms);
    //chprintf(serial, "wrap at %d, prev THD %d, last THD %d\r\n", (cbufsize-cbufin)*tsample, prevthd, fixthd);
    return trig;
}

void printRSSIBuffer(bool details, uint16_t tsample) {
    uint8_t rssiMin = 255;
    uint8_t rssiMax = 0;
    uint64_t rssiAvg = 0;
    uint64_t rssiVar = 0;
    uint16_t n = 0;

    while (cbufout != cbufin) {
        //int16_t fei = cbuf2[cbufout];
        uint8_t rssi = cbuf[cbufout++];
        cbufout %= cbufsize;
        //if (details) chprintf(serial, "%7d, %3d, %d\r\n", tsample*n, rssi, (rssi & 0x01 ? 40 : 0)/*, fei*61*/);
        if (rssi < rssiMin) rssiMin = rssi;
        if (rssi > rssiMax) rssiMax = rssi;
        rssiAvg += rssi;
        rssiVar += rssi*rssi;
        n++;
    };
    rssiVar = ((rssiVar - rssiAvg*rssiAvg/n) / (n-1));
    rssiAvg /= n;
    chprintf(serial, "avg=%d, var=%d, min=%3d, max=%3d, n=%d\r\n", (uint32_t)rssiAvg, (uint32_t)rssiVar, rssiMin, rssiMax, n);
    //chprintf(serial, "%3d, %6d, %3d, %3d, ", (uint32_t)rssiAvg, (uint32_t)rssiVar, rssiMin, rssiMax);
}

//Sleep resolution is SystemTick configured by CH_CFG_ST_FREQUENCY (1ms)
//void pollRSSI() {
//  systime_t t_step = US2ST(400);
//  systime_t now = chVTGetSystemTime();
//  systime_t soon = now + t_step;
//  while (true) {
//    if (clear) {
//      return;
//    }
//    uint8_t rssi = rf.readRSSI();
//    chprintf(serial, "%02x\r\n", rssi);
//    chThdSleepUntilWindowed(now, soon);
//    now = soon;
//    soon += t_step;
//  }
//}

void sweep() {
    const uint16_t tsample = 50;
    rtcnt_t t_step = US2RTC(STM32_SYSCLK, tsample);
    rtcnt_t now = chSysGetRealtimeCounterX();
    rtcnt_t soon = now + t_step;

    rf.DataModule(0x60); //setup FSK
    //frqkHz = 433920;
    frqkHz = 868400;
    //bw=12; //25kHz FSK
    //bw=13; //12.5kHz FSK
    //bw=14; //6.25kHz FSK
    bw=15; //3.13kHz FSK

    //NOTE: BitRate < 2* RxBw !
    bitrate = 6000;
    rf.setFrequency(frqkHz);
    rf.setBW(bw);
    rf.setBitrate(bitrate);
    rf.readAllRegs();
    while(1) { //scan forever
        //for (frqkHz = 433400; frqkHz < 434200; frqkHz += 10) {
        //for (frqkHz = 867800; frqkHz < 869000; frqkHz += 10) {
        for (frqkHz = 868200; frqkHz < 868450; frqkHz += 3) {
            rf.setFrequency(frqkHz);
            //wait to settle PLL.
            chSysPolledDelayX(t_step);
            uint8_t rssi = ~rf.readRSSI();
            chprintf(serial, "%d,", rssi);

//            //sleep at resolutions higher then system tick
//            now = chSysGetRealtimeCounterX();
//            rtcnt_t sleep = soon - now;
//            if (sleep<t_step && sleep > 0) {
//                chSysPolledDelayX(sleep);
//            }
//            soon += t_step;

        }
        chprintf(serial, "\r\n");
    }


    rf.DataModule(0x68); //setup OOK
}


static THD_FUNCTION(Radio, /*arg*/) {

    sweep();
    //end it never returns

    //uint32_t frqkHz = 433920;
    frqkHz = 433920;
    //uint8_t bw = 0; //0=250kHz, 9=100kHz
    bw=12;
    bitrate = 1000;
    fixthd = 70;

    const uint16_t dur =40000; //duration of scan in ms
    const uint16_t sample = 25; //sample interval in us
    const uint16_t lead = 125; //buffered data before trigger in ms.
    rf.setFrequency(frqkHz);
    rf.setBW(bw);
    rf.readAllRegs();
    //uint16_t cycle = 0;
    rf.setThd(fixthd);
    while(1) {
        //for (bw=8; bw<15; bw++) {
        for (fixthd=65; fixthd<96; fixthd+=5) {
            rf.setThd(fixthd);
            rf.setBW(bw);
            for (frqkHz = 433860; frqkHz < 433861; frqkHz += 2) {
            //for (frqkHz = 433600; frqkHz < 434200; frqkHz += 10) {
            //for (frqkHz = 432800; frqkHz < 435000; frqkHz += 100) {
                rf.setFrequency(frqkHz);
              for (uint8_t j=0; j<2; j++) {
                if (cycle & 1) {
                    //bitrate=2000;
                    rf.OOKthdMode(0x40); //peak mode
                } else {
                    //bitrate=500;
                    rf.OOKthdMode(0x00); //fixed mode
                }
                rf.setBitrate(bitrate);
                //rf.setThd(fixthd);
                chprintf(serial, "\r\nScanning f=%3d.%3dkHz BW-idx=%2d, BR=%d, OOKTHd = %d, run %d\r\n", frqkHz/1000, frqkHz%1000, bw, bitrate, fixthd, cycle);
                //chprintf(serial, "%d, %d, ", bw, frqkHz);
                //for (uint8_t i = 0; i < 3; i++) {
                    //fixthd = 70 + ((2*cycle) & 0x1F);
                    //chprintf(serial, "\r\nScanning f=%3d.%3dkHz OOKTHd = %d, run %d\r\n", frqkHz/1000, frqkHz%1000, fixthd, cycle);
                    bool found = pollRSSI(dur, sample, lead);
                    //bool found = pollData(dur, sample, lead);
                    printRSSIBuffer(found, sample);
                //}
                chprintf(serial, "\r\n");
                //chprintf(serial, "AFC=%dHz, FEI=%dHz\r\n", rf.readAFC()*61, rf.readFEI(false)*61);
                cycle++;
              }//j loop
            }//frq loop
        }//fixthd loop
    }// while forever
    return 0;
}

//static virtual_timer_t vt;
//static int status = 0;

const uint16_t binsize_us = 10;
const uint16_t histsize = 700;
static uint16_t hist_0[histsize];
static uint16_t hist_1[histsize];
static uint16_t hist_0s[histsize];
static uint16_t hist_1s[histsize];


static thread_t *aStp = NULL;
static THD_WORKING_AREA(waAS, 512);

static THD_FUNCTION(afterSync, /*arg*/) {
    //void afterSync(void *p) {
    bool decoded = false;

    frqkHz = 433860;

    //uint8_t bw = 0; //0=250kHz, 9=100kHz
    bw=13;
    cycle = 0;

    //const uint16_t dur =40000; //duration of scan in ms
    //const uint16_t sample = 50; //sample interval in us
    //const uint16_t lead = 200; //buffered data before trigger in ms.
    rf.setFrequency(frqkHz);
    rf.setBW(bw);
    rf.readAllRegs();
    //uint16_t cycle = 0;
    fixthd = 70;
    rf.setThd(fixthd);


    while (TRUE) {
        msg_t msg;

        /* Waiting for the IRQ to happen.*/
        chSysLock();
        aStp = chThdGetSelfX();
        //aStp = chThdSelf();
        chThdSleepS(TIME_INFINITE);
        //chSchGoSleepS(THD_STATE_SUSPENDED);
        msg = chThdGetSelfX()->p_u.rdymsg; /* Retrieving the message, optional.*/
        chSysUnlock();
        /* Perform processing here.*/
        //status = readReg(REG_IRQFLAGS1);
        //print hist
        //for (int i = 0; i < 40; i++) {
        if (msg) {
            systime_t ts_printnow = chVTGetSystemTime();
            if (cycle <= 10) {
                chprintf(serial, "\r\nts=%d, f=%3d.%3dkHz BW-idx=%2d, OOKTHd = %d, run %d - ", ts_printnow, frqkHz/1000, frqkHz%1000, bw, fixthd, cycle);
                reportOOK("ORSCV1", orscV1);
            }

        }
        if (hist_0[histsize-1] < 30) cycle++;
        for (int i = 0; i < histsize; i++) {
            if (hist_0[histsize-1] < 30) {
                //chprintf(serial, "%d, %d\r\n", hist_0[i], hist_1[i]);
                if (cycle == 10) chprintf(serial, "%d, %d\r\n", hist_0s[i], hist_1s[i]);
                if (cycle ==0) {
                    hist_0s[i] = hist_0[i];
                    hist_1s[i] = hist_1[i];
                } else {
                    hist_0s[i] += hist_0[i];
                    hist_1s[i] += hist_1[i];
                }
            }
            hist_0[i] = hist_1[i] = 0;
        }
    }
    return 0;
}

//static THD_FUNCTION(afterSync, /*arg*/) {
//    //void afterSync(void *p) {
//    bool decoded = false;
//
//    frqkHz = 433860;
//    //uint8_t bw = 0; //0=250kHz, 9=100kHz
//    bw=13;
//
//    //const uint16_t dur =40000; //duration of scan in ms
//    //const uint16_t sample = 50; //sample interval in us
//    //const uint16_t lead = 200; //buffered data before trigger in ms.
//    rf.setFrequency(frqkHz);
//    rf.setBW(bw);
//    rf.readAllRegs();
//    //uint16_t cycle = 0;
//    fixthd = 70;
//    rf.setThd(fixthd);
//
//
//    while (TRUE) {
//        msg_t msg;
//
//        /* Waiting for the IRQ to happen.*/
//        chSysLock();
//        aStp = chThdGetSelfX();
//        //aStp = chThdSelf();
//        chThdSleepS(TIME_INFINITE);
//        //chSchGoSleepS(THD_STATE_SUSPENDED);
//        msg = chThdGetSelfX()->p_u.rdymsg; /* Retrieving the message, optional.*/
//        chSysUnlock();
//        /* Perform processing here.*/
//        //status = readReg(REG_IRQFLAGS1);
//        if (msg == 0) {
//            chprintf(serial, "RSSI\r\n");
//            maxdur = 0;
//            icnt = 0;
//        }
//        else {
//            bool on = msg & 0x80000000;
//            msg &= 0x7FFFFFFF;
//            if (msg > maxdur) {
//                maxdur = msg;
//            }
//            //(on ? chSequentialStreamPut(serial, (uint8_t)'|') : chSequentialStreamPut(serial, (uint8_t)'.'));
//            chprintf(serial, "%d,", msg);
//            //chprintf(serial, "dur: %d us\r\n", msg);
//            //chprintf(serial, "%02x %d%d %d %d\r\n", readReg(REG_RSSIVALUE), data_on, on, icnt, msg);
//            icnt = 0;
//
//            //new_edge = chSysGetRealtimeCounterX();
//            decoded = processBit(msg);
//
//        }
//    }
//    return 0;
//}

void clearRX(/*void *p*/) {

    chSysLockFromISR();
    //chVTSetI(&vt, MS2ST(1000), do_something, p);
    clear = true;
    chSysUnlockFromISR();

    /* Periodic code here.*/
}

rtcnt_t last_edge = (rtcnt_t)0;

/* Triggered by DIO3 = RSSI*/
static void DIO3int(EXTDriver *extp, expchannel_t channel) {

    (void)extp;
    (void)channel;
    //pressed = true;
    //  chSysLockFromISR();
    //  if (aStp != NULL) {
    //    aStp->p_u.rdymsg = (msg_t)0; /* 1 = SyncWord*/
    //    chSchReadyI(aStp);
    //    aStp = NULL;
    //  }
    //  //if (!vtarmed) {
    //  //  chVTSetI(&vt, MS2ST(200), clearRX, NULL);
    //  //  vtarmed = true;
    //  //}
    //  chSysUnlockFromISR();
    //  last_edge = chSysGetRealtimeCounterX();
}

uint32_t startms = RTC2MS(STM32_SYSCLK, chSysGetRealtimeCounterX());

/* Triggered by DIO2 = DATA*/
static void DIO2int(EXTDriver *extp, expchannel_t channel) {

    (void)extp;
    (void)channel;
    if (clear)
    return;
    rtcnt_t now = chSysGetRealtimeCounterX();
    chSysLockFromISR();
    //uint32_t on = palReadPad(GPIOB, 5)<<31;
    uint32_t on = palReadPad(GPIOA, 9)<<31;
    data_on = !on;

//    if (aStp != NULL) {
//        aStp->p_u.rdymsg = (msg_t)RTC2US(STM32_SYSCLK, now - last_edge) | on; /* Sending the message, optional.*/
//        chSchReadyI(aStp);
//        aStp = NULL;
//    }

    uint32_t nowms = RTC2MS(STM32_SYSCLK, now);
    uint32_t dur = RTC2US(STM32_SYSCLK, now - last_edge);
    if (orscV1.nextPulse(dur)) {
        bool decoded = true;
        if (aStp != NULL) {
            aStp->p_u.rdymsg = (msg_t)decoded; /* Sending the message, optional.*/
            chSchReadyI(aStp);
            aStp = NULL;
        }
        startms = nowms;
    }
//    if (nowms > startms+10000) {
//        if (aStp != NULL) {
//            aStp->p_u.rdymsg = (msg_t)false; /* Sending the message, optional.*/
//            chSchReadyI(aStp);
//            aStp = NULL;
//        }
//        startms = nowms;
//    }

    if (dur < 0xFFFFFFFF - binsize_us>>1) {
        dur += binsize_us>>1;
    }
    dur /= binsize_us;
    if (dur >= histsize) dur = histsize-1;
    if (data_on) {
        hist_1[dur]++;
    } else
    {
        hist_0[dur]++;
    }


    //if (!vtarmed) {
    //  chVTSetI(&vt, MS2ST(500), clearRX, NULL);
    //  vtarmed = true;
    //}
    last_edge = now;
    icnt++;
    chSysUnlockFromISR();
}

// static const EXTConfig extcfg = { {
// /*Px00*/{EXT_CH_MODE_DISABLED, NULL},
// /*Px01*/{EXT_CH_MODE_DISABLED, NULL},
// /*Px02*/{EXT_CH_MODE_DISABLED, NULL},
// /*Px03*/{EXT_CH_MODE_RISING_EDGE
// | EXT_CH_MODE_AUTOSTART
// | EXT_MODE_GPIOB,
// DIO3int}, //D3 = PB3 DIO3
// /*Px04*/{EXT_CH_MODE_DISABLED, NULL}, //{EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART | EXT_MODE_GPIOB, DIO1int}, //D5 = PB4 DIO1
// /*Px05*/{EXT_CH_MODE_BOTH_EDGES
// | EXT_CH_MODE_AUTOSTART
// | EXT_MODE_GPIOB,
// DIO2int}, //D4 = PB5 DIO2
// /*Px06*/{EXT_CH_MODE_DISABLED, NULL},
// /*Px07*/{EXT_CH_MODE_DISABLED, NULL}, //{EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART | EXT_MODE_GPIOC, DIO0int}, //D9 = PC7 DIO0
// /*Px08*/{EXT_CH_MODE_DISABLED, NULL}, //D7=PA8
// /*Px09*/{EXT_CH_MODE_DISABLED, NULL},
// /*Px10*/{EXT_CH_MODE_DISABLED, NULL}, //{EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART | EXT_MODE_GPIOA, DIO5int}, //D2 = PA10 DIO5
// /*Px11*/{EXT_CH_MODE_DISABLED, NULL},
// /*Px12*/{EXT_CH_MODE_DISABLED, NULL},
// /*Px13*/{EXT_CH_MODE_DISABLED, NULL}, //PC13 button
// /*Px14*/{EXT_CH_MODE_DISABLED, NULL},
// /*Px15*/{EXT_CH_MODE_DISABLED, NULL},
// {EXT_CH_MODE_DISABLED, NULL}, {
// EXT_CH_MODE_DISABLED, NULL},
// {EXT_CH_MODE_DISABLED, NULL}}};

static const EXTConfig extcfg = {{
        /*Px00*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px01*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px02*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px03*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px04*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px05*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px06*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px07*/{EXT_CH_MODE_DISABLED, NULL}, //{EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART | EXT_MODE_GPIOC, DIO0int}, //D9 = PC7 DIO0
        /*Px08*/{EXT_CH_MODE_DISABLED, NULL}, //D7=PA8
        /*Px09*/{EXT_CH_MODE_BOTH_EDGES
            | EXT_CH_MODE_AUTOSTART
            | EXT_MODE_GPIOA,
            DIO2int}, //D8 = PA9 DIO2
        /*Px10*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px11*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px12*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px13*/{EXT_CH_MODE_DISABLED, NULL}, //PC13 button
        /*Px14*/{EXT_CH_MODE_DISABLED, NULL},
        /*Px15*/{EXT_CH_MODE_DISABLED, NULL},
        /*Res1*/{EXT_CH_MODE_DISABLED, NULL},
        /*Res2*/{EXT_CH_MODE_DISABLED, NULL},
        /*Res3*/{EXT_CH_MODE_DISABLED, NULL}}};

int main() {
    halInit();
    chSysInit();

    // disable JTAG to free PB3, PB4
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_DISABLE;

    palSetPadMode(GPIOB, 3, PAL_MODE_INPUT);

    palSetPadMode(GPIOA, 9, PAL_MODE_INPUT); //DIO2 = DATA

    sdStart(&SD2, NULL);

    chprintf(serial, "\r\n[OOK]\r\n");
    chprintf(serial, "STM32_SYSCLK %d Hz\r\n", STM32_SYSCLK);
    chprintf(serial, "STM32_HSICLK %d Hz\r\n", STM32_HSICLK);

//      /*
//       * Activates the EXT driver 1 (External interrupts).
//       */
//      extStart(&EXTD1, &extcfg);

    rf.init(11, 42, 433920);

    //chThdCreateStatic(waAS, sizeof waAS, NORMALPRIO + 5, afterSync, 0);
    chThdCreateStatic(waRadio, sizeof waRadio, NORMALPRIO + 1, Radio, 0);

    while (true)
    chThdYield();

    return 0;
}
