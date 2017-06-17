\ Serial flash memory SPI chip interface for WinBond W25Q16, etc.
\ uses spi words, assumes spi-init has been called

: smem-cmd ( cmd -- )  +spi >spi ;
: smem-page ( u -- )  dup 8 rshift >spi >spi 0 >spi ;
: smem-wait ( -- )  \ wait in a busy loop as long as spi memory is busy
  -spi  $05 smem-cmd  begin spi> 1 and 0= until  -spi ;
: smem-wcmd ( cmd -- )  smem-wait  $06 smem-cmd -spi smem-cmd ;

: smem-id ( -- u)  \ return the SPI memory's manufacturer and device ID
  $9F smem-cmd spi> 8 lshift spi> or 8 lshift spi> or -spi ;
: smem-size ( -- u )  \ return size of spi memory chip in KB
  smem-id $FF and 10 -  bit ;

: smem32b ( -- u )  \ get 4 SPI bytes and return them as one 32b word
  0  4 0 do 8 lshift spi> or loop ;
: smem-uid ( -- u1 u2 )  \ return the chip's 64-bit unique ID
  $4B smem-cmd 4 0 do 0 >spi loop smem32b smem32b -spi ;

: smem-wipe ( -- )  \ wipe entire flash memory
  $60 smem-wcmd  smem-wait ;
: smem-erase ( page -- )  \ erase one 4K sector in flash memory
  $20 smem-wcmd smem-page  smem-wait ;

: smem> ( addr page )  \ read 256 bytes from specified page
  $03 smem-cmd smem-page  256 0 do spi> over c! 1+ loop drop  -spi ;
: >smem ( addr page )  \ write 256 bytes to specified page
  $02 smem-wcmd smem-page  256 0 do dup c@ >spi 1+ loop drop  smem-wait ;
