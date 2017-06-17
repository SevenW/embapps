\ hardware timers
\ this only handles TIMER 2, 3, 6, and 7

$00 constant TIM.CR1
$04 constant TIM.CR2
$0C constant TIM.DIER
$28 constant TIM.PSC
$2C constant TIM.ARR

: timer-base ( n -- addr )  \ return base address for timer 1..14
  2- $400 * $40000000 + ;

: timer-enabit ( n -- bit addr )  \ return bit and enable address for timer n
  2- bit RCC-APB1ENR ;

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
