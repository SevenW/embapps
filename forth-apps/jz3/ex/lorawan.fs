
\ LoraWAN
\ Author: SevenW from sevenwatt.com
\ Date  : 2017-Feb-27
\ 
\ Description:
\ LoraWAN ABP
\ Supports TX, RX1, RX2, ACK/ACKREQ and ADR
\ 
\ Requires : lora1276.fs
\ Requires : aes128.fs
\ Requires : aes-ctr-cmac.fs
\ 
\ Usage    : See demo app in lorawan-abp.fs
\ 
\ Tweaks: 
\ n tune.delay !     \ move RXn window by n ms
\ true upl.ackreq !  \ request ACK from server
\ true upl.adrreq !  \ request ADR update from server
\ n rf.sf !          \ ( n=7..12 ) set the spread factor
\ 
\ Limitations: 868MHz (EU) only, LoraWAN ABP only.
\ 
\ 

: h.16 ( caddr -- )
  16 0 do dup i + c@ h.2 ."  " loop drop cr ;

: h.n ( caddr len -- )
  ( len ) 0 do dup i + c@ h.2 ."  " loop drop cr ;

\ LoraWAN / Radio constants
\ $19 constant RF:PktSnrValue
\ $1A constant RF:PktRssiValue
$1F constant RF:SymbTimeoutLSB
$33 constant RF:InvertIQ

\ 868 frequencies. Duty cycle max 0.01 at those frequencies. g-band
create FREQUENCIES
868100000 ,
868300000 ,
868500000 ,
867100000 ,
867300000 ,
867500000 ,
867700000 ,
867900000 , 

8 constant MAX.FRQS
7 variable freq.idx

9  variable rf.sf
14 variable rf.power
\ 0 variable rf.snr
\ 0 variable rf.rssi

\ 
\ Node ID and keys
\ 

0 variable nwskey.ptr
0 variable appskey.ptr
0 variable dev.addr

\ 
\ LMIC protocol
\ 

\ HDR frame types
$40 constant LW.FT-UP
$60 constant LW.FT-DN
$80 constant LW.FT-UPCNF
$A0 constant LW.FT-DNCNF
$E0 constant LW.FT \ mask

\ frame control attributes
$80 constant LW.FCT-ADREN
$40 constant LW.FCT-ADRREQ
$20 constant LW.FCT-ACK
$10 constant LW.FCT-MORE
$0F constant LW.FCT-OPTLEN

$03 constant CMD:LADR-REQ
$03 constant CMD:LADR-ANS

0 variable txend            \ timestamp of exact end of TX
false variable sleep.wait   \ false=wait, true=sleep

0 variable dndir            \ 0 = uplink, 1 = downlink
0 variable seqno
0 variable seqno.rx

64 constant MAX.FRAME       \ 64 bytes mazimum frame length fro LORA. Fits 66 bytes rf.buf   
\ MAX.FRAME buffer: frame   \ use rf.buf
0 variable framelen         \ frame length (total)
9 variable pl-start         \ payload start index in frame
9 variable pl-len           \ length of payload in frame

false variable dnl.ackreq   \ The downlink (received msg) requested for an ACK
false variable upl.ackreq   \ The application requests an ACK from the server on next transmit msg
false variable upl.adrreq   \ The application requests an ADR update from the server on next transmit msg
false variable dnl.ack      \ ACK received from downlink in last received msg
true variable dnl.valid     \ The downlink (received msg) has valid headers
true variable dnl.micvalid  \ The downlink (received msg) has a valid MIC
0 variable dnl.optlen       \ length of options in received frame buffer
0 variable dnl.port         \ port of the received message
0 variable dnl.pl-start     \ payload start index in received frame buffer
0 variable dnl.received     \ 0=no msg received. 1=recevied in RX1. 2=received in RX2
0 variable dnl.process      \ function pointer to process received payload
0 variable dnl.acked        \ function pointer to process received ACK

\ ADR
false variable adr.enabled
$FF00 variable adr.ch-mask
14    variable adr.power
9     variable adr.sf
0     variable adr.ack
0     variable adr.command  \ function pointer to process ADR command

\ 
\ LoraWAN Radio
\ 

create rf:init-LW  \ initialise the radio, each 16-bit word is <reg#,val>
hex
  0188 h, \ OpMode = LoRA+LF+sleep
  0188 h, \ repeat
  098E h, \ 14dBm output power                   \ adapted
  0B2B h, \ Over-current protection @100mA       \ adapted
  0C23 h, \ max LNA gain
  0D00 h, \ FIFO ptr = 0
  0E00 h, \ FIFO TX base = 0
  0F00 h, \ FIFO RX base = 0
  1000 h, \ FIFO RX current = 0
