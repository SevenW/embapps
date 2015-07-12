/// @file
/// Decoders for 433 MHz OOK signals.
// Oregon Scientific V1 decoder added - Frank Benschop
// Oregon Scientific V2 decoder added - Dominique Pierre
// Oregon Scientific V3 decoder revisited - Dominique Pierre
// 2013-03-15 <info@sevenwatt.com> http://opensource.org/licenses/mit-license.php

/// OOK decoder for WS249 plant humidity sensor.
class WS249 : public DecodeOOK {
  public:
    WS249 () {}
    WS249 (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      uint8_t is_low = !last_signal;
      uint8_t is_sync = width >= 5400;
      if ((170 < width && width < 2600) || (5400 < width && width < 6100)) {
        uint8_t w = width >= 1600;
        switch (state) {
          case UNKNOWN:
            //flip++;
            //a 5600us low after a high is start signal
            if ((width >= 5400) /*&& (flip > 0)*/) {
              //Serial.print("WSy");
              state = OK;
              //break;
            }
            return 0;
          case OK:
            //receiving a ON around 700us.
            state = T0;
            if (is_sync)
              return -1;
            else
              return 0;
          case T0:
            if (is_sync) {
              //Serial.print("WSend");
              return -1;
            } else {
              //Serial.print(w);
              gotBit(w);
              if (pos >= 8) {
                return 1;
              }
              else
                return 0;
            }
            break;
        }
        //return 0;
      }
      if (width >= 5400 && pos >= 8) {
        return 1;
      }
      if (pos >= 16)
        return pos = bits = 1;
      return -1;
    }
};

/// OOK decoder for Philips outdoor temperature sensor.
class Philips : public DecodeOOK {
  public:
    Philips () {
    }
    Philips (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      if (1400 < width && width < 2600 || 5400 < width && width < 6900) {
        uint8_t w = width >= 3600;
        //one bit is a high+low. short-high,long-low = 0, decide on first part of pulse.
        switch (state) {
          case UNKNOWN:
            //premable detection: the first out of 3 repeats the preamble has an additional four bits.
            //8 or 12 0-bits is preamble, followed by a 1-bit.
            //Current detection is just on timeout of signal train.
            if (last_signal) {
              gotBit(w);
            } else {
              //Receiver was ON before first bit.
              gotBit(!w);
              state = T0;
            }
            break;
          case OK:
            //process second part of bit
            state = T0;
            break;
          case T0:
            //detect on first part of bit
            gotBit(w);
            break;
        }
        return 0;
      } else {
        if (pos > 3) {

          alignTail(4); //throws away 2 preamble nibbles from long message, one from short.
          reverseBits();
          //for (uint8_t i = 0; i < pos; ++i) {
          //    chprintf(serial, "%02X ", data[i]);
          //}
          //chprintf(serial, "\r\n");
          //for (uint8_t i = 0; i < pos; ++i) {
          //  uint8_t dat = data[i];
          //  for (uint8_t j = 0; j < 8; ++j) {
          //      chprintf(serial, "%d", dat >> 7);
          //    dat <<= 1;
          //  }
          //}
          //chprintf(serial, "\r\n");
          return 1;
        } else {
          return -1;
        }
      }
      if (pos >= 16)
        return pos = bits = 1;
      return -1;
    }
};

