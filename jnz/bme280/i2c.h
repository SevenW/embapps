// I2C setup for STM32L0xx with libopencm3

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>

#include <libopencm3/stm32/usart.h>
#include <stdio.h>


class I2CDev {
	static void wait_i2c(uint32_t i2c) {
		int wait;
	    wait = true;
	    while (wait) {
	        if (i2c_transmit_int_status(i2c)) {
	            wait = false;
	        }
	        while (i2c_nack(i2c));
	    }
	}

public:

    static const uint16_t addr = 0x76;
    static void setup (uint32_t i2c) {
		/* Enable clocks for I2C1. */
		rcc_periph_clock_enable(RCC_GPIOB);
		rcc_periph_clock_enable(RCC_I2C1);

		i2c_reset(i2c);

		/* Set alternate functions for the SCL and SDA pins of I2C2. */
		gpio_set_af(GPIOB, GPIO_AF1, GPIO6|GPIO7);
		gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_HIGH, GPIO6|GPIO7);
		gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6|GPIO7);

		/* Disable the I2C before changing any configuration. */
		i2c_peripheral_disable(i2c);

		/* Use HSI16 as the main I2C clock source */
		RCC_CCIPR |= RCC_CCIPR_I2C1SEL_HSI16 << RCC_CCIPR_I2C1SEL_SHIFT;

		/* The timing settings for I2C are all set in I2C_TIMINGR; */
		/* There's a xls for computing it: STSW-STM32126 AN4235 (For F0 and F3) */

		//value form embello Forth BME280
		I2C1_TIMINGR = (uint32_t)0x00300619;

		//value from flabbergast i2ctest
		//I2C1_TIMINGR = (uint32_t)0x00503D5A;

		//7-bit address mode
		i2c_set_7bit_addr_mode(i2c);

		/* If everything is configured -> enable the peripheral. */
		i2c_peripheral_enable(i2c);
    }

    static void read (uint32_t i2c, uint8_t i2c_addr, uint8_t reg, uint8_t size, uint8_t *data) {
        read_i2c(i2c, i2c_addr, reg, size, data);
        return;
    }

    static void write (uint32_t i2c, uint8_t i2c_addr, uint8_t reg, uint8_t size, uint8_t *data) {
        write_i2c(i2c, i2c_addr, reg, size, data);
        return;
    }

    static void read_alt (uint32_t i2c, uint8_t i2c_addr, uint8_t reg, uint8_t size, uint8_t *data) {
        //printf("R%02X:", reg);
        int i;
        while (i2c_busy(i2c) == 1);
        while (i2c_is_start(i2c) == 1);
        /*Setting transfer properties*/
        i2c_set_bytes_to_transfer(i2c, 1); //is reg 16 or 8bits?
        i2c_set_7bit_address(i2c, i2c_addr);
        i2c_set_write_transfer_dir(i2c);
        i2c_disable_autoend(i2c);
        /*start transfer*/
        i2c_send_start(i2c);
        wait_i2c(i2c);
        //i2c_send_data(i2c, (uint8_t)(reg>>8));
        //wait_i2c(i2c);
        i2c_send_data(i2c, (uint8_t)(reg&0xFF));

        while (i2c_is_start(i2c) == 1);
        /*Setting transfer properties*/
        i2c_set_bytes_to_transfer(i2c, size);
        i2c_set_7bit_address(i2c, i2c_addr);
        i2c_set_read_transfer_dir(i2c);
        i2c_enable_autoend(i2c);
        /*start transfer*/
        i2c_send_start(i2c);

        for (i = 0; i < size; i++) {
            while (i2c_received_data(i2c) == 0);
            data[i] = i2c_get_data(i2c);
            //printf("%02X", data[i]);
        }

        //printf(" ");
        return; //read_i2c(i2c, i2c_addr, reg, size, data);
    }

    static void write_alt (uint32_t i2c, uint8_t i2c_addr, uint8_t reg, uint8_t size, uint8_t *data) {
        //printf("W%02X:", reg);
        int i;
        while (i2c_busy(i2c) == 1);
        while (i2c_is_start(i2c) == 1);
        /*Setting transfer properties*/
        i2c_set_bytes_to_transfer(i2c, size + 1); //Is reg send as two or one byte?
        i2c_set_7bit_address(i2c, (i2c_addr & 0x7F));
        i2c_set_write_transfer_dir(i2c);
        i2c_enable_autoend(i2c);
        /*start transfer*/
        i2c_send_start(i2c);

        wait_i2c(i2c);
        //i2c_send_data(i2c, (uint8_t)(reg>>8));
        //wait_i2c(i2c);
        i2c_send_data(i2c, (uint8_t)(reg&0xFF));
        for (i = 0; i < size; i++) {
        	wait_i2c(i2c);
            i2c_send_data(i2c, data[i]);
            //printf("%02X", data[i]);
        }

        //printf(" ");
        return;
    }
};


