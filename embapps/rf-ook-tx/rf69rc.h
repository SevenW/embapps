// Native mode RF69 driver.

template<typename SPI>
class RF69RC: public RF69<SPI> {
public:
	void init(uint8_t id, uint8_t group, int freq);
	void initOOKpckt(uint8_t id, uint8_t group, int freq);
	void initOOKcont(uint8_t id, uint8_t group, int freq);
	void setBitrate (uint32_t br);
	void sendook(uint8_t header, const void* ptr, int len);
	void transmitOOKOn();
	void transmitOOKOff();

protected:
	enum {
		REG_FIFO = 0x00,
		REG_OPMODE = 0x01,
        REG_BRMSB = 0x03,
		REG_FRFMSB = 0x07,
		REG_PALEVEL = 0x11,
		REG_LNAVALUE = 0x18,
		REG_AFCMSB = 0x1F,
		REG_AFCLSB = 0x20,
		REG_FEIMSB = 0x21,
		REG_FEILSB = 0x22,
		REG_RSSIVALUE = 0x24,
		REG_IRQFLAGS1 = 0x27,
		REG_IRQFLAGS2 = 0x28,
		REG_SYNCVALUE1 = 0x2F,
		REG_SYNCVALUE2 = 0x30,
		REG_NODEADDR = 0x39,
		REG_BCASTADDR = 0x3A,
		REG_PKTCONFIG2 = 0x3D,
		REG_AESKEYMSB = 0x3E,

		MODE_SLEEP = 0 << 2,
		MODE_STANDBY  = 1<<2,
		MODE_TRANSMIT = 3 << 2,
		MODE_RECEIVE = 4 << 2,

		IRQ1_MODEREADY = 1 << 7,
		IRQ1_RXREADY = 1 << 6,
		IRQ1_SYNADDRMATCH = 1 << 0,

		IRQ2_FIFONOTEMPTY = 1 << 6,
		IRQ2_PACKETSENT = 1 << 3,
		IRQ2_PAYLOADREADY = 1 << 2,
	};
};

// driver implementation

static const uint8_t configRegsOOKpckt [] = {
    0x01, 0x00, // OpMode = sleep
    0x02, 0x08, // DataModul = packet mode, OOK
    0x03, 0x19, // BitRateMsb, data rate = 5000bps (200us)
    0x04, 0x00, // BitRateLsb, divider = 32 MHz / 650
    //0x05, 0x02, // FdevMsb = 45 KHz
    //0x06, 0xE1, // FdevLsb = 45 KHz
    //0x0B, 0x20, // Low M
    //0x19, 0x4A, // RxBw 100 KHz
    //0x1A, 0x42, // AfcBw 125 KHz
    //0x1E, 0x0C, // AfcAutoclearOn, AfcAutoOn
    ////0x25, 0x40, //0x80, // DioMapping1 = SyncAddress (Rx)
    //0x29, 0xA0, // RssiThresh -80 dB
    0x2D, 0x00, // PreambleSize = 0 NO PREAMBLE
    0x2E, 0x00, // SyncConfig = sync OFF
    //0x2F, 0x2D, // SyncValue1 = 0x2D
    0x37, 0x80, // PacketConfig1 = variable length, advanced items OFF
    0x38, 0x00, // PayloadLength = 0, unlimited
    //0x3C, 0x8F, // FifoTresh, not empty, level 15
    //0x3D, 0x12, // 0x10, // PacketConfig2, interpkt = 1, autorxrestart off
    //0x6F, 0x20, // TestDagc ...
    //0x71, 0x02, // RegTestAfc
    0
};

