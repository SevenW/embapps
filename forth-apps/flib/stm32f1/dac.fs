\ DAC output, software-driven, timer-driven, or DMA wavetable-driven

$40007400 constant DAC
     DAC $00 + constant DAC-CR
\    DAC $04 + constant DAC-SWTRIGR
     DAC $08 + constant DAC-DHR12R1
     DAC $14 + constant DAC-DHR12R2
     DAC $20 + constant DAC-DHR12RD

$40020400 constant DMA2
    DMA2 $30 + constant DMA2-CCR3
    DMA2 $34 + constant DMA2-CNDTR3
    DMA2 $38 + constant DMA2-CPAR3
    DMA2 $3C + constant DMA2-CMAR3

: 2dac! ( u1 u2 -- )  \ send values to each of the DACs
  16 lshift or DAC-DHR12RD ! ;

: dac-init ( -- )  \ initialise the two D/A converters on PA4 and PA5
  29 bit RCC-APB1ENR bis!  \ DACEN clock enable
  IMODE-ADC PA4 io-mode!
  IMODE-ADC PA5 io-mode!
  $00010001 DAC-CR !  \ enable channel 1 and 2
  0 0 2dac!
;

: dac-triangle ( -- )  \ software-driven dual triangle waveform until keypress
  dac-init
  begin
    $1000 0 do  i          $FFF i - 2dac! loop
    $1000 0 do  $FFF i -   i        2dac! loop
  key? until
;

: dac1-noise ( u -- )  \ generate noise on DAC1 (PA4) with given period
  6 timer-init dac-init
           0 bit     \ EN1
  %1011 8 lshift or  \ MAMP1 max
    %01 6 lshift or  \ WAVE1 = noise
                     \ TSEL1 = timer 6 TRGO
           2 bit or  \ TEN1
  DAC-CR ! ;

: dac1-triangle ( u -- )  \ generate triangle on DAC1 (PA4) with given period
  6 timer-init dac-init
           0 bit     \ EN1
  %1011 8 lshift or  \ MAMP1 max
    %10 6 lshift or  \ WAVE1 = noise
                     \ TSEL1 = timer 6 TRGO
           2 bit or  \ TEN1
  DAC-CR ! ;

: dac1-dma ( addr count -- )  \ feed DAC1 from wave table at given address
        1 bit RCC-AHBENR bis!  \ DMA2EN clock enable
          2/ DMA2-CNDTR3 !     \ 2-byte entries
              DMA2-CMAR3 !     \ read from address passed as input
  DAC-DHR12R1 DMA2-CPAR3 !     \ write to DAC1

                0   \ register settings for CCR3 of DMA2:
  %01 10 lshift or  \ MSIZE = 16-bits
   %01 8 lshift or  \ PSIZE = 16 bits
          7 bit or  \ MINC
          5 bit or  \ CIRC
          4 bit or  \ DIR = from mem to peripheral
          0 bit or  \ EN
      DMA2-CCR3 !

\ set up DAC1 to convert on each write from DMA1
  12 bit DAC-CR bis!  \ DMAEN1
;