// 433 MHz decoders
class OregonDecoderV1 : public DecodeOOK {
  private:
    uint8_t syncs;
  public:
    OregonDecoderV1 () {}
    OregonDecoderV1 (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    // add one bit to the packet data buffer
    virtual void gotBit (int8_t value) {
      //Serial.print(value, BIN);
      data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
      total_bits++;
      pos = total_bits >> 3;
      if (pos >= sizeof data) {
        resetDecoder();
        //Serial.print("gotBit reset");
        return;
      }
      state = OK;
    }

    virtual int8_t decode (uint16_t width) {
      //the detection algorithm does not explicitely make use of knwoledge on on or off states.
      //the algorithm looks at transitions of the signal.
      //However a short period in the off-state is shorter than in the on state.
      //the same applies to on states.
      //TODO: For more robust detection, this algorithm should have separate ranges for short
      //and long pulse in on and off states.
      //if (width<940) {Serial.print(".");};
      if (940 <= width && width < 7400) {
        uint8_t w = (last_signal ? (width >= 2400) : (width>=1950));
        uint8_t s = width >= 3700;
        //Serial.print(" ");
        //Serial.print(width);
        //Serial.print(" ");
        switch (state) {
          case UNKNOWN:
            if (s != 0) {
              //to enter here, the flip>=22  (11 one bits after first one bit from preamble
              //however, at the start of transmission, the receiver may adapt its gain and
              //miss a couple of bits in pre-ample.
              //therefore, no test on flips.
              syncs++;
              //Serial.print("S");
              if (syncs >= 3) {
                ////trigger logic analyser
                //digitalWrite(11, 1);

                //Serial.print("\n");
                //Serial.print("sync ");
                syncs = 0;
                resetDecoder();
                if (width < 5940) {
                  //first bit will become 1, we are half-way.
                  flip = 1;
                  state = T0;
                  return 0;
                } else {
                  //first it is zero and fully detected.
                  manchester(0);
                  state = OK;
                  return 0;
                };
                //return -1;
              }
            } else if (w == 0) {
              syncs = 0;
              ++flip;
              //Serial.print("s");
            } else {
              syncs = 0;
              //Serial.print("l");
              return -1;
            }
            break;
          case OK:
            if (w == 0)
              state = T0;
            else
              manchester(1);
            break;
          case T0:
            if (w == 0)
              manchester(0);
            else {
              //Serial.print("end? expected short got long width: ");
              //Serial.print(width);
              //Serial.print(" ");
              return -1;
            }
            break;
        }
      } else {
        return -1;
      }
      return  total_bits == 32 ? 1 : 0;
    }
};

class OregonDecoderV2 : public DecodeOOK {
  public:
    OregonDecoderV2() {}
    OregonDecoderV2 (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    // add one bit to the packet data buffer
    virtual void gotBit (int8_t value) {
      if (!(total_bits & 0x01))
      {
        data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
      }
      total_bits++;
      pos = total_bits >> 4;
      if (pos >= sizeof data) {
        resetDecoder();
        return;
      }
      state = OK;
    }

    virtual int8_t decode (uint16_t width) {
      if (200 <= width && width < 1200) {
        uint8_t w = width >= 700;
        switch (state) {
          case UNKNOWN:
            if (w != 0) {
              // Long pulse
              ++flip;
            } else if (32 <= flip) {
              // Short pulse, start bit
              flip = 0;
              state = T0;
            } else {
              // Reset decoder
              return -1;
            }
            break;
          case OK:
            if (w == 0) {
              // Short pulse
              state = T0;
            } else {
              // Long pulse
              manchester(1);
            }
            break;
          case T0:
            if (w == 0) {
              // Second short pulse
              manchester(0);
            } else {
              // Reset decoder
              return -1;
            }
            break;
        }
      } else {
        return -1;
      }
      return total_bits == 160 ? 1 : 0;
    }
};

class OregonDecoderV3 : public DecodeOOK {
  public:
    OregonDecoderV3() {}
    OregonDecoderV3 (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    // add one bit to the packet data buffer
    virtual void gotBit (int8_t value) {
      data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
      total_bits++;
      pos = total_bits >> 3;
      if (pos >= sizeof data) {
        resetDecoder();
        return;
      }
      state = OK;
    }

    virtual int8_t decode (uint16_t width) {
      if (200 <= width && width < 1200) {
        uint8_t w = width >= 700;
        switch (state) {
          case UNKNOWN:
            if (w == 0)
              ++flip;
            else if (32 <= flip) {
              flip = 1;
              manchester(1);
            } else
              return -1;
            break;
          case OK:
            if (w == 0)
              state = T0;
            else
              manchester(1);
            break;
          case T0:
            if (w == 0)
              manchester(0);
            else
              return -1;
            break;
        }
      } else {
        return -1;
      }
      return  total_bits == 80 ? 1 : 0;
    }
};

/// OOK decoder for Oregon Scientific devices.
class OregonDecoder : public DecodeOOK {
  public:
    OregonDecoder () {}
    OregonDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      if (200 <= width && width < 1200) {
        uint8_t w = width >= 700;
        switch (state) {
          case UNKNOWN:
            if (w == 0)
              ++flip;
            else if (10 <= flip && flip <= 50) {
              flip = 1;
              manchester(1);
            } else
              return -1;
            break;
          case OK:
            if (w == 0)
              state = T0;
            else
              manchester(1);
            break;
          case T0:
            if (w == 0)
              manchester(0);
            else
              return -1;
            break;
        }
        return 0;
      }
      if (width >= 2500 && pos >= 9)
        return 1;
      return -1;
    }
};

/// OOK decoder for Cresta devices.
class CrestaDecoder : public DecodeOOK {
    // http://members.upc.nl/m.beukelaar/Crestaprotocol.pdf
  public:
    CrestaDecoder () {}
    CrestaDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      if (200 <= width && width < 1300) {
        uint8_t w = width >= 750;
        switch (state) {
          case UNKNOWN:
            if (w == 1)
              ++flip;
            else if (2 <= flip && flip <= 10)
              state = T0;
            else
              return -1;
            break;
          case OK:
            if (w == 0)
              state = T0;
            else
              gotBit(1);
            break;
          case T0:
            if (w == 0)
              gotBit(0);
            else
              return -1;
            break;
        }
        return 0;
      }
      if (width >= 2500 && pos >= 7)
        return 1;
      return -1;
    }
};

/// OOK decoder for Klik-Aan-Klik-Uit devices.
class KakuDecoder : public DecodeOOK {
  public:
    KakuDecoder () {}
    KakuDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      //if ((180 <= width && width < 450) || (950 <= width && width < 1250)) {
      if ((99 <= width && width < 650) || (800 <= width && width < 1450)) {
        uint8_t w = width >= 700;
        switch (state) {
          case UNKNOWN:
          case OK:
            if (w == 0)
              state = T0;
            else
              return -1;
            break;
          case T0:
            if (w)
              state = T1;
            else
              return -1;
            break;
          case T1:
            state += w + 1;
            break;
          case T2:
            if (w)
              gotBit(0);
            else
              return -1;
            break;
          case T3:
            if (w == 0)
              gotBit(1);
            else
              return -1;
            break;
        }
        return 0;
      }
      if (width >= 2500 && 8 * pos + bits == 12) {
        for (uint8_t i = 0; i < 4; ++i)
          gotBit(0);
        alignTail(2);
        return 1;
      }
      return -1;
    }
};