\  11F7 h, \ mask all interrupts excpet TXdone    \ adapted
  1137 h, \ mask all interrupts excpet TXdone, RXdone and RXTimeout    \ adapted
  1f00 h, \ rx-single timeout                    \ adapted limc = 00, rxsymbtimeout
  2000 h, 2108 h, \ preamble of 8
  \ 2342 h, \ max payload of 66 bytes
  2340 h,                                        \ adapted to 64
  2400 h, \ no freq hopping
  2604 h, \ enable LNA AGC
  2700 h, \ no ppm freq correction
  31C3 h, \ detection optimize for SF7-12        \ adapted
  3367 h, \ no I/Q invert                        \ adapted
  370A h, \ detection threshold for SF7-12
  4000 h, \ DIO mapping 1
  4100 h, \ DIO mapping 2

  \ 0A08 h, \ PAconf
  \ 0C21 h, \ 
  \ 1D00 h, \ 
  \ 3B5D h, \ 
  \ 40F0 h, \ 
  \ 5311 h, \ 

  0 h,  \ sentinel
decimal align

\ val1 = BW+CR val2 = SF val3 = MobileNode/LNA
\ 125Khz bw, 4/5 coding rate, spreading fct 9 = ????bps
: rf!bw125cr45sf9  $72 $94 $04 rf!rate 125000 rf.bw ! ;

: rf-power ( power -- ) \ Set the TX power level in dB (2..20)
  dup 17 > if 
    $87 RF:PADAC rf! 
    20 rf-power ! drop 17 
  else
    $84 RF:PADAC rf!
    dup rf-power !
  then
  dup 2 < if drop 2 dup rf-power ! then
  2 - RF:PA rf@ $F0 and or $80 or RF:PA rf! ;

: rf-sf ( sf -- ) \ set spreading factor 6-12
  dup rf.sf !
  RF:M_STDBY rf!mode 
  ( sf ) dup 4 lshift $04 or RF:MODEMCONF2 rf! 
  ( sf ) 11 >= if $0C else $04 then RF:MODEMCONF3 rf! ;

: rf-sleep-until ( t-ms -- )  \ wait until a given timestamp with radio in SLEEP
  \ lpmode stop100ms can't be used as it stops the systick.
  dup millis - dup 22 > if    \ wait longer then 22 ms?
    RF:M_SLEEP rf!mode        \ set radio to sleep
    20 - ms                   \ wait until 20ms before deadline
    RF:M_STDBY rf!mode        \ set radio to standby
  else drop then
  millis - ms ;               \ wait until deadline

: rf-wait-until ( t-ms -- )   \ wait until a given timestamp
  millis - ms ;               \ wait until deadline

: rf-init-LW ( frq id ) \ initialize the radio for LoraWAN
  spi-init
  $AA rf-check  $55 rf-check  \ will hang if there is no radio!
  rf:init-LW rf-config!
  RF:M_SLEEP rf!mode \ init rf.mode var
  ( id ) rf!sync ( frq) rf!freq rf!bw125cr45sf9 ;

: rf-band-sel ( sf -- )  \ Select next frequency and set SF=n, CR=4/5, BW=125
  freq.idx @ 1 + dup MAX.FRQS >= if drop 0 then dup freq.idx !
  ( idx ) cells FREQUENCIES + @ rf!freq
  rf!bw125cr45sf9
  ( sf ) rf-sf ;

: rf@rxstat ( -- ) \ retrieve FEI, SNR and RSSI from last RX
  rf@status ;
  \ RF:PktSnrValue  rf@ rf.snr  !
  \ RF:PktRssiValue rf@ 137 - rf.rssi ! ;

: rf-rxdone ( -- flag ) \ true if packet received
  begin RF:IRQFLAGS rf@ RF:IRQ_RXDONE RF:IRQ_RXTIMEOUT or and 0= while 50 ms repeat 
  RF:IRQFLAGS rf@ RF:IRQ_RXDONE and 0<> ;

