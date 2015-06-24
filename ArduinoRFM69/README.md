# embapps ArduinoRFM69
Various OOK receive and transmit programs for the RFM69
Using JeeLib, and aiming at the Classic JeeNode and JeeLink V3C.

##Unmodified JeeLink V3C
The following sketches only require standard SPI wiring of the RFM69, and no additional soldering for DATA/DIO2 lines is required.
rfm69-RF12demo-ook-TX is the classic RF12demo sketch that can send KAKU and FS20 commands.
rfm69-ook-receive-rssi can receive various OOK signals by sampling the RSSI signal register.
rfm69-ook-relay-rssi receives OOK signals, and the decoded signal is transmitted on the standard JeeLib RF12-packet network.

##Solder-brdige closed JeeLink V3C
The next sketch is using the DIO2/DATA line, connected to D3=INT0, for example by closing the soldering bridge in the JeeLinkV3C
rfm69-ook-receive-dio2

Supported decoders can be found in the decoders433.h and decoders868.h. From more backgound see http://jeelabs.org/2011/02/03/ook-relay-revisited-2/

more examples using the DIO2 line can be expected.

