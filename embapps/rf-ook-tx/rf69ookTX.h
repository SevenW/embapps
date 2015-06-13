// Native mode RF69 driver.

template< typename SPI >
class RF69 {
public:
    void init (uint8_t id, uint8_t group, int freq);
    void encrypt (const char* key);
    void txPower (uint8_t level);
    
    int receive (void* ptr, int len);
    void send (uint8_t header, const void* ptr, int len);
    void sendook (uint8_t header, const void* ptr, int len);
    void transmitOOKOn ();
    void transmitOOKOff ();
    void sleep ();
    
    int16_t afc;
    uint8_t rssi;
    uint8_t lna;
    uint8_t myId;
    uint8_t parity;

protected:
    enum {
        REG_FIFO          = 0x00,
        REG_OPMODE        = 0x01,
        REG_FRFMSB        = 0x07,
        REG_PALEVEL       = 0x11,
        REG_LNAVALUE      = 0x18,
        REG_AFCMSB        = 0x1F,
        REG_AFCLSB        = 0x20,
        REG_FEIMSB        = 0x21,
        REG_FEILSB        = 0x22,
        REG_RSSIVALUE     = 0x24,
        REG_IRQFLAGS1     = 0x27,
        REG_IRQFLAGS2     = 0x28,
        REG_SYNCVALUE1    = 0x2F,
        REG_SYNCVALUE2    = 0x30,
        REG_NODEADDR      = 0x39,
        REG_BCASTADDR     = 0x3A,
        REG_PKTCONFIG2    = 0x3D,
        REG_AESKEYMSB     = 0x3E,

        MODE_SLEEP        = 0<<2,
        MODE_TRANSMIT     = 3<<2,
        MODE_RECEIVE      = 4<<2,

        IRQ1_MODEREADY    = 1<<7,
        IRQ1_RXREADY      = 1<<6,
        IRQ1_SYNADDRMATCH = 1<<0,

        IRQ2_FIFONOTEMPTY = 1<<6,
        IRQ2_PACKETSENT   = 1<<3,
        IRQ2_PAYLOADREADY = 1<<2,
    };

    uint8_t readReg (uint8_t addr) {
        return spi.rwReg(addr, 0);
    }
    void writeReg (uint8_t addr, uint8_t val) {
        spi.rwReg(addr | 0x80, val);
    }
    void setMode (uint8_t newMode);
    void configure (const uint8_t* p);
    void setFrequency (uint32_t freq);

    SPI spi;
    volatile uint8_t mode;
};

// driver implementation

template< typename SPI >
void RF69<SPI>::setMode (uint8_t newMode) {
    mode = newMode;
    writeReg(REG_OPMODE, (readReg(REG_OPMODE) & 0xE3) | newMode);
    while ((readReg(REG_IRQFLAGS1) & IRQ1_MODEREADY) == 0)
        ;
}

template< typename SPI >
void RF69<SPI>::setFrequency (uint32_t hz) {
    // accept any frequency scale as input, including KHz and MHz
    // multiply by 10 until freq >= 100 MHz (don't specify 0 as input!)
    while (hz < 100000000)
        hz *= 10;

    // Frequency steps are in units of (32,000,000 >> 19) = 61.03515625 Hz
    // use multiples of 64 to avoid multi-precision arithmetic, i.e. 3906.25 Hz
    // due to this, the lower 6 bits of the calculated factor will always be 0
    // this is still 4 ppm, i.e. well below the radio's 32 MHz crystal accuracy
    // 868.0 MHz = 0xD90000, 868.3 MHz = 0xD91300, 915.0 MHz = 0xE4C000  
    uint32_t frf = (hz << 2) / (32000000L >> 11);
    writeReg(REG_FRFMSB, frf >> 10);
    writeReg(REG_FRFMSB+1, frf >> 2);
    writeReg(REG_FRFMSB+2, frf << 6);
}

template< typename SPI >
void RF69<SPI>::configure (const uint8_t* p) {
    while (true) {
        uint8_t cmd = p[0];
        if (cmd == 0)
            break;
        writeReg(cmd, p[1]);
        p += 2;
    }
}

