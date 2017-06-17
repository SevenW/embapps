\ LCD driver for PCD8544 (available from Adafruit)

\ Pin definitions.

PB2  constant LCD_RESET
PB10 constant LCD_CD

\ LCD constants.

84   constant LCD_WIDTH
48   constant LCD_HEIGHT
6    constant LCD_ROW_HEIGHT

$01  constant EXTENDEDINSTRUCTION
$4   constant DISPLAYNORMAL
$5   constant DISPLAYINVERTED
$20  constant FUNCTIONSET
$08  constant DISPLAYCONTROL
$40  constant SETYADDR
$80  constant SETXADDR
$10  constant SETBIAS
$80  constant SETVOP

\ Framebuffer.

LCD_WIDTH LCD_ROW_HEIGHT * buffer: LCD_BUFFER

\ Hardware words.

: lcd-pins ( -- ) OMODE-PP dup LCD_CD io-mode! dup LCD_RESET io-mode! drop ;
: lcd-reset ( -- ) LCD_RESET dup ioc! 500 ms ios! 500 ms ;
: lcd-command ( command -- ) LCD_CD ioc! +spi >spi -spi ;
: lcd-data ( data -- ) LCD_CD ios! +spi >spi -spi ;
: lcd-invert ( -- ) DISPLAYCONTROL DISPLAYINVERTED or lcd-command  ;
: lcd-normal ( -- ) DISPLAYCONTROL DISPLAYNORMAL or lcd-command  ;

\ This init is cribbed from the Arduino library
\ https://github.com/adafruit/Adafruit-PCD8544-Nokia-5110-LCD-library/blob/master/Adafruit_PCD8544.cpp

: lcd-init ( -- ) 
    spi-init
    lcd-pins
    lcd-reset
    FUNCTIONSET EXTENDEDINSTRUCTION or lcd-command
    SETBIAS $4 or lcd-command
    SETVOP 40 or lcd-command
    FUNCTIONSET lcd-command
    DISPLAYCONTROL DISPLAYNORMAL or lcd-command ;
: lcd-put-byte ( r x y -- ) 
    SETYADDR or lcd-command 
    SETXADDR or lcd-command 
    lcd-data 
    0 SETYADDR or lcd-command ;

\ Framebuffer words.

: put-byte ( r x y -- ) LCD_WIDTH * LCD_BUFFER + + c! ;
: get-byte ( x y -- r ) LCD_WIDTH * LCD_BUFFER + + c@ ;

: putpixel ( x y -- )
    8 /mod swap \ Get the row as a byte offset
    1 swap lshift \ Shift 1 left (remainder) times
    -rot
    2dup get-byte
    3 pick or
    -rot
    put-byte
    drop
;

: clear-byte ( x y -- ) 0 -rot put-byte ;
: clear-row ( y -- ) LCD_WIDTH 0 do dup i swap clear-byte loop ;
: clear ( -- ) LCD_ROW_HEIGHT 0 do i clear-row loop ;

: display ( -- ) 
    LCD_WIDTH 0 do
        LCD_ROW_HEIGHT 0 do
            j i get-byte 
            j i lcd-put-byte
        loop
    loop
;

\ vim: set ft=forth :
