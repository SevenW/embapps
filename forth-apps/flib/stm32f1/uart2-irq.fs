\ interrupt-based USART2 with input ring buffer
\ needs ring.fs
\ needs uart2-stm32f1.fs

128 4 + buffer: uart-ring

: uart-irq-handler ( -- )  \ handle the USART receive interrupt
  USART2-DR @  \ will drop input when there is no room left
  uart-ring dup ring? if >ring else 2drop then ;

$E000E104 constant NVIC-EN1R \ IRQ 32 to 63 Set Enable Register

: uart-irq-init ( -- )  \ initialise the USART2 using a receive ring buffer
  uart-init
  uart-ring 128 init-ring
  ['] uart-irq-handler irq-usart2 !
  6 bit NVIC-EN1R !  \ enable USART2 interrupt 38
  5 bit USART2-CR1 bis!  \ set RXNEIE
;

: uart-irq-key? ( -- f )  \ input check for interrupt-driven ring buffer
  uart-ring ring# 0<> ;
: uart-irq-key ( -- c )  \ input read from interrupt-driven ring buffer
  begin uart-irq-key? until  uart-ring ring> ;
