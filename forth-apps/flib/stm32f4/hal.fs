\ base definitions for STM32F407
\ adapted from mecrisp-stellaris 2.2.1a (GPL3)
\ needs io.fs

: chipid ( -- u1 u2 u3 3 )  \ unique chip ID as N values on the stack
  $1FFF7A10 @ $1FFF7A14 @ $1FFF7A18 @ 3 ;
: hwid ( -- u )  \ a "fairly unique" hardware ID as single 32-bit int
  chipid 1 do xor loop ;
: flash-kb ( -- u )  \ return size of flash memory in KB
  $1FFF7A22 h@ ;

: io.all ( -- )  \ display all the readable GPIO registers
  io-ports 0 do i 0 io io. loop ;

$40010000 constant AFIO
     AFIO $4 + constant AFIO-MAPR

$40004800 constant USART3
   USART3 $8 + constant USART3-BRR

$40023800 constant RCC
     RCC $00 + constant RCC-CR
     RCC $04 + constant RCC-PLLCRGR
     RCC $08 + constant RCC-CFGR
\ ?  RCC $10 + constant RCC-APB1RSTR
\ ?  RCC $18 + constant RCC-APB2ENR
\ ?  RCC $1C + constant RCC-APB1ENR

$40023C00 constant FLASH
    FLASH $0 + constant FLASH-ACR

\ adjusted for STM32F407 @ 168 MHz (original STM32F407 by Igor de om1zz, 2015)

25000000 variable clock-hz  \ 25 MHz crystal, HSI is 16 MHz

: 168MHz ( -- )  \ set the main clock to 168 MHz, keep baud rate at 115200
  $103 Flash-ACR !   \ 3 Flash Waitstates for 120 MHz with more than 2.7 V Vcc
                     \ Prefetch buffer enabled.
  22 bit \ PLLSRC    \ HSE clock as 25 MHz source
  25 0 lshift or  \ PLLM Division factor for main PLL and audio PLL input clock 
                  \ 25 MHz / 25 =  1 MHz. Divider before VCO. Frequency
                  \ entering VCO to be between 1 and 2 MHz.
 336 6 lshift or  \ PLLN Main PLL multiplication factor for VCO
                  \ between 192 and 432 MHz - 1 MHz * 336 = 336 MHz
  7 24 lshift or  \ PLLQ = 7, 336 MHz / 8 = 48 MHz
  0 16 lshift or  \ PLLP Division factor for main system clock
                  \ 0: /2  1: /4  2: /6  3: /8
                  \ 336 MHz / 2 = 168 MHz 
  RCC-PLLCRGR !

  24 bit RCC-CR bis!  \ PLLON - Wait for PLL to lock:
  begin 25 bit RCC-CR bit@ until  \ PLLRDY

  2                 \ Set PLL as clock source
  %101 10 lshift or \ APB  Low speed prescaler (APB1)
                    \ Max 42 MHz ! Here 168/4 MHz = 42 MHz.
  %100 13 lshift or \ APB High speed prescaler (APB2)
                    \ Max 84 MHz ! Here 168/2 MHz = 84 MHz.
  RCC-CFGR !
  168000000 clock-hz !
  $16d USART3-BRR ! \ Set Baud rate divider for 115200 Baud at 42 MHz. 22.786
;

: systick ( ticks -- )  \ enable systick interrupt
  1- $E000E014 !  \ How many ticks between interrupts ?
  7 $E000E010 !   \ Enable the systick interrupt.
;

0 variable ticks

: ++ticks ( -- ) 1 ticks +! ;  \ for use as systick irq handler

: systick-hz ( u -- )  \ enable systick counter at given frequency
  ['] ++ticks irq-systick !
  clock-hz @ swap / systick ;
: systick-hz? ( -- u ) \ derive current systick frequency from clock
  clock-hz @  $E000E014 @ 1+  / ;

: list ( -- )  \ list all words in dictionary, short form
  cr dictionarystart begin
    dup 6 + ctype space
  dictionarynext until drop ;

\ : cornerstone ( "name" -- )  \ define a flash memory cornerstone
\   <builds begin here $3FF and while 0 h, repeat
\   does>   begin dup  $3FF and while 2+   repeat  cr eraseflashfrom ;