//static const uint8_t configRegsPacketOOKPkt [] = {
//    0x01, 0x00, // OpMode = sleep
//    0x02, 0x08, // DataModul = packet mode, OOK
//    0x03, 0x19, // BitRateMsb, data rate = 5000bps (200us)
//    0x04, 0x00, // BitRateLsb, divider = 32 MHz / 650
//    0x05, 0x02, // FdevMsb = 45 KHz
//    0x06, 0xE1, // FdevLsb = 45 KHz
//    0x0B, 0x20, // Low M
//    0x19, 0x4A, // RxBw 100 KHz
//    0x1A, 0x42, // AfcBw 125 KHz
//    0x1E, 0x0C, // AfcAutoclearOn, AfcAutoOn
//    //0x25, 0x40, //0x80, // DioMapping1 = SyncAddress (Rx)
//    0x29, 0xA0, // RssiThresh -80 dB
//    0x2D, 0x00, // PreambleSize = 0 NO PREAMBLE
//    0x2E, 0x00, // SyncConfig = sync OFF
//    0x2F, 0x2D, // SyncValue1 = 0x2D
//    0x37, 0x80, // PacketConfig1 = variable length, advanced items OFF
//    0x38, 0x00, // PayloadLength = 0, unlimited
//    0x3C, 0x8F, // FifoTresh, not empty, level 15
//    0x3D, 0x12, // 0x10, // PacketConfig2, interpkt = 1, autorxrestart off
//    0x6F, 0x20, // TestDagc ...
//    0x71, 0x02, // RegTestAfc
//    0
//};

