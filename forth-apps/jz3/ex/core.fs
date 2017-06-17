\ core libraries

<<<board>>>
cr compiletoflash
( core start: ) here dup hex.

: rf-send ; \ to make varint happy even without radio driver
include ../../flib/any/buffers.fs
\ include ../../flib/spi/rf69.fs
include ./lora1276.fs
include ../../flib/any/varint.fs
\ include gps.fs

include ./aes128.fs
\ include ./aes-ctr-cmac.fs
include ./lorawan.fs

( core end, size: ) here dup hex. swap - .
cornerstone <<<core>>>
compiletoram
