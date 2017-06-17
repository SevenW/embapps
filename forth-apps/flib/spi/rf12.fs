\ RF12 driver, receive-only for now
\ needs rf12-spi-init and >rf12> words

' nop variable rf12.reset
' nop variable rf12.state
    5 variable rf12.grp
    0 variable rf12.len
    0 variable rf12.pos
    0 variable rf12.crc
    72 buffer: rf12.rx

: >rf12 ( u -- ) >rf12> drop ;
: rf12-fifo> ( -- u) $B000 >rf12> $FF and ;

: showrx ( -- )
  rf12.crc @ 0= if
    ." RF12 "
    rf12.len @ 2- 0 ?do
      i 4 = if space then
      rf12.rx i + c@ h.2
    loop
    cr
  then ;

: rf12>rx ( b -- )
  dup rf12.crc @ crc16 rf12.crc !
      rf12.pos @ rf12.rx + c!
    1 rf12.pos +!
  rf12.pos @ rf12.len @ >= if
    rf12.rx c@ $10 -
    dup $F0 > if $F0 or then  \ limit error count to 15
    dup rf12.rx c!
    showrx
    $0F and rf12.rx !  rf12.reset @ execute
  then ;

: rf12/recv ( -- ) rf12-fifo> rf12>rx ;

: rf12/len ( -- )
  rf12-fifo> dup 66 <= if
    dup rf12>rx  6 + rf12.len !  \ err/band grp hdr len <data> crc1 crc2
    ['] rf12/recv
  else
    drop rf12.reset @
  then rf12.state ! ;

: rf12/hdr ( -- )
  $10 rf12.rx c+!  \ increment headers-seen count
  rf12-fifo> rf12>rx
  ['] rf12/len rf12.state ! ;

: rf12-sleep ( -- ) $8205 >rf12 ;

: rf12-go ( -- )  \ start or resume normal reception mode
  rf12-sleep
             1 rf12.pos !
             5 rf12.len !
         $FFFF rf12.crc !
    rf12.grp @ rf12>rx
  ['] rf12/hdr rf12.state !
             0 >rf12
         $B000 >rf12
         $82DD >rf12  \ receiver on
;

: rf12-freq ( offset -- )  \ change frequency offset
  $A000 + >rf12  \ 96-3960 freq range within band, default is 1600
;

: rf12-init ( grp freq -- )  \ initialise the RFM12B radio module
  dup 100 / rf12.rx !
  case 915 of $30 endof 868 of $20 endof true ?of $10 endof endcase
  swap dup rf12.grp !  ( band group )

  IMODE-PULL RF12-IRQ io-mode!  RF12-IRQ ios!  \ IRQ pin is input w/ pull-up
\ still guessing what it takes to ALWAYS get out of reset mode correctly...
  rf12-spi-init rf12-spi-init
  0 >rf12  $B800 >rf12  begin 0 >rf12  RF12-IRQ io@ until
  rf12-sleep  1600 rf12-freq

  $CE00 + >rf12  \ group, SYNC=2DXX；
  $80C7 + >rf12  \ band, EL (ena TX), EF (ena RX FIFO), 12.0pF
    $C606 >rf12  \ approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps
    $94A2 >rf12  \ VDI,FAST,134kHz,0dBm,-91dBm
    $C2AC >rf12  \ AL,!ml,DIG,DQD4
    $CA83 >rf12  \ FIFO8,2-SYNC,!ff,DR
    $C483 >rf12  \ @PWR,NO RSTRIC,!st,!fi,OE,EN
    $9850 >rf12  \ !mp,90kHz,MAX OUT
    $CC77 >rf12  \ OB1，OB0, LPX,！ddy，DDIT，BW0
    $E000 >rf12  \ NOT USE
    $C800 >rf12  \ NOT USE
    $C049 >rf12  \ 1.66MHz,3.1V
;

: rf12-start ( -- )  \ start receiving data
  ['] rf12-go dup rf12.reset ! execute ;

: rf12-poll ( -- )  \ polled access to RFM12
\   RF12-IRQ io@ 0= if 0 >rf12 rf12.state @ execute then
    0 >rf12> $8000 and if rf12.state @ execute then ;