: rf-recv-pkt ( t-rx -- flag )  \ receive one packet
  RF:M_STDBY rf!mode
  \ LNA LIMC sets $0C = $21, but that is unspecified value. Use default $23
  \ Payload max: at default $40
  \ use inverted I/Q signal (prevent mote-to-mote communication)
  RF:InvertIQ rf@ $40 or RF:InvertIQ rf!
  \ set symbol timeout (for single rx)
  5 RF:SymbTimeoutLSB rf!
  \ set sync word: Already done by rf-init
  \ configure DIO mapping DIO0=RxDone DIO1=RxTout DIO2=NOP \ skip. Polling mode
  \ $?? RF:DIOMAPPING1 rf!
  \ $?? RF:DIOMAPPING2 rf!
  rf!clrirq
  \ external interrupt: set IRQ flag mask here. Enable interrupts
  ( t-rx ) sleep.wait @ if rf-wait-until else rf-sleep-until then \ test RX performance with wait and sleep
  RF:M_RXSINGLE rf!mode               \ start receiver
  millis . ." RXn" cr                 \ print millisecond timestamp
  rf-rxdone
  RF:M_STDBY rf!mode
  ;

\ 
\ encryption and MIC
\ 

: copy-mic ( mic buf -- )
  4 move ;

: b= ( c-addr1 c-addr2 -- flag ) c@ swap c@ = ;

: mic= ( buf1 buf2 ) true 4 0 do 2 pick 2 pick b= and loop -rot drop drop ;

16 buffer: lora.iv

: iv-init ( -- )  \ initialize IV. Generic part
  lora.iv 16 0 fill
  dndir @   lora.iv 5 + c!
  dev.addr   lora.iv  6 + 4 move   \ NOTE: LSB first. Could be platform dependent
  dndir @ 0= if seqno else seqno.rx then
  ( seqno ) lora.iv 10 + 4 move ; \ NOTE: LSB first. Could be platform dependent

: iv-ctr ( -- iv ) \ Initialize IV for AES-CTR
  iv-init
  1 lora.iv c!
  1 lora.iv 15 + c! 
  lora.iv ;

: iv-cmac ( len -- iv )  \ Initialize IV for AES-CMAC (MIC)
  iv-init
  $49 lora.iv c!
  ( len ) lora.iv 15 + c! 
  lora.iv ;

\ 
\ Build message
\ 

: clear-frame ( -- )
  rf.buf MAX.FRAME 0 fill ;

: adr-answer-opt ( -- optlen )
  adr.ack @ 0<> if
    CMD:LADR-ANS rf.buf 8 + c!
    adr.ack @ rf.buf 9 + c!
    0 adr.ack ! \ clear
    2 \ optlen
  else
    0
  then ;


: init-frame ( -- ) \ Initialize LoraWAN TX message header
  clear-frame
  \ $40 = HDR_FTYPE_DAUP | HDR_MAJOR_V1
  \ $80 = HDR_FTYPE_DCUP | HDR_MAJOR_V1
  upl.ackreq @ if LW.FT-UPCNF else LW.FT-UP then rf.buf c!
  false upl.ackreq ! 

  dev.addr rf.buf 1+ 4 move \ TODO: Check endiannes. Might not be portable.

  \ flags + options length
  \ LMIC.adrEnabled   =  FCT_ADREN = $80
  \ AckRequested: LMIC.dnConf = HDR_FTYPE_DCDN = $20
  \ ( n ) rf.buf 5 + c! (LMIC.dnConf | LMIC.adrEnabled | (LMIC.adrAckReq >= 0 ? FCT_ADRARQ : 0) | (end-OFF_DAT_OPTS))
  adr.enabled @ if LW.FCT-ADREN else $00 then
    dnl.ackreq @ if LW.FCT-ACK or then 
    false dnl.ackreq !
    upl.adrreq @ if LW.FCT-ADRREQ or then 
    false upl.adrreq !
    rf.buf 5 + c!
  
  seqno @ rf.buf 7 + over 8 rshift over c! ( n rf.buf6 ) 1- c!

  adr-answer-opt ( optlen )
  dup rf.buf 5 + c@ or rf.buf 5 + c!
  9 + pl-start !
 
  \ TxPort (=1) put in last byte before payload
  pl-start @  1- rf.buf + 1 swap c!
;

: >payload ( c-addr len -- ) \ Add payload to frame buffer
  dup pl-len ! tuck rf.buf pl-start @ + swap move 
  dup pl-start @ + framelen !
  ( len ) drop ;

\ 
\ Transmit message
\ 
: tx-pars. ( ms -- ) \ Print TX parameters
  ." TX:   "
  ." len=" framelen @ .
  ( ms ) ." dur=" . ." ms "
  ." #=" seqno @ . 
  ." frq=" rf.nomfreq @ .
  ." pwr=" rf.power @ .
  ." SF=" rf.sf @ .
  ." CR=4/5 "
  ." BW=125 "
  rf.buf 5 + c@ LW.FCT-ACK and 0<> if ." ACK " then
  rf.buf c@  LW.FT-UPCNF = if ." ACKREQ " then
  cr ;

