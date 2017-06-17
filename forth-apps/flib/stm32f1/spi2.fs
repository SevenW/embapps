\ hardware SPI driver, 2nd device

[ifndef] ssel2  PB12 variable ssel2  [then]  \ can be changed at run time
[ifndef] SCLK2  PB13 constant SCLK2  [then]
[ifndef] MISO2  PB14 constant MISO2  [then]
[ifndef] MOSI2  PB15 constant MOSI2  [then]

$40003800 constant SPI2
     SPI2 $0 + constant SPI2-CR1
     SPI2 $4 + constant SPI2-CR2
     SPI2 $8 + constant SPI2-SR
     SPI2 $C + constant SPI2-DR

: spi2. ( -- )  \ display SPI hardware registers
  cr ." CR1 " SPI2-CR1 @ h.4
    ."  CR2 " SPI2-CR2 @ h.4
     ."  SR " SPI2-SR @ h.4 ;

: +spi2 ( -- ) ssel2 @ ioc! ;  \ select SPI
: -spi2 ( -- ) ssel2 @ ios! ;  \ deselect SPI

: >spi2> ( c -- c )  \ hardware SPI, 8 bits
  SPI2-DR !  begin SPI2-SR @ 1 and until  SPI2-DR @ ;

\ single byte transfers
: spi2> ( -- c ) 0 >spi2> ;  \ read byte from SPI
: >spi2 ( c -- ) >spi2> drop ;  \ write byte to SPI

: spi2-init ( -- )  \ set up hardware SPI
  OMODE-PP ssel2 @ io-mode! -spi2
  OMODE-AF-PP SCLK2 io-mode!
  IMODE-FLOAT MISO2 io-mode!
  OMODE-AF-PP MOSI2 io-mode!
  14 bit RCC-APB1ENR bis!  \ set SPI2EN
  %0000000001001100 SPI2-CR1 !  \ clk/4, i.e. 9 MHz, master
  SPI2-SR @ drop  \ appears to be needed to avoid hang in some cases
  2 bit SPI2-CR2 bis!  \ SS output enable
;
