// BME280 driver.

#include <libopencm3/stm32/usart.h>
#include <stdio.h>



template< typename I2C >
class BME280 {
  public:
    void init ();
    uint8_t getID (void);
    void update ();
    uint32_t getTemperature ();
    uint32_t getPressure ();
    uint32_t getHumidity ();
    float getfTemperature ();
    float getfPressure ();
    float getfHumidity ();

  private:
    uint16_t calib_T1;
    int16_t  calib_T2;
    int16_t  calib_T3;

    uint16_t calib_P1;
    int16_t  calib_P2;
    int16_t  calib_P3;
    int16_t  calib_P4;
    int16_t  calib_P5;
    int16_t  calib_P6;
    int16_t  calib_P7;
    int16_t  calib_P8;
    int16_t  calib_P9;

    uint8_t  calib_H1;
    int16_t  calib_H2;
    uint8_t  calib_H3;
    int16_t  calib_H4;
    int16_t  calib_H5;
    int8_t   calib_H6;

    uint8_t calib[32];

    uint8_t data[8];
    uint32_t t_fine;

    void getCalibration ();
    void calcTfine ();

  protected:
    enum {
      I2C_ADDR				= 0x76,
	  DEVICE_ID				= 0xD0,
	  CALIB00_REG			= 0x88,
	  CALIB25_REG			= 0xA1,
	  CALIB26_REG			= 0xE1,
	  HUMI_CONTROL_REG		= 0xF2,
	  MEAS_CONTROL_REG		= 0xF4,
	  CONFIG_REG			= 0xF5,
	  PRESSURE_MSB_REG		= 0xF7,
	  TEMPERATURE_MSB_REG	= 0xFA,
	  HUMIDITY_MSB_REG		= 0xFD
    };
    I2C i2c;
};

// driver implementation
template< typename I2C >
void BME280<I2C>::getCalibration () {
	i2c.read(I2C1, I2C_ADDR, CALIB00_REG, 24, calib);
	i2c.read(I2C1, I2C_ADDR, CALIB25_REG, 1, calib+24);
	i2c.read(I2C1, I2C_ADDR, CALIB26_REG, 7, calib+25);

	calib_T1 = calib[0] | calib[1]<<8;
	calib_T2 = calib[2] | calib[3]<<8;
	calib_T3 = calib[4] | calib[5]<<8;

	calib_P1 = calib[6] | calib[7]<<8;
	calib_P2 = calib[8] | calib[9]<<8;
	calib_P3 = calib[10] | calib[11]<<8;
	calib_P4 = calib[12] | calib[13]<<8;
	calib_P5 = calib[14] | calib[15]<<8;
	calib_P6 = calib[16] | calib[17]<<8;
	calib_P7 = calib[18] | calib[19]<<8;
	calib_P8 = calib[20] | calib[21]<<8;
	calib_P9 = calib[22] | calib[23]<<8;

	calib_H1 = calib[24];
	calib_H2 = calib[25] | calib[26]<<8;
	calib_H3 = calib[27];
	calib_H4 = (calib[28]<<4) | (calib[29]&0x0F);
	calib_H5 = (calib[29]>>4) | (calib[30]<<4);
	calib_H6 = calib[31];

//	int i = 0;
//	calib_T1 = calib[i++] | calib[i++]<<8;
//	calib_T2 = calib[i++] | calib[i++]<<8;
//	calib_T3 = calib[i++] | calib[i++]<<8;
//
//	calib_P1 = calib[i++] | calib[i++]<<8;
//	calib_P2 = calib[i++] | calib[i++]<<8;
//	calib_P3 = calib[i++] | calib[i++]<<8;
//	calib_P4 = calib[i++] | calib[i++]<<8;
//	calib_P5 = calib[i++] | calib[i++]<<8;
//	calib_P6 = calib[i++] | calib[i++]<<8;
//	calib_P7 = calib[i++] | calib[i++]<<8;
//	calib_P8 = calib[i++] | calib[i++]<<8;
//	calib_P9 = calib[i++] | calib[i++]<<8;
//
//	calib_H1 = calib[i++];
//	calib_H2 = calib[i++] | calib[i++]<<8;
//	calib_H3 = calib[i++];
//	calib_H4 = calib[i++]<<4 | calib[i]&0x0F;
//	calib_H5 = calib[i++]>>4 | calib[i++]<<4;
//	calib_H6 = calib[i++];
}

