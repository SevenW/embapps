\ I/O pin primitives

$50000000 constant GPIO-BASE
      $00 constant GPIO.MODER   \ Reset 0 Port Mode Register
                                \   00=Input  01=Output  10=Alternate  11=Analog
      $04 constant GPIO.OTYPER  \ Reset 0 Port Output type register
                                \   (0) Push/Pull vs. (1) Open Drain
      $08 constant GPIO.OSPEEDR \ Reset 0 Output Speed Register
                                \   00=2 MHz  01=25 MHz  10=50 MHz  11=100 MHz
      $0C constant GPIO.PUPDR   \ Reset 0 Pullup / Pulldown 
                                \   00=none  01=Pullup  10=Pulldown
      $10 constant GPIO.IDR     \ RO      Input Data Register
      $14 constant GPIO.ODR     \ Reset 0 Output Data Register
      $18 constant GPIO.BSRR    \ WO      Bit set/reset register
      $20 constant GPIO.AFRL    \ Reset 0 Alternate function  low register
      $24 constant GPIO.AFRH    \ Reset 0 Alternate function high register

: bit ( u -- u )  \ turn a bit position into a single-bit mask
  1 swap lshift  1-foldable ;
: bit! ( mask addr f -- )  \ set or clear specified bit(s)
  if bis! else bic! then ;

: io ( port# pin# -- pin )  \ combine port and pin into single int
  swap 8 lshift or  2-foldable ;
: io# ( pin -- u )  \ convert pin to bit position
  $1F and  1-foldable ;
: io-mask ( pin -- u )  \ convert pin to bit mask
  io# bit  1-foldable ;
: io-port ( pin -- u )  \ convert pin to port number (A=0, B=1, etc)
  8 rshift  1-foldable ;
: io-base ( pin -- addr )  \ convert pin to GPIO base address
  $F00 and 2 lshift GPIO-BASE +  1-foldable ;
: io@ ( pin -- u )  \ get pin value (0 or -1)
  dup io-base GPIO.IDR + @ swap io# rshift 1 and negate ;
: ios! ( pin -- )  \ set pin to high
  dup io-mask swap io-base GPIO.BSRR + ! ;
: ioc! ( pin -- )  \ clear pin to low
  16 + ios! ;
: io! ( f pin -- )  \ set pin value
  \ use upper 16 bits in BSRR to reset with same operation
  swap 0= $10 and + ios! ;
: iox! ( pin -- )  \ toggle pin
  dup io@ 0= swap io! ;

\ b6 = type, b5-4 = pull, b3-2 = mode, b1..0 = speed

%0000000 constant IMODE-FLOAT  \ input, floating
%0010000 constant IMODE-HIGH   \ input, pull up
%0100000 constant IMODE-LOW    \ input, pull down
%0001100 constant IMODE-ADC    \ input, analog

%0001010 constant OMODE-AF-PP  \ alternate function, push-pull
%1001010 constant OMODE-AF-OD  \ alternate function, open drain
%0000110 constant OMODE-PP     \ output, push-pull
%1000110 constant OMODE-OD     \ output, open drain

-2 constant OMODE-WEAK  \ add to OMODE-* for 400 KHz iso 10 MHz drive
-1 constant OMODE-SLOW  \ add to OMODE-* for 2 MHz iso 10 MHz drive
 1 constant OMODE-FAST  \ add to OMODE-* for 35 MHz iso 10 MHz drive


: io-config ( bits pin offset -- )  \ replace 2 bits in specified h/w register
  over io-base + >r   ( bits pin R: addr )
  io# shl             ( bits shift R: addr )
  %11 over lshift     ( bits shift mask R: addr )
  r@ @ swap bic       ( bits shift cleared R: addr )
  rot %11 and         ( shift cleared value R: addr )
  rot lshift or r> ! ;

: io-mode! ( mode pin -- )  \ set the CNF and MODE bits for a pin
  over          over GPIO.OSPEEDR io-config
  over 2 rshift over GPIO.MODER   io-config
  over 4 rshift over GPIO.PUPDR   io-config
  \ open drain mode config
  dup io-mask swap io-base GPIO.OTYPER +
  ( mode mask addr ) rot %1000000 and bit! ;

: io. ( pin -- )  \ display readable GPIO registers associated with a pin
  cr
   ." PIN " dup io#  dup .  10 < if space then
  ." PORT " dup io-port [char] A + emit
  io-base
    ."   MODER " dup @ hex. 4 +
  ."    OTYPER " dup @ h.4  4 +
  ."   OSPEEDR " dup @ hex. 4 +
      ."  PUPD " dup @ hex. 4 +
  cr 14 spaces
       ."  IDR " dup @ h.4  4 +
       ."  ODR " dup @ h.4  12 +
    ."    AFRL " dup @ hex. 4 +
       ." AFRH " dup @ hex. drop ;