: rf-tx ( addr count -- ) \ LoraWAN rf-transmit.
  millis -rot
  1- swap dup 1+ -rot c@ rf-send    
  millis txend !
  txend @ . ." TX" cr                 \ print millisecond timestamp
  millis swap - tx-pars. ;

\ 
\ Receive message
\ 

: seqno-rx@ ( buf -- ) \ Extract seqno from RX buffer
  dup 1+ c@ 8 lshift swap c@ or
  \ TODO: Figure out the C code
  \ ( new-seqno ) seqno.rx @ - $FFFF and seqno.rx @ + \ seqno = LMIC.seqnoDn + (u2_t)(seqno - LMIC.seqnoDn);
  seqno.rx ! ;

: hdr-check ( buf len -- ) \ Check validaity of RX header and extract parameters
  true dnl.valid !
  over c@                           \ hdr checks
  dup LW.FT and dup LW.FT-DN <> swap LW.FT-DNCNF <> and if ." rx header type error" cr false dnl.valid ! then
  dup $03 and 0<> if ." rx header type error" cr false dnl.valid ! then
  LW.FT and LW.FT-DNCNF = dnl.ackreq !
  ( buf len ) dup 12 < if ." rx data too short" cr false dnl.valid ! then  \ check minimal message length
  over 5 + c@                       \ flags field checks
  ( fct ) dup LW.FCT-OPTLEN and 
  ( fct optlen ) dup dnl.optlen ! 9 + dnl.pl-start ! \ Extract option length. Calculate start of payload.
  ( fct ) LW.FCT-ACK and 0<> dnl.ack !
  \ b4 of byte 5 is FCT-MORE. Purpose?

  dnl.optlen @ 8 + c@ dnl.port c!

  \ TODO: Check on dev.addr
  \ TODO: Check seqno > previous

  ( buf len ) drop drop
;

create adr.pow-lvl
  \ 20 C, 14 C, 11 C, 8 C, 5 C, 2 C, 0 C, 0 C, \ max power protocol = 20
  15 C, 14 C, 11 C, 8 C, 5 C, 2 C, 0 C, 0 C,   \ max power RFM95 = 15 ( or 17? )

: decode-options ( opts len -- )
  dup 0 > if
    ( opts len) over c@ CMD:LADR-REQ = if \ ADR command from gateway
      swap ( len opts )
      \ MCMD_LADR_ANS_POWACK = 4 | MCMD_LADR_ANS_DRACK = 2 | MCMD_LADR_ANS_CHACK = 1
      \ Only respond to power and SF in next uplink message
      $00 adr.ack ! 
      \ opts[1] txpow + dr (sf) 
      dup 1+ c@ dup 
        4 rshift dup 6 < if 12 swap - adr.sf ! adr.ack @ $02 or adr.ack ! else drop then
        $0F and dup 6 < if adr.pow-lvl + c@ adr.power ! adr.ack @ $04 or adr.ack ! else drop then
      \ opts[2..3] channel mask. this app uses first 8 channels. 
      \ expected value = $FF $00
      dup dup 2+ c@ 8 lshift swap 3 + c@ or adr.ch-mask !
      \ opts[4] mask $F0 chpage is always 0
      \ opts[4] mask $0F up repeat count is not used in LMIC      $06 adr.ack ! 
      swap ( opts len )
    then
  then drop drop ;

: decode ( len -- c-addr len ) \ Decode RX message. Decrypt. Check MIC.
  \ rf.buf over ." buf:   " h.n
  rf.buf over hdr-check
  rf.buf 8 + dnl.optlen @ decode-options
  4 -                                 \ len excluding mic
  1 dndir !                           \ used in deriving IV for AES-CTR and AES-CMAC
  rf.buf 6 + seqno-rx@
  dup rf.buf swap dup nwskey.ptr @ swap ( len ) iv-cmac ( iv )
  \ over ." key:   " h.16
  \ dup ." iv:    " h.16
  ( len buf len key iv ) aes-cmac ( mic )
  \ dup ." mic:   " h.16
  ( len mic ) over rf.buf + mic= dnl.micvalid ! 
  ( len ) dnl.pl-start @ - dup 0 > if
    ( plen )dup rf.buf dnl.pl-start @ + swap  appskey.ptr @ iv-ctr
    ( plen buf plen key iv ) aes-ctr  \ decrypt payload
    \ ." decr:  " rf.buf dnl.pl-start @ + over h.n
  else
    \ ." decr: none" cr
  then
  ( plen ) rf.buf dnl.pl-start @ + swap ( buf pl-len )
  0 dndir !
