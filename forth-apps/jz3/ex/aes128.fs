\ 128-bit AES Encryption
\ Author: SevenW from sevenwatt.com
\ Date  : 2017-Feb-21
\ 
\ Description:
\ AES128 block cipher encryption
\ AES128 CTR en- and de-cryption of a stream buffer
\ AES128 CMAC hash key calculation
\ Implementation is optimized on (low) memory usage.
\ For AES-CTR and AES-CMAC as used in LoraWAN, only AES-encryption is needed.
\ Decryption is supported in the separate file: aes128inv.fs
\ Testmodules are provided at https://github.com/SevenW/Forth-AES128
\ 
\ Usage    : ( c-addr key ) +aes
\ With     : c-addr : input data in a 16-byte buffer
\            key    : the encryption key in 16-bytes 
\ Output   : Encryption is in-situ so the 16-byte input data buffer contains the encrypted output.
\ 
\ Usage    : ( buf len key iv -- ) aes-ctr
\ With     : buf        : c-addr input data in a byte buffer
\            len        : the length of the encrypted data
\            key        : c-addr of the encryption key
\            iv         : c-addr of the initialization vector
\ Output   : Encryption is in-situ so the input data buffer contains the encrypted output.
\ Note     : Decryption is achieved by calling the encrypted data with the same IV and key.
\ 
\ Usage    : ( buf len key iv -- mic ) aes-cmac
\ With     : buf        : c-addr input data in a byte buffer
\            len        : the length of the encrypted data
\            key        : c-addr of the encryption key
\            iv         : c-addr of the initialization vector
\ Output   : mic        : c-addr of the mac. Lora uses the first four bytes as 32-bit mic
\ 
\ 


create s.box
hex
  63 C, 7C C, 77 C, 7B C, F2 C, 6B C, 6F C, C5 C, 30 C, 01 C, 67 C, 2B C, FE C, D7 C, AB C, 76 C,
  CA C, 82 C, C9 C, 7D C, FA C, 59 C, 47 C, F0 C, AD C, D4 C, A2 C, AF C, 9C C, A4 C, 72 C, C0 C,
  B7 C, FD C, 93 C, 26 C, 36 C, 3F C, F7 C, CC C, 34 C, A5 C, E5 C, F1 C, 71 C, D8 C, 31 C, 15 C,
  04 C, C7 C, 23 C, C3 C, 18 C, 96 C, 05 C, 9A C, 07 C, 12 C, 80 C, E2 C, EB C, 27 C, B2 C, 75 C,
  09 C, 83 C, 2C C, 1A C, 1B C, 6E C, 5A C, A0 C, 52 C, 3B C, D6 C, B3 C, 29 C, E3 C, 2F C, 84 C,
  53 C, D1 C, 00 C, ED C, 20 C, FC C, B1 C, 5B C, 6A C, CB C, BE C, 39 C, 4A C, 4C C, 58 C, CF C,
  D0 C, EF C, AA C, FB C, 43 C, 4D C, 33 C, 85 C, 45 C, F9 C, 02 C, 7F C, 50 C, 3C C, 9F C, A8 C,
  51 C, A3 C, 40 C, 8F C, 92 C, 9D C, 38 C, F5 C, BC C, B6 C, DA C, 21 C, 10 C, FF C, F3 C, D2 C,
  CD C, 0C C, 13 C, EC C, 5F C, 97 C, 44 C, 17 C, C4 C, A7 C, 7E C, 3D C, 64 C, 5D C, 19 C, 73 C,
  60 C, 81 C, 4F C, DC C, 22 C, 2A C, 90 C, 88 C, 46 C, EE C, B8 C, 14 C, DE C, 5E C, 0B C, DB C,
  E0 C, 32 C, 3A C, 0A C, 49 C, 06 C, 24 C, 5C C, C2 C, D3 C, AC C, 62 C, 91 C, 95 C, E4 C, 79 C,
  E7 C, C8 C, 37 C, 6D C, 8D C, D5 C, 4E C, A9 C, 6C C, 56 C, F4 C, EA C, 65 C, 7A C, AE C, 08 C,
  BA C, 78 C, 25 C, 2E C, 1C C, A6 C, B4 C, C6 C, E8 C, DD C, 74 C, 1F C, 4B C, BD C, 8B C, 8A C,
  70 C, 3E C, B5 C, 66 C, 48 C, 03 C, F6 C, 0E C, 61 C, 35 C, 57 C, B9 C, 86 C, C1 C, 1D C, 9E C,
  E1 C, F8 C, 98 C, 11 C, 69 C, D9 C, 8E C, 94 C, 9B C, 1E C, 87 C, E9 C, CE C, 55 C, 28 C, DF C,
  8C C, A1 C, 89 C, 0D C, BF C, E6 C, 42 C, 68 C, 41 C, 99 C, 2D C, 0F C, B0 C, 54 C, BB C, 16 C,