static const uint8_t configRegsOOKcont[] = {
		0x01, 0x04, // OpMode = standby
		0x02, 0x68, // DataModul = conti mode, ook, no shaping
		0x03, 0x03, // BitRateMsb, 32768bps
		0x04, 0xD1, // BitRateLsb, divider = 32 MHz / 650
		//0x05, 0x01, // FdevMsb = 25 KHz
		//0x06, 0x99, // FdevLsb = 25 KHz
		//0x07, 0xD9, // FrfMsb, freq = 868.250 MHz
		//0x08, 0x13, // FrfMib, divider = 14221312
		//0x09, 0x00, // FrfLsb, step = 61.03515625
		//0x0B, 0x20, // AfcCtrl, afclowbetaon
		//0x18, 0x81, // LNA fixed highest gain, Z=200ohm
		//0x19, 0x40, // RxBw DCC=4%, Man=00b Exp = 0 =>BWOOK=250.0kHz
		//0x1B, 0x40, // OOK peak, 0.5dB once/chip
		//0x1C, 0x80, // OOK avg thresh /4pi
		//0x1D, 0x38, // OOK fixed thresh
		//0x1E, 0x00, // No auto AFC
		0x25, 0x80, // Dio 0: RSSI 1: Dclk 2: Data 3: RSSI
		0x26, 0x37, // Dio 5: ModeReady, CLKOUT = OFF
		//0x29, 0xFF, // RssiThresh: lowest possible threshold to start receive
		//0x2E, 0x00, // SyncConfig = sync off
		//0x3C, 0x8F, // FifoTresh, not empty, level 15
		//0x3D, 0x10, // PacketConfig2, interpkt = 1, autorxrestart off
		//0x58, 0x2D, // Sensitivity boost (TestLNA)
		//0x6F, 0x20, // TestDagc ...
		0 };

template<typename SPI>
void RF69RC<SPI>::init(uint8_t id, uint8_t group, int freq) {
	RF69<SPI>::init(id, group, freq);
	printf("RF69RC::init()\n");
	//this->configure(configRegsOOKcont);
	//this->setFrequency(freq);
}

template<typename SPI>
void RF69RC<SPI>::initOOKpckt(uint8_t id, uint8_t group, int freq) {
	this->configure(configRegsOOKpckt);
	this->setFrequency(freq);
}

template<typename SPI>
void RF69RC<SPI>::initOOKcont(uint8_t id, uint8_t group, int freq) {
	this->configure(configRegsOOKcont);
	this->setFrequency(freq);
}

template< typename SPI >
void RF69RC<SPI>::setBitrate (uint32_t br) {
    if (br < 489) {
        br = 0xFFFF;
    } else {
        br = 32000000L / br;
    }
    this->writeReg(REG_BRMSB, br >> 8);
    this->writeReg(REG_BRMSB+1, br);
}

template<typename SPI>
void RF69RC<SPI>::sendook(uint8_t header, const void* ptr, int len) {
	//this->setMode(MODE_TRANSMIT);

#if RF69_SPI_BULK
	this->spi.enable();
	this->spi.transfer(REG_FIFO | 0x80);
	//write 8 bits OFF to start with
	this->writeReg(REG_FIFO, 0);
	for (int i = 0; i < len; ++i)
	this->spi.transfer(((const uint8_t*) ptr)[i]);
	this->spi.disable();
#else
	//write 8 bits OFF to start with
	this->writeReg(REG_FIFO, 0);
	for (int i = 0; i < len; ++i)
		this->writeReg(REG_FIFO, ((const uint8_t*) ptr)[i]);
#endif
//	for (int i = 0; i < len; ++i)
//		printf("%02X", ((const uint8_t*) ptr)[i]);
//	printf("\n");

	//send after filling FIFO (RFM should be in sleep or standby)
	this->setMode(MODE_TRANSMIT);
	while ((this->readReg(REG_IRQFLAGS2) & IRQ2_PACKETSENT) == 0)
		chThdYield();

	this->setMode(MODE_STANDBY);
}

template<typename SPI>
void RF69RC<SPI>::transmitOOKOn() {
	this->setMode(MODE_TRANSMIT);
}

template<typename SPI>
void RF69RC<SPI>::transmitOOKOff() {
	//this->setMode(MODE_RECEIVE);
	this->setMode(MODE_STANDBY);
}