static const uint8_t configRegsOOKCont [] = {
	    0x01, 0x04, // OpMode = standby
	    0x02, 0x68, // DataModul = conti mode, ook, no shaping
	    0x03, 0x03, // BitRateMsb, 32768bps
	    0x04, 0xD1, // BitRateLsb, divider = 32 MHz / 650
	    0x05, 0x01, // FdevMsb = 25 KHz
	    0x06, 0x99, // FdevLsb = 25 KHz
	    0x07, 0xD9, // FrfMsb, freq = 868.250 MHz
	    0x08, 0x13, // FrfMib, divider = 14221312
	    0x09, 0x00, // FrfLsb, step = 61.03515625
	    0x0B, 0x20, // AfcCtrl, afclowbetaon
	    0x18, 0x81, // LNA fixed highest gain, Z=200ohm
	    0x19, 0x40, // RxBw DCC=4%, Man=00b Exp = 0 =>BWOOK=250.0kHz
	    0x1B, 0x40, // OOK peak, 0.5dB once/chip
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

//static const uint8_t configRegsFSKCont [] = {
//    0x01, 0x00, // OpMode = sleep
//    0x02, 0x60, // DataModul = cont mode, fsk
//    0x03, 0x02, // BitRateMsb, data rate = 49,261 khz
//    0x04, 0x8A, // BitRateLsb, divider = 32 MHz / 650
//    0x05, 0x02, // FdevMsb = 45 KHz
//    0x06, 0xE1, // FdevLsb = 45 KHz
//    0x0B, 0x20, // Low M
//    0x19, 0x4A, // RxBw 100 KHz
//    0x1A, 0x42, // AfcBw 125 KHz
//    0x1E, 0x0C, // AfcAutoclearOn, AfcAutoOn
//    //0x25, 0x40, //0x80, // DioMapping1 = SyncAddress (Rx)
//    0x29, 0xA0, // RssiThresh -80 dB
//    0x2D, 0x05, // PreambleSize = 5
//    0x2E, 0x88, // SyncConfig = sync on, sync size = 2
//    0x2F, 0x2D, // SyncValue1 = 0x2D
//    0x37, 0xD4, // PacketConfig1 = fixed, white, filt node + bcast
//    0x38, 0x42, // PayloadLength = 0, unlimited
//    0x3C, 0x8F, // FifoTresh, not empty, level 15
//    0x3D, 0x12, // 0x10, // PacketConfig2, interpkt = 1, autorxrestart off
//    0x6F, 0x20, // TestDagc ...
//    0x71, 0x02, // RegTestAfc
//    0
//};


template< typename SPI >
void RF69<SPI>::init (uint8_t id, uint8_t group, int freq) {
    myId = id;

    // b7 = group b7^b5^b3^b1, b6 = group b6^b4^b2^b0
    parity = group ^ (group << 4);
    parity = (parity ^ (parity << 2)) & 0xC0;

    // 10 MHz, i.e. 30 MHz / 3 (or 4 MHz if clock is still at 12 MHz)
    spi.master(3);
    do
        writeReg(REG_SYNCVALUE1, 0xAA);
    while (readReg(REG_SYNCVALUE1) != 0xAA);
    do
        writeReg(REG_SYNCVALUE1, 0x55);
    while (readReg(REG_SYNCVALUE1) != 0x55);

    configure(configRegsOOKCont);
    configure(configRegsOOKCont); // ???
    setFrequency(freq);

    writeReg(REG_SYNCVALUE2, group);
    writeReg(REG_NODEADDR, myId | parity);
    writeReg(REG_BCASTADDR, parity);
}

template< typename SPI >
void RF69<SPI>::encrypt (const char* key) {
    uint8_t cfg = readReg(REG_PKTCONFIG2) & ~0x01;
    if (key) {
        for (int i = 0; i < 16; ++i) {
            writeReg(REG_AESKEYMSB + i, *key);
            if (*key != 0)
                ++key;
        }
        cfg |= 0x01;
    }
    writeReg(REG_PKTCONFIG2, cfg);
}

template< typename SPI >
void RF69<SPI>::txPower (uint8_t level) {
    writeReg(REG_PALEVEL, (readReg(REG_PALEVEL) & ~0x1F) | level);
}

template< typename SPI >
void RF69<SPI>::sleep () {
    setMode(MODE_SLEEP);
}

template< typename SPI >
int RF69<SPI>::receive (void* ptr, int len) {
    switch (mode) {
    case MODE_RECEIVE: {
        static uint8_t lastFlag;
        if ((readReg(REG_IRQFLAGS1) & IRQ1_RXREADY) != lastFlag) {
            lastFlag ^= IRQ1_RXREADY;
            if (lastFlag) { // flag just went from 0 to 1
                rssi = readReg(REG_RSSIVALUE);
                lna = (readReg(REG_LNAVALUE) >> 3) & 0x7;
#if RF69_SPI_BULK
                spi.enable();
                spi.transfer(REG_AFCMSB);
                afc = spi.transfer(0) << 8;
                afc |= spi.transfer(0);
                spi.disable();
#else
                afc = readReg(REG_AFCMSB) << 8;
                afc |= readReg(REG_AFCLSB);
#endif
            }
        }

        if (readReg(REG_IRQFLAGS2) & IRQ2_PAYLOADREADY) {
            
#if RF69_SPI_BULK
            spi.enable();
            spi.transfer(REG_FIFO);
            int count = spi.transfer(0);
            for (int i = 0; i < count; ++i) {
                uint8_t v = spi.transfer(0);
                if (i < len)
                    ((uint8_t*) ptr)[i] = v;
            }
            spi.disable();
#else
            int count = readReg(REG_FIFO);
            for (int i = 0; i < count; ++i) {
                uint8_t v = readReg(REG_FIFO);
                if (i < len)
                    ((uint8_t*) ptr)[i] = v;
            }
#endif

            return count;
        }
        break;
    }
    case MODE_TRANSMIT:
        break;
    default:
        setMode(MODE_RECEIVE);
    }
    return -1;
}

template< typename SPI >
void RF69<SPI>::send (uint8_t header, const void* ptr, int len) {
    // while the mode is MODE_TRANSMIT, receive polling will not interfere
    setMode(MODE_TRANSMIT);

#if RF69_SPI_BULK
    spi.enable();
    spi.transfer(REG_FIFO | 0x80);
    spi.transfer(len + 2);
    spi.transfer((header & 0x3F) | parity);
    spi.transfer((header & 0xC0) | myId);
    for (int i = 0; i < len; ++i)
        spi.transfer(((const uint8_t*) ptr)[i]);
    spi.disable();
#else
    writeReg(REG_FIFO, len + 2);
    writeReg(REG_FIFO, (header & 0x3F) | parity);
    writeReg(REG_FIFO, (header & 0xC0) | myId);
    for (int i = 0; i < len; ++i)
        writeReg(REG_FIFO, ((const uint8_t*) ptr)[i]);
#endif

    while ((readReg(REG_IRQFLAGS2) & IRQ2_PACKETSENT) == 0)
        chThdYield();

    setMode(MODE_RECEIVE);
}

template< typename SPI >
void RF69<SPI>::sendook (uint8_t header, const void* ptr, int len) {
    // while the mode is MODE_TRANSMIT, receive polling will not interfere
    setMode(MODE_TRANSMIT);

#if RF69_SPI_BULK
    spi.enable();
    spi.transfer(REG_FIFO | 0x80);
    //spi.transfer(len + 2);
    //spi.transfer((header & 0x3F) | parity);
    //spi.transfer((header & 0xC0) | myId);

    //write 8 bits OFF to start with
    writeReg(REG_FIFO, 0);
    for (int i = 0; i < len; ++i)
        spi.transfer(((const uint8_t*) ptr)[i]);
    spi.disable();
#else
    //writeReg(REG_FIFO, len + 2);
    //writeReg(REG_FIFO, (header & 0x3F) | parity);
    //writeReg(REG_FIFO, (header & 0xC0) | myId);
    //write 8 bits OFF to start with
    writeReg(REG_FIFO, 0);
    for (int i = 0; i < len; ++i)
        writeReg(REG_FIFO, ((const uint8_t*) ptr)[i]);
#endif
    for (int i = 0; i < len; ++i)
        printf("%02X", ((const uint8_t*) ptr)[i]);
    printf("\n");

    ////send after filling FIFO
    //setMode(MODE_TRANSMIT);

    while ((readReg(REG_IRQFLAGS2) & IRQ2_PACKETSENT) == 0)
        chThdYield();

    setMode(MODE_RECEIVE);
}

template< typename SPI >
void RF69<SPI>::transmitOOKOn () {
    setMode(MODE_TRANSMIT);
}

template< typename SPI >
void RF69<SPI>::transmitOOKOff () {
    setMode(MODE_RECEIVE);
    //setMode(MODE_SLEEP);
}

