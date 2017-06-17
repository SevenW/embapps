\ Pulse Width Modulation
\ needs io-stm32f1.fs
\ needs timer-stm32f1.fs

\ The following pins are supported for PWM setup on STM32F1xx:
\   TIM1:   PA8  PA9  PA10 PA11
\   TIM2:   PA0  PA1  PA2  PA3
\   TIM3:   PA6  PA7  PB0  PB1
\   TIM4:   PB6  PB7  PB8  PB9
\ Pins sharing a timer will run at the same repetition rate.
\ Repetition rates which are a divisor of 7200 will be exact.

: p2tim ( pin -- n ) \ convert pin to timer (1..4)
  case
    dup PA4 <                ?of 2 endof
    dup PB1 >                ?of 4 endof
    dup PA7 > over PB0 < and ?of 1 endof
    dup PB6 <                ?of 3 endof
  endcase ;

: p2cmp ( pin -- n ) \ convert pin to output comp-reg# - 1 (0..3)
  dup
  case
    dup PA4 <                ?of 0 endof
    dup PB1 >                ?of 2 endof
    dup PA7 > over PB0 < and ?of 0 endof
    dup PB6 <                ?of 2 endof
  endcase
  + 3 and ;

\ : t dup p2tim . p2cmp . ." : " ;
\ : u                             \ expected output:
\   cr PA8 t PA9 t PA10 t PA11 t  \  1 0 : 1 1 : 1 2 : 1 3 :
\   cr PA0 t PA1 t PA2  t PA3  t  \  2 0 : 2 1 : 2 2 : 2 3 :
\   cr PA6 t PA7 t PB0  t PB1  t  \  3 0 : 3 1 : 3 2 : 3 3 :
\   cr PB6 t PB7 t PB8  t PB9  t  \  4 0 : 4 1 : 4 2 : 4 3 :
\ ;
\ u

: pwm-init ( hz pin -- )  \ set up PWM for pin, using specified repetition rate
  >r  OMODE-PP r@ io-mode!  r@ ioc!  \ start with pwm zero, i.e. fully off
  7200 swap / 1- 16 lshift 10000 or  r@ p2tim timer-init
  $78 r@ p2cmp 1 and 8 * lshift ( $0078 or $7800 )
  r@ p2tim timer-base $18 + r@ p2cmp 2 and 2* + bis!
  r@ p2cmp 4 * bit r> p2tim timer-base $20 + bis! ;

: pwm-deinit ( pin -- )  \ disable PWM, but leave timer running
  dup p2cmp 4 * bit swap p2tim timer-base $20 + bic! ;

\ since zero PWM generates a single blip, set the GPIO pin to normal mode and
\ set its output to "0" - in all other cases, switch the pin to alternate mode

: pwm ( u pin -- )  \ set pwm rate, 0 = full off, 10000 = full on
  over if OMODE-AF-PP else OMODE-PP then over io-mode!  \ for fully-off case
  10000 rot - swap  \ reverse the sense of the PWM count value
  dup p2cmp cells swap p2tim timer-base + $34 + !  \ save to CCR1..4
;
