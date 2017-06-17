\ interrupt-based USART2 with input ring buffer
\ needs ring.fs
\ needs uart2.fs

128 4 + buffer: uart2-ring

: uart2-irq-handler ( -- )  \ handle the USART receive interrupt
  USART2-RDR @  \ will drop input when there is no room left
  uart2-ring dup ring? if >ring else 2drop then ;

$E000E100 constant NVIC-EN0R \ IRQ 0 to 31 Set Enable Register

: uart2-init ( -- )  \ initialise the USART2 using a receive ring buffer
  uart2-init
  uart2-ring 128 init-ring
  ['] uart2-irq-handler irq-usart2 !
  28 bit NVIC-EN0R bis!  \ enable USART2 interrupt 28
  5 bit USART2-CR1 bis!  \ set RXNEIE
;

: uart2-key? ( -- f )  \ input check for interrupt-driven ring buffer
  uart2-ring ring# 0<> ;
: uart2-key ( -- c )  \ input read from interrupt-driven ring buffer
  begin uart2-key? until  uart2-ring ring> ;
