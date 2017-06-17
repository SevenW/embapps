\ tft driver for R61505U chip, connected as 16-bit parallel via FSMC

$A0000000 constant FSMC-BCR1
$A0000004 constant FSMC-BTR1

$60000000 constant LCD-REG
$60020000 constant LCD-RAM

: tft-pins ( -- )  \ enable FSMC and set up pins needed to drive the TFT LCD
  8 bit RCC-AHBENR bis!  \ enable FSMC clock
  OMODE-AF-PP OMODE-FAST + dup PD0 %1100111110110011 io-modes!
                               PE0 %1111111110000000 io-modes! ;

: tft-fsmc ( -- )  \ configure the FSMC, SRAM bank 1
  $80               \ keep reset value
\                   \ FSMC_DataAddressMux_Disable
\                   \ FSMC_MemoryType_SRAM
  %01 4 lshift or   \ FSMC_MemoryDataWidth_16b
\                   \ FSMC_BurstAccessMode_Disable
\                   \ FSMC_WaitSignalPolarity_Low
\                   \ FSMC_WrapMode_Disable
\                   \ FSMC_WaitSignalActive_BeforeWaitState
  1 12 lshift or    \ FSMC_WriteOperation_Enable
\                   \ FSMC_WaitSignal_Disable
\                   \ FSMC_AsynchronousWait_Disable
\                   \ FSMC_ExtendedMode_Disable
\                   \ FSMC_WriteBurst_Disable
  FSMC-BCR1 !

  0
  3 0 lshift or     \ FSMC_AddressSetupTime = 4x HCLK
\                   \ FSMC_AddressHoldTime = 0
  5 8 lshift or     \ FSMC_DataSetupTime = 6x HCLK
\                   \ FSMC_BusTurnAroundDuration = 0x00
\                   \ FSMC_CLKDivision = 0x00
\                   \ FSMC_DataLatency = 0x00
\                   \ FSMC_AccessMode_A
  FSMC-BTR1 !

  1 FSMC-BCR1 bis!  \ MBKEN:Memorybankenablebit
;

create tft:R61505U
hex
    E5 h, 8000 h,  00 h, 0001 h,  2B h, 0010 h,  01 h, 0100 h,  02 h, 0700 h,
    03 h, 1018 h,  04 h, 0000 h,  08 h, 0202 h,  09 h, 0000 h,  0A h, 0000 h,
    0C h, 0000 h,  0D h, 0000 h,  0F h, 0000 h,  50 h, 0000 h,  51 h, 00EF h,
    52 h, 0000 h,  53 h, 013F h,  60 h, 2700 h,  61 h, 0001 h,  6A h, 0000 h,
    80 h, 0000 h,  81 h, 0000 h,  82 h, 0000 h,  83 h, 0000 h,  84 h, 0000 h,
    85 h, 0000 h,  90 h, 0010 h,  92 h, 0000 h,  93 h, 0003 h,  95 h, 0110 h,
    97 h, 0000 h,  98 h, 0000 h,  10 h, 0000 h,  11 h, 0000 h,  12 h, 0000 h,
    13 h, 0000 h, 100 h, #100 h,  10 h, 17B0 h,  11 h, 0004 h, 100 h,  #50 h,
    12 h, 013E h, 100 h,  #50 h,  13 h, 1F00 h,  29 h, 000F h, 100 h,  #50 h,
    20 h, 0000 h,  21 h, 0000 h,  30 h, 0204 h,  31 h, 0001 h,  32 h, 0000 h,
    35 h, 0206 h,  36 h, 0600 h,  37 h, 0500 h,  38 h, 0505 h,  39 h, 0407 h,
    3C h, 0500 h,  3D h, 0503 h,  07 h, 0173 h, 100 h,  #50 h, 200 h,
decimal align

: tft@ ( reg -- val )  LCD-REG h! LCD-RAM h@ ;
: tft! ( val reg -- )  LCD-REG h! LCD-RAM h! ;

$0000 variable tft-bg
$FFFF variable tft-fg

: tft-init ( -- )
  tft-pins tft-fsmc
  tft:R61505U begin
    dup h@ dup $200 < while  ( addr reg )
    over 2+ h@ swap  ( addr val reg )
    dup $100 = if drop ms else tft! then
  4 + repeat 2drop ;

\ clear, putpixel, and display are used by the graphics.fs code

: clear ( -- )
  0 $20 tft!  0 $21 tft!  $22 LCD-REG h!
  tft-bg @  320 240 * 0 do dup LCD-RAM h! loop  drop ;

: putpixel ( x y -- )  \ set a pixel in display memory
  $21 tft! $20 tft! tft-fg @ $22 tft! ;

: display ( -- ) ;  \ update tft from display memory (ignored)
