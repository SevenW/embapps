\ hardware i2c driver - not working yet

$40005400 constant I2C1
$40005800 constant I2C2
     I2C1 $00 + constant I2C1-CR1
     I2C1 $04 + constant I2C1-CR2
     I2C1 $08 + constant I2C1-OAR1
     I2C1 $0C + constant I2C1-OAR2
     I2C1 $10 + constant I2C1-DR
     I2C1 $14 + constant I2C1-SR1
     I2C1 $18 + constant I2C1-SR2
     I2C1 $1C + constant I2C1-CCR
     I2C1 $20 + constant I2C1-TRISE

: i2c-init ( -- )  \ initialise I2C hardware
  IMODE-PULL PB6 io-mode!  PB6 ios!
  IMODE-PULL PB7 io-mode!  PB7 ios!
  OMODE-AF-OD PB6 io-mode!
  OMODE-AF-OD PB7 io-mode!
  21 bit RCC-APB1ENR bis!  \ set I2C1EN
  36 I2C1-CR2 h!           \ 36 MHz
  31 bit 30 + I2C1-CCR h!  \ fast mode 400 KHz
  11 I2C1-TRISE h!         \ max 300 nS rise time
  0 bit I2C1-CR1 hbis!     \ PE, enable device
;

: i2c-start ( -- )  8 bit I2C1-CR1 hbis! ;
: i2c-stop  ( -- )  9 bit I2C1-CR1 hbis! ;

: >i2c ( b -- nak )  \ send one byte
  I2C1-DR h!
  begin I2C1-SR1 h@ 7 bit and until
  10 bit I2C1-SR1  2dup h@ and 0<>  -rot hbic! ;
: i2c> ( nak -- b )  \ read one byte
  10 bit I2C1-CR1  rot if bic! else bis! then
  I2C1-DR h@
  begin I2C1-SR1 h@ 6 bit and until ;

: i2c-tx ( addr -- nak )  \ start device send
  i2c-start shl >i2c ;
: i2c-rx ( addr -- nak )  \ start device receive
  i2c-start shl 1+ >i2c ;

: i2c? I2C1-CR1 h@ hex. I2C1-SR1 h@ hex. I2C1-SR2 h@ hex. ;

: i2c. ( -- )  \ scan and report all I2C devices on the bus
  128 0 do
    cr i h.2 ." :"
    16 0 do  space
      i j +  dup i2c-rx i2c-stop  if drop ." --" else h.2 then
    loop
  16 +loop ;
