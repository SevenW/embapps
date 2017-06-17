\ variable int encoding for use in RF packets. Encodes up to 32-bit integers using
\ a variable-length encoding of 1..5 bytes. All values are encoded as signed
\ integers, which means that if an unsigned value is encoded the recipient needs
\ to know the integer's size in order to be able to cast it from int to uint.
\ For example, the unsigned 32-bit int 0xffffffff is encoded as if it was the
\ signed value -1. At the receiving end it will be decoded as -1, but as long as the
\ receiver knows that this is a 32-bit value it can be type-cast to uint32 to recover
\ 0xffffffff.
\ 
\ The encoding is as follows:
\ - the int is rotated 1 bit left placing the sign bit in the lowest bit position
\ - if bit 0 (now sign bit) is 1 (negative) all other bits are inverted
\ - group bits in the integer in 7-bit groups, emit a byte for each 7-bit group
\   starting with the highest non-zero group
\ - when emitting the last byte for the lowest bits, set the top bit of the byte
\ - the value zero encodes to 0x80
\ 
\ Dependency: needs rf-send to be defined, e.g. from rf69.h

\ Definitions to emit 32-bit ints as varints into the print buffer

: <v ( -- ) <# ;  \ prepare variable output
: v> ( -- caddr len ) 0 0 #> ;  \ finish, then return buffer and length

: >var ( n -- )  \ add one 32-bit value to output
  \ shift one position left - if negative, invert all bits (puts sign in bit 0)
  \ this compresses better for *signed* values of small magnitude
  rol dup 1 and 0<> shl xor
  \ output lowest 7 bits
  dup $80 or hold
  \ output higher 7-bit groups
  begin
    7 rshift
  dup while
    dup $7F and hold
  repeat drop ;

\ some definitions to build up and send a packet with varints.
\ for example, to send a packet of type 123, with values 11, 2222, and -3333:
\   123 <pkt 11 n+> 2222 n+> -3333 n+> pkt>rf

20 cells buffer: pkt.buf  \ room to collect up to 20 values for sending
      0 variable pkt.ptr  \ current position in this packet buffer

: >+pkt ( n -- ) pkt.ptr @ ! 4 pkt.ptr +! ;  \ append 32-bit signed value to packet

: <pkt ( format -- ) pkt.buf pkt.ptr ! >+pkt ;  \ start collecting values
: pkt>rf ( -- )  \ broadcast the collected values as RF packet
  <v
    pkt.ptr @  begin  4 - dup @ >var  dup pkt.buf u<= until  drop
  v> 0 rf-send ;

: *++ ( addr -- c )  dup @ c@  1 rot +! ;

\ variable-int decoding: call var-init once, then var> until it returns 0
\ see "var." below for an example of how these can be used

0 variable var.ptr
0 variable var.end

: var-init ( addr cnt -- )
  over + var.end ! var.ptr ! ;

: var> ( -- 0 | n 1 ) \ extract a signed number from the var buffer
  0
  var.ptr @ var.end @ u< if 
    begin
      7 lshift  var.ptr *++  tuck + swap
    $80 and until
    $80 -
    dup 1 and 0<> shl xor ror \ handle sign
    1
  then ;

: var. ( addr cnt -- )  \ decode and display all the varints
  var-init begin var> while . repeat ;
