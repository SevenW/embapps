\ read out the VEML6040 sensor
\ needs i2c

: veml-init ( -- nak )
  i2c-init  $10 i2c-addr  3 0 do $00 >i2c loop  0 i2c-xfer ;

: veml-rd ( reg -- val )
  $10 i2c-addr  >i2c  2 i2c-xfer drop  i2c>h ;

: veml-data  ( -- r g b w )
  $8 veml-rd $9 veml-rd $A veml-rd $B veml-rd ;


\ veml-init .
\ veml-data . . . .