decimal

\ allocate working memory in RAM
16 buffer: scratch

\ lookup byte in s.box
: s.b@ ( b -- b ) dup $0F and swap 4 rshift $0F and 16 * + s.box + c@ ;

\ substitute scratch with bytes from s.box
: s.b-all@ scratch 16 0 do dup i + dup c@ s.b@ swap c! loop drop ;

\ shift bytes
: r1<
  4 scratch + dup c@ swap
  3 0 do dup i + dup 1+ c@ swap c! loop 3 + c! ;
: r2<
  8 scratch + dup dup c@ swap
  dup 2+ dup -rot c@ swap c! c!
  1+ dup c@ swap
  dup 2+ dup -rot c@ swap c! c! ;
: r3<
  12 scratch + dup 3 + c@ swap
  1 3 do dup i + dup 1- c@ swap c! -1 +loop c! ;
: sh-bytes r1< r2< r3< ;

\ mix columns
4 buffer: m1
4 buffer: m2

\ galios field mulitply modulus
: gfmod ( a -- a' )
  ( a ) dup 1 lshift ( a 2a ) swap ( 2a a ) $80 and $80 = if $1B xor ( 2a' ) then ( 2a ) ;

\ multiply column with vector [ 2, 3, 1, 1 ] for encryption
: m1m2
  ( r c )       over tuck ( r r c r ) 4 * + scratch + c@
  ( r r b1 )    dup gfmod
  ( r r b1 b2 ) rot ( r b1 b2 r ) m2 + c!
  ( r b1 )      swap m1 + c! ;

: mixing
  m2 c@ m1 1+ c@ m2 1+ c@ m1 2+ c@ m1 3 + c@ xor xor xor xor scratch i + c!
  m1 c@ m2 1+ c@ m1 2+ c@ m2 2+ c@ m1 3 + c@ xor xor xor xor scratch 4 + i + c!
  m1 c@ m1 1+ c@ m2 2+ c@ m1 3 + c@ m2 3 + c@ xor xor xor xor scratch 8 + i + c!
  m1 c@ m2 c@ m1 1+ c@ m1 2+ c@ m2 3 + c@ xor xor xor xor scratch 12 + i + c! ;

: mix-col
  4 0 do
    4 0 do i j m1m2 loop
    mixing
  loop ;

\ Round key (rk)
16 buffer: round.key
4 buffer: rk-val

: rk-init
  round.key 12 + rk-val 4 move ;

: rk-rotsub
  rk-val dup c@ s.b@ swap
  3 0 do dup i + dup 1+ c@ s.b@ swap c! loop 3 + c! ;

\ calculate rcon
: rcon ( round -- rcon )
  1 swap
  ( rcon round ) begin dup 1 <> while 
    swap dup $80 and swap 1 lshift swap $80 = if $1B xor then
    swap 1- 
  repeat drop ;

: xor-rcon ( round -- )
  \ only first byte xor
  rk-val dup c@ rot rcon xor swap c! ;

: rk-calc
  ( ii io )          4 * over + round.key + 
  ( ii rki )         swap rk-val +
  ( rki rkvali )     over c@ over c@ xor
  ( rki rkvali val ) tuck swap c! swap c! ;

: rk-update
  4 0 do
    4 0 do i j rk-calc loop
  loop ;  

\ Rijndael (incremental) key expansion
: round-key ( round -- )
  rk-init rk-rotsub xor-rcon rk-update ;

: rk+calc
  ( r c ) over over swap 4 * + scratch + -rot
  ( scratch-idx r c ) 4 * + round.key +
  ( scratch-idx rk-idx ) c@ over c@ xor swap c! ;

\ add round key
: round-key+
  4 0 do
    4 0 do i j rk+calc loop
  loop ;

\ move input block to scratch  
: >aes ( caddr -- ) \ input data block (16-bytes)
  scratch swap 
  4 0 do
    4 0 do
       ( scratch data )over over i + j 4 * + swap
       ( scratch data data-idx scratch) i 4 * + j + swap
       ( scratch data scratch-idx data-idx ) c@ swap c!
    loop
  loop drop drop ; 
 
\ move scratch to output block
: aes> ( caddr -- ) \ output encrypted data block (16-bytes)
  scratch 
  4 0 do
    4 0 do
       ( data scratch )over over i 4 * + j + swap
       ( data scratch scratch-idx data) i + j 4 * + swap
       ( data scratch data-idx scratch-idx ) c@ swap c! 
    loop
  loop drop drop ; 
 
\ store the first round key
: key-in ( caddr -- )
  round.key 16 move ;

\ perform one round of encryption
: one-round ( round )
  s.b-all@
  sh-bytes
  dup 10 <> if mix-col then
  ( round ) round-key
  round-key+ ;

\ encrypt block
: aes ( c-addr key -- ) \ aes128 encrypt block
  key-in
  dup >aes
  round-key+
  11 1 do i one-round loop
  \ input block is chiphered in-situ
  ( c-addr ) aes> ;

\ alias
: +aes ( c-addr key -- ) \ aes128 encrypt block (identical to aes)
  aes ; 
  
16 buffer: AESkey
16 buffer: AESaux

\ 
\ AES-CTR
\ 

16 buffer: ctr
0 variable buf-addr
0 variable buf-seg

: xor-buf-key+1 ( buf key -- buf+1 key+1 )
    ( buf key ) over c@ ( buf key val ) over c@ xor ( buf key val2 ) rot ( key val2 buf ) tuck ( key buf val2 buf ) c! 
    ( key buf ) 1+ swap 1+ \ increment pointers
    ( buf key ) ;

: aes-ctr-int ( buf-addr buf-len -- ) \ AES-CTR encrypt buffer.
  ctr swap
  ( buf ctr len ) 0 do
    i $0F and 0= if
      ( buf ctr ) drop ctr              \ reset ctr to bit 0
      ( buf ctr ) AESaux over 16 move   \ copy AESaux to ctr
      ( buf ctr ) dup AESkey aes        \ get exncrypted ctr
      ( buf ctr ) 1 AESaux 15 + c+!     \ used to be at end of loop but ctr contains the right key
    then
    ( buf ctr ) xor-buf-key+1
    ( buf+1 ctr+1 )
  loop drop drop ;

: aes-ctr ( buf len key iv )              \ AES-CTR encrypt buffer. Encryption is in-situ.
                                          \ buf: c-addr of buffer to be encrypted
                                          \ len: encryption length
                                          \ key: c-addr of 128-bit encryption key
                                          \ iv : c-addr of 16-byte initialization vector
  ( iv  ) AESaux 16 move
  ( key ) AESkey 16 move
  ( buf len ) aes-ctr-int ;


\ 
\ AES-CMAC
\ 

16 buffer: final.key
false variable padding
0 variable carry

: xor-key ( buf key len -- )
  ( len ) 0 do xor-buf-key+1 loop drop drop ;

: buf<<1 ( c-addr len )
  ( buf len )             0 swap \ initialise carry bit
  ( buf carry len )       0 swap 1- do \ loop from len to 0
  ( buf carry )           over i + tuck c@
  ( buf buf+i carry val ) tuck shl or ( buf  buf+i val val2 ) rot c!
  ( buf val )             $80 and if 1 else 0 then
  ( buf carry )           -1 +loop 
  drop drop ;

: cmac-calc-kn ( c-addr-fkey -- c-addr-fkey )
  ( fkey ) dup c@ $80 and 0<> \ if first bit of first byte is set
  ( fkey flag ) over 16 buf<<1
  ( fkey flag ) if dup 15 + dup c@ $87 xor swap c! then
  ( fkey ) ;

: cmac-xor-k1k2
  final.key
  ( fkey ) dup 16 0 fill
  ( fkey ) dup AESkey aes
  ( fkey ) cmac-calc-kn \ calc K1
  ( fkey ) padding @ if cmac-calc-kn then \ calc K2
  ( fkey ) AESaux swap 16 xor-key 
  ;

: cmac-calc ( buf len -- )
  padding over $0F and 0<> swap ! \ padding if len is not mulitple of 16.
  dup 1+ 0 do 
  ( buf+i len-i ) dup 0= if
  ( buf+i len-i )   AESaux i $0F and +  dup c@ $80 xor swap c! \ xor last byte with $80
  ( buf+i len-i )   cmac-xor-k1k2 \ perform this for last byte in buffer
  ( buf+i len-i )   AESaux AESkey aes
  ( buf+i len-i ) else 
  ( buf+i len-i )   over AESaux i $0F and + tuck ( . . auxaddr buf+i auxaddr ) c@ swap c@ xor swap c! \ xor aux and buf
  ( buf+i len-i )   i 1+ $0F and 0= if AESaux AESkey aes then \ mulitples of 16 bytes
  ( buf+i len-i ) then
  ( buf+i len-i ) 1- swap 1+ swap \ decrement len, increment pointer
  loop drop drop ;

: aes-cmac-noaux ( buf len )
  AESaux 16 0 fill
  cmac-calc ;

: aes-cmac-int ( buf-addr buf-len -- )  \ AES-CMAC hash key (mic) calculation
  AESaux AESkey aes
  cmac-calc ;

: aes-cmac ( buf len key iv -- mic )      \ AES-CMAC hash key calculation
                                          \ buf: c-addr of buffer to be encrypted
                                          \ len: encryption length
                                          \ key: c-addr of 128-bit encryption key
                                          \ iv : c-addr of 16-byte initialization vector
                                          \ mic: c-addr of message integrity check
  ( iv  ) AESaux 16 move
  ( key ) AESkey 16 move
  ( buf len ) aes-cmac-int
  AESaux ; \ put on stack as mic address

