\ polled access to the second UART (USART2)

$40004400 constant USART2
   USART2 $00 + constant USART2-SR
   USART2 $04 + constant USART2-DR
   USART2 $08 + constant USART2-BRR
   USART2 $0C + constant USART2-CR1
\  USART2 $10 + constant USART2-CR2
\  USART2 $14 + constant USART2-CR3
\  USART2 $18 + constant USART2-GPTR

: uart. ( -- )
  cr ." SR " USART2-SR @ h.4
\   ."  DR " USART2-DR @ h.4
   ."  BRR " USART2-BRR @ h.4
   ."  CR1 " USART2-CR1 @ h.4 ;
\  ."  CR2 " USART2-CR2 @ h.4
\  ."  CR3 " USART2-CR3 @ h.4
\ ."  GPTR " USART2-GPTR @ h.4 ;

: uart-init ( -- )
  OMODE-AF-PP OMODE-FAST + PA2 io-mode!
  IMODE-FLOAT PA3 io-mode!
  17 bit RCC-APB1ENR bis!  \ set USART2EN
  115200 baud 2/ USART2-BRR ! \ set baud rate assuming PCLK1 = system-clock / 2
  %0010000000001100 USART2-CR1 ! ;

: uart-key? ( -- f ) 1 5 lshift USART2-SR bit@ ;
: uart-key ( -- c ) begin uart-key? until  USART2-DR @ ;
: uart-emit? ( -- f ) 1 7 lshift USART2-SR bit@ ;
: uart-emit ( c -- ) begin uart-emit? until  USART2-DR ! ;
