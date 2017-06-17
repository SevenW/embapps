\ control the Si570 adjustable clock generator
\ needs i2c

[ifndef] SI570.FREQ  100000000 constant SI570.FREQ  [then]

        6 buffer:  si.buf
        0 variable si.max
114285000 variable si.mul

: si@ ( u -- b ) 7 - si.buf + c@ ;
: si! ( b u -- ) 7 - si.buf + c! ;

: freq@ ( -- ud )
  12 si@
  11 si@  8 lshift +
  10 si@ 16 lshift +
   9 si@ 24 lshift +
   8 si@ $3F and ;

: freq! ( ud -- )
  8 si@ or       8 si!
  dup 24 rshift  9 si!
  dup 16 rshift 10 si!
  dup  8 rshift 11 si!
                12 si! ;

: si-rd ( addr n reg -- addr+n )
  $55 i2c-addr >i2c
  dup i2c-xfer drop
  0 do i2c> over c! 1+ loop ;

: si-wr ( addr n reg -- addr+n )
  $55 i2c-addr >i2c
  0 do dup c@ >i2c 1+ loop
  0 i2c-xfer drop ;

: si-set
  $55 i2c-addr 137 >i2c 1 i2c-xfer drop i2c>
  dup $55 i2c-addr 137 >i2c $10 or >i2c 0 i2c-xfer drop
  si.buf 6 7 si-wr drop
  $55 i2c-addr 137 >i2c $EF and >i2c 0 i2c-xfer drop
  $55 i2c-addr 135 >i2c $40 >i2c 0 i2c-xfer drop ;

: si570-init ( -- nak )
  i2c-init  $55 i2c-addr 135 >i2c 1 >i2c 0 i2c-xfer
  dup 0= if 
    20 ms  si.buf 6 7 si-rd drop
    SI570.FREQ
    ( hs: ) 7 si@ 5 rshift 4 +
    ( n1: ) 7 si@ $1F and 2 lshift  8 si@ 6 rshift  + 1+
    * um* ( d:freq*hs*n1 )
    28 bit 0 udm* 2drop ( d:freq*hs*n1<<28 )
    freq@ ud/mod 2nip ( d:[freq*hs*n1<<28]/rfreq )
    drop si.mul !
  then ;

: try-div ( n1 hs -- n1 hs f )
\ ." n1? " over . ." hs? " dup . ." #" depth .
  2dup * si.max @ <= ;

: divisors ( khz -- n1 hs )
  5670000 over /
\ dup ." max " .
  si.max !
  4849999 swap / 11 / 1+
\ dup ." min " .
  begin
    dup 1 and 0= if
      11 try-div if exit then
      2- try-div if exit then
      2- try-div if exit then
      1- try-div if exit then
      1- try-div if exit then
      1- try-div if exit then
      drop
    then
    1+
  again ;

: si570-freq ( freq -- )
  dup 1000 < if 1000 * then
  dup 1000000 < if 1000 * then
  dup 1000 / divisors ( freq n1 hs )
\ 2dup ." hs " . ." n1 " .
  2dup 4 - 5 lshift over 1- 2 rshift or 7 si!
       1- 6 lshift                      8 si!
  * um* ( d:freq*hs*n1 )
  28 bit 0 udm* 2drop ( d:freq*hs*n1<<28 )
  si.mul @ 0 ud/mod 2nip ( d:[freq*hs*n1<<28]/rfreq )
\ 2dup hex. hex.
  freq! si-set ;

\ si570-init .
\ 123 si570-freq ( 123 MHz )
\ 123456 si570-freq ( 123.456 MHz )
\ 123456789 si570-freq ( 123.456789 MHz )
