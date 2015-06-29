// OOK/RSSI RF69 driver

//control statistics logging
#ifndef STATLOG
#define STATLOG 0
#endif

class RF69A {
  public:
    typedef void (*ooktrans_cb)(uint16_t pulse_dur, uint8_t signal, uint8_t rssi);
    RF69A();
    void init (uint8_t id, uint8_t group, uint32_t freq);
    uint8_t readRSSI();
    void setThd(uint8_t thd);
    void setBW(uint8_t bw);
    void setFrequency(uint32_t frq);
    void setBitrate(uint32_t br);

    //test functions
    void DataModule(uint8_t module);
    void OOKthdMode(uint8_t thdmode);
    void exit_receive();
    void receiveOOK_forever(ooktrans_cb processBit);
    void sendook(uint8_t header, const void* ptr, int len);
    void init_transmit(uint8_t band);
    void exit_transmit();
    void readAllRegs();
    //int readStatus();

    uint8_t myGroup;
    uint8_t myId;

  protected:
    enum {
      REG_FIFO          = 0x00,
      REG_OPMODE        = 0x01,
      REG_DATAMOD       = 0x02,
      REG_BRMSB         = 0x03,
      REG_FRFMSB        = 0x07,
      REG_PALEVEL       = 0x11,
      REG_LNA           = 0x18,
      REG_OOKPEAK       = 0x1B,
      REG_OOKFIX        = 0x1D,
      REG_AFCFEI        = 0x1E,
      REG_AFCMSB        = 0x1F,
      REG_AFCLSB        = 0x20,
      REG_FEIMSB        = 0x21,
      REG_FEILSB        = 0x22,
      REG_RSSICONFIG    = 0x23,
      REG_RSSIVALUE     = 0x24,
      REG_IRQFLAGS1     = 0x27,
      REG_IRQFLAGS2     = 0x28,
      REG_RSSITHRESH    = 0x29,
      REG_PREAMPSIZE    = 0x2D,
      REG_SYNCCONFIG    = 0x2E,
      REG_SYNCVALUE1    = 0x2F,
      REG_SYNCVALUE2    = 0x30,
      REG_PKTCONFIG1    = 0x37,
      REG_PAYLOADLEN    = 0x38,
      REG_NODEADDR      = 0x39,
      REG_BCASTADDR     = 0x3A,
      REG_PKTCONFIG2    = 0x3D,
      REG_AESKEYMSB     = 0x3E,

      REG_TEMP1         = 0x4E,
      REG_TEMP2         = 0x4F,

      MODE_SLEEP        = 0 << 2,
      MODE_STANDBY      = 1 << 2,
      MODE_TRANSMITTER  = 3 << 2,
      MODE_RECEIVE      = 4 << 2,

      IRQ1_MODEREADY    = 1 << 7,
      IRQ1_RXREADY      = 1 << 6,

      IRQ2_FIFOFULL     = 1 << 7,
      IRQ2_FIFONOTEMPTY = 1 << 6,
      IRQ2_FIFOOVERRUN  = 1 << 4,
      IRQ2_PACKETSENT   = 1 << 3,
      IRQ2_PAYLOADREADY = 1 << 2,
    };

    void configure (const uint8_t* p);
    void writeReg (uint8_t addr, uint8_t value);
    uint8_t readReg (uint8_t addr);
    void setMode (uint8_t newMode);
    void printHex(int val);

    volatile uint8_t mode;

    uint8_t tsample;
    uint8_t fixthd;
    uint32_t bitrate;
    uint8_t bw;
    uint32_t frqkHz;
};

// driver implementation

static const uint8_t configRegsOOK [] = {
  0x01, 0x04, // OpMode = standby
  0x02, 0x68, // DataModul = conti mode, ook, no shaping
  0x03, 0x03, // BitRateMsb, data rate = 32768bps maxOOK
  0x04, 0xD1, // BitRateLsb, divider = 32 MHz / 650
  0x0B, 0x20, // AfcCtrl, afclowbetaon
  0x18, 0x81, // LNA fixed highest gain, Z=200ohm
  0x19, 0x40, // RxBw DCC=4%, Man=00b Exp = 0 =>BWOOK=250.0kHz
  0x1B, 0x43, // OOK peak, 0.5dB once/8 chips (slowest)
  0x1C, 0x80, // OOK avg thresh /4pi
  0x1D, 0x38, // OOK fixed thresh
  0x1E, 0x00, // No auto AFC
  0x25, 0x80, // Dio 0: RSSI 1: Dclk 2: Data 3: RSSI
  0x26, 0x07, // CLKOUT = OFF
  0x29, 0xFF, // RssiThresh: lowest possible threshold to start receive
  0x2E, 0x00, // SyncConfig = sync off
  //0x58, 0x2D, // Sensitivity boost (TestLNA)
  0x6F, 0x20, // TestDagc ...
  0
};

void RF69A::writeReg (uint8_t addr, uint8_t value) {
  RF69::control(addr | 0x80, value);
}

uint8_t RF69A::readReg (uint8_t addr) {
  return RF69::control(addr, 0);
}

