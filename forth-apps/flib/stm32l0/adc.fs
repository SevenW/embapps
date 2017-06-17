\ simple one-shot ADC

$40012400 constant ADC1
    ADC1 $000 + constant ADC-ISR
    ADC1 $004 + constant ADC-IER
    ADC1 $008 + constant ADC-CR
    ADC1 $00C + constant ADC-CFGR1
    ADC1 $010 + constant ADC-CFGR2
    ADC1 $014 + constant ADC-SMPR
    ADC1 $020 + constant ADC-TR
    ADC1 $028 + constant ADC-CHSELR
    ADC1 $040 + constant ADC-DR
    ADC1 $0B4 + constant ADC-CALFACT
    ADC1 $308 + constant ADC-CCR

: adc? ( -- )
  ADC1
  cr ."     ISR " dup @ hex. 4 +
     ."     IER " dup @ hex. 4 +
  cr ."      CR " dup @ hex. 4 +
  cr ."   CFGR1 " dup @ hex. 4 +
     ."   CFGR2 " dup @ hex. 4 +
  cr ."    SMPR " dup @ hex. $C +
     ."      TR " dup @ hex. 8 +
  cr ."  CHSELR " dup @ hex. $18 +
     ."      DR " dup @ hex. $74 +
  cr ." CALFACT " dup @ hex. $254 +
     ."     CCR " dup @ hex. drop ;

: adc-calib ( -- )  \ perform an ADC calibration cycle
  31 bit ADC-CR bis!  \ set ADCAL
  begin 31 bit ADC-CR bit@ 0= until  \ wait until ADCAL is clear
;

: adc-once ( -- u )  \ read ADC value once
  2 bit ADC-CR bis!  \ set ADSTART to start ADC
  begin 2 bit ADC-ISR bit@ until  \ wait until EOC set
  ADC-DR @ ;

: adc-init ( -- )  \ initialise ADC
\ FIXME can't call this twice, recalibration will hang!
  9 bit RCC-APB2ENR bis!  \ set ADCEN
  adc-calib  1 ADC-CR !   \ perform calibration, then set ADEN to enable ADC
  adc-once drop ;

: adc-deinit ( -- )  \ de-initialise ADC
  1 bit ADC-CR bis! 9 bit RCC-APB2ENR bic! ;

: adc ( pin -- u )  \ read ADC value 2x to avoid chip erratum
\ IMODE-ADC over io-mode!
  io# bit ADC-CHSELR !  adc-once drop adc-once ;

: adc-vcc ( -- mv )  \ measure current Vcc
  22 bit ADC-CCR bis!  ADC-SMPR @  %111 ADC-SMPR !
  $1FF80078 h@ 3000 * 17 adc /
  swap  22 bit ADC-CCR bic!  ADC-SMPR ! ;

: adc-temp ( -- degc )  \ measure chip temperature
  23 bit ADC-CCR bis!  ADC-SMPR @  %111 ADC-SMPR !
  18 adc  swap  23 bit ADC-CCR bic!  ADC-SMPR !
  adc-vcc 3000 */ $1FF8007A h@ - 100 $1FF8007E h@ $1FF8007A h@ - */ 30 + ;
