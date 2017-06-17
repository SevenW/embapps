\ rf69 ook driver
\ uses spi, rf69
\ requires constant DIO2 to be defined by boards.fs

  $03 constant RF:BRMSB
  $19 constant RF:RXBW
  $1B constant RF:OOKPEAK
  $1D constant RF:OOKFIX
  $1E constant RF:AFCFEI
  $29 constant RF:RSSITHRESH

32000000 11 rshift constant RF.FREQDIV

: rf-freq ( u -- )  \ set the frequency, supports any input precision
  begin dup 100000000 < while 10 * repeat
  ( f )       dup RF.FREQDIV u/mod nip swap \ avoid / use u/mod instead
  ( msb f )   RF.FREQDIV u/mod drop 8 lshift RF.FREQDIV u/mod nip
  ( msb lsb ) RF:FRF 2+ rf!
  ( msb )     dup RF:FRF 1+ rf!
  ( msb )     8 rshift RF:FRF rf! ;


    0 variable ook.dio2
    0 variable ook.ts
    0 variable ook.thd

    \ moving average buffer
    5 constant OOK.ALEN
    1 OOK.ALEN lshift 1- constant ook.mask
    0 variable ook.filter


create ook:init  \ initialise the radio for ook, each, 16-bit word is <reg#,val>
hex
  0104 h, \ OpMode = standby
  0268 h, \ DataModul = conti mode, ook, no shaping
  0303 h, \ BitRateMsb, data rate = 32768bps maxOOK
  04D1 h, \ BitRateLsb, divider = 32 MHz / 650
  0B00 h, \ High, M
  \ 0B20 h, \ AfcCtrl, afclowbetaon
  1881 h, \ LNA fixed highest gain, Z=200ohm
  1940 h, \ RxBw DCC=4%, Man=00b Exp = 0 =>BWOOK=250.0kHz
  1B43 h, \ OOK peak, 0.5dB once/8 chips (slowest)
  1C80 h, \ OOK avg thresh, /4pi
  1D38 h, \ OOK fixed thresh
  1E00 h, \ No auto AFC
  2580 h, \ Dio 0: RSSI 1: Dclk 2: Data 3: RSSI
  \ 2637 h, \ Dio 5: ModeReady, CLKOUT = OFF
  29FF h, \ RssiThresh: lowest possible threshold to start receive
  2E00 h, \ SyncConfig = sync off
  581B h, \ No Sensitivity boost (TestLNA)
  \ 582D h, \ Sensitivity boost (TestLNA)
  6F30 h, \ TestDagc ...
  7100 h, \ RegTestAfc
  0 h,  \ sentinel
decimal align

: ook-exit ( -- ) \ Some registers requires reseting to defaults.
  \ Jeelib drivers do not program them, but rely on proper values.
  $80 RF:LNA rf!          \ LNA automatic gain, Z=200ohm
  $E4 RF:RSSITHRESH rf! ; \ RssiThresh $E4

: ook-thdMode ( u -- )  \ change OOK thresholdmode
  RF:OOKPEAK rf! ;

: ook-thd ( u -- )  \ change OOK threshold
  dup ook.thd ! RF:OOKFIX rf! ;

: ook-bw ( u -- )  \ change reciever bandwidth
  $40 or RF:RXBW rf! ;

: ook-bitrate ( u -- )  \ change reciever bitrate
  dup 489 < if drop $FFFF else 32000000 swap / then
  ( u ) dup 8 rshift  RF:BRMSB rf!
  ( u ) RF:BRMSB 1+ rf! ;

: ook-clrafc \ clear AFC. Essential for wideband OOK signals. No other way to reset from SW.
  2 RF:AFCFEI rf! ;

: ook-rssi@ ( -- u ) \ get RSSI value
  RF:RSSI rf@  dup rf.rssi ! ;

: ook-init ( freq -- )  \ init the RFM69 radio module
  spi-init
  $AA rf-check  $55 rf-check  \ will hang if there is no radio!
  ook:init rf-config!
  rf-freq
  $40 ook-thdMode
  32768 ook-bitrate \ max OOK 32768bps, max FSK 300000bps
  16 ook-bw \ 0=250kHz, 8=200kHz, 16=167kHz, 1=125kHz, 9=100kHz, 17=83kHz 2=63kHz, 10=50kHz
  60 ook-thd
  ook-clrafc
  RF:M_RX rf!mode ;

: ook-dio2 ( -- b ) \ get DIO2 value
  DIO2 io@ dup LED io! ;

: ook-bitcount ( u .. u ) \ count 1 bits in number
  0 swap begin ( cnt u ) dup while ( cnt u' ) dup 1- and swap 1+ swap repeat
  drop ;

: ook-filter ( b -- b ) \ moving average filter for bit stream
  ook.filter @ 1 lshift ook.mask and ( b ) or dup ook.filter !
  ook-bitcount OOK.ALEN 1 rshift > if 1 else 0 then ;

\ rrsi delay queue to match ook moving averaing delay
    3 constant OOK.RSSI.OFFSET
    OOK.ALEN 1 rshift 1+ OOK.RSSI.OFFSET + 
      constant OOK.RSSI.QLEN
    0 variable ook.rssi.qi

\ TODO: Clear this buffer
OOK.RSSI.QLEN 2 rshift 1+ 2 lshift buffer: ook.rssi.q \ don't trust align
align

: ook>rssi>delay ( u -- u ) \ delay rssi signal to be well within previous level after flip
  \ ook.rssi.q is a circular buffer.
  ook.rssi.qi @ 1+ dup OOK.RSSI.QLEN >= if drop 0 then
  ook.rssi.q + c@ \ get delayed rssi
  swap ook.rssi.qi @ ook.rssi.q + c! \ store new rssi at i
  ook.rssi.qi @ 1 + dup OOK.RSSI.QLEN >= if drop 0 then ook.rssi.qi ! ;

\ RSSI of received OOK signal
\ circular buffer for ON and OFF rssi signals
    3 constant OOK.RSSI.BUFEXP \ powers of 2 for efficientcy
    1 OOK.RSSI.BUFEXP lshift
      constant OOK.RSSI.SIZE
      OOK.RSSI.SIZE buffer: ook.rssi.buf
align
    0 variable ook.rssi.bi

: ook>rssi ( signal rssi --- )
  dup if \ rssi > 0
    swap if \ signal = ON
      ook.rssi.bi @ 2 + OOK.RSSI.SIZE 2 - and dup ook.rssi.bi !
      ook.rssi.buf + c!
    else \ signal = OFF
      ook.rssi.buf ook.rssi.bi @ + 1+ c!
    then
  else drop drop then ;

: ook.rssi.print \ conditionally print RSSI during OFF and ON signal
  OOK.RSSI.SIZE 1 rshift dup 0 0 rot 0 do
    ook.rssi.buf i 2* + c@ + swap ook.rssi.buf i 2* + 1+ c@ + swap
  loop
  ( n u u ) over over 0<> swap 0<> and if
    rot tuck / ."  (" . ." /" / . ." ) "
  else drop drop drop then
  \ OOK.RSSI.SIZE 0 do ook.rssi.buf i + c@ . loop 
  ;

: ook.rssi.clr
  ook.rssi.buf OOK.RSSI.SIZE 0 fill ;