//calc temperature intermediate result
template< typename I2C >
void BME280<I2C>::calcTfine () {
	uint32_t raw_t = data[3]<<12 | data[4]<<4 | data[5]>>4;
	int32_t var1, var2;

	var1 = ((((raw_t>>3) - ((int32_t)calib_T1 <<1))) *
			((int32_t)calib_T2)) >> 11;

	var2 = (((((raw_t>>4) - ((int32_t)calib_T1)) *
			((raw_t>>4) - ((int32_t)calib_T1))) >> 12) *
			((int32_t)calib_T3)) >> 14;

	t_fine = var1 + var2;
	return;
}


template< typename I2C >
void BME280<I2C>::init () {
	i2c.setup(I2C1);
	uint8_t dat[1]={0};
	dat[0] = 0b101;
	i2c.write(I2C1, I2C_ADDR, HUMI_CONTROL_REG, 1, dat);
	dat[0] = 0b10110111;
	i2c.write(I2C1, I2C_ADDR, MEAS_CONTROL_REG, 1, dat);
	dat[0] = 0b10100000;
	i2c.write(I2C1, I2C_ADDR, CONFIG_REG, 1, dat);

	getCalibration();
}

template< typename I2C >
uint8_t BME280<I2C>::getID (void) {
	uint8_t id[1] = {0};
	i2c.read(I2C1, I2C_ADDR, DEVICE_ID , 1, id);
	return id[0];
}

template< typename I2C >
void BME280<I2C>::update () {
	i2c.read(I2C1, I2C_ADDR, PRESSURE_MSB_REG, 8, data);
	calcTfine();
	return;
}

//get temperature as unsigned integer in milli-degrees
template< typename I2C >
uint32_t BME280<I2C>::getTemperature () {
	return (t_fine * 50 + 128) >> 8;
}

//get pressure as unsigned integer in deci-pascal (0.001 mbar)
template< typename I2C >
uint32_t BME280<I2C>::getPressure () {
	uint32_t raw_p = data[0]<<12 | data[1]<<4 | data[2]>>4;
	int64_t var3, var4, p;
	var3 = ((int64_t)t_fine) - 128000;
	var4 = var3 * var3 * (int64_t)calib_P6;
	var4 = var4 + ((var3*(int64_t)calib_P5)<<17);
	var4 = var4 + (((int64_t)calib_P4)<<35);
	var3 = ((var3 * var3 * (int64_t)calib_P3)>>8) + ((var3 * (int64_t)calib_P2)<<12);
	var3 = (((((int64_t)1)<<47)+var3))*((int64_t)calib_P1)>>33;

	if (var3 == 0) {
		p = 0;  // avoid exception caused by division by zero
	} else {
		p = 1048576 - raw_p;
		p = (((p<<31) - var4)*3125) / var3;
		var3 = (((int64_t)calib_P9) * (p>>13) * (p>>13)) >> 25;
		var4 = (((int64_t)calib_P8) * p) >> 19;

		p = ((p + var3 + var4) >> 8) + (((int64_t)calib_P7)<<4);
	}
	return (10*p) >> 8;
}

//get humidity as unsigned integer in 0.001 %rH
template< typename I2C >
uint32_t BME280<I2C>::getHumidity () {
	uint16_t raw_h = data[6]<<8 | data[7];
	int32_t v_x1_u32r;

	v_x1_u32r = (t_fine - ((int32_t)76800));

	v_x1_u32r = (((((raw_h << 14) - (((int32_t)calib_H4) << 20) -
				(((int32_t)calib_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
				(((((((v_x1_u32r * ((int32_t)calib_H6)) >> 10) *
				(((v_x1_u32r * ((int32_t)calib_H3)) >> 11) + ((int32_t)32768))) >> 10) +
				((int32_t)2097152)) * ((int32_t)calib_H2) + 8192) >> 14));

	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
				((int32_t)calib_H1)) >> 4));

	v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
	v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
	//float h = (v_x1_u32r>>12);
	//return h / 1024.0;
	return (1000*(v_x1_u32r>>12))>>10;
}

//get temperature as float
template< typename I2C >
float BME280<I2C>::getfTemperature () {
	return getTemperature()/1000.0;
}

//get pressure as float
template< typename I2C >
float BME280<I2C>::getfPressure () {
	return getPressure()/1000.0;
}

//get humidity float
template< typename I2C >
float BME280<I2C>::getfHumidity () {
	return getHumidity()/1000.0;
}






