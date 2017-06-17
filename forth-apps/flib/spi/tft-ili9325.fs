\ tft driver for ILI9325 chip

create tft:init
hex
  E7 h, 0010 h, 00 h, 0001 h, 01 h, 0100 h, 02 h, 0700 h, 03 h, 1038 h,
  04 h, 0000 h, 08 h, 0207 h, 09 h, 0000 h, 0A h, 0000 h, 0C h, 0001 h,
  0D h, 0000 h, 0F h, 0000 h, 10 h, 0000 h, 11 h, 0007 h, 12 h, 0000 h,
  13 h, 0000 h, -1 h, #50  h, 10 h, 1590 h, 11 h, 0227 h, -1 h, #50  h,
  12 h, 009C h, -1 h, #50  h, 13 h, 1900 h, 29 h, 0023 h, 2B h, 000E h,
  -1 h, #50  h, 20 h, 0000 h, 21 h, 0000 h, -1 h, #50  h, 30 h, 0007 h,
  31 h, 0707 h, 32 h, 0006 h, 35 h, 0704 h, 36 h, 1F04 h, 37 h, 0004 h,
  38 h, 0000 h, 39 h, 0706 h, 3C h, 0701 h, 3D h, 000F h, -1 h, #50  h,
  50 h, 0000 h, 51 h, 00EF h, 52 h, 0000 h, 53 h, 013F h, 60 h, A700 h,
  61 h, 0001 h, 6A h, 0000 h, 80 h, 0000 h, 81 h, 0000 h, 82 h, 0000 h,
  83 h, 0000 h, 84 h, 0000 h, 85 h, 0000 h, 90 h, 0010 h, 92 h, 0000 h,
  93 h, 0003 h, 95 h, 0110 h, 97 h, 0000 h, 98 h, 0000 h, 07 h, 0133 h,
  20 h, 0000 h, 21 h, 0000 h, 0 ,
decimal

: tft-init ( u - )  \ init tft: cmd=0/data=2 + write=0/read=1
  +spi $70 or >spi ;
: -tft ( -- ) -spi ;

: tft@ ( reg -- val )
  0 tft-init 0 >spi >spi -tft
  3 tft-init spi> 8 lshift spi> or -tft ;

: tft! ( val reg -- )
  0 tft-init 0 >spi >spi -tft
  2 tft-init dup 8 rshift >spi >spi -tft ;

\ FIXME looks like SPI reads are not working...
\ : tft. ( -- )  \ dump ILI9325 register contents
\   cr space 16 0 do 2 spaces i h.2 loop
\   $A0 0 do
\     cr  i h.2 space
\     16 0 do  i j + tft@ h.4  loop
\   $10 +loop ;

: tft-config ( -- )
  tft:init begin
    dup @  ?dup while
      dup 16 rshift swap ( addr val reg )
      dup $100 and if drop ms else tft! then
  4 + repeat drop ;

$0000 variable tft-bg
$FC00 variable tft-fg

: tft-init ( -- )
  PB0 ssel !  \ use PB0 to select the TFT display
  spi-init
\ switch to alternate SPI pins, PB3..5 iso PA5..7
  $03000001 AFIO-MAPR !  \ also disable JTAG & SWD to free PB3 PB4 PA15
  IMODE-FLOAT PA5 io-mode!
  IMODE-FLOAT PA6 io-mode!
  IMODE-FLOAT PA7 io-mode!
  OMODE-AF-PP PB3 io-mode!
  IMODE-FLOAT PB4 io-mode!
  OMODE-AF-PP PB5 io-mode!
  OMODE-PP PB2 io-mode!  PB2 ios!
  %0000000001010110 SPI1-CR1 !  \ clk/16, i.e. 4.5 MHz, master, CPOL=1 (!)
  tft-config ;

\ clear, putpixel, and display are used by the graphics.fs code

: clear ( -- )  \ clear display memory
  0 $21 tft! 0 $20 tft!
  tft-bg @ 320 240 * 0 do dup $22 tft! loop drop ;

: putpixel ( x y -- )  \ set a pixel in display memory
  $21 tft! $20 tft! tft-fg @ $22 tft! ;

: display ( -- ) ;  \ update tft from display memory (ignored)