/// OOK decoder for Klik-Aan-Klik-Uit type A devices.
class KakuADecoder : public DecodeOOK {
    enum { Pu, P1, P5, P10 }; //pulsetypes: Pu = unknown, P1 = between 100 and 600uS, P5 = between 800 and 1800uS, P10 = between 2 and 3 mS
    uint8_t backBuffer[4]; //we need to keep a 4 bit history
    uint8_t pulse;

  public:
    KakuADecoder () {
      clearBackBuffer();
    }
    KakuADecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {
      clearBackBuffer();
    }

    void clearBackBuffer()
    {
      backBuffer[0] = Pu;
      backBuffer[1] = Pu;
      backBuffer[2] = Pu;
      backBuffer[3] = Pu;
    }

    virtual int8_t decode (uint16_t width) {
      if ((width >= 100) && (width <= 600))
        pulse = P1;
      else if ((width >= 800) && (width <= 1800))
        pulse = P5;
      else if ((width >= 2000) && (width <= 3000))
        pulse = P10;
      else
      {
        clearBackBuffer();//out-of-protocol pulsewidth, abort;
        return -1; //reset decoder
      }

      backBuffer[3] = backBuffer[2];
      backBuffer[2] = backBuffer[1];
      backBuffer[1] = backBuffer[0];
      backBuffer[0] = pulse;

      switch (state)
      {
        case UNKNOWN:
          if ( backBuffer[2] == P1 && backBuffer[1] == P10 && backBuffer[0] == P1 ) //received start/sync signal
          {
            state = T0;
            clearBackBuffer();
          }
          break;
        case OK: //returning after receiving a good bit
        case T0:
          if ( pulse == P10 ) //received start/sync signal
          {
            clearBackBuffer();
            return -1; //reset decoder
          }
          if ( backBuffer[3] != Pu ) //depending on the preceding pulsetypes we received a 1 or 0
          {
            if ( (backBuffer[3] == P5) && (backBuffer[2] == P1) && (backBuffer[1] == P1) && (backBuffer[0] == P1))
              gotBit(1);
            else if ( (backBuffer[3] == P1) && (backBuffer[2] == P1) && (backBuffer[1] == P5) && (backBuffer[0] == P1))
              gotBit(0);
            else
            {
              state = UNKNOWN;
              break;
            }
            clearBackBuffer();
            if ( pos >= 4 ) //we expect 4 uint8_ts
              return 1;
          }
          break;
        default:
          break;
      }
      return 0;
    }
};

