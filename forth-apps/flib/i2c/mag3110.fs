\ read out the MAG3110 sensor
\ needs i2c

: mag-init ( -- nak )
  i2c-init  $0E i2c-addr  $10 >i2c $80 >i2c $C9 >i2c  0 i2c-xfer
            $0E i2c-addr  $10 >i2c $81 >i2c           0 i2c-xfer or ;

: mag-data ( -- x y z )
  $0E i2c-addr  $01 >i2c  6 i2c-xfer drop  i2c>h i2c>h i2c>h ;

\ mag-init .
\ mag-data . . .
