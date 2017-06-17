\ I/O pin primitives

$40010800 constant GPIO-BASE
      $00 constant GPIO.CRL   \ reset $44444444 port Conf Register Low
      $04 constant GPIO.CRH   \ reset $44444444 port Conf Register High
      $08 constant GPIO.IDR   \ RO              Input Data Register
      $0C constant GPIO.ODR   \ reset 0         Output Data Register
      $10 constant GPIO.BSRR  \ reset 0         port Bit Set/Reset Reg
      $14 constant GPIO.BRR   \ reset 0         port Bit Reset Register

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
: io@ ( pin -- f )  \ get pin value (true or false)
  dup io-base GPIO.IDR + @ swap io# rshift 1 and negate ;
: ioc! ( pin -- )  \ clear pin to low
  dup io-mask swap io-base GPIO.BRR + ! ;
: ios! ( pin -- )  \ set pin to high
  dup io-mask swap io-base GPIO.BSRR + ! ;
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
  dup io-base GPIO.CRL + over 8 and shr + >r ( R: crl/crh )
  io# 7 and 4 * ( mode shift )
  $F over lshift not ( mode shift mask )
  r@ @ and -rot lshift or r> ! ;

: io-modes! ( mode pin mask -- )  \ shorthand to config multiple pins of a port
  16 0 do
    i bit over and if
      >r  2dup ( mode pin mode pin R: mask ) $F bic i or io-mode!  r>
    then
  loop 2drop drop ;

: io. ( pin -- )  \ display readable GPIO registers associated with a pin
  cr
    ." PIN " dup io#  dup .  10 < if space then
   ." PORT " dup io-port [char] A + emit
  io-base
  ."   CRL " dup @ hex. 4 +
   ."  CRH " dup @ hex. 4 +
   ."  IDR " dup @ h.4  4 +
  ."   ODR " dup @ h.4 drop ;
