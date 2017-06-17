\ bit-banged i2c driver
\ adapted from http://excamera.com/sphinx/article-forth-i2c.html

[ifndef] SCL  PB6 constant SCL  [then]
[ifndef] SDA  PB7 constant SDA  [then]

0 variable i2c.adr
0 variable i2c.nak
0 variable i2c.prv
0 variable i2c.cnt

: i2c-init ( -- )  \ initialise bit-banged I2C
  OMODE-PP SCL io-mode!
  OMODE-OD SDA io-mode!
;

: i2c-half ( -- )  \ half-cycle timing delay for I2C
  [ifdef] I2C.DELAY  I2C.DELAY 0 do loop  [then] inline ;

: i2c-start ( -- )  \ with SCL high, change SDA from 1 to 0
  SDA ios! i2c-half SCL ios! i2c-half SDA ioc! i2c-half SCL ioc! ;
: i2c-stop  ( -- )  \ with SCL high, change SDA from 0 to 1
  SDA ioc! i2c-half SCL ios! i2c-half SDA ios! i2c-half ;

: b>i2c ( f -- )  \ send one I2C bit
  0<> SDA io! i2c-half SCL ios! i2c-half SCL ioc! ;
: i2c>b ( -- b )  \ receive one I2C bit
  SDA ios! i2c-half SCL ios! i2c-half SDA io@ SCL ioc! ;

: x>i2c ( b -- nak )  \ send one byte
  8 0 do dup 128 and b>i2c shl loop drop i2c>b ;
: xi2c> ( nak -- b )  \ read one byte
  0 8 0 do shl i2c>b + loop swap b>i2c ;

: i2c-flush ( -- )
  i2c.prv @ x>i2c  ?dup if i2c.nak ! then ;

: >i2c ( u -- )  \ send one byte out to the I2C bus
  i2c-flush  i2c.prv ! ;

: i2c> ( -- u )  \ read one byte back from the I2C bus
  i2c.cnt @ dup if
    1- dup i2c.cnt !
    0= xi2c>
  then ;

: i2c>h ( -- u )  i2c> i2c> 8 lshift or ;

: i2c-addr ( u -- )  \ start a new I2C transaction
  shl  dup i2c.adr !  i2c.prv !  0 i2c.nak !  i2c-start ;

: i2c-xfer ( u -- nak )  \ prepare for the reply
  i2c-flush
  dup i2c.cnt !  if
    i2c-start i2c.adr @ 1+ i2c.prv ! i2c-flush
  else
    SCL ios!  \ i2c-stop
  then
  i2c.nak @
  dup if i2c-stop 0 i2c.cnt ! then  \ ignore reads if we had a nak
;

\ RTC example, this is small enough to leave it in
\ nak's will be silently ignored
\
\ : rtc: ( reg -- ) \ common i2c preamble for RTC
\   $68 i2c-tx drop >i2c drop ;
\ : rtc! ( v reg -- ) \ write v to RTC register
\   rtc: >i2c drop i2c-stop ;
\ : rtc@ ( reg -- v ) \ read RTC register
\   rtc: i2c-start $68 i2c-rx drop 1 i2c> i2c-stop ;

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
