\ simple one-shot ADC

$40012400 constant ADC1
    ADC1 $00 + constant ADC1-SR
    ADC1 $04 + constant ADC1-CR1
    ADC1 $08 + constant ADC1-CR2
    ADC1 $0C + constant ADC1-SMPR1
    ADC1 $10 + constant ADC1-SMPR2
    ADC1 $2C + constant ADC1-SQR1
    ADC1 $30 + constant ADC1-SQR2
    ADC1 $34 + constant ADC1-SQR3
    ADC1 $4C + constant ADC1-DR

$40020000 constant DMA1
    DMA1 $00 + constant DMA1-ISR
    DMA1 $04 + constant DMA1-IFCR
    DMA1 $08 + constant DMA1-CCR1
    DMA1 $0C + constant DMA1-CNDTR1
    DMA1 $10 + constant DMA1-CPAR1
    DMA1 $14 + constant DMA1-CMAR1

: adc-calib ( -- )  \ perform an ADC calibration cycle
  2 bit ADC1-CR2 bis!  begin 2 bit ADC1-CR2 bit@ 0= until ;

: adc-once ( -- u )  \ read ADC value once
  0 bit ADC1-CR2 bis!  \ set ADON to start ADC
  begin 1 bit ADC1-SR bit@ until  \ wait until EOC set
  ADC1-DR @ ;

: adc-init ( -- )  \ initialise ADC
  9 bit RCC-APB2ENR bis!  \ set ADC1EN
  23 bit  \ set TSVREFE for vRefInt use
   0 bit or ADC1-CR2 bis!  \ set ADON to enable ADC
  \ 7.5 cycles sampling time is enough for 18 kΩ to ground, measures as zero
  \ even 239.5 cycles is not enough for 470 kΩ, it still leaves 70 mV residual
  %111 21 lshift ADC1-SMPR1 bis! \ set SMP17 to 239.5 cycles for vRefInt
  %110110110 ADC1-SMPR2 bis! \ set SMP0/1/2 to 71.5 cycles, i.e. 83 µs/conv
  adc-once drop ;

: adc# ( pin -- n )  \ convert pin number to adc index
\ nasty way to map the pins (a "c," table offset lookup might be simpler)
\   PA0..7 => 0..7, PB0..1 => 8..9, PC0..5 => 10..15
  dup io# swap  io-port ?dup if shl + 6 + then ;

: adc ( pin -- u )  \ read ADC value
\ IMODE-ADC over io-mode!
\ nasty way to map the pins (a "c," table offset lookup might be simpler)
\   PA0..7 => 0..7, PB0..1 => 8..9, PC0..5 => 10..15
  adc# ADC1-SQR3 !  adc-once ;

: adc1-dma ( addr count pin rate -- )  \ continuous DMA-based conversion
  3 timer-init        \ set the ADC trigger rate using timer 3
  adc-init  adc drop  \ perform one conversion to set up the ADC
  2dup 0 fill         \ clear sampling buffer

    0 bit RCC-AHBENR bis!  \ DMA1EN clock enable
      2/ DMA1-CNDTR1 !     \ 2-byte entries
          DMA1-CMAR1 !     \ write to address passed as input
  ADC1-DR DMA1-CPAR1 !     \ read from ADC1

                0   \ register settings for CCR1 of DMA1:
  %01 10 lshift or  \ MSIZE = 16-bits
   %01 8 lshift or  \ PSIZE = 16 bits
          7 bit or  \ MINC
          5 bit or  \ CIRC
                    \ DIR = from peripheral to mem
          0 bit or  \ EN
      DMA1-CCR1 !

                 0   \ ADC1 triggers on timer 3 and feeds DMA1:
          23 bit or  \ TSVREFE
          20 bit or  \ EXTTRIG
  %100 17 lshift or  \ timer 3 TRGO event
           8 bit or  \ DMA
           0 bit or  \ ADON
        ADC1-CR2 ! ;

: adc-vcc ( -- mv )  \ return estimated Vcc, based on 1.2V internal bandgap
  3300 1200  17 ADC1-SQR3 !  adc-once  */ ;