void RF69A::configure (const uint8_t* p) {
  while (true) {
    uint8_t cmd = p[0];
    if (cmd == 0)
      break;
    writeReg(cmd, p[1]);
    p += 2;
  }
}

RF69A::RF69A() {
    tsample = 25; //25 us samples
    fixthd = 55;
    bitrate = 32768;
    bw = 16; //0=250kHz, 8=200kHz, 16=167kHz, 1=125kHz, 9=100kHz, 17=83kHz 2=63kHz, 10=50kHz
    frqkHz = 868400;
}

void RF69A::init (uint8_t id, uint8_t group, uint32_t freq) {
  frqkHz = freq;
  myId=id;
  //RF69<SPI>::init(id, group, freq);
  configure(configRegsOOK);
  OOKthdMode(0x40); //0x00=fix, 0x40=peak, 0x80=avg
  setBitrate(bitrate);
  setFrequency(frqkHz);
  setBW(bw);
  setThd(fixthd);
  //clear AFC. Essential for wideband OOK signals. No other way to reset from SW.
  writeReg(REG_AFCFEI, (1 << 1));
  //writeReg(REG_AFCFEI, readReg(REG_AFCFEI) | (1 << 1)); //does not work
  myGroup = group;
  setMode(MODE_RECEIVE);
}

void RF69A::setMode (uint8_t newMode) {
  mode = newMode;
  writeReg(REG_OPMODE, (readReg(REG_OPMODE) & 0xE3) | newMode);
  while ((readReg(REG_IRQFLAGS1) & IRQ1_MODEREADY) == 0)
    ;
}


uint8_t RF69A::readRSSI() {
  uint8_t rssi = 0;
  //  if (true)
  //  {
  //    //RSSI trigger not needed if DAGC is in continuous mode
  //    writeReg(REG_RSSICONFIG, RF_RSSI_START);
  //    while ((readReg(REG_RSSICONFIG) & RF_RSSI_DONE) == 0x00); // Wait for RSSI_Ready
  //  }
  rssi = readReg(REG_RSSIVALUE);
  //rssi >>= 1;
  return rssi;
}

void RF69A::setThd (uint8_t thd) {
  writeReg(REG_OOKFIX, thd);
}

void RF69A::setBW (uint8_t bw) {
  writeReg(0x19, 0x40 | bw);
}

void RF69A::setFrequency (uint32_t frq) {
  writeReg(REG_AFCFEI, (1 << 1));
  //RF69<SPI>::setFrequency(frq);

  // accept any frequency scale as input, including KHz and MHz
  while (frq < 100000000)
    frq *= 10;
  uint64_t frf = ((uint64_t)frq << 8) / (32000000L >> 11);
  writeReg(REG_FRFMSB, frf >> 16);
  writeReg(REG_FRFMSB + 1, frf >> 8);
  writeReg(REG_FRFMSB + 2, frf);
}

void RF69A::setBitrate (uint32_t br) {
  if (br < 489) {
    br = 0xFFFF;
  } else {
    br = 32000000L / br;
  }
  writeReg(REG_BRMSB, br >> 8);
  writeReg(REG_BRMSB + 1, br);
}

void RF69A::DataModule(uint8_t module) {
  setMode(MODE_SLEEP);
  writeReg(REG_DATAMOD, module);
  delayMicroseconds(400);
  setMode(MODE_RECEIVE);
}

void RF69A::OOKthdMode(uint8_t thdmode) {
  writeReg(REG_OOKPEAK, thdmode);
}

void RF69A::exit_receive() {
  //Some registers requires reseting to defaults/
  //Jeelib drivers do not program them, but rely on proper values.
  writeReg(REG_LNA, 0x80);        // LNA automatic gain, Z=200ohm
  writeReg(REG_RSSITHRESH, 0xE4); // RssiThresh 0xE4
}

