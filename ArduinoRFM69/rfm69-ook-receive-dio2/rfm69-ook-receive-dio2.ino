#define RF69_COMPAT 1 // define this to use the RF69 driver i.s.o. RF12
#define STATLOG 0 //0=no statistics logging 1=statistics logging

#include <JeeLib.h>
//#include <Time.h>
#include "radio-ook.h"
#include "decodeOOK.h"
//#include "decodeOOK_TEST.h"

// Config items ------------------------------------------------------------
#define RF69_RX_DATA 3	//D3 = bit 3 in PORTD - JeeLink V3C + solder-bridge!
#define RF69_RX_EXTINT 1
#define RF69_RX_PIN PIND
#define RF69_RX_DDR DDRD

// Config items ------------------------------------------------------------
#define FREQ_BAND 868 //868 or 433
#define SERIAL_BAUD 57600

#if FREQ_BAND == 433
#define RF12_BAND RF12_433MHZ
uint32_t frqkHz = 433920;
#else
#define RF12_BAND RF12_868MHZ
uint32_t frqkHz = 868280;
#endif

const uint8_t max_decoders = 6; //Too many decoders slows processing down.
DecodeOOK* decoders[max_decoders] = {NULL};
uint8_t di = 0;
void printOOK (class DecodeOOK* decoder);

#if FREQ_BAND == 433
//433MHz
#include "decoders433.h"
//OregonDecoderV2   orscV2(  5, "ORSV2", printOOK);
//CrestaDecoder     cres(    6, "CRES ", printOOK);
KakuDecoder         kaku(    7, "KAKU ", printOOK);
//XrfDecoder        xrf(     8, "XRF  ", printOOK);
//HezDecoder        hez(     9, "HEZ  ", printOOK);
//ElroDecoder       elro(   10, "ELRO ", printOOK);
//FlamingoDecoder   flam(   11, "FMGO ", printOOK);
//SmokeDecoder      smok(   12, "SMK  ", printOOK);
//ByronbellDecoder  byro(   13, "BYR  ", printOOK);
//KakuADecoder      kakuA(  14, "KAKUA", printOOK);
WS249               ws249(  20, "WS249", printOOK);
Philips             phi(    21, "PHI  ", printOOK);
OregonDecoderV1     orscV1( 22, "ORSV1", printOOK);
//OregonDecoderV3   orscV3( 23, "ORSV3", printOOK);
void setupDecoders() {
  decoders[di++] = &ws249;
  decoders[di++] = &phi;
  decoders[di++] = &orscV1;
  decoders[di++] = &kaku;
}
#else
//868MHz
#include "decoders868.h"
//VisonicDecoder    viso(    1, "VISO ", printOOK);
//EMxDecoder          emx(     2, "EMX  ", printOOK);
//KSxDecoder        ksx(     3, "KSX  ", printOOK);
FSxDecoder          fsx(     4, "FS20 ", printOOK);
//FSxDecoderA       fsxa(   44, "FS20A", printOOK);
//
void setupDecoders() {
  //decoders[di++] = &emx;
  decoders[di++] = &fsx;
}
#endif
// End config items --------------------------------------------------------

//global instance of the OOK RF69 class
RF69A rf;

void printDigits(int val) {
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (val < 10)
    Serial.print('0');
  Serial.print(val);
}

void printHex(int val) {
  if (val < 16)
    Serial.print('0');
  Serial.print(val, HEX);
}

#define RSSI_BUF_EXP 3 //keep it powers of 2
#define RSSI_BUF_SIZE  (1<<RSSI_BUF_EXP)
uint8_t rssi_buf[RSSI_BUF_SIZE];
uint8_t rssi_buf_i = 0;

void printRSSI() {
  uint16_t avgonrssi = 0;
  uint16_t avgoffrssi = 0;
  for (uint8_t i = 0; i < RSSI_BUF_SIZE; i += 2) {
    avgonrssi += rssi_buf[i];
    avgoffrssi += rssi_buf[i + 1];
    //Serial.print(" (");
    //Serial.print(rssi_buf[i+1]);
    //Serial.print("/");
    //Serial.print(rssi_buf[i]);
    //Serial.print(") ");
  }
  Serial.print(" (");
  Serial.print(avgoffrssi >> RSSI_BUF_EXP - 1);
  Serial.print("/");
  Serial.print(avgonrssi >> RSSI_BUF_EXP - 1);
  Serial.print(")");
}