/// OOK decoder for X11 over RF devices.
class XrfDecoder : public DecodeOOK {
  public:
    XrfDecoder () {}
    XrfDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    // see also http://davehouston.net/rf.htm
    virtual int8_t decode (uint16_t width) {
      if (width > 2000 && pos >= 4)
        return 1;
      if (width > 5000)
        return -1;
      if (width > 4000 && state == UNKNOWN) {
        state = OK;
        return 0;
      }
      if (350 <= width && width < 1800) {
        uint8_t w = width >= 720;
        switch (state) {
          case OK:
            if (w == 0)
              state = T0;
            else
              return -1;
            break;
          case T0:
            gotBit(w);
            break;
        }
        return 0;
      }
      return -1;
    }
};

/// OOK decoder for FS20 type HEZ devices.
class HezDecoder : public DecodeOOK {
  public:
    HezDecoder () {}
    HezDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    // see also http://homeeasyhacking.wikia.com/wiki/Home_Easy_Hacking_Wiki
    virtual int8_t decode (uint16_t width) {
      if (200 <= width && width < 1200) {
        gotBit(width >= 600);
        return 0;
      }
      if (width >= 5000 && pos >= 5) {
        for (uint8_t i = 0; i < 6; ++i)
          gotBit(0);
        alignTail(7); // keep last 56 bits
        return 1;
      }
      return -1;
    }
};

/// OOK decoder for Elro devices.
class ElroDecoder : public DecodeOOK {
  public:
    ElroDecoder () {}
    ElroDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      if (50 <= width && width < 600) {
        uint8_t w = (width - 40) / 190; // 40 <= 0 < 230 <= 1 < 420 <= 2 < 610
        switch (state) {
          case UNKNOWN:
          case OK:
            if (w == 0)
              state = T0;
            else if (w == 2)
              state = T2;
            break;
          case T0:
          case T2:
            if (w == 1)
              ++state;
            else
              return -1;
            break;
          case T1:
            if (w == 0) { // sync pattern has 0-1-0-1 patterns
              resetDecoder();
              break;
            }
            if (w != 2)
              return -1;
            gotBit(0);
            break;
          case T3:
            if (w != 0)
              return -1;
            gotBit(1);
            break;
        }
        return 0;
      }
      if (pos >= 11)
        return 1;
      return -1;
    }
};

// The following three decoders were contributed bij Gijs van Duimen:
//    FlamingoDecoder = Flamingo FA15RF
//    SmokeDecoder = Flamingo FA12RF
//    ByronbellDecoder = Byron SX30T
// see http://www.domoticaforum.eu/viewtopic.php?f=17&t=4960&start=90#p51118
// there's some weirdness in this code, I've edited it a bit -jcw, 2011-10-16

/// OOK decoder for Flamingo devices.
class FlamingoDecoder : public DecodeOOK {
  public:
    FlamingoDecoder () {}
    FlamingoDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      if ((width > 740 && width < 780) || (width > 2650 && width < 2750) ||
          (width > 810 && width < 950) || (width > 1040 && width < 1450)) {
        gotBit(width >= 950);
        return 0;
      }
      // if (pos >= 4 && data[0] == 84 && data[1] == 85 &&
      //                 data[2] == 85 && data[3] == 85)
      if (pos >= 4)
        return 1;
      return -1;
    }
};

/// OOK decoder for Flamingo smoke devices.
class SmokeDecoder : public DecodeOOK {
  public:
    SmokeDecoder () {}
    SmokeDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      if (width > 20000 && width < 21000 || width > 6900 && width < 7000 ||
          width > 6500 && width < 6800) {
        gotBit(1);
        // if (width > 3000 && width < 4000)
        //     uint8_t w = width < 100;
        return 0;
      }
      if (pos >= 4)
        return pos = bits = 1;
      return -1;
    }
};

/// OOK decoder for Byronbell devices.
class ByronbellDecoder : public DecodeOOK {
  public:
    ByronbellDecoder () {}
    ByronbellDecoder (uint8_t id, const char* tag, decoded_cb cb) : DecodeOOK (id, tag, cb) {}

    virtual int8_t decode (uint16_t width) {
      if (660 < width && width < 715 || 5100 < width && width < 5400) {
        gotBit(width > 1000);
        return 0;
      }
      if (pos >= 8)
        return pos = bits = 1;
      return -1;
    }
};