void RF69A::receiveOOK_forever(ooktrans_cb processBit) {
  //moving average buffer
  uint8_t avg_len = 5;
  uint32_t filter = 0;
  uint32_t sig_mask = (1 << avg_len) - 1;

  OOKthdMode(0x40); //0x00=fix, 0x40=peak, 0x80=avg
  setBitrate(bitrate);
  setFrequency(frqkHz);
  setBW(bw);
  setThd(fixthd);
  //readAllRegs();
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
  //uint8_t last_data = palReadPad(DIO2_port, DIO2_pad);
  uint8_t last_data = ~readRSSI() > slicethd;

  uint16_t flip_cnt = 0;

  uint32_t ts_rssi = micros();
  uint8_t rssi = ~readRSSI();
  uint32_t thdUpd = millis();
  uint32_t thdUpdCnt = 0;

  uint8_t last_steady_rssi = 0;
  uint8_t rssi_q_off = 2; //stay >75us away from flip. 2 samples.
  uint8_t rssi_q_len = avg_len + rssi_q_off + 1;
  uint8_t rssi_q[rssi_q_len];
  uint8_t rssi_qi = 0;

  while (true) {
    rssi = ~readRSSI();
    //uint8_t data_in = palReadPad(DIO2_port, DIO2_pad);
    uint8_t data_in = rssi > slicethd;
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

    //statistics update every 1ms
    if ((micros() - ts_rssi) > (1000)) {
      //rssi = ~readRSSI();
      sumrssi += rssi;
      sumsqrssi += rssi * rssi;
      nrssi++;
      ts_rssi = micros();
      if (rssi > rssimax) rssimax = rssi;
    }

    //Update minimum slice threshold (fixthd) every 10s
    uint32_t ts_thdUpdNow = millis();
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
      //adapt rssi thd to noise level if variance is low
      if (rssivar < 36) {
        uint8_t delta_thd = 3 * stddev;
        if (delta_thd < 6)  delta_thd = 6;
        if (fixthd != rssiavg + delta_thd) {
          fixthd = rssiavg + delta_thd;
          //if (fixthd < 70) fixthd = 70;
          setThd(fixthd);
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
        Serial.print(F(") THD:fix/peak/maxp:"));
        Serial.print(fixthd);
        Serial.print("/");
        Serial.print(slicethd);
        Serial.print("/");
        Serial.println(max_thd);

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

    //emulate PEAK-mode OOK threshold
    decthdcnt++;
    if (((decthdcnt & 3) == 0) and (slicethd > fixthd)) {
      //decrement slicethd every 4th poll (~280us) with 0.5dB
      slicethd--;
    }
    prev_thd = slicethd;
    if (rssi > slicethd + 12) {
      //deal with outlier rssi?
      slicethd = rssi - 12;
      //limit printing
      if (slicethd > prev_thd + 12) {
        //Serial.print( "PEAK-THD: set %d\r\n", slicethd);
      }
      if (rssi > 200) {
        slicethd = 188;
        //Serial.print(F("PEAK-THD: outlier rssi "));
        //Serial.println(rssi);
      }
      if (slicethd < fixthd) slicethd = fixthd;
    }
    if (slicethd > max_thd) max_thd = slicethd;

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

void RF69A::init_transmit(uint8_t band) {
  writeReg(REG_OPMODE, 0x00);         // OpMode = sleep
  writeReg(REG_DATAMOD, 0x08);        // DataModul = packet mode, OOK
  writeReg(REG_PREAMPSIZE, 0x00);     // PreambleSize = 0 NO PREAMBLE
  writeReg(REG_SYNCCONFIG, 0x00);     // SyncConfig = sync OFF
  writeReg(REG_PKTCONFIG1, 0x80);     // PacketConfig1 = variable length, advanced items OFF
  writeReg(REG_PAYLOADLEN, 0x00);     // PayloadLength = 0, unlimited
  if (band == 0 /*RF12_433MHZ*/) {
    setFrequency(434920);
    setBitrate(2667);
  } else {
    setFrequency(868400);
    setBitrate(5000);
  }
}

void RF69A::exit_transmit() {
  //Registers that do not get reset by reinitialization
  writeReg(REG_PREAMPSIZE, 0x03);     // PreambleSize = 3 (RFM69 default)
  //rf12_configSilent();
}

void RF69A::sendook(uint8_t header, const void* ptr, int len) {
  //setMode(MODE_TRANSMITTER);

  //write 8 bits OFF to start with
  writeReg(REG_FIFO, 0);
  for (int i = 0; i < len; ++i)
    writeReg(REG_FIFO, ((const uint8_t*) ptr)[i]);
  //  for (int i = 0; i < len; ++i)
  //      printf("%02X", ((const uint8_t*) ptr)[i]);
  //  printf("\n");

  //send after filling FIFO (RFM should be in sleep or standby)
  setMode(MODE_TRANSMITTER);
  while ((readReg(REG_IRQFLAGS2) & IRQ2_PACKETSENT) == 0)
  { //nop
  };

  setMode(MODE_STANDBY);
}

////for debugging
void RF69A::printHex(int val) {
    if (val < 16)
      Serial.print('0');
    Serial.print(val, HEX);
  }
void RF69A::readAllRegs() {
    uint8_t regVal;

    Serial.println("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  D");
    Serial.print("00: 00");
    for (uint8_t regAddr = 1; regAddr <= 0x4F; regAddr++) {
        regVal = readReg(regAddr);
        if (regAddr % 16 == 0) {
          Serial.println();
          printHex(regAddr);
        Serial.print(":");
        }
        Serial.print(" ");
        printHex(regVal);
    }
    Serial.println();
}

// int RF69A::readStatus() {
// uint8_t f1;
// uint8_t f2;
// switch (mode) {
// case MODE_RECEIVE:
// case MODE_TRANSMIT:
// f1 = readReg(REG_IRQFLAGS1);
// f2 = readReg(REG_IRQFLAGS2);
// if (f1 & 0x08) {
// //Serial.print( BYTETOBINARYPATTERN, BYTETOBINARY(f1));
// //Serial.print( "  ");
// //Serial.print( BYTETOBINARYPATTERN, BYTETOBINARY(f2));
// //Serial.print( "  ");
// //fixthd += 1;
// //writeReg(0x1D, fixthd);
// //Serial.print(fixthd);
// //Serial.print(" ");
// Serial.println(readRSSI());
// }
// default:
// break;
// }
// return 0;
// }