void printOOK (class DecodeOOK* decoder) {
    static uint32_t last_print = 0;
    static uint8_t mesgcnt = 1;
    uint8_t pos;
    const uint8_t* data = decoder->getData(pos);
    uint32_t now =millis();
    if (now -last_print < 1000) {
        mesgcnt++;
        //chprintf(serial, "%12d     %3d %1d ", now, now-last_print, mesgcnt);
        //Serial.print(now);
        //Serial.print("      ");
        //Serial.print(now-last_print);
        //Serial.print(" ");
        //Serial.print(mesgcnt);
        //Serial.print("  ");
    } else {
        mesgcnt = 1;
        //chprintf(serial, "\r\n%12d %3d     %1d ", now, (now-last_print)/1000, mesgcnt);
        Serial.println();
        //Serial.print(now);
        //Serial.print(" ");
        //Serial.print((now-last_print)/1000);
        //Serial.print("      ");
        //Serial.print(mesgcnt);
        //Serial.print("  ");
    }
    last_print = now;
    
  //Serial.println("");
  //Serial.print(hour());
  //printDigits(minute());
  //printDigits(second());
  //Serial.print(" ");
  //int8_t textbuf[25];
  //rf12_settings_text(textbuf);
  //Serial.print(textbuf);
  //Serial.print(' ');
  Serial.print(decoder->tag);
  Serial.print(' ');
  for (uint8_t i = 0; i < pos; ++i) {
    printHex(data[i]);
  }
  printRSSI();
  Serial.println();

  decoder->resetDecoder();
}

void processBit(uint16_t pulse_dur, uint8_t signal, uint8_t rssi) {
  if (rssi) {
    if (signal) {
      rssi_buf_i += 2;
      rssi_buf_i &= (RSSI_BUF_SIZE - 2);
      rssi_buf[rssi_buf_i] = rssi;
    } else {
      rssi_buf[rssi_buf_i + 1] = rssi;
    }
  }
  for (uint8_t i = 0; decoders[i]; i++) {
    if (decoders[i]->nextPulse(pulse_dur, signal))
      decoders[i]->decoded(decoders[i]);
  }
}

//TODO: Integrate DIO2 based receive function in radio-ook.h
//
uint8_t tsample = 25; //25 us samples
//static volatile uint8_t fixthd = 0x10;
uint8_t fixthd = 65;
uint32_t bitrate = 32768;
uint8_t bw = 16; //0=250kHz, 8=200kHz, 16=167kHz, 1=125kHz, 9=100kHz, 17=83kHz 2=63kHz, 10=50kHz

