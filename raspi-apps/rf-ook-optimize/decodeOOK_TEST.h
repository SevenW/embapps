/// @file
/// Generalized decoder framework for 868 MHz and 433 MHz OOK signals.


static uint16_t long1width = 500;
static uint16_t long0width = 500;

/// This is the general base class for implementing OOK decoders.
class DecodeOOK {
  protected:
    uint8_t total_bits, bits, flip, state, pos, data[25];
    // the following fields are used to deal with duplicate packets
    uint16_t lastCrc, lastTime;
    uint8_t repeats, minGap, minCount;
    uint8_t last_signal;
    uint16_t last_width;

    //for logging pulse lengths
#define max_pulse_cnt 150
    uint16_t pulseON[max_pulse_cnt];
    uint16_t pulseOFF[max_pulse_cnt];
    uint8_t pulse_cnt;



    // gets called once per incoming pulse with the width in us
    // return values: 0 = keep going, 1 = done, -1 = no match
    virtual int8_t decode (uint16_t width) {};

    // add one bit to the packet data buffer
    virtual void gotBit (int8_t value) {
      total_bits++;
      uint8_t *ptr = data + pos;
      *ptr = (*ptr >> 1) | (value << 7);

      if (++bits >= 8) {
        bits = 0;
        if (++pos >= sizeof data) {
          resetDecoder();
          return;
        }
      }
      state = OK;
    }

    // store a bit using Manchester encoding
    void manchester (int8_t value) {
      flip ^= value; // manchester code, long pulse flips the bit
      gotBit(flip);
    }

    // move bits to the front so that all the bits are aligned to the end
    void alignTail (uint8_t max = 0) {
      // align bits
      if (bits != 0) {
        data[pos] >>= 8 - bits;
        for (uint8_t i = 0; i < pos; ++i)
          data[i] = (data[i] >> bits) | (data[i + 1] << (8 - bits));
        bits = 0;
      }
      // optionally shift uint8_ts down if there are too many of 'em
      if (max > 0 && pos > max) {
        uint8_t n = pos - max;
        pos = max;
        for (uint8_t i = 0; i < pos; ++i)
          data[i] = data[i + n];
      }
    }

    void reverseBits () {
      for (uint8_t i = 0; i < pos; ++i) {
        uint8_t b = data[i];
        for (uint8_t j = 0; j < 8; ++j) {
          data[i] = (data[i] << 1) | (b & 1);
          b >>= 1;
        }
      }
    }

    void reverseNibbles () {
      for (uint8_t i = 0; i < pos; ++i)
        data[i] = (data[i] << 4) | (data[i] >> 4);
    }

    //  bool checkRepeats () {
    //    // calculate the checksum over the current packet
    //    uint16_t crc = ~0;
    //    for (uint8_t i = 0; i < pos; ++i)
    //    crc = _crc16_update(crc, data[i]);
    //    // how long was it since the last decoded packet
    //    uint16_t now = millis() / 100; // tenths of seconds
    //    uint16_t since = now - lastTime;
    //    // if different crc or too long ago, this cannot be a repeated packet
    //    if (crc != lastCrc || since > minGap)
    //    repeats = 0;
    //    // save last values and decide whether to report this as a new packet
    //    lastCrc = crc;
    //    lastTime = now;
    //    return repeats++ == minCount;
    //  }

    bool checkRepeats () {
      return 1;
    }

