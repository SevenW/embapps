\ Pulse Width Modulation
\ needs timer-stm32l0.fs

\ The following pins are supported for PWM setup on STM32L05x:
\   TIM2:   PA0  PA1  PA2  PA3
\ Pins sharing a timer will run at the same repetition rate.
\ Repetition rates which are a divisor of 1600 will be exact.

: p2tim ( pin -- n ) drop 2 ;  \ convert pin to timer (1..4)

: p2cmp ( pin -- n ) $3 and ;  \ convert pin to output comp-reg# - 1 (0..3)

\ : t dup p2tim . p2cmp . ." : " ;
\ : u                             \ expected output:
\   cr PA0 t PA1 t PA2  t PA3  t  \  2 0 : 2 1 : 2 2 : 2 3 :
\ ;
\ u

: pwm-init ( hz pin -- )  \ set up PWM for pin, using specified repetition rate
  >r  OMODE-AF-PP r@ io-mode!
  1600 swap / 1- 16 lshift 10000 or  r@ p2tim timer-init
  $78 r@ p2cmp 1 and 8 * lshift ( $0078 or $7800 )
  r@ p2tim timer-base $18 + r@ p2cmp 2 and 2* + bis!
  r@ p2cmp 4 * bit r> p2tim timer-base $20 + bis! ;

: pwm-deinit ( pin -- )  \ disable PWM, but leave timer running
  dup p2cmp 4 * bit swap p2tim timer-base $20 + bic! ;

: pwm ( u pin -- )  \ set pwm rate, 0 = full off, 10000 = full on
  10000 rot - swap  \ reverse to sense of the PWM count value
  dup p2cmp cells swap p2tim timer-base + $34 + !  \ save to CCR1..4
;
