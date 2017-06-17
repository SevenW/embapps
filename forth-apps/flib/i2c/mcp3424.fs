\ read out the MCP3424 sensor
\ needs i2c

[ifndef] MCP.ADDR  $68 constant MCP.ADDR  [then]

: mcp-init ( -- nak )
  i2c-init  MCP.ADDR i2c-addr  $1100 >i2c  0 i2c-xfer ;

: mcp-data ( chan -- n )  \ measure as signed 17-bit, wait in stop mode
  5 lshift %10001100 or  \ 18-bit readings, 3.75 SPS, PGA x1
  MCP.ADDR i2c-addr  >i2c  0 i2c-xfer drop
  begin
    stop100ms
    MCP.ADDR i2c-addr  4 i2c-xfer drop  4 0 do i2c> loop
  7 bit and while  \ loop while NRDY
    drop drop drop  \ ignore returned value
  repeat
  swap 8 lshift or swap 16 lshift or  14 lshift 14 arshift ;


\ mcp-init .
\ 0 mcp-data .
