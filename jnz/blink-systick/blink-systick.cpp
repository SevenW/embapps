// Simple LED blink demo for the JeeNode Zero.
//
// See also https://github.com/libopencm3/libopencm3-examples/blob/master/
//                      examples/stm32/l0/stm32l0538-disco/miniblink/miniblink.c

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

uint32_t timertick = 0;

uint32_t millis ();

void setup (void) {
    //gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);   // rev1
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);    // rev3
    //gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8);    // rev4

    timertick = millis();

    return;
}

void loop () {
    while ((millis() - timertick) < 1000)
    	__asm("");
    timertick = millis();

//	for (int i = 0; i < 1000000; ++i)
//            __asm("");

    //gpio_toggle(GPIOA, GPIO15); // rev1
    gpio_toggle(GPIOB, GPIO5);  // rev3
    //gpio_toggle(GPIOA, GPIO8);  // rev4
}
