//============================================================================
// Name        : rf-ook.cpp
// Author      : SevenWatt
// Version     : 1.0
// Copyright   : sevenwatt.com (c) 2015
// Description : Receive and decode OOK signals from various sources
//
//============================================================================
#define STATLOG 1

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#include "spi.h"
#include "rf69.h"
#include "rf69-ook.h"

//configuration items
uint8_t DIO2 = 5; //Wiring GPIO. 5 = raspi GPIO24 = pin DIO2(=DATA)


#define FREQ_BAND 433
//#define FREQ_BAND 868

#if FREQ_BAND == 433
uint32_t frqkHz = 433920;
#else
uint32_t frqkHz = 868280;
#endif

uint8_t g_rssiavg = 0;
uint8_t fixthd = 72;
uint32_t bitrate = 32768; //max OOK 32768bps, max FSK 300000bps
uint8_t bw = 16; //0=250kHz, 8=200kHz, 16=167kHz, 1=125kHz, 9=100kHz, 17=83kHz 2=63kHz, 10=50kHz

//Configure the main heart beat to sample the RFM69 DATA pin
const uint8_t tsample = 25; //us samples
uint32_t samplesSec = 1000000 / tsample;

// volatile uint32_t sampleTicks = 0;
// extern "C" void SysTick_Handler(void) {
// sampleTicks++;
// }

RF69A<SpiDev0> rfa;
#include "decodeOOK.h"
//#include "decodeOOK_TEST.h"

const uint8_t max_decoders = 6; //Too many decoders slows processing down.
DecodeOOK* decoders[max_decoders] = { NULL };
uint8_t di = 0;
void printOOK(class DecodeOOK* decoder); //void relay (class DecodeOOK* decoder);

#if FREQ_BAND == 433
//433MHz
#include "decoders433.h"
//OregonDecoderV2   orscV2(  5, "ORSV2", printOOK);
//CrestaDecoder     cres(    6, "CRES ", printOOK);
KakuDecoder kaku( 7, "KAKU ", printOOK);
//XrfDecoder        xrf(     8, "XRF  ", printOOK);
//HezDecoder        hez(     9, "HEZ  ", printOOK);
ElroDecoder       elro(   10, "ELRO ", printOOK);
//FlamingoDecoder   flam(   11, "FMGO ", printOOK);
//SmokeDecoder      smok(   12, "SMK  ", printOOK);
//ByronbellDecoder  byro(   13, "BYR  ", printOOK);
//KakuADecoder      kakuA(  14, "KAKUA", printOOK);
WS249 ws249( 20, "WS249", printOOK);
Philips phi( 21, "PHI  ", printOOK);
OregonDecoderV1 orscV1( 22, "ORSV1", printOOK);
//OregonDecoderV3   orscV3( 23, "ORSV3", printOOK);
void setupDecoders() {
	decoders[di++] = &ws249;
	decoders[di++] = &phi;
	decoders[di++] = &orscV1;
	decoders[di++] = &kaku;
	decoders[di++] = &elro;
}
#else
//868MHz
#include "decoders868.h"
//VisonicDecoder    viso(    1, "VISO ", printOOK);
//EMxDecoder          emx(     2, "EMX  ", printOOK);
//KSxDecoder        ksx(     3, "KSX  ", printOOK);
FSxDecoder fsx(4, "FS20 ", printOOK);
//FSxDecoderA       fsxa(   44, "FS20A", printOOK);
//
void setupDecoders() {
	//   decoders[di++] = &emx;
	decoders[di++] = &fsx;
}
#endif
// End config items --------------------------------------------------------

//small circular buffer for ON rssi signals
#define RSSI_BUF_EXP 3 //keep it powers of 2
#define RSSI_BUF_SIZE  (1<<RSSI_BUF_EXP)
uint8_t rssi_buf[RSSI_BUF_SIZE];
uint8_t rssi_buf_i = 0;

//uint8_t rssi_b[4096];
//uint16_t rssi_bi = 0;


