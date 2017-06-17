\ Semtech sx1276 LoRA driver.
\ This radio driver uses the LoRA modulation on the sx1276 to send and receive packets.

\ The radio is connected via SPI and at this point the driver is primarily
\ geared towards 433Mhz operation. Some minor changes need to be made to register
\ settings and rssi calculations for operation in the 868Mhz or 915Mhz bands.

\ The driver assumes that the antenna is connected to PA_BOOST, which is the case for the
\ HopeRF rfm95 thru rfm98 modules, as well as the Dorji DRF1276 modules.

\ Packet header byte like old JeeLabs format:
\ http://jeelabs.org/2011/06/10/rf12-broadcasts-and-acks/index.html
\ bit 7 ctrl: 0=data 1=special
\ bit 6 dest: 0=to-GW 1=from-GW
\ bit 5 ack : 0=no-ack 1=ack-req
\ bit 4 unused
\ bits 0-3: 15 nodes, 0=broadcast
\ c=0, a=0 : data, no ack requested
\ c=0, a=1 : data, ack requested
\ c=1, a=0 : ack
\ c=1, a=1 : unused

\ Packet type byte (2nd byte):
\ bit 7 info: 0=no info, 1=last 2 bytes have rssi & fei
\ bit 6 unused
\ bit 0-5: 32 packet types

\ Packet type:
\  0: empty (may have info)
\  1: node info Vstart[mV], Vend[mV], Temp[cC], PktSent, PktRecv, ...?
\  2: 

\ Ack packets either consist of just the header byte or header plus info.

$00 constant RF:FIFO
$01 constant RF:OP
$06 constant RF:FRF
$09 constant RF:PA
$0C constant RF:LNA
$0D constant RF:FIFOPTR
$10 constant RF:FIFORXCURR
$11 constant RF:IRQMASQ
$12 constant RF:IRQFLAGS
$13 constant RF:RXBYTES
$18 constant RF:MODEMSTAT
$19 constant RF:PKTSNR
$1A constant RF:PKTRSSI
$1D constant RF:MODEMCONF1
$1E constant RF:MODEMCONF2
$22 constant RF:PAYLENGTH
$26 constant RF:MODEMCONF3
$27 constant RF:PPMCORR
$28 constant RF:FEI
$39 constant RF:SYNC
$40 constant RF:DIOMAPPING1
$41 constant RF:DIOMAPPING2
$4D constant RF:PADAC

0 constant RF:M_SLEEP
1 constant RF:M_STDBY
2 constant RF:M_FSTX
3 constant RF:M_TX
4 constant RF:M_FSRX
5 constant RF:M_RXCONT
6 constant RF:M_RXSINGLE
7 constant RF:M_CAD

  7 bit constant RF:IRQ_RXTIMEOUT
  6 bit constant RF:IRQ_RXDONE
  5 bit constant RF:IRQ_CRCERROR
\ 4 bit constant RF:IRQ_VALIDHDR
  3 bit constant RF:IRQ_TXDONE
  2 bit constant RF:IRQ_CADDONE
\ 1 bit constant RF:IRQ_FHSCHG
\ 0 bit constant RF:IRQ_CADDETECT

   0 variable rf.mode    \ current op mode (3 bits)
   0 variable rf.bw      \ bandwidth in Hz
   0 variable rf.snr     \ last pkt rcv SNR in dB
   0 variable rf.rssi    \ last pkt rcv RSSI in dBm
   0 variable rf.fei     \ last pkt rcv FEI in Hz

  66 constant RF:MAXPKT  \ max packet length supported
RF:MAXPKT buffer:  rf.buf

433500000 variable rf.nomfreq \ nominal frequency in Hz
433500000 variable rf.actfreq \ actual frequency in Hz

\ ===== reading/writing registers

\ r/w access to the RF registers
: rf! ( b reg -- ) $80 or >spi2 ; \ inline ;     \ write register
: rf@ ( reg -- b ) spi2> ; \ inline ;            \ read register
: rf-n@spi ( addr len -- ) $00 spiN> ; \ inline ;  \ read N bytes from the FIFO
: rf-n!spi ( addr len -- ) $80 >spiN ; \ inline ;  \ write N bytes to the FIFO

\ ===== changing radio configuration

: rf!mode ( b -- )  \ set the radio mode, and store a copy in a variable
  dup rf.mode !
  RF:OP rf@ $F8 and  or RF:OP rf! ;

: rf>freq ( u -- ) \ set the frequency, internal operation
  dup rf.actfreq !
  524288 32000000 u*/
  dup 16 rshift RF:FRF rf!
  dup  8 rshift RF:FRF 1+ rf!
  RF:FRF 2+ rf!
  ;

