\ read out the BME280 sensor
\ needs i2c

[ifndef] BME.ADDR  $76 constant BME.ADDR  [then]

: bme-init ( -- nak )
  i2c-init
  BME.ADDR i2c-addr
  $F2 >i2c %1 >i2c 
  $F4 >i2c %100111 >i2c 
  $F5 >i2c %10100000 >i2c
  0 i2c-xfer ;

32 buffer: params  \ calibration data
 8 buffer: values  \ last reading
0 variable tfine   \ used for p & h compensation

: bme-i2c+ ( addr -- addr+1 ) i2c> over c! 1+ ;

: bme-rd ( addr n reg -- addr+n )
  BME.ADDR i2c-addr >i2c
  dup i2c-xfer drop
  0 do bme-i2c+ loop ;

: bme-calib ( -- )
  params
  24 $88 bme-rd
   1 $A1 bme-rd
   7 $E1 bme-rd
  drop ;

: bme-u8 ( off -- val ) params + c@ ;
: bme-u16 ( off -- val ) dup bme-u8 swap 1+ bme-u8 8 lshift or ;
: bme-s16 ( off -- val ) bme-u16 16 lshift 16 arshift ;
: bme-u20be ( off -- val )
  values + dup c@ 12 lshift swap 1+
           dup c@  4 lshift swap 1+
               c@  4 rshift  or or ;

: bme-data ( -- )  \ get a sensor reading from the BME280
  values  8 $F7 bme-rd drop ;

: bme-hpt ( -- rawh rawp rawt )
  values 6 + dup c@ 8 lshift swap 1+ c@ or  0 bme-u20be  3 bme-u20be ;

: *>> ( n1 n2 u -- n ) >r * r> arshift ;  \ (n1 * n2) >> u
: ^2>> ( n1 u -- n ) >r dup * r> arshift ;  \ (n1 * n1) >> u

: tcalc ( rawt -- t100 )
  3 rshift dup shr swap
  ( T1: ) 0 bme-u16 shl - ( T2: ) 2 bme-s16 11 *>>
  swap ( T1: ) 0 bme-u16 - 12 ^2>> ( T3: ) 4 bme-s16 14 *>> +
  dup tfine !  5 * 128 + 8 arshift ;

: pcalc ( rawp -- p1 )
  tfine @ 2/ 64000 -                                  ( rawp var1 )
  dup 2 arshift 11 ^2>> ( P6: ) 16 bme-s16 *          ( rawp var1 var2 )
  over ( P5: ) 14 bme-s16 shl * +                     ( rawp var1 var2 )
  2 arshift ( P4: ) 12 bme-s16 16 lshift + swap       ( rawp var2 var1 )
  ( P3: ) 10 bme-s16 over 2 arshift 13 ^2>> 3 *>>     ( rawp var2 var1 x )
  swap ( P2: ) 8 bme-s16 1 *>> + 18 arshift           ( rawp var2 var1 )
  32768 + ( P1: ) 6 bme-u16 15 *>>                    ( rawp var2 var1 )
  dup if                                              ( rawp var2 var1 )
    rot 1048576 swap - rot 12 arshift - 3125 *        ( var2 var1 p )
    dup 0< if swap shl else shl swap then u/mod nip   ( p )
    ( P9: ) 22 bme-s16 over 3 arshift 13 ^2>> 12 *>>  ( p var1 )
    over 2 arshift ( P8: ) 20 bme-s16 13 *>>          ( p var1 var2 )
    ( P7: ) 18 bme-s16 + + 4 arshift +                ( p )
  else nip nip then ;

: hcalc ( rawh -- h100 )
  tfine @ 76800 - >r
  14 lshift
  ( H4: ) 28 bme-u8 24 lshift 20 arshift 29 bme-u8 $F and or
  20 lshift -
  ( H5: ) 29 bme-s16 4 arshift
  r@ * - 16384 + 15 arshift
  ( H6: ) 31 bme-u8 24 lshift 24 arshift
  r@ 10 *>>
  ( H3: ) 27 bme-u8
  r> 11 *>> 32768 + 10 *>> 2097152 +
  ( H2: ) 25 bme-s16
  * 8192 + 14 arshift *
  dup 15 arshift 7 ^2>>
  ( H1: ) 24 bme-u8
  4 *>> -  0 max  419430400 min  12 arshift
  100 * 512 + 10 arshift  \ convert 1/1024's to 1/100's, w/ rounding
;

: bme-calc ( -- h p t )  \ convert reading to calibrated values
  bme-hpt tcalc -rot pcalc -rot hcalc -rot ;

\ bme-init .
\ bme-calib params 32 dump
\ bme-data bme-calc . . .
