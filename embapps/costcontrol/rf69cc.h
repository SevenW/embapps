// ELV Cost Control / Energy Count 3000 - RF69 driver.

template<typename SPI>
class RF69CC: public RF69<SPI> {
public:
	//void init(uint8_t id, uint8_t group, int freq);
	void initCCreceive(int freq);
	void exitCCreceive();
	void setBitrate(uint32_t br);
	void setPayloadLen(uint32_t plen);
	int receive_fixed(void* ptr, int len);

protected:
	enum {
		REG_FIFO = 0x00,
		REG_OPMODE = 0x01,
		REG_BRMSB = 0x03,
		REG_FRFMSB = 0x07,
		REG_PALEVEL = 0x11,
		REG_LNAVALUE = 0x18,
		REG_AFCFEI = 0x1E,
		REG_AFCMSB = 0x1F,
		REG_AFCLSB = 0x20,
		REG_FEIMSB = 0x21,
		REG_FEILSB = 0x22,
		REG_RSSIVALUE = 0x24,
		REG_IRQFLAGS1 = 0x27,
		REG_IRQFLAGS2 = 0x28,
		REG_SYNCVALUE1 = 0x2F,
		REG_SYNCVALUE2 = 0x30,
		REG_PAYLOADLEN = 0x38,
		REG_NODEADDR = 0x39,
		REG_BCASTADDR = 0x3A,
		REG_PKTCONFIG2 = 0x3D,
		REG_AESKEYMSB = 0x3E,

		MODE_SLEEP = 0 << 2,
		MODE_STANDBY = 1 << 2,
		MODE_TRANSMIT = 3 << 2,
		MODE_RECEIVE = 4 << 2,

		IRQ1_MODEREADY = 1 << 7,
		IRQ1_RXREADY = 1 << 6,
		IRQ1_TIMEOUT = 1 << 2,
		IRQ1_SYNADDRMATCH = 1 << 0,

		IRQ2_FIFONOTEMPTY = 1 << 6,
		IRQ2_FIFOLEVEL = 1 << 5,
		IRQ2_PACKETSENT = 1 << 3,
		IRQ2_PAYLOADREADY = 1 << 2,

		PCFG2_RXRESTART = 1 << 2,
	};
	uint16_t fei;
};

// driver implementation

static const uint8_t configRegsCC[] = {
		0x01, 0x00, // OpMode = sleep
		0x02, 0x00, // DataModul = packet mode, fsk
		0x03, 0x06, // BitRateMsb, data rate = 20.3 khz
		0x04, 0x28, // BitRateLsb, divider = 32 MHz / 650
		0x05, 0x01, // FdevMsb = 20 KHz
		0x06, 0x48, // FdevLsb = 20 KHz
		//0x05, 0x02, // FdevMsb = 45 KHz
		//0x06, 0xE1, // FdevLsb = 45 KHz
		//0x05, 0x09, // FdevMsb = 150 KHz
		//0x06, 0x9B, // FdevLsb = 150 KHz
		0x07, 0xD9, // FrfMsb, freq = 868.250 MHz
		0x08, 0x13, // FrfMib, divider = 14221312
		0x09, 0x00, // FrfLsb, step = 61.03515625
		//0x0B, 0x20, // Low M
		0x0B, 0x00, // High M

		//TODO: AGC disrupts reception of FSK signal. Why?
		//0x18, 0x80, // LNA variable gain, Z=200ohm
		0x18, 0x81, // LNA fixed gain 1, Z=200ohm
		0x19, 0x43, // RxBw 62.5 KHz
		//0x19, 0x42, // RxBw 125 KHz
		//0x19, 0x41, // RxBw 250 KHz
		//0x19, 0x48, // RxBw 400 KHz

		//0x1A, 0x41, // AfcBw 250 KHz - Too noise for good AFC?
		0x1A, 0x42, // AfcBw 2 = 125 KHz 3= 62khz 4=31khz

		//TODO: AFC disrupts reception of FSK signal, but less then AGC. Why?
		0x1E, 0x0C, // AfcAutoclearOn, AfcAutoOn
		//0x1E, 0x00, // No auto AFC

		0x26, 0x07, // disable clkout
		0x29, 0xA8, // RssiThresh -80 dB
		0x2B, 0x80, // Timeout after RSSI Threshold 2 * 128 bits
		//0x2D, 0x00, // PreambleSize = 0

		//0x2E, 0x00, // SyncConfig = sync off
		0x2E, 0xA0, // SyncConfig = sync on, sync size = 5, tol = 0
		0x2F, 0x13, // SyncValues = 0x13, 0xF1, 0x85, 0xD3, 0xAC
		0x30, 0xF1, //
		0x31, 0x85, //
		0x32, 0xD3, //
		0x33, 0xAC, //
		//0x34, 0xD2, // 0xD2 descrambles to HDLC code 0x7E

//		//0x2E, 0x00, // SyncConfig = sync off
//		0x2E, 0x88, // SyncConfig = sync on, sync size = 2, tol = 0
//		0x2F, 0xD3, // SyncValues = 0x13, 0xF1, 0x85, 0xD3, 0xAC
//		0x30, 0xAC, //

		//TODO: Fixed packet lens fails to generate PayloadReady interrupt! Why?
		0x37, 0x00, // PacketConfig1 = variable, no DC-free, no filtering
		//0x37, 0x88, // PacketConfig1 = fixed, no DC-free, no filtering

		0x38, 0x38, // PayloadLength = 41 payload + 2 ctrl + 3-4 postamble + 8 stuffing
		0x3C, 0x8F, // FifoTresh, not empty, level 15
		0x3D, 0x12, // 0x10, // PacketConfig2, interpkt = 1, autorxrestart on
		//0x6F, 0x20, // TestDagc Low M
		0x6F, 0x30, // TestDagc High M
		//0x71, 0x05, // RegTestAfc Low M F-offset in 488Hz steps
		0 };

