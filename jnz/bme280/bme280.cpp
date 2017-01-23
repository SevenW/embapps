// BME280 demo for the JeeNode Zero.
//
// See also https://github.com/flabbergast/libopencm3-ex/blob/master/src/i2ctest/i2ctest.c

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>

#include "i2c.h"
#include "bme280.h"

BME280<I2CDev> bme280;

uint32_t timertick = 0;
int j = 0, c = 0;

uint32_t millis ();

void setup (void) {
    printf("\n[bme280]\n");

	//gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);   // rev1
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);    // rev3
    //gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8);    // rev4

    timertick = millis();

    bme280.init();
    if (bme280.getID() == 0x60) {
        printf("BME280 initialized\n");
    } else {
        printf("BME280 not detected\n");
    }

    return;
}

void loop () {
    while ((millis() - timertick) < 2000)
    	__asm("");
    timertick = millis();

//	for (int i = 0; i < 1000000; ++i)
//            __asm("");

    //gpio_toggle(GPIOA, GPIO15); // rev1
    gpio_toggle(GPIOB, GPIO5);  // rev3
    //gpio_toggle(GPIOA, GPIO8);  // rev4

    //read bme280 for latest values.
    //this is required to get an update from the BME280
    bme280.update();

    //integer results for efficiency
    uint32_t T = bme280.getTemperature();
    uint32_t p = bme280.getPressure();
    uint32_t H = bme280.getHumidity();

    //float results for convenience
    float fT = bme280.getfTemperature();
    float fp = bme280.getfPressure();
    float fH = bme280.getfHumidity();

    printf("T= %d, p= %d, H=%d\n", T, p, H);
    printf("T= %.2f, p= %.2f, H=%.2f\n", fT, fp, fH);
}
