\ hardware i2c driver

[ifndef] SCL  PB6 constant SCL  [then]
[ifndef] SDA  PB7 constant SDA  [then]

$40005400 constant I2C1
     I2C1 $00 + constant I2C1-CR1
     I2C1 $04 + constant I2C1-CR2
\    I2C1 $08 + constant I2C1-OAR1
\    I2C1 $0C + constant I2C1-OAR2
     I2C1 $10 + constant I2C1-TIMINGR
\    I2C1 $14 + constant I2C1-TIMEOUTR
     I2C1 $18 + constant I2C1-ISR
     I2C1 $1C + constant I2C1-ICR
\    I2C1 $20 + constant I2C1-PXCR
     I2C1 $24 + constant I2C1-RXDR
     I2C1 $28 + constant I2C1-TXDR

: i2c? ( -- )
  I2C1
  cr ."       CR1 " dup @ hex. 4 +
          ."  CR2 " dup @ hex. 4 +
         ."  OAR1 " dup @ h.4 space 4 +
         ."  OAR2 " dup @ h.4 space 4 +
      ."  TIMINGR " dup @ hex. 4 +
  cr ."  TIMEOUTR " dup @ hex. 4 +
          ."  ISR " dup @ hex. 4 +
         ."   ICR " dup @ h.4 space 4 +
         ."  PXCR " dup @ h.2 space 4 +
       ."    RXDR " dup @ h.2 space 4 +
         ."  TXDR " dup @ h.2 space drop ;

: i2c-init ( -- )  \ initialise I2C hardware
  OMODE-AF-OD SCL io-mode!
  OMODE-AF-OD SDA io-mode!

  \ TODO this assumes PB6+PB7 and messes up the settings of the other pins
  $11000000 PB6 io-base GPIO.AFRL + !

  21 bit RCC-APB1ENR bis!  \ set I2C1EN
  $00300619 I2C1-TIMINGR !
;

100 buffer: i2c.buf
 0 variable i2c.ptr

: i2c-reset ( -- )  i2c.buf i2c.ptr ! ;

: i2c-addr ( u -- )  shl I2C1-CR2 !  i2c-reset ;

: i2c++ ( -- addr )  i2c.ptr @  dup 1+ i2c.ptr ! ;

: >i2c ( u -- )  i2c++ c! ;
: i2c> ( -- u )  i2c++ c@ ;
: i2c>h ( -- u )  i2c> i2c> 8 lshift or ;


: i2c-start ( rd -- )
  if 10 bit I2C1-CR2 bis! then  \ RD_WRN
  13 bit I2C1-CR2 bis!  \ START
;

: i2c-stop  ( -- )
  14 bit I2C1-CR2 bis!  \ STOP
  begin 15 bit I2C1-ISR bit@ not until  \ !BUSY
;

: i2c-setn ( u -- )  \ prepare for N-byte transfer and reset buffer pointer
  16 lshift I2C1-CR2 @ $FF00FFFF and or I2C1-CR2 !  i2c-reset ;
  
: i2c-wr ( -- )  \ send bytes to the I2C interface
  begin
    begin %1011001 I2C1-ISR bit@ until  \ wait for TCR, STOPF, NACKF, or TXE
  %1011000 I2C1-ISR bit@ not while  \ while !TCR, !STOPF, and !NACKF
    i2c> I2C1-TXDR c!
  repeat
;

: i2c-rd ( -- )  \ receive bytes from the I2C interface
  begin
    begin %1011100 I2C1-ISR bit@ until  \ wait for TCR, STOPF, NACKF, or RXNE
  2 bit I2C1-ISR bit@ while  \ while RXNE
    I2C1-RXDR c@ >i2c
  repeat ;

\ there are 4 cases:
\   tx>0 rx>0 : START - tx - RESTART - rx - STOP
\   tx>0 rx=0 : START - tx - STOP
\   tx=0 rx>0 : START - rx - STOP
\   tx=0 rx=0 : START - STOP          (used for presence detection)

: i2c-xfer ( u -- nak )
  0 bit I2C1-CR1 bic!  0 bit I2C1-CR1 bis!  \ toggle PE low to reset
  i2c.ptr @ i2c.buf - ?dup if
    i2c-setn  0 i2c-start  i2c-wr  \ tx>0
  else
    dup 0= if 0 i2c-start then  \ tx=0 rx=0
  then
  ?dup if
    i2c-setn  1 i2c-start  i2c-rd  \ rx>0
  then
  i2c-stop i2c-reset
  4 bit I2C1-ISR bit@ 0<>  \ NAKF
;

: i2c. ( -- )  \ scan and report all I2C devices on the bus
  128 0 do
    cr i h.2 ." :"
    16 0 do  space
      i j +
      dup $08 < over $77 > or if drop 2 spaces else
        dup i2c-addr  0 i2c-xfer  if drop ." --" else h.2 then
      then
    loop
  16 +loop ;
