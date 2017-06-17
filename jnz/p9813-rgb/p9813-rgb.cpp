// P9813 RGB LED strip p9813 demo for the JeeNode Zero.
//

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>

#include "p9813-rgb.h"

static P9813 p9813 = P9813(GPIOA, GPIO13, GPIOA, GPIO14);

uint32_t timertick = 0;
int j = 0, c = 0;

uint32_t millis ();

void delay(uint32_t ms)
{
	uint32_t ts = millis();
	while ((millis() - ts) <= ms) __asm("");
}

void delay_us(uint32_t);

void setup (void) {
    printf("\n[p9813-rgb]\n");
    delay(500);
    printf("\n[p9813-rgb]\n");
    delay_us(20);
    printf("\n[p9813-rgb]\n");

	//gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);   // rev1
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);    // rev3
    //gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8);    // rev4

    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);

    //p9813 = P9813(GPIOA, GPIO13, GPIOA, GPIO14);
    gpio_set(GPIOA, GPIO13);
    gpio_clear(GPIOA, GPIO14);

//    printf("start timer loop\n" );
//
//    for (int i=0; i<100000; i++)
//    {
//    	delay_us(20);
//    }
//    gpio_clear(GPIOA, GPIO13);
//    gpio_set(GPIOA, GPIO14);
//    for (int i=0; i<100000; i++)
//    {
//    	delay_us(20);
//    }
//    printf("  end timer loop\n" );

//    timertick = millis();

    return;
}

void loop () {
//    while ((millis() - timertick) < 2000)
//    	__asm("");
//    timertick = millis();
	  printf("loop\n");
	  p9813.begin(); // begin
	  p9813.SetColor(255, 0, 0); //Red. first node data
	  p9813.end();
	  delay(500);
	  p9813.begin(); // begin
	  p9813.SetColor(0, 255, 0); //Green. first node data
	  p9813.end();
	  delay(500);
	  p9813.begin(); // begin
	  p9813.SetColor(0, 0, 255);//Blue. first node data
	  p9813.end();
	  delay(500);
}