: rf!freq ( u -- )  \ set the frequency, supports any input precision
  begin dup 100000000 < while 10 * repeat
  dup rf.nomfreq !
  rf>freq ;

: rf!sync ( u -- ) RF:SYNC rf! ;  \ set the sync byte

: rf-power ( n -- )  \ change TX power level in dB (5..20)
  RF:PA rf@ $E0 and or RF:PA rf! ;

: rf-sleep ( -- ) RF:M_SLEEP rf!mode ;  \ put radio module to sleep

: rf!rate ( mc1 mc2 mc3 -- ) \ sets modem config registers 1, 2 and 3
  RF:M_STDBY rf!mode RF:MODEMCONF3 rf! RF:MODEMCONF2 rf! RF:MODEMCONF1 rf!
  ;

\ == data rates used by LoRaWAN
\ The name consists of the bandwidth.spreading-factor, the coding rate is 4/5 on all,
\ the two slowest have low-data-rate optimization. The comment shows the raw bit rate,
\ the time to Tx a 20-byte packet, and the Rx sensitivitry.

: rf!lora125.12  $72 $c4 $0C rf!rate 125000 rf.bw ! ; \   250bps, 1318ms, -137dBm
: rf!lora125.11  $72 $b4 $0C rf!rate 125000 rf.bw ! ; \   440bps,  660ms, -136dBm
: rf!lora125.10  $72 $a4 $04 rf!rate 125000 rf.bw ! ; \   980bps,  370ms, -134dBm
: rf!lora125.9   $72 $94 $04 rf!rate 125000 rf.bw ! ; \  1760bps,  185ms, -131dBm
: rf!lora125.8   $72 $84 $04 rf!rate 125000 rf.bw ! ; \  3125bps,  103ms, -128dBm
: rf!lora125.7   $72 $74 $04 rf!rate 125000 rf.bw ! ; \  5470bps,   57ms, -125dBm
: rf!lora250.7   $82 $74 $04 rf!rate 250000 rf.bw ! ; \ 11000bps,   28ms, -122dBm

\ == data rates from 40bps to 15kbps in steps of 6dBi sensitivity

\ 10.4khz bw, 4/6 coding rate, spreading fct 9 =  163bps, -140dBm
\ : rf!bw10cr6sf9    $14 $94 $0C rf!rate  10400 rf.bw ! ;
\ 10.4khz bw, 4/6 coding rate, spreading fct 7  = 650bps, -136dBm
\ : rf!bw10cr6sf7    $14 $74 $04 rf!rate  10400 rf.bw ! ;
\ 62.5khz bw, 4/6 coding rate, spreading fct 9  = 976bps, -134dBm
\ : rf!bw62cr6sf9    $64 $94 $04 rf!rate  62500 rf.bw ! ;
\ 62.5khz bw, 4/6 coding rate, spreading fct 7 = 3906bps, -128dBm
\ : rf!bw62cr6sf7    $64 $74 $04 rf!rate  62500 rf.bw ! ;
\ 250khz bw, 4/5 coding rate, spreading fct 7 = 15625bps, -122dBm
\ : rf!bw250cr5sf7   $82 $74 $04 rf!rate 250000 rf.bw ! ;

\ == combinations from radiohead library

\ 500Khz bw, 4/5 coding rate, spreading fct 7 = 21875bps
\ : rf!bw500cr45sf7  $92 $74 $04 rf!rate 500000 rf.bw ! ;
\ 125Khz bw, 4/5 coding rate, spreading fct 7 = 5468bps
\ : rf!bw125cr45sf7  $72 $74 $04 rf!rate 125000 rf.bw ! ;
\ 125Khz bw, 4/8 coding rate, spreading fct 12 = 183bps
\ : rf!bw125cr48sf12 $78 $c4 $04 rf!rate 125000 rf.bw ! ;
\ 31.25Khz bw, 4/8 coding rate, spreading fct 9 = 275bps
\ : rf!bw31cr48sf9   $48 $94 $04 rf!rate  31250 rf.bw ! ;

: rf-correct ( -- ) \ corrrect for fei: change center freq and adjust bit rate
  rf.fei @ 2 arshift \ apply 1/4 of measured offset as correction
  rf.bw @ 2 arshift  \ don't apply more than 1/4 of rx bandwidth
  over 0< if negate max else min then
  rf.actfreq @ swap - dup rf>freq
  rf.nomfreq @ dup -rot - ( nomfreq delta-freq )
  swap 20 arshift /  \ ppm offset measured
  RF:PPMCORR rf!     \ set bit rate correction
  ;

\ ===== utilities