static void receiveOOK() {
  //moving average buffer
  uint8_t avg_len = 5;
  uint32_t filter = 0;
  uint32_t sig_mask = (1 << avg_len) - 1;

  rf.OOKthdMode(0x40); //0x00=fix, 0x40=peak, 0x80=avg
  rf.setBitrate(bitrate);

  rf.setFrequency(frqkHz);
  rf.setBW(bw);
  rf.setThd(fixthd);
  rf.readAllRegs();
  uint8_t t_step = tsample;
  uint32_t now = micros();
  uint32_t soon = now + t_step;

  uint32_t nrssi = 0;
  uint64_t sumrssi = 0;
  uint64_t sumsqrssi = 0;
  uint32_t rssivar = 0;

  uint8_t rssimax = fixthd + 6;
  uint8_t slicethd = 100;
  uint8_t decthdcnt = 0;
  uint8_t prev_thd = 0;
  uint8_t max_thd = 0;
  uint8_t prev_rssi = 0;
  uint16_t log = 0;

  uint32_t new_edge = now;
  uint32_t last_edge = now;
  uint8_t last_data = bitRead(RF69_RX_PIN, RF69_RX_DATA);

  //uint8_t last_data = ~rf.readRSSI() > slicethd;

  uint16_t flip_cnt = 0;

  uint32_t ts_rssi = micros();
  uint8_t rssi = ~rf.readRSSI();
  uint32_t thdUpd = millis();
  uint32_t thdUpdCnt = 0;

  uint8_t last_steady_rssi = 0;
  uint8_t rssi_q_off = 2; //stay >75us away from flip. 2 samples.
  uint8_t rssi_q_len = avg_len + rssi_q_off + 1;
  uint8_t rssi_q[rssi_q_len];
  uint8_t rssi_qi = 0;

  while (true) {
    rssi = ~rf.readRSSI();
    //statistics update every 1ms
    if ((micros() - ts_rssi) > (1000)) {
      //rssi = ~rf.readRSSI();
      sumrssi += rssi;
      sumsqrssi += (uint16_t)rssi * (uint16_t)rssi;
      nrssi++;
      ts_rssi = micros();
      if (rssi > rssimax) rssimax = rssi;
    }

    uint8_t data_in = bitRead(RF69_RX_PIN, RF69_RX_DATA);
    //uint8_t data_in = rssi > slicethd;
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

    //delay rssi to sync with moving average data
    //delay: half the average buffer + 50-75us further back (3 samples)
    uint8_t j = rssi_qi + rssi_q_len - (avg_len >> 1) - rssi_q_off;
    if (j >= rssi_q_len)
      j -= rssi_q_len;
    uint8_t delayed_rssi = rssi_q[j];
    rssi_q[rssi_qi++] = rssi;
    if (rssi_qi >= rssi_q_len)
      rssi_qi = 0;

    uint32_t ts_thdUpdNow = millis();

    if (data_out != last_data) {
      new_edge = micros();
      processBit(new_edge - last_edge, last_data, last_steady_rssi);
      last_edge = new_edge;
      last_data = data_out;
      flip_cnt++;
    } else if (micros() - last_edge > 10000) {
      //send fake pulse to notify end of transmission to decoders
      processBit(micros() - last_edge, last_data, 0);
      processBit(1, !last_data, 0);
    } else
      last_steady_rssi = delayed_rssi;

    //Update minimum slice threshold (fixthd) every 10s
    if (ts_thdUpdNow - thdUpd >= 10000) {
      //rssivar = ((sumsqrssi - sumrssi*sumrssi/nrssi) / (nrssi-1)); //64 bits
      rssivar = ((sumsqrssi - ((sumrssi >> 8) * ((sumrssi << 8) / nrssi))) / (nrssi - 1)); //32bits n<65000
      uint32_t rssiavg = sumrssi / nrssi;
      //determine stddev (no sqrt avaialble?)
      uint8_t stddev = 1;
      while (stddev < 10) {
        //safe until sqr(15)
        if (stddev * stddev >= rssivar) break;
        stddev++;
      }
      //printf("n=%d, sum=%d, sumsqr=%d\n", nrssi, sumrssi, sumsqrssi);
//      Serial.println(nrssi);
//      Serial.println((uint32_t)(sumrssi >> 16), HEX);
//      Serial.println((uint32_t)(sumrssi & 0xFFFF), HEX);
//      Serial.println((uint32_t)(sumsqrssi >> 16), HEX);
//      Serial.println((uint32_t)(sumsqrssi & 0xFFFF), HEX);
      //adapt rssi thd to noise level if variance is low
      if (rssivar < 36) {
        uint8_t delta_thd = 3 * stddev;
        if (delta_thd < 6)  delta_thd = 6;
        if (fixthd != rssiavg + delta_thd) {
          fixthd = rssiavg + delta_thd;
          //if (fixthd < 70) fixthd = 70;
          rf.setThd(fixthd);
          //printf("THD:%3d\r\n", fixthd);
        } else {
          //printf("THD:keep\r\n");
        }
      } else {
        //printf("THD:keep\r\n");
      }
      if (STATLOG) {
        Serial.print("RSSI: ");
        Serial.print(rssiavg);
        Serial.print("(v");
        Serial.print(rssivar);
        Serial.print("-s");
        Serial.print(stddev);
        Serial.print("-m");
        Serial.print(rssimax);
        //Serial.print(F(") THD:fix/peak/maxp:"));
        Serial.print(F(") THD:"));
        Serial.println(fixthd);
        //Serial.print("/");
        //Serial.print(slicethd);
        //Serial.print("/");
        //Serial.println(max_thd);

        Serial.print(thdUpdCnt);
        Serial.print(F(" polls took "));
        Serial.print(ts_thdUpdNow - thdUpd);
        Serial.print("ms = ");
        Serial.print((1000 * (ts_thdUpdNow - thdUpd)) / thdUpdCnt);
        Serial.print("us - flips = ");
        Serial.println(flip_cnt);
      }
      nrssi = sumrssi = sumsqrssi = rssimax = max_thd = 0;
      thdUpd = ts_thdUpdNow;
      thdUpdCnt = 0;

      flip_cnt = 0;

      //clear scope trigger
      //palWritePad(GPIOB, 4, 0);
    }
    thdUpdCnt++;

    if (rssi > 120) {
      //set scope trigger
      //palWritePad(GPIOB, 4, 1);
      //log = 1;
    }
    if (log > 0) {
      Serial.print(rssi);
      Serial.print(",");
      log++;
      if (log > 2000) log = 0;
    }

    //sleep at resolutions higher then system tick
    int32_t delay_us = soon - micros();
    if (delay_us > 0)
      delayMicroseconds(delay_us);
    soon = micros() + t_step;
  }
  return;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.print(F("\r\n[OOK-RX-dio2-poll]\r\n"));
  setupDecoders();
  if (di>max_decoders)
  Serial.print(F("ERROR: decoders-array too small. Memory corruption."));

  //D3 (INT1) as input
  pinMode(3, INPUT); //D3 is input
  digitalWrite(3, HIGH); //pull up D3

  //same as above?
  RF69_RX_DDR &= ~_BV(RF69_RX_DATA);
  detachInterrupt(RF69_RX_EXTINT);

  rf12_initialize(11, RF12_BAND, 42, 1600);// calls rf69_initialize()
  //setup for OOK
  rf.init(11, 42, frqkHz);

}

void loop() {
    receiveOOK();
}