    void print_stats () {
      //print some statistics
      //array L1, L0, S1, S0
      char*names[] = {"L1", "L0", "S1", "S0"};
      uint16_t val[4] = {0, 0, 0, 0};
      uint32_t avg[4] = {0, 0, 0, 0};
      uint16_t min[4] = {65000, 65000, 65000, 65000};
      uint16_t max[4] = {0, 0, 0, 0};
      uint8_t cnt[4] = {0, 0, 0, 0};
      for (int i = 0; i < 4; i++) {
        min[i] = 65000;
        max[i] = 0;
        avg[i] = 0;
        cnt[i] = 0;
      }
      //skip first and last
      for (int i = 1; i < pulse_cnt - 1; i++) {
        val[0] = val[2] = pulseON[i];
        val[1] = val[3] = pulseOFF[i];

        for (uint8_t j = 0; j < 2; j++) {
          if (val[j] > long1width) {
            avg[j] += val[j];
            cnt[j]++;
            if (min[j] > val[j]) min[j] = val[j];
            if (max[j] < val[j]) max[j] = val[j];
          }
        }
        for (uint8_t j = 2; j < 4; j++) {
          if (val[j] <= long0width) {
            avg[j] += val[j];
            cnt[j]++;
            if (min[j] > val[j]) min[j] = val[j];
            if (max[j] < val[j]) max[j] = val[j];
          }
        }
      }
      //chprintf(serial, "\r\n");
      printf("\r\n");
      for (uint8_t j=0; j<4; j++) {
          if (cnt[j] == 0)
              avg[j]=min[j]=max[j]=0;
          else
              avg[j] /= cnt[j];
          //chprintf(serial, "%s: %4d-%4d-%4d #%2d\r\n", names[j], min[j], avg[j], max[j], cnt[j]);
          printf("%s: %4d-%4d-%4d #%2d\r\n", names[j], min[j], avg[j], max[j], cnt[j]);
      }
      return;
    }

  private:
    char es;

  public:
    enum { UNKNOWN, T0, T1, T2, T3, OK, DONE };
    typedef void (*decoded_cb)(DecodeOOK*);

    const char* tag;
    uint8_t id;
    decoded_cb decoded;

    DecodeOOK (uint8_t gap = 5, uint8_t count = 0)
      : es(0), id(0), tag(&es), decoded(NULL), lastCrc (0), lastTime (0), repeats (0), minGap (gap), minCount (count)
    {
      resetDecoder();
    }

    DecodeOOK (uint8_t nid, const char* ntag, decoded_cb cb, uint8_t gap = 5, uint8_t count = 0)
      : es(0), lastCrc (0), lastTime (0), repeats (0), minGap (gap), minCount (count), id (nid), tag (ntag), decoded (cb)
    {
      resetDecoder();
    }

    virtual bool nextPulse (uint16_t width) {
      if (state != DONE)
        switch (decode(width)) {
          case -1: // decoding failed
            resetDecoder();
            break;
          case 1: // decoding finished
            while (bits)
              gotBit(0); // padding
            state = checkRepeats() ? DONE : UNKNOWN;

            //print stats
            //print_stats();

            //dump pulse buffers
            for (int i=0; i<pulse_cnt; i++) {
                //chprintf(serial, "%d,", pulseON[i]);
               printf("%d,", pulseON[i]);
            }
            //chprintf(serial, "\r\n");
            printf("\r\n");
            for (int i=0; i<pulse_cnt; i++) {
                //chprintf(serial, "%d,", pulseOFF[i]);
               printf("%d,", pulseOFF[i]);
            }
            //chprintf(serial, "\r\n");
            printf("\r\n");
            break;
        }
      return state == DONE;
    }

    virtual bool nextPulse (uint16_t width, uint8_t signal) {
      last_signal = signal;
      if (pulse_cnt < max_pulse_cnt) {
        if (signal) {
          pulseON[pulse_cnt] = width;
        } else {
          pulseOFF[pulse_cnt++] = width;
        }
        //        if (pulse_cnt>=176 || (width == 1 and pulse_cnt > 10)) {
        //            //print_stats();
        //                            //dump pulse buffers
        //                            for (int i=0; i<pulse_cnt; i++) {
        //                                printf("%d,", pulseON[i]);
        //                            }
        //                            printf("\r\n");
        //                            for (int i=0; i<pulse_cnt; i++) {
        //                                printf("%d,", pulseOFF[i]);
        //                            }
        //                            printf("\r\n");
        //            pulse_cnt=0;
        //        }
        //        if (width == 1) pulse_cnt=0;
      }
      return DecodeOOK::nextPulse(width);
    }

    const uint8_t* getData (uint8_t& count) const {
      count = pos;
      return data;
    }

    virtual void resetDecoder ()
    {
      total_bits = bits = pos = flip = 0;
      state = UNKNOWN;

      pulse_cnt = 0;
    }
};

