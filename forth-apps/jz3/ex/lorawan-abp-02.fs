
<<<core>>>
compiletoflash

\ LoraWAN-ABP
\ Author: SevenW from sevenwatt.com
\ Date  : 2017-Feb-27
\ 
\ Description:
\ Demo of LoraWAN ABP
\ Supports TX, RX1, RX2, and ACK/ACKREQ
\ 
\ Requires : lora1276.fs
\ Requires : aes128.fs
\ Requires : aes-ctr-cmac.fs
\ Requires : lorawan.fs
\ 
\ Usage    : Copy nwskey, appskey and devaddr from TTN console
\          : setup run
\ 
\ n tune.delay !     \ move RXn window by n ms
\ true upl.ackreq !  \ request ACK from server
\ n rf.sf !          \ ( n=7..12 ) set the spread factor
\ 
\ Limitations: 868MHz (EU) only, LoraWAN ABP only.
\ 
\ 

: h.16 ( caddr -- )
  16 0 do dup i + c@ h.2 ."  " loop drop cr ;

: h.n ( caddr len -- )
  ( len ) 0 do dup i + c@ h.2 ."  " loop drop cr ;

\ 
\ Application
\ 

: stop60s   ( -- )  \ sleep in low-power for 60 sec
  2220000 stop-freq ;

: helloworld s" LoraWAN Forth" ;

0 [if]
: test-msg
  init-frame helloworld >payload 
  cr
  ." input: " rf.buf 32 h.n
  framelen @ . cr
  \ appskey.ptr @ cipher-init
  
  \ \ AESkey is now filled
  rf.buf pl-start @ + pl-len @ appskey.ptr @ iv-ctr aes-ctr \ AESaux is filled
  ." key:   " appskey.ptr @ h.16
  ." iv:    " lora.iv h.16
  ." ctr:   " rf.buf 32 h.n
  \ framelen @ nwskey.ptr @ cmac-init \ AESkey is now filled
  ." key:   " nwskey.ptr @ h.16
  ." iv :   " lora.iv h.16
  \ AESaux gets cleared by cmac
  \ AESkey may need to be reinitialized!!!!
  rf.buf framelen @ nwskey.ptr @ framelen @ ( len ) iv-cmac aes-cmac rf.buf framelen @ + copy-mic 4 framelen +!
  ." cmac:  " rf.buf 32 h.n
  ." key:   " AESkey h.16
  ." aux:   " AESaux h.16
  \ appskey.ptr @ cipher-init
  \ \ AESkey is now filled
  \ ." key:   " AESkey h.16
  \ ." aux:   " AESaux h.16
  \ rf.buf pl-start @ + pl-len @ aes-ctr \ AESaux is filled
  \ ." ctr:   " rf.buf 32 h.n
;

[else]
: test-msg
  init-frame helloworld >payload                                \ copy payload to rf.buf buffer
  \ cr ." input: " rf.buf 32 h.n
  rf.buf pl-start @ + pl-len @ appskey.ptr @ iv-ctr             \ prepare for encryption
  ( buf len key iv ) aes-ctr                                    \ encrypt payload
  rf.buf framelen @ nwskey.ptr @ framelen @ ( len ) iv-cmac     \ prepare for MIC (hash key)
  ( buf len key iv ) aes-cmac ( mic )                           \ calculate MIC
  rf.buf framelen @ + copy-mic 4 framelen +!                    \ copy MAC to end of rf.buf buffer
;

[then]


0 [if]
hex
create nwskey
  D0 C, 9E C, 6B C, F4 C, A0 C, 75 C, EC C, D5 C, 8B C, 4B C, A8 C, 84 C, D3 C, CD C, A3 C, 36 C,
create appskey
  FE C, 97 C, CE C, 91 C, 2E C, 1E C, 17 C, 7F C, DB C, 5E C, 98 C, 26 C, FC C, 7B C, EE C, 85 C,
2601181D constant devaddr
decimal
[else]
hex
create nwskey
  25 C, C0 C, B9 C, AB C, 7D C, 68 C, 79 C, 68 C, C5 C, EC C, EF C, 0D C, 02 C, A0 C, 33 C, 8A C,
create appskey
  3E C, CF C, CC C, EF C, 7A C, 44 C, 61 C, FB C, 45 C, 81 C, 43 C, A2 C, 1B C, 1C C, EF C, A4 C,
\ 260118AC constant devaddr \ dev-01
260119D7 constant devaddr \ dev-02
decimal
[then]

10 buffer: PL \ small payload buffer