;

0 variable tune.delay \ For experimenting with exact timing of opening RXn window

: preamble-delay ( sf -- msdelay ) \ Calculate offset to open RX window half-way preamble
  \ PAMBL_SYMS * hsym - LMIC.rxsyms * hsym = ( 8 * hsym - 5 * hsym )
  $80 swap 5 - lshift \ half symbol time in us
  3 *                 \ delay in us
  10 rshift           \ delay in ms /1024 is clode enough
  tune.delay @ -      \ correction
  ;

: rx-pars. ( len rxn -- ) \ Print RX paramters
  1 = if ." RX1:  " else ." RX2:  " then
  ." len=" .
  ." #=" seqno.rx @ . 
  ." fei=" rf.fei @ . 
  ." snr=" rf.snr @ . 
  ." rssi=" rf.rssi @ .
  dnl.valid @ if ." HDR:  OK " else ." HDR: NOK " then
  dnl.micvalid @ if ." MIC:  OK " else ." MIC: NOK " then
  dnl.ack @ if ." ACK " then
  dnl.ackreq @ if ." ACKREQ " then
  cr ;

: specials. ( -- ) \ Print special message types
  dnl.optlen @ dup 0 > if ." Opts: " rf.buf 8 + swap h.n else drop then ;

: options. ( -- ) \ Print options buffer
  dnl.optlen @ dup 0 > if ." Opts: " rf.buf 8 + swap h.n else drop then ;

: payload. ( buf len  -- ) \ Print payload buffer
  dup 0 > if ." PayL: " h.n else drop drop then ;

: process-raw-rx ( RXn ) \ Interpret received message. Call application functions
    dnl.received !                          \ store RXn
    rf@rxstat                               \ retrieve FEI, SNR and RSSI
    rf-rxpkt ( len )                        \ extract packet to rf.buf
    ( len ) decode ( buf len )              \ decode buffer
    ( len ) dup dnl.received @ dup 0<> if ( len rxn ) rx-pars. else drop drop then
    over over payload.                      \ print payload 
    options.                                \ print options if any
    ( buf len ) dup 0 > if
      dnl.process @ execute                 \ call application process-payload word
    else
      drop drop
    then
    dnl.ack @ if dnl.acked @ execute then   \ call application process-ack word
    false dnl.ack !                         \ clear the ACK flag
    adr.ack @ if adr.command @ execute then \ call application adr-command word
    false adr.ack !                         \ clear the ADR-ACK flag
;

: rf-rxn ( RXn t-rx -- ) \ Open recieve window n. Process or timeout
  ( RXn t-rx ) rf-recv-pkt if ( RXn ) process-raw-rx else drop then ;

: rf-rx1 ( -- ) \ Open recieve window RX1. Process or timeout
  \ use transmit frequency and spread factor
  1 \ RXn
  txend @ 1000 + rf.sf @ preamble-delay + \ timestamp for receive
  rf-rxn ;
  
: rf-rx2  ( -- ) \ Open recieve window RX2. Process or timeout
  RF:M_STDBY rf!mode
  869525000 rf!freq
  rf!bw125cr45sf9
  2 \ RXn
  txend @ 2000 + 9 preamble-delay + \ timestamp for receive
  rf-rxn ;

: rf-txrx
    rf.buf framelen @ rf-tx           \ send message
    rf-rx1                            \ Receive in window RX1
    rf-rx2                            \ Receive in window RX2
    RF:M_SLEEP rf!mode                \ Set Radio to sleep
    1 seqno +!                        \ Increment sequence number
  ;

: init-msg ( buf len ) \ Compose the TX message with payload buffer as input
  init-frame ( buf len ) >payload                               \ copy payload to rf.buf buffer
  \ cr ." input: " rf.buf 32 h.n
  rf.buf pl-start @ + pl-len @ appskey.ptr @ iv-ctr             \ prepare for encryption
  ( buf len key iv ) aes-ctr                                    \ encrypt payload
  rf.buf framelen @ nwskey.ptr @ framelen @ ( len ) iv-cmac     \ prepare for MIC (hash key)
  ( buf len key iv ) aes-cmac ( mic )                           \ calculate MIC
  rf.buf framelen @ + copy-mic 4 framelen +!                    \ copy MAC to end of rf.buf buffer
;
