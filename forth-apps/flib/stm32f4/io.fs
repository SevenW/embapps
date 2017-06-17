\ I/O pin primitives

$40020000 constant GPIO-BASE
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
: io@ ( pin -- u )  \ get pin value (0 or 1)
  dup io-base GPIO.IDR + @ swap io# rshift 1 and ;
: ios! ( pin -- )  \ set pin to high
  dup io-mask swap io-base GPIO.BSRR + ! ;
: ioc! ( pin -- )  \ clear pin to low
  16 + ios! ;
: io! ( f pin -- )  \ set pin value
  \ use upper 16 bits in BSRR to reset with same operation
  swap 0= $10 and + ios! ;
: iox! ( pin -- )  \ toggle pin
  dup io@ 0= swap io! ;

%0000 constant IMODE-ADC    \ input, analog
%0100 constant IMODE-FLOAT  \ input, floating
%1000 constant IMODE-PULL   \ input, pull-up/down

%0001 constant OMODE-PP     \ output, push-pull
%0101 constant OMODE-OD     \ output, open drain
%1001 constant OMODE-AF-PP  \ alternate function, push-pull
%1101 constant OMODE-AF-OD  \ alternate function, open drain

  %01 constant OMODE-SLOW   \ add to OMODE-* for 2 MHz iso 10 MHz drive
  %10 constant OMODE-FAST   \ add to OMODE-* for 50 MHz iso 10 MHz drive

: io-mode! ( mode pin -- )  \ set the CNF and MODE bits for a pin
  dup io-base GPIO.MODER + over 8 and shr + >r ( R: crl/crh )
  io# 7 and 4 * ( mode shift )
  $F over lshift not ( mode shift mask )
  r@ @ and -rot lshift or r> ! ;

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
