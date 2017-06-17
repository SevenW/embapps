\ read out the TSL4531 sensor
\ needs i2c

: tsl-init ( -- nak )
  i2c-init $29 i2c-addr  $03 >i2c  0 i2c-xfer ;

: tsl-data ( -- v )
  $29 i2c-addr  $84 >i2c  2 i2c-xfer drop  i2c>h ;

\ tsl-init .
\ tsl-data .