//template<typename SPI>
//void RF69CC<SPI>::init(uint8_t id, uint8_t group, int freq) {
//	RF69<SPI>::init(id, group, freq);
//	printf("RF69CC::init()\n");
//	this->setFrequency(freq);
//	RF69<SPI>::sleep();
//}

template<typename SPI>
void RF69CC<SPI>::initCCreceive(int freq) {
	RF69<SPI>::sleep();
	this->configure(configRegsCC);
	//this->configure(configRegsCC);
	this->setFrequency(freq);
	//RF69<SPI>::sleep();
}

template<typename SPI>
void RF69CC<SPI>::exitCCreceive() {
	RF69<SPI>::sleep();
	this->writeReg(0x18, 0x80); // LNA auto gain, Z=200ohm
	this->writeReg(0x2B, 0x00); // Disable timeout after RSSI Threshold
}

template<typename SPI>
void RF69CC<SPI>::setBitrate(uint32_t br) {
	if (br < 489) {
		br = 0xFFFF;
	} else {
		br = 32000000L / br;
	}
	this->writeReg(REG_BRMSB, br >> 8);
	this->writeReg(REG_BRMSB + 1, br);
}

template<typename SPI>
void RF69CC<SPI>::setPayloadLen(uint32_t plen) {
	this->writeReg(REG_PAYLOADLEN, plen );
}

static uint16_t lcnt = 0;
template<typename SPI>
int RF69CC<SPI>::receive_fixed(void* ptr, int len) {

	if (this->mode != MODE_RECEIVE)
		this->setMode(MODE_RECEIVE);
	else {
		static uint8_t lastFlag;
		if ((this->readReg(REG_IRQFLAGS1) & IRQ1_RXREADY) != lastFlag) {
			lastFlag ^= IRQ1_RXREADY;
			if (lastFlag) { // flag just went from 0 to 1
				this->rssi = this->readReg(REG_RSSIVALUE);
				this->lna = (this->readReg(REG_LNAVALUE) >> 3) & 0x7;
#if RF69_SPI_BULK
				spi.enable();
				spi.transfer(REG_AFCMSB);
				afc = spi.transfer(0) << 8;
				afc |= spi.transfer(0);
				spi.disable();
#else
				this->afc = this->readReg(REG_AFCMSB) << 8;
				this->afc |= this->readReg(REG_AFCLSB);
				printf("RSSI %d, LNA %d, AFC %d\r\n", this->rssi, this->lna,
						this->afc);
				this->fei = this->readReg(REG_FEIMSB) << 8;
				this->fei |= this->readReg(REG_FEILSB);
				printf("FEI %d\r\n", this->fei);
#endif
			}
		}
//		lcnt++;
//		if (lcnt == 0)
//			printf("IRQ1 %02X IRQ2 %02X\r\n", this->readReg(REG_IRQFLAGS1),
//					this->readReg(REG_IRQFLAGS2));
		if (this->readReg(REG_IRQFLAGS2) & IRQ2_PAYLOADREADY) {
//			printf("---> IRQ1 %02X IRQ2 %02X\r\n", this->readReg(REG_IRQFLAGS1),
//					this->readReg(REG_IRQFLAGS2));

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
			//int count = readReg(REG_FIFO);
			int count = 56;
			for (int i = 0; i < count; ++i) {
				uint8_t v = this->readReg(REG_FIFO);
				if (i < len)
					((uint8_t*) ptr)[i] = v;
			}
#endif
//			printf("<--- IRQ1 %02X IRQ2 %02X\r\n", this->readReg(REG_IRQFLAGS1), this->readReg(REG_IRQFLAGS2));
			lastFlag = 0; //just force clear. The loop might be too slow
			return count;

		}

		//Handle timeout after RSSI interrupt occurred, without receiving data
		if (this->readReg(REG_IRQFLAGS1) & IRQ1_TIMEOUT) {
//			printf("T--> IRQ1 %02X IRQ2 %02X\r\n", this->readReg(REG_IRQFLAGS1),
//					this->readReg(REG_IRQFLAGS2));
			//Flush FIFO
			while (this->readReg(REG_IRQFLAGS2) & IRQ2_FIFONOTEMPTY) {
				  this->readReg(REG_FIFO);
			}
			lastFlag = 0;
			this->writeReg(REG_PKTCONFIG2, this->readReg(REG_PKTCONFIG2) | PCFG2_RXRESTART); //b3 = RXRestart command
//			printf("T<-- IRQ1 %02X IRQ2 %02X\r\n", this->readReg(REG_IRQFLAGS1),
//					this->readReg(REG_IRQFLAGS2));
		}

	}
	return -1;
}