: rf!clrirq ( -- ) \ clear IRQ
  $ff RF:IRQFLAGS rf! ;

: rf@status ( -- ) \ fetch snr, rssi, lna, and fei for packet and save
  RF:PKTSNR rf@ 24 lshift 26 arshift    \ sign-extend and divide by 4
  dup rf.snr !
  RF:PKTRSSI rf@ dup 4 arshift + 164 -  \ calculate rssi
  swap dup 0< if + else drop then       \ if snr<0 adjust rssi
  rf.rssi !
  RF:FEI rf@ 28 lshift 12 arshift      \ sign-extend
  RF:FEI 1+ rf@ 8 lshift or
  RF:FEI 2+ rf@ or
  rf.bw @ 100 / * 9537 /               \ 9537=32Mhz*500/2^24/100
  rf.fei !
  \ RF:LNA rf@ 5 rshift rf.lna !       \ apparently can't get actual LNA
  ;

\ ===== printing

: rf. ( -- )  \ print out all the radio registers
  cr 4 spaces  base @ hex  16 0 do space i . loop  base !
  $60 $00 do
    cr
    i h.2 ." :"
    16 0 do  space
      i j + ?dup if rf@ h.2 else ." --" then
    loop
  $10 +loop ;

: rf.state ( -- ) \ print rx/tx state
  ." mode=" RF:OP rf@ 7 and .
  ." modem=" RF:MODEMSTAT rf@ h.2
  ."  irq=" RF:IRQFLAGS rf@ h.2
  ."  rssi=" $1B rf@ 164 - .
  cr
  \ rf. cr
  ;

: rf.lna>db ( u -- u ) \ convert lna register value 1..6 to dB
  case 
  1 of 0 endof
  2 of -6 endof
  3 of -12 endof
  4 of -24 endof
  5 of -36 endof
  6 of -48 endof
  0
  endcase
  ;

: .n ( n -- ) \ print signed integer without following space
  dup abs 0 <# #S rot sign #> type ;

: rf-info ( -- )  \ display reception parameters as hex string
  rf.snr @ h.2 rf.rssi @ h.2 rf.fei @ h.4 ;

: rf>uart ( len -- len )  \ print reception parameters
  rf.nomfreq @ dup .n
  rf.actfreq @ swap -
  dup 0 >= if [char] + emit then .n  ." Hz "
  rf.snr @ .n ." dB "
  rf.rssi @ .n ." dBm "
  \ rf.lna @ rf.lna>db .n ." dB " doesn't seem to work...
  rf.fei @ .n ." Hz "
  dup .n ." b "
  ;

\ ===== send & receive

: rf+info ( c-addr -- ) \ add 2 info bytes at c-addr, which should be at end of packet
  rf.rssi @ 164 + 127 min
  over c!
  rf.fei @ 64 + 7 arshift
  \ dup 0< if 1+ then \ round towards 0
  swap 1+ c!
  ;

: rf-rxpkt ( -- b ) \ extract packet and return length
  RF:FIFORXCURR rf@ RF:FIFOPTR rf!                 \ set fifo pointer to start of pkt
  rf.buf RF:RXBYTES rf@ 66 min swap over rf-n@spi  \ pull packet out of fifo
  rf@status rf!clrirq                              \ get SNR, RSSI, etc, then clear IRQ
  ;

: rf-recv ( -- b )  \ check whether a packet has been received, return #bytes
  RF:IRQFLAGS rf@ RF:IRQ_RXDONE and if            \ check whether we got a packet
    rf-rxpkt
  else
    rf.mode @ RF:M_RXCONT <> if RF:M_RXCONT rf!mode then \ start RX if not running
    0 \ no packet recvd
  then
  ;

: rxend begin RF:IRQFLAGS rf@ $c0 and until ;

: rf-ack? ( -- f )  \ turn on receiver and wait for ack, return length of ack pkt
  rf!clrirq
  RF:M_RXSINGLE rf!mode
  rxend
  RF:IRQFLAGS rf@
  dup RF:IRQ_RXTIMEOUT and if drop 0 exit then
  dup RF:IRQ_CRCERROR and if drop 0 exit then
  drop rf-rxpkt
  \ ." LoRa ACK " rf>uart ." : "
  \ . cr \ 0 do rf.buf i + c@ . loop cr
  rf-correct ;

: rf-txdone ( -- )
  begin RF:IRQFLAGS rf@ RF:IRQ_TXDONE and 0= while 1 ms repeat ;

