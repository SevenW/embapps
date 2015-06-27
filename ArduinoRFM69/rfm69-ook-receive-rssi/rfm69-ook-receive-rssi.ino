#define RF69_COMPAT 1 // define this to use the RF69 driver i.s.o. RF12
#define STATLOG 0 //0=no statistics logging 1=statistics logging

#include <JeeLib.h>
//#include <Time.h>
#include "radio-ook.h"
#include "decodeOOK.h"
//#include "decodeOOK_TEST.h"

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
EMxDecoder          emx(     2, "EMX  ", printOOK);
//KSxDecoder        ksx(     3, "KSX  ", printOOK);
FSxDecoder          fsx(     4, "FS20 ", printOOK);
//FSxDecoderA       fsxa(   44, "FS20A", printOOK);
//
void setupDecoders() {
  decoders[di++] = &emx;
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

//small circular buffer for ON rssi signals
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
  uint8_t pos;
  const uint8_t* data = decoder->getData(pos);
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

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.print(F("\r\n[OOK-RX-rssi]\r\n"));
  setupDecoders();
  if (di>max_decoders)
  Serial.print(F("ERROR: decoders-array too small. Memory corruption."));
  rf12_initialize(11, RF12_BAND, 42, 1600);// calls rf69_initialize()
  //setup for OOK
  rf.init(11, 42, frqkHz);
}

void loop() {
  rf.receiveOOK_forever(processBit);
}
