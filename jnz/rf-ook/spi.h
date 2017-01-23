// SPI setup for STM32L0xx with libopencm3

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

class SpiDev {
    static uint8_t spiTransferByte (uint8_t out) {
        return spi_xfer(SPI1, out);
    }

    public:

    static const uint16_t spi_cs0 = GPIO15;
    static void master (int /*div*/) {
        rcc_periph_clock_enable(RCC_SPI1);

        //CS0 - PA15
        gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, spi_cs0);
        gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, spi_cs0);
        gpio_set(GPIOA, spi_cs0);

//        gpio_set_af(GPIOA, GPIO_AF0, spi_cs0);
//        gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, spi_cs0);
//        gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, spi_cs0);
//        gpio_set(GPIOA, spi_cs0);

        //SCK - PB3
        gpio_set_af(GPIOB, GPIO_AF0, GPIO3);
        gpio_mode_setup(GPIOB, GPIO_MODE_AF,GPIO_PUPD_NONE, GPIO3);
        gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO3 );

        //MISO MOSI standaard pins
        gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6 | GPIO7);
        gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO6 | GPIO7);

        spi_set_master_mode(SPI1);
        spi_set_baudrate_prescaler(SPI1, SPI_CR1_BR_FPCLK_DIV_8);
        spi_enable_software_slave_management(SPI1);
        spi_set_nss_high(SPI1);
        spi_enable(SPI1);
    }

    static uint8_t rwReg (uint8_t cmd, uint8_t val) {
        gpio_clear(GPIOA, spi_cs0);
        spiTransferByte(cmd);
        uint8_t in = spiTransferByte(val);
        gpio_set(GPIOA, spi_cs0);
        return in;
    }
};
