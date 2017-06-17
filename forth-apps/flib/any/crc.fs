\ crc calculation

create crc16-table
hex
  $0000 h, $CC01 h, $D801 h, $1400 h, $F001 h, $3C00 h, $2800 h, $E401 h,
  $A001 h, $6C00 h, $7800 h, $B401 h, $5000 h, $9C01 h, $8801 h, $4400 h,
decimal

: crc16@ ( u -- u ) $F and shl crc16-table + h@ ;
: crc16h ( crc u -- crc ) crc16@ swap dup crc16@ swap 4 rshift xor xor ;

: crc16 ( b crc -- crc )  \ update CRC16 with given byte
  over crc16h swap 4 rshift crc16h ;
