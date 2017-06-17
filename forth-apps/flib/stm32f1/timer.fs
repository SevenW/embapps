\ hardware timers

$00 constant TIM.CR1
$04 constant TIM.CR2
$0C constant TIM.DIER
$28 constant TIM.PSC
$2C constant TIM.ARR

create timer-table
  111 c,  \ TIM1  APB2
  0   c,  \ TIM2  APB1
  1   c,  \ TIM3  APB1
  2   c,  \ TIM4  APB1
  3   c,  \ TIM5  APB1
  4   c,  \ TIM6  APB1
  5   c,  \ TIM7  APB1
  113 c,  \ TIM8  APB2
  119 c,  \ TIM9  APB2
  120 c,  \ TIM10 APB2
  121 c,  \ TIM11 APB2
  6   c,  \ TIM12 APB1
  7   c,  \ TIM13 APB1
  8   c,  \ TIM14 APB1
calign

: timer-lookup ( n - pos ) 1- timer-table + c@ ;

: timer-base ( n -- addr )  \ return base address for timer 1..14
  timer-lookup
  dup 100 < if  $400 * $40000000  else  111 - $400 * $40012C00  then + ;

: timer-enabit ( n -- bit addr )  \ return bit and enable address for timer n
  timer-lookup
  dup 100 < if  bit RCC-APB1ENR  else  100 - bit RCC-APB2ENR  then ;

: timer-init ( u n -- )  \ enable timer n as free-running with period u
  dup timer-enabit bis!  \ clock enable
             timer-base >r
  dup 16 rshift TIM.PSC r@ + h!    \ upper 16 bits are used to set prescaler
                TIM.ARR r@ + h!    \ period is auto-reload value
         8 bit TIM.DIER r@ + bis!  \ UDE
  %010 4 lshift TIM.CR2 r@ + !     \ MMS = update
          0 bit TIM.CR1 r> + !     \ CEN
;

: timer-deinit ( n -- )  \ disable timer n
  timer-enabit bic! ;
