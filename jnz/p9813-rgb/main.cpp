/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>,
 * Copyright (C) 2011 Piotr Esden-Tempski <piotr@esden.net>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/systick.h>
#include <stdio.h>
#include <errno.h>

static void clock_setup () {
	rcc_clock_setup_hsi16_3v3(&rcc_hsi16_3v3[RCC_CLOCK_3V3_32MHZ]);

    /* Enable clocks for GPIO port A/B for LED and USART1. */
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    //GPIOC only used for a few lines on JNZRev3 (check Rev4). Only Ext OSC.
    //rcc_periph_clock_enable(RCC_GPIOC);
//    rcc_periph_clock_enable(RCC_AFIO);
    rcc_periph_clock_enable(RCC_USART1);
}

static void usart_setup(void)
{
//	/* Enable clocks for GPIO port A (for GPIO_USART1_TX) and USART1. */
//	rcc_periph_clock_enable(RCC_GPIOA);
//	rcc_periph_clock_enable(RCC_USART1);


	/* USART1 pins: PA9/TX PA10/RX */
    gpio_set_af(GPIOA, GPIO_AF4, GPIO9|GPIO10);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_HIGH, GPIO9|GPIO10);
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9|GPIO10);

	/* Setup UART parameters. */
	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_stopbits(USART1, USART_CR2_STOP_1_0BIT);
	usart_set_mode(USART1, USART_MODE_TX_RX);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

	/* Finally enable the USART. */
	usart_enable(USART1);
}

extern "C" int _write (int file, const char *ptr, int len) {
    if (file == 1) {
        for (int i = 0; i < len; ++i)
            usart_send_blocking(USART1, ptr[i]);
        return len;
    }
    errno = EIO;
    return -1;
}

int serial_getc () {
//	static uint8_t data = 'A';
//	/* Check if we were called because of RXNE. */
//	if (((USART_CR1(USART1) & USART_CR1_RXNEIE) != 0) &&
//	    ((USART_ISR(USART1) & USART_ISR_RXNE) != 0)) {
//		data = usart_recv(USART1);
//	}
//	return data;

	return USART_ISR(USART1) & USART_ISR_RXNE ? usart_recv(USART1) : -1;
}

static void systick_setup () {
	/* 32MHz 32000000 counts/second */
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);

    /* 32000000/64 = 200000 overflows per second - every 2us one interrupt */
    /* SysTick interrupt every N clock pulses: set reload to N-1 */
    systick_set_reload(63);

    systick_interrupt_enable();

    /* Start counting. */
    systick_counter_enable();
}

static volatile uint32_t ticks;

extern "C" void sys_tick_handler () {
    ++ticks;
}

void delay_us (uint32_t delay)
{
  //systick_counter_enable ();
  delay = (delay+1)/2;	/* ticks produces every 2 us */
  uint32_t ts = ticks;
  while (ticks - ts <= delay ) __asm("");
  //systick_counter_disable ();
}

// inaccurate delay function due to 20us timer resolution
void delay20(void)
{
	uint32_t ts = ticks;
	while (ticks - ts <= 1 ) __asm(""); 		// sync with first transition
	ts = ticks;
	while (ticks - ts <= 1 ) __asm(""); 	// wait for one tick to expire
}

uint32_t millis () {
    return ticks/500;
}

// the following creates an Arduino-like environment with setup() and loop()

extern void setup ();
extern void loop ();

int main () {
    clock_setup();
    usart_setup();
    systick_setup();

    setup();

    while (true)
        loop();

    // never returns
}
