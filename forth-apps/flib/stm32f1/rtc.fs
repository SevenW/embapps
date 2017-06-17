\ Real time clock, based on an attached 32 KHz crystal
\ no calendar functions, just a 32-bit counter incrementing once a second

     RCC $20 + constant RCC-BDCR

$40007000 constant PWR
      PWR $0 + constant PWR-CR

$40002800 constant RTC
     RTC $04 + constant RTC-CRL
     RTC $0C + constant RTC-PRLL
     RTC $18 + constant RTC-CNTH
     RTC $1C + constant RTC-CNTL

: rtc-wait ( -- ) begin 1 bit RCC-BDCR bit@ until ;

: rtc-init ( -- )  \ restart internal RTC using attached 32,768 Hz crystal
  %11 27 lshift RCC-APB1ENR bis!   \ enable PWREN and BKPEN
               8 bit PWR-CR bis!   \ disable backup domain write protection
\           16 bit RCC-BDCR bis!   \ reset backup domain
            16 bit RCC-BDCR bic!   \ releases backup domain
                 1 RCC-BDCR bis!   \ LESON
                   rtc-wait
      %01 8 lshift RCC-BDCR bis!   \ RTCSEL = LSE
            15 bit RCC-BDCR bis!   \ RTCEN
  begin 3 bit RTC-CRL hbit@ until  \ wait RSF
                   rtc-wait
              4 bit RTC-CRL bis!   \ set CNF
             32767 RTC-PRLL h!     \ set PRLL for 32 KHz crystal
              4 bit RTC-CRL bic!   \ clear CNF
                   rtc-wait ;

: now! ( u -- )  \ set current time
            rtc-wait
       4 bit RTC-CRL bis!
        dup RTC-CNTL h!
  16 rshift RTC-CNTH h!
       4 bit RTC-CRL hbic! ;

: now ( -- u )  \ return current time in seconds
\ use a spinloop to read consistent CNTL + CNTH values
  0 0  begin  2drop  RTC-CNTL h@ RTC-CNTH h@  over RTC-CNTL h@ = until
  16 lshift or ;
