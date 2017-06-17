\ read out the TLV493 sensor
\ needs i2c

: evenParity? ( u -- f )  \ true if the 32-bit input has even parity
  dup 16 lshift xor
  dup  8 lshift xor
  dup  4 lshift xor
  dup  2 lshift xor
  dup       shl xor  
  31 arshift  not ;

10 buffer: tlv.info

: tlv-rd ( n -- )  \ read n bytes from sensor to info buffer
  $5E i2c-rx drop
  tlv.info  swap 1- 0 do 0 i2c> over c! 1+ loop  1 i2c> swap c!
  i2c-stop ;

: tlv-wr ( u -- )  \ write configuration bytes to sensor
  dup evenParity? 15 bit and or
  $5E i2c-tx drop  3 0 do dup >i2c drop 8 rshift loop i2c-stop drop ;

: tlv-init ( -- )  \ configure the TLV493 for 10 Hz ultra-low power sampling
  9 tlv-rd
  tlv.info 10 dump
\ write regs 0..3: $00 MOD1 r8 MOD2
\   MOD1 = P r7<6:3> 0 0 1
\   MOD2 = 0 0 0 r9<4:0>
  tlv.info 7 +
  dup     ( r7 ) c@ %01111000 and %1 or 8 lshift
  over 1+ ( r8 ) c@ 16 lshift or
  swap 2+ ( r9 ) c@ %00011111 and 24 lshift or
  dup evenParity? 15 bit and or
  tlv-wr ;

: tlv-sign ( u u -- n )
  $F and swap 4 lshift or  
  dup $800 and if $1000 - then ;

: tlv-data ( -- x y z )
  6 tlv-rd
\ 10 0 do tlv.info i + c@ h.2 space loop
  tlv.info dup    c@  over 4 + c@ 4 rshift tlv-sign
          over 1+ c@  over 4 + c@          tlv-sign
       rot dup 2+ c@  swap 5 + c@          tlv-sign ;

\ tlv-init
\ tlv-data . . .