//Optimize
//   BitRate
//   Thresholding
//   Moving Avg filter len
//   RXBW
int fl = 7;
int sd = 3;
uint8_t delta_thd = 6;
uint32_t bitrates[] = {32768, 20000, 12000, 8000, 3000, 1000, 0};
uint8_t bri = 0;

void receiveOOK(uint32_t br, uint8_t fl, uint8_t bw, uint8_t sdf);

void receiveOOK() {
	// while(true) {
		// while (bitrates[bri] != 0) {
			// bitrate = bitrates[bri++];
			// for (fl = 13; fl > 0; fl -= 4) {
				// for (sd = 3; sd > 1; sd--) {
					// printf("scan br%d, BW%d, FL%d, SD%d\r\n", bitrate, bw, fl, sd);
					// receiveOOK(bitrate, fl, bw, sd);
				// }
			// }
			
		// }
		// bri = 0;
		// printf("Finished full loop\r\n");
	// }
	while(true) {
			bitrate = 3000;
			fl = 13;
			sd = 2;
				for ( fixthd = 64; fixthd < 79; fixthd += 2) {
					printf("scan br%d, BW%d, FL%d, THD%d\r\n", bitrate, bw, fl, fixthd);
					receiveOOK(bitrate, fl, bw, sd);
				}
		printf("Finished full loop\r\n");
	}
}


void printRSSI() {
	uint16_t avgonrssi = 0;
	uint16_t avgoffrssi = 0;
	for (uint8_t i = 0; i < RSSI_BUF_SIZE; i += 2) {
		avgonrssi += rssi_buf[i];
		avgoffrssi += rssi_buf[i + 1];
		//        printf( " (%d/%d)", rssi_buf[i+1],rssi_buf[i]);
	}
	printf(" (,%d,%d,)", avgoffrssi >> (RSSI_BUF_EXP - 1),
	avgonrssi >> (RSSI_BUF_EXP - 1));
}

