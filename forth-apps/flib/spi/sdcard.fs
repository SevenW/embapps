\ SD Card interface using SPI
\ uses spi

: sd>slow> ( c -- c )  \ bit-banged SPI, 8 bits, well under 400 KHz
  8 0 do
    dup $80 and MOSI io!
    2 us  SCLK ios!  2 us
    shl
    MISO io@ or
    2 us  SCLK ioc!  2 us
  loop
  $FF and ;

: sd-slow ( u -- )
  -spi 2 us +spi 2 us
  dup 24 rshift dup $3F and [char] # emit . sd>slow> drop
              0 sd>slow> drop
              0 sd>slow> drop
  dup 16 rshift sd>slow> drop
   dup 8 rshift sd>slow> drop
                sd>slow> drop
            $FF sd>slow> drop
            $FF sd>slow> . ;

: sd-init ( -- )
  spi-init
  10 0 do $FF sd>slow> drop loop
  +spi
  $40000095 sd-slow  \ CMD0 go idle
\ $41000000 sd-slow  \ CMD1 send op cond
\ $50020000 sd-slow  \ CMD16 send blocklen 512
\ $7B000000 sd-slow  \ CMD59 crc off
\ $4801AA87 sd-slow  \ CMD8 send if cond 3.3v
  $77000000 sd-slow  $69000000 sd-slow  \ ACMD41 send op cond
  200 ms
  $77000000 sd-slow  $69000000 sd-slow  \ ACMD41 send op cond
;

: sd-cmd ( cmd arg -- )
  swap dup $80 and if  $77 0 recurse  $7F and  then
  -spi +spi
  begin $FF >spi> $FF = until
  cr dup $3F and .
                >spi
  dup 24 rshift >spi
  dup 16 rshift >spi
   dup 8 rshift >spi
                >spi
              0 >spi
            $FF >spi
            $FF >spi> . ;

: sd-wait ( -- )
  -1  50 0 do  $FF >spi> $FE = if drop i leave then  loop  . ;

: sd-read ( page addr -- f )  \ read one 512-byte page from sdcard
  $51 rot 9 lshift sd-cmd sd-wait
  512 0 do  $FF >spi> over i + c!  loop
  drop true ;  \ TODO return actual success flag

512 buffer: sd.buf

0 variable sd.fat
0 variable sd.spc
0 variable sd.root
0 variable sd.data

: sd-c>s ( cluster -- sect ) 2- sd.spc @ * sd.data @ + ;

: sd-mount ( -- )
                sd-init       \ initialise interface and card
       0 sd.buf sd-read drop  \ read block #0
  sd.buf $1C6 + @             \ get location of boot sector
         dup 1+ sd.fat !      \ start sector of FAT area
     dup sd.buf sd-read drop  \ read boot record
   sd.buf $0D + c@            \ sectors per cluster
                sd.spc !      \ depends on formatted disk size
   sd.buf $0E + h@            \ reserved sectors
   sd.buf $10 + c@            \ number of FAT copies
   sd.buf $16 + h@            \ sectors per fat
      * + + dup sd.root !     \ start sector of root directory
   sd.buf $11 + h@            \ max root entries
     4 rshift + sd.data !     \ start sector of data area

\ sd.buf $2B + 11 type  [char] : emit  sd.buf $36 + 8 type  \ label & format
;