: rf-send ( addr count hdr -- )  \ send out one packet
  RF:M_STDBY rf!mode
  \ $40 RF:DIOMAPPING1 rf!
  \ $40 RF:DIOMAPPING2 rf!
  rf!clrirq
  0 RF:FIFOPTR rf!         \ reset fifo pointer
  over 1+ RF:PAYLENGTH rf! \ set packet length, incl header byte
  RF:FIFO rf!              \ push hdr byte
  ( addr count ) rf-n!spi  \ push rest of payload
  RF:M_TX rf!mode          \ start transmitter
  rf-txdone \ ??
  ;

: rf-ack ( -- ) \ send ACK
  0 0 $C0 rf.buf c@ $0F and or rf-send ;

\ ===== init / config

\ init values copied from Go driver
create rf:init  \ initialise the radio, each 16-bit word is <reg#,val>
hex
  0188 h, \ OpMode = LoRA+LF+sleep
  0188 h, \ repeat
  09FF h, \ 17dBm output power
  0B32 h, \ Over-current protection @150mA
  0C23 h, \ max LNA gain
  0D00 h, \ FIFO ptr = 0
  0E00 h, \ FIFO TX base = 0
  0F00 h, \ FIFO RX base = 0
  1000 h, \ FIFO RX current = 0
  1112 h, \ mask valid header and FHSS change interrupts
  1f40 h, \ rx-single timeout
  2000 h, 2108 h, \ preamble of 8
  2342 h, \ max payload of 66 bytes
  23ff h,
  2400 h, \ no freq hopping
  2604 h, \ enable LNA AGC
  2700 h, \ no ppm freq correction
  3103 h, \ detection optimize for SF7-12
  3327 h, \ no I/Q invert
  370A h, \ detection threshold for SF7-12
  4000 h, \ DIO mapping 1
  4100 h, \ DIO mapping 2
  0 h,  \ sentinel
decimal align

: rf-h! ( h -- ) \ used to write config: top byte is reg-addr, bottom is value to write
  dup $FF and swap 8 rshift rf! ;

: rf-config! ( addr -- ) \ write config sequence
  begin  dup h@  ?dup while  rf-h!  2+ repeat drop ;

: rf-check ( b -- )  \ check that the chip can be accessed over SPI using the FRF-lsb reg
  begin  dup $08 rf!  $08 rf@  over = until drop
  0 $08 rf! \ set things back
  ;

: rf-init ( freq sync -- )  \ init the LoRA radio module
  spi-init
  $AA rf-check  $55 rf-check  \ will hang if there is no radio!
  rf:init rf-config!
  RF:M_SLEEP rf!mode \ init rf.mode var
  rf!sync rf!freq rf!lora125.8 ;

\ ===== sample tests

: q. ( c -- ) \ print character with quoting
  dup $20 < if [char] ~ emit h.2 else
  dup $7f > if [char] ~ emit h.2 else
  emit then then
  ;

0 variable p

: rf-listen ( -- )  \ init RFM69 and report incoming packets until key press
  RF:M_RXCONT rf!mode
  begin
    rf-recv ?dup if
      ." LoRa " rf>uart ." : "
      0 do rf.buf i + c@ q. loop cr
      rf-correct
    else 5 ms p dup c@ tuck 1+ swap c!
      0= if rf.state then
    then
  key? until ;

: rf-txtest ( n -- )  \ send out a test packet with the number as ASCII chars
  16 rf-power  0 <# #s #> 0 rf-send ;

\ Questions about the sx1276:
\ - It appears that AGC is available in LoRa mode, but it's not described anywhere and the
\   reference driver does not use it. What's the deal?
\ - What are the relative LNA gains in dB for the LNA settings 1..6 in LoRa mode? The
\   datasheet only has info about FSK/OOK (table 38).
\ - Is there a way to get the "packet RSSI" in FSK mode after an RxDone interrupt or is
\   only the instantaneous smoothed value at the time of reading the register available?
\ - What are the tradeoffs between LoRa bit rates that have the same receiver sensitivity
\   and data rates? For example, 10.4kHz/sf9/cr8 and 62.5kHz/sf12/cr8 both have approx
\   -140dB sensitivity (LF band) and 33-36ms tx time for a 10 byte packet. Clearly one uses
\   less spectrum and the other requires lower xtal tolerances. For what other reasons
\   might one choose one over the other?
\ - In LoRa mode, are the bit in the IRQ flags cleared by any state machine transitions or do
\   they have to be cleared explicitly?
\ - What is an ideal SNR or RSSI to target if one is adjusting the TX power in order to
\   save battery?
\ - How can I troubleshoot noise issues? I use explicit header mode and always enable
\   CRC generation, yet I see noise turning into valid packets (no CRC payload error).
\   How can I tell what happened and how do I protect against it? Put differently, is there
\   a way to tell whether a received packet actually had CRC protection?

\ [then]