void printOOK(class DecodeOOK* decoder) {
	uint8_t pos;
	const uint8_t* data = decoder->getData(pos);

	static uint32_t last_print = 0;
	static uint8_t mesgcnt = 1;
	uint32_t now = millis();
	printf("BR,%d, BW,%d, FL,%d, SD,%d, THD,%d, NS,%d,", bitrate, bw, fl, sd, fixthd, g_rssiavg);
	if (now - last_print < 1000) {
		mesgcnt++;
		printf("%d,  ,   %3d, %1d, ", now, now - last_print, mesgcnt);
		//printf("%12d ", now);
	} else {
		mesgcnt = 1;
		printf("%d, %3d,  ,   %1d, ", now, (now - last_print) / 1000 ,mesgcnt);
		//printf("\r\n%12d ", now);
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
	printf("%s, ", decoder->tag);
	//Serial.print(' ');
	for (uint8_t i = 0; i < pos; ++i) {
		printf("%02x", data[i]);
	}
	printRSSI();
	printf("\r\n");

	//    uint16_t j = rssi_bi;
	//    for (uint16_t i = 0; i < 4096; i++) { //not interested in last 300 samples.
	//        printf( "%d,", rssi_b[j++]);
	//        j &= 0xFFF;
	//    }
	//    printf( "\r\n");

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


void receiveOOK(uint32_t br, uint8_t fl, uint8_t bw, uint8_t sdf) {
	//moving average buffer
	uint8_t avg_len = fl;
	uint32_t filter = 0;
	uint32_t sig_mask = (1 << avg_len) - 1;

	//Experimental: Fixed threshold
	rfa.OOKthdMode(0x40); //0x00=fix, 0x40=peak, 0x80=avg
	rfa.setBitrate(br);
	//rfa.setBitrate(3000);

	rfa.setFrequency(frqkHz);
	rfa.setBW(bw);
	rfa.setThd(fixthd);
	rfa.readAllRegs();
	uint8_t t_step = tsample;
	uint32_t now = micros();
	uint32_t soon = now + t_step;

	uint32_t nrssi = 0;
	//uint64_t sumrssi = 0;
	//uint64_t sumsqrssi = 0;
	uint32_t sumrssi = 0;
	uint32_t sumsqrssi = 0;
	uint32_t rssivar = 0;

	uint8_t rssimax = fixthd + 6;
	uint8_t slicethd = 100;
	uint8_t decthdcnt = 0;
	uint8_t prev_thd = 0;
	uint8_t max_thd = 0;
	uint8_t prev_rssi = 0;
	uint16_t log = 0;
	uint16_t log1 = 0;

	uint32_t new_edge = now;
	uint32_t last_edge = new_edge;
	uint8_t last_data = digitalRead(DIO2);
	//uint8_t last_data = ~rfa.readRSSI() > slicethd;

	uint16_t flip_cnt = 0;
	uint16_t flush_cnt = 0;
	uint16_t flush_max = 10000 / tsample; //10ms
	//flush_max = 5000/tsample;
	uint8_t last_steady_rssi = 0;
	uint8_t rssi_q_off = 3;
	uint8_t rssi_q_len = avg_len + rssi_q_off + 1;
	uint8_t rssi_q[rssi_q_len];
	uint8_t rssi_qi = 0;

	uint32_t ts_rssi = micros();
	uint8_t rssi = ~rfa.readRSSI();
	uint32_t thdUpd = millis();
	uint32_t thdUpdCnt = 0;
	
	uint32_t startloop = millis();
	while (millis()-startloop < 300000) {
		//rssi = ~rfa.readRSSI();

		//		rssi_b[rssi_bi++] = rssi;
		//		rssi_bi &= 0xFFF;

		//		if (log1 > 0)
		//			log1++;
		//
		//		if (log1 > 4000) {
		//		    uint16_t j = rssi_bi;
		//		    for (uint16_t i = 0; i < 4096; i++) {
		//		        printf( "%d,", rssi_b[j++]);
		//		        j &= 0xFFF;
		//		    }
		//		    printf( "\r\n");
		//		    log1=0;
		//		}


		uint8_t data_in = digitalRead(DIO2);
		//uint8_t data_in = rssi > slicethd;
		filter = (filter << 1) | (data_in & 0x01);
		//efficient bit count, from literature
		uint8_t c = 0;
		uint32_t v = filter & sig_mask;
		for (c = 0; v; c++) {
			v &= v - 1; // clear the least significant bit set
		}
		uint8_t data_out = (c > (avg_len >> 1)); //bit count moving average
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

		static uint32_t delay_rssi = 0;
		delay_rssi++;
		if (data_out != last_data) {
			new_edge = micros();
			
			processBit(micros() - last_edge, last_data,
			last_steady_rssi);
			//if (log1 == 1) printf("%d\r\n", tsample *(new_edge - last_edge));
			last_edge = new_edge;
			last_data = data_out;
			flush_cnt = 0;
			delay_rssi = 0;
			flip_cnt++;
			
						last_steady_rssi = ~rfa.readRSSI();
			

		} else if (flush_cnt >= flush_max /* && !data_out */) {
			//send fake pulse to notify end of transmission to decoders
			processBit(micros() - last_edge, last_data, 0);
			processBit(1, !last_data, 0);
		} else if (delay_rssi == 1) {
			//read rssi in loop after flip ~25us delayed
			//last_steady_rssi = ~rfa.readRSSI();
		}
		//TODO: depends on moment of reading rssi
		//last_steady_rssi = delayed_rssi;

		flush_cnt++;

		//		//statistics update every cycle
		//		sumrssi += rssi;
		//		sumsqrssi += rssi * rssi;
		//		nrssi++;
		//		if (rssi > rssimax)
		//			rssimax = rssi;
		//statistics update every millisecond
		if ((micros() - ts_rssi) > (1000)) {
			rssi = ~rfa.readRSSI();
			sumrssi += rssi;
			sumsqrssi += rssi * rssi;
			nrssi++;
			ts_rssi = micros();
			if (rssi > rssimax)
			rssimax = rssi;
		}

		//Update minimum slice threshold (fixthd) every 10s
		//systime_t ts_printnow = chVTGetSystemTime();
		if (ts_thdUpdNow - thdUpd >= 10000) {
			//rssivar = ((sumsqrssi - sumrssi*sumrssi/nrssi) / (nrssi-1)); //64 bits
			rssivar = ((sumsqrssi - ((sumrssi>>8)*((sumrssi<<8)/nrssi))) / (nrssi-1)); //32bits n<65000
			uint32_t rssiavg = sumrssi / nrssi;
			//determine stddev (no sqrt avaialble?)
			uint8_t stddev = 1;
			while (stddev < 10) {
				//safe until sqr(15)
				if (stddev * stddev >= rssivar)
				break;
				stddev++;
			}
			//adapt rssi thd to noise level if variance is low
			if (rssivar < 36) {
				/*uint8_t*/ delta_thd = sd * stddev;
				if (delta_thd < 3/*6*/)
				delta_thd = 3/*6*/;
				//if (delta_thd < 10)  delta_thd = 10;
				//const uint8_t delta_thd = 4;
//TODO: Repair after test
g_rssiavg = rssiavg;
				// if (fixthd != rssiavg + delta_thd) {
					// fixthd = rssiavg + delta_thd;
					// //if (fixthd < 70) fixthd = 70;
					// rfa.setThd(fixthd);
					// //printf( "THD:%3d\r\n", fixthd);
				// } else {
					// //printf( "THD:keep\r\n");
				// }
			} else {
				//printf( "THD:keep\r\n");
				//printf( "RSSI: %3d, %5d\r\n", rssiavg, rssivar);
			}

#if STATLOG
			printf("RSSI: %3d(v%3d-s%2d-m%3d) THD:fix/peak/maxp:%d/%d/%d\r\n",
			rssiavg, rssivar, stddev, rssimax, fixthd, slicethd, max_thd);
			printf("%d polls took %d ms = %d us - flips = %d\r\n", thdUpdCnt,
			(ts_thdUpdNow - thdUpd),
			1000*(ts_thdUpdNow - thdUpd)/thdUpdCnt, flip_cnt);
#endif

			nrssi = sumrssi = sumsqrssi = rssimax = max_thd = 0;
			thdUpd = ts_thdUpdNow;
			thdUpdCnt = 0;
			flip_cnt = 0;

			//clear scope trigger
			//palWritePad(GPIOB, 4, 0);
		}

		//emulate PEAK-mode OOK threshold
		decthdcnt++;
		if (((decthdcnt & 7) == 0) and (slicethd > fixthd)) {
			//decrement slicethd every 8th poll (=200us) with 0.5dB
			slicethd--;
		}
		prev_thd = slicethd;
		if (rssi > slicethd + 12) {
			//deal with outlier rssi?
			slicethd = rssi - 12;
			//limit printing
			if (slicethd > prev_thd + 12) {
				//printf( "PEAK-THD: set %d\r\n", slicethd);
			}
			if (rssi > 200) {
				slicethd = 188;
				//printf( "PEAK-THD: outlier rssi %d\r\n", rssi);
			}
			if (slicethd < fixthd)
			slicethd = fixthd;
		}
		if (slicethd > max_thd)
		max_thd = slicethd;


		thdUpdCnt++;

		if (rssi > 140) {
			//set scope trigger
			//palWritePad(GPIOB, 4, 1);
			//log = 1;
			log1 = 1;
		}
		if (log > 0) {
			printf("%d,", rssi);
			log++;
			if (log > 2000)
			log = 0;
		}

		//sleep at resolutions higher then system tick
		
		// while ((soon - micros() > 0) && !((soon - micros()) & 0x80000000)) {
		// //nop
		// }
		
		int32_t delay_us = soon - micros();
		if (delay_us > 0)
		delayMicroseconds(delay_us);
		
		soon = micros() + t_step;
	}
	return;
}

int main() {
	wiringPiSetup();
	int myFd = wiringPiSPISetup (0, 8000000);

	if (myFd < 0) {
		printf("Can't open the SPI bus: %d\n", errno);
		return 1;
	}
	uint8_t nodeId = 60;

	printf("\n[rf_ook] on raspi\n");

	pinMode (DIO2, INPUT);
	setupDecoders();
	if (di > max_decoders)
	printf("ERROR: decoders-array too small. Memory corruption.");

	rfa.init(nodeId, 42, frqkHz);

	while (true) {
		receiveOOK();
	}
}