: rf-temperature@ ( -- T ) \ Get RFM95 Temerature for what it is worth
  RF:M_SLEEP rf!mode
  $00 RF:OP rf!           \ FSK
  RF:M_RXCONT rf!mode
  $3C rf@
  RF:M_SLEEP rf!mode
  $88 RF:OP rf!           \ back to LoRa sleep
  ;

: rf-stats-PL
  rf-temperature@ PL c!              \ read sensors and add to PL buffer
  rf.snr @ PL 1+ c!
  rf.rssi @ dup PL 2+ c! 8 arshift PL 3 + c!
  \ rf.rssi PL 2+ 2 move
  rf.fei PL 4 + 4 move
  seqno.rx @ dup PL 8 + c! 8 rshift PL 9 + c! ;

: set-led ( buf len -- ) \ turn LED on/off based on payload
  dup 0<> if
    ( buf len ) drop c@ 0= if led ios! else led ioc! then
  else
    ( buf len ) drop drop
  then ;

: msg-received  ( buf len -- ) \ the dnl.process word
  rf-stats-PL
  set-led ;

: ack-received  (  -- ) \ the dnl.acked word
  \ dnl.ack @ if ." received ACK from server" cr then 
  rf-stats-PL ;

: adr-received  (  -- ) \ the adr.command word
  \ decide to respond to ADR or not:
0 [if]    
  0 adr.ack ! \ clear ADR ack. Answer will not be send
[else]
  \ or adapt power and sf
  ." Apply ADR. Update power and SF" cr
  led-on
  adr.power @ rf-power \ TODO: Check whether this is a proper moment to set TX power.
  adr.sf @ rf.sf !
  \ do not clear flag here. TX needs to set confirmation options.
[then]
  ;

: setup
  16MHz 1000 systick-hz
  \ The Forth implementation does not perform HW reset, 
  \ and polls IRQ registers of the radio.
  \ IMODE-FLOAT PA13 io-mode! \ reset
  \ IMODE-FLOAT PA8  io-mode! \ DIO0
  \ IMODE-FLOAT PA1  io-mode! \ DIO1
  \ IMODE-FLOAT PB0  io-mode! \ DIO2

  lptim-init

  \ test-msg
  1 tune.delay !
  9 rf.sf !
  868100 $34 rf-init-LW \ $34 is LORAWAN Preamble!

\ Some PA config in the LMIC code. Needed?
\  14 $80 or RF:PA rf!
\  $5A rf@ $04 or $5A rf! \ RegPADAC

\ 
  $32 $0B rf! \ Over-current protection @150mA
\  $87 RF:PADAC rf! \ 20dB mode
\  09FF h, \ 17dBm output power


  \ rf.buf framelen @ rf-tx

  nwskey nwskey.ptr !
  appskey appskey.ptr !
  devaddr dev.addr !

  \ NOTE: dnl.prcoess functions take ( payload-buf payload-len -- ) as stack input output
  ['] msg-received dnl.process !      \ set XT to process received payload
  ['] ack-received dnl.acked !        \ set XT to ACK received
  ['] adr-received adr.command !      \ set XT to process ADR command 
  ;

: run-one
    \ cr ." Sending: " helloworld h.n cr
    \ test-msg
    PL 10 init-msg                    \ Initialize Lora message with PL
    \ rf.buf framelen @ h.n
    rf.sf @ rf-band-sel               \ select new band and use given SF
    \ RF:M_STDBY rf!mode -12000 rf.fei ! rf-correct \ correct according latest rf.fei
    rf-txrx                           \ send uplink message and receive donwlink
  ;

: run
  begin
    seqno @ 10 mod 0= if true upl.ackreq ! then \ every tenth message ask for ACK to determine FEI. 
    seqno @ 35 mod 0= if true upl.adrreq ! then \ every nth message ask for ADR request to check how options work. 
    run-one
    \ led ios!
    1 ms \ allow usart to empty.
    2.1MHz drop 1000 systick-hz
    stop60s \ hsi-on needed for recovery? Seems best to first switch to 2.1MHz.
    stop60s
    16MHz drop 1000 systick-hz
    \ led ioc!
h.s
  key? until
  ;

1 [if]

: init init unattended
  setup
  false adr.enabled !  \ reset ADR administration at TTN
  run-one 5000 ms 
  run-one 5000 ms 
  led-off
  true adr.enabled !
  20 rf-power
  12 rf.sf ! run ;
[then]

compiletoram

\ setup
\ cr ." Lora Radio initialized" cr
\ 12 rf.sf !
\ run
