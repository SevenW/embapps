\ polled access to second UART (USART2)

[ifndef] TX2  PA2 constant TX2  [then]
[ifndef] RX2  PA3 constant RX2  [then]

$40004400 constant USART2
     USART2 $00 + constant USART2-CR1
     \ USART2 $04 + constant USART2-CR2
     USART2 $08 + constant USART2-CR3
     USART2 $0C + constant USART2-BRR \ baud rate register
     \ USART2 $10 + constant USART2-GTPR \ guard time and prescaler register
     USART2 $1C + constant USART2-ISR \ interrupt status register
     USART2 $20 + constant USART2-ICR \ interrupt flag clear register
     USART2 $24 + constant USART2-RDR \ receive data register
     USART2 $28 + constant USART2-TDR \ transmit data register

: uart2. ( -- )
  USART2
  cr ."     CR1 " dup @ hex. 4 +
     ."     CR2 " dup @ hex. 4 +
     ."     CR3 " dup @ hex. 4 +
  cr ."     BRR " dup @ hex. 16 +
     ."     ISR " dup @ hex. 4 +
     ."  X  ICR " dup @ hex. drop ;

: uart2-key? ( -- f ) 5 bit USART2-ISR bit@ ;
: uart2-emit? ( -- f ) 7 bit USART2-ISR bit@ ;
\ if overrun detection is enabled: uart2-key? ( -- f ) 8 USART2-ICR bis! 5 bit USART2-ISR bit@ ;

: uart2-key ( -- c ) begin uart2-key? until USART2-RDR @ ;
: uart2-emit ( c -- ) begin uart2-emit? until USART2-TDR ! ;

: uart2-baud ( n -- ) dup 2/ 16000000 + swap / USART2-BRR ! ; \ assumes 16Mhz clock

: uart2-init ( -- )  \ set up hardware USART
  OMODE-AF-PP TX2   io-mode!
  OMODE-AF-PP RX2   io-mode!
  TX2 io-base GPIO.AFRL + dup @ $ff00 bic $4400 or swap ! \ assumes PA2&PA3

  17 bit RCC-APB1ENR bis!  \ enable clock: set USART2EN
  $0 USART2-CR1 ! \ make sure it's disabled
  12 bit USART2-CR3 ! \ disable overrun detection
  9600 uart2-baud
  $00121b5f USART2-ICR ! \ clear all status flags
  USART2-RDR @ drop \ clear rx register
  $D USART2-CR1 ! \ tx-en, rx-en, uart-en
  ;
