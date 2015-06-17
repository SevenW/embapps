// OOK/RSSI RF69 driver

class RF69A {
  public:
    void init (uint8_t id, uint8_t group, int freq);
    uint8_t readRSSI();
    int16_t readAFC();
    int16_t readFEI(bool start);
    int readStatus();
    void setThd(uint8_t thd);
    void setBW(uint8_t bw);
    void setFrequency(uint32_t frq);
    void setBitrate(uint32_t br);

    //test functions
    void DataModule(uint8_t module);
    void OOKthdMode(uint8_t thdmode);
    void readAllRegs();

    uint8_t myGroup;

  protected:
    enum {
      REG_FIFO          = 0x00,
      REG_OPMODE        = 0x01,
      REG_DATAMOD       = 0x02,
      REG_BRMSB         = 0x03,
      REG_FRFMSB        = 0x07,
      REG_PALEVEL       = 0x11,
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
      REG_SYNCVALUE1    = 0x2F,
      REG_SYNCVALUE2    = 0x30,
      REG_NODEADDR      = 0x39,
      REG_BCASTADDR     = 0x3A,
      REG_PKTCONFIG2    = 0x3D,
      REG_AESKEYMSB     = 0x3E,

      REG_TEMP1         = 0x4E,
      REG_TEMP2         = 0x4F,

      MODE_SLEEP        = 0 << 2,
      MODE_TRANSMIT     = 3 << 2,
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

    volatile uint8_t mode;
};

// driver implementation

static const uint8_t configRegsOOK [] = {
  0x01, 0x04, // OpMode = standby
  0x02, 0x68, // DataModul = conti mode, ook, no shaping
  //0x02, 0x60, // DataModul = conti mode, fsk, no shaping
  //0x02, 0x00, // DataModul = packet mode, fsk, no shaping
  //0x03, 0x7D, // BitRateMsb, 1000bps
  //0x04, 0x00, // BitRateLsb, divider = 32 MHz / 650
  //0x03, 0x00, // BitRateMsb, data rate = 300 kbps max FSK
  //0x04, 0x6B, // BitRateLsb, divider = 32 MHz / 650
  0x03, 0x03, // BitRateMsb, data rate = 32768bps maxOOK
  0x04, 0xD1, // BitRateLsb, divider = 32 MHz / 650
  //0x05, 0x01, // FdevMsb = 25 KHz
  //0x06, 0x99, // FdevLsb = 25 KHz
  //0x07, 0xD9, // FrfMsb, freq = 868.250 MHz
  //0x08, 0x13, // FrfMib, divider = 14221312
  //0x09, 0x00, // FrfLsb, step = 61.03515625
  0x0B, 0x20, // AfcCtrl, afclowbetaon
  0x18, 0x81, // LNA fixed highest gain, Z=200ohm
  0x19, 0x40, // RxBw DCC=4%, Man=00b Exp = 0 =>BWOOK=250.0kHz
  0x1B, 0x43, // OOK peak, 0.5dB once/8 chips (slowest)
  0x1C, 0x80, // OOK avg thresh /4pi
  0x1D, 0x38, // OOK fixed thresh
  0x1E, 0x00, // No auto AFC
  0x25, 0x80, // Dio 0: RSSI 1: Dclk 2: Data 3: RSSI
  0x26, 0x37, // Dio 5: ModeReady, CLKOUT = OFF
  0x29, 0xFF, // RssiThresh: lowest possible threshold to start receive
  0x2E, 0x00, // SyncConfig = sync off
  0x3C, 0x8F, // FifoTresh, not empty, level 15
  0x3D, 0x10, // PacketConfig2, interpkt = 1, autorxrestart off
  //0x58, 0x2D, // Sensitivity boost (TestLNA)
  0x6F, 0x20, // TestDagc ...
  0
};

void RF69A::init (uint8_t id, uint8_t group, int freq) {
  //RF69<SPI>::init(id, group, freq);
  this->configure(configRegsOOK);
  //clear AFC. Essential for wideband OOK signals. No other way to reset from SW.
  this->writeReg(REG_AFCFEI, (1 << 1));
  //this->writeReg(REG_AFCFEI, this->readReg(REG_AFCFEI) | (1 << 1)); //does not work
  myGroup = group;
  this->setMode(MODE_RECEIVE);
}

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
  rssi = this->readReg(REG_RSSIVALUE);
  //rssi >>= 1;
  return rssi;
}

