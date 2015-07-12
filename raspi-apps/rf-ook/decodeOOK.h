/// @file
/// Generalized decoder framework for 868 MHz and 433 MHz OOK signals.


/// This is the general base class for implementing OOK decoders.
class DecodeOOK {
  protected:
    uint8_t total_bits, bits, flip, state, pos, data[25];
    // the following fields are used to deal with duplicate packets
    uint16_t lastCrc, lastTime;
    uint8_t repeats, minGap, minCount;
    uint8_t last_signal;
    uint16_t last_width;


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

    uint16_t crc16_update(uint16_t crc, uint8_t a)
    {
      int i;

      crc ^= a;
      for (i = 0; i < 8; ++i)
      {
        if (crc & 1)
          crc = (crc >> 1) ^ 0xA001;
        else
          crc = (crc >> 1);
      }

      return crc;
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
            break;
        }
      return state == DONE;
    }

    virtual bool nextPulse (uint16_t width, uint8_t signal) {
      last_signal = signal;
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
    }
};

typedef void (*decoded_cb)(DecodeOOK*);