int16_t RF69A::readFEI(bool start) {
  int16_t fei = 0;
  if (start)
  {
    this->writeReg(REG_AFCFEI, 1 << 5);
    while ((this->readReg(REG_AFCFEI) & (1 << 6)) == 0x00); // Wait for RSSI_Ready
  }
  fei = this->readReg(REG_FEILSB);
  fei |= (this->readReg(REG_FEIMSB) << 8);
  return fei;
}

int16_t RF69A::readAFC() {
  int16_t afc = 0;
  //  if (true)
  //  {
  //    //RSSI trigger not needed if DAGC is in continuous mode
  //    writeReg(REG_RSSICONFIG, RF_RSSI_START);
  //    while ((readReg(REG_RSSICONFIG) & RF_RSSI_DONE) == 0x00); // Wait for RSSI_Ready
  //  }
  afc = this->readReg(REG_AFCLSB);
  afc |= (this->readReg(REG_AFCMSB) << 8);
  //reset AFC
  //    if (true)
  //    {
  //        this->writeReg(REG_AFCFEI, (1<<1));
  //        //this->writeReg(REG_AFCFEI, this->readReg(REG_AFCFEI) | (1 << 1)); //does not work
  //        //while ((this->readReg(REG_AFCFEI) & (1 << 6)) == 0x00); // Wait for RSSI_Ready
  //    }

  return afc;
}

int RF69A::readStatus() {
  uint8_t f1;
  uint8_t f2;
  switch (this->mode) {
    case MODE_RECEIVE:
    case MODE_TRANSMIT:
      f1 = readReg(REG_IRQFLAGS1);
      f2 = readReg(REG_IRQFLAGS2);
      if (f1 & 0x08) {
        //Serial.print( BYTETOBINARYPATTERN, BYTETOBINARY(f1));
        //Serial.print( "  ");
        //Serial.print( BYTETOBINARYPATTERN, BYTETOBINARY(f2));
        //Serial.print( "  ");
        //fixthd += 1;
        //writeReg(0x1D, fixthd);
        //Serial.print(fixthd);
        //Serial.print(" ");
        Serial.println(readRSSI());
      }
    default:
      break;
  }
  return 0;
}

void RF69A::setThd (uint8_t thd) {
  this->writeReg(REG_OOKFIX, thd);
}

void RF69A::setBW (uint8_t bw) {
  this->writeReg(0x19, 0x40 | bw);
}

void RF69A::setFrequency (uint32_t frq) {
  this->writeReg(REG_AFCFEI, (1 << 1));
  //RF69<SPI>::setFrequency(frq);

  // accept any frequency scale as input, including KHz and MHz
  while (frq < 100000000)
    frq *= 10;
  uint64_t frf = ((uint64_t)frq << 8) / (32000000L >> 11);
  this->writeReg(REG_FRFMSB, frf >> 16);
  this->writeReg(REG_FRFMSB + 1, frf >> 8);
  this->writeReg(REG_FRFMSB + 2, frf);
}

void RF69A::setBitrate (uint32_t br) {
  if (br < 489) {
    br = 0xFFFF;
  } else {
    br = 32000000L / br;
  }
  this->writeReg(REG_BRMSB, br >> 8);
  this->writeReg(REG_BRMSB + 1, br);
}

void RF69A::DataModule(uint8_t module) {
  this->setMode(MODE_SLEEP);
  this->writeReg(REG_DATAMOD, module);
  delayMicroseconds(400);
  this->setMode(MODE_RECEIVE);
}

void RF69A::OOKthdMode(uint8_t thdmode) {
  this->writeReg(REG_OOKPEAK, thdmode);
}


////for debugging
//void RF69A::readAllRegs() {
//    uint8_t regVal;
//
//    Serial.print( "     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  D"
//    "\r\n00: 00");
//    for (uint8_t regAddr = 1; regAddr <= 0x4F; regAddr++) {
//        this->spi.enable();
//        this->spi.transfer(regAddr & 0x7f);   // send address + r/w bit
//        regVal = this->spi.transfer(0);
//        this->spi.disable();
//
//        if (regAddr % 16 == 0)
//        Serial.print( "\r\n%02X:", regAddr);
//        Serial.print( " %02x", regVal);
//    }
//    Serial.print( "\r\n");
//}
