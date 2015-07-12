/// @file
/// Decoders for 868 MHz OOK signals.
// Alecto / Fine Offset WH1080 V2, V2a decoder added - Frank Benschop
// 2013-03-15 <info@sevenwatt.com> http://opensource.org/licenses/mit-license.php

// 868 MHz decoders

//Alecto WS3000, WS4000, Fine Offset WH1080, ....
//Only uses pulse widths, no signal level information
//Detection on preample of 8 bits one.
//Message end on crc8
//Capable of decoding both 9 and 10 uint8_t messages (WS3000, WS4000)
class WH1080DecoderV2 : public DecodeOOK {
protected:
    uint8_t msglen;
public:
    WH1080DecoderV2 (uint8_t msg_len = 10, uint8_t gap =5, uint8_t count =0) : DecodeOOK(gap, count), msglen(msg_len) {}

    //Dallas One-Wire CRC-8 - incremental calculation.
    uint8_t crc8_update( uint8_t crc, uint8_t inuint8_t)
    {
        uint8_t i;
        for (i = 8; i; i--) {
            uint8_t mix = (crc ^ inuint8_t) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inuint8_t >>= 1;
        }
        return crc;
    }

    //Dallas One-Wire CRC-8.
    uint8_t crc8( const uint8_t *addr, uint8_t len)
    {
        uint8_t crc = 0;
        while (len--) {
            uint8_t inuint8_t = *addr++;
            crc = crc8_update(crc, inuint8_t);
        }
        return crc;
    }


    // see also http://lucsmall.com/2012/04/29/weather-station-hacking-part-2/
    // 200 < bit-1 < 800 < low < 1200 < bit-0 < 1700
    virtual int8_t decode (uint16_t width) {
        if (140 <= width && width < 2500) {
            uint8_t w = width >= 1000;
            //option 1: only looking at durations
            uint8_t is_low = (780 <= width && width < 1200);
            switch (state) {
            case UNKNOWN:
                if (!is_low) {
                    if (!w) {
                        // Short pulse = 1
                        ++flip;
                        if (flip >= 15) {
                            flip = 0;
                            state = T0;
                        }
                    } else {
                        // Long pulse. Reset decoder
                        return -1;
                    }
                }  else {
                    ++flip;
                }
                break;
            case OK:
                if (!is_low) {
                    gotBit(!w);
                    state = T0;
                } else {
                    //expecting high signal, got low
                    return -1;
                }
                break;
            case T0:
                if (is_low) {
                    state = OK;
                } else {
                    //expecting low signal, got high
                    return -1;
                }
                break;
            }
        } else {
            //signal (low or high) out of spec
            return -1;
        }
        if (total_bits >= msglen * 8) {
            if (crc8(data, msglen - 1) == data[msglen - 1]) {
                reverseBits();
                return 1;
            } else {
                //failed crc at maximum message length
                return -1;
            }
        }
        return 0;
    }
};

//Alecto WS3000, WS4000, Fine Offset WH1080, ....
//Uses signal level information. More tolerant to jitter.
//Detection on preample of 8 bits one.
//Message end on crc8
//Capable of decoding both 9 and 10 uint8_t messages (WS3000, WS4000)
class WH1080DecoderV2a : public WH1080DecoderV2 {
public:
    WH1080DecoderV2a(uint8_t msg_len = 10, uint8_t gap =5, uint8_t count =0) : WH1080DecoderV2(msg_len, gap, count) {}


    // see also http://lucsmall.com/2012/04/29/weather-station-hacking-part-2/
    // 200 < bit-1 < 800 < low < 1200 < bit-0 < 1700
    virtual int8_t decode (uint16_t width) {
        if (140 <= width && width < 2500) {
            uint8_t w = width >= 1000;
            //option 2: having knowledge on the signal value, we can allow for more jitter on timing
            uint8_t is_low = !last_signal;
            switch (state) {
            case UNKNOWN:
                if (!is_low) {
                    if (!w) {
                        // Short pulse = 1
                        ++flip;
                        if (flip >= 8) {
                            flip = 0;
                            state = T0;
                        }
                    } else {
                        // Long pulse. Reset decoder
                        return -1;
                    }
                }
                break;
            case OK:
                if (!is_low) {
                    gotBit(!w);
                    state = T0;
                } else {
                    //expecting high signal, got low
                    return -1;
                }
                break;
            case T0:
                if (is_low) {
                    state = OK;
                } else {
                    //expecting low signal, got high
                    return -1;
                }
                break;
            }
        } else {
            //signal (low or high) out of spec
            return -1;
        }
        if (total_bits >= msglen * 8) {
            if (crc8(data, msglen - 1) == data[msglen - 1]) {
                reverseBits();
                return 1;
            } else {
                //failed crc at maximum message length
                return -1;
            }
        }
        return 0;
    }
};

/// OOK decoder for Visonic devices.
class VisonicDecoder : public DecodeOOK {
public:
    VisonicDecoder () {}

    virtual int8_t decode (uint16_t width) {
        if (200 <= width && width < 1000) {
            uint8_t w = width >= 600;
            switch (state) {
            case UNKNOWN:
            case OK:
                state = w == 0 ? T0 : T1;
                break;
            case T0:
                gotBit(!w);
                if (w)
                    return 0;
                break;
            case T1:
                gotBit(!w);
                if (!w)
                    return 0;
                break;
            }
            // sync error, flip all the preceding bits to resync
            for (uint8_t i = 0; i <= pos; ++i)
                data[i] ^= 0xFF;
        } else if (width >= 2500 && 8 * pos + bits >= 36 && state == OK) {
            for (uint8_t i = 0; i < 4; ++i)
                gotBit(0);
            alignTail(5); // keep last 40 bits
            // only report valid packets
            uint8_t b = data[0] ^ data[1] ^ data[2] ^ data[3] ^ data[4];
            if ((b & 0xF) == (b >> 4))
                return 1;
        } else
            return -1;
        return 0;
    }
};

/// OOK decoder for FS20 type EM devices.
class EMxDecoder : public DecodeOOK {
public:
    EMxDecoder () : DecodeOOK (30) {} // ignore packets repeated within 3 sec

    // see also http://fhz4linux.info/tiki-index.php?page=EM+Protocol
    virtual int8_t decode (uint16_t width) {
        if (200 <= width && width < 1000) {
            uint8_t w = width >= 600;
            switch (state) {
            case UNKNOWN:
                if (w == 0)
                    ++flip;
                else if (flip > 20)
                    state = OK;
                else
                    return -1;
                break;
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
        } else if (width >= 1500 && pos >= 9)
            return 1;
        else
            return -1;
        return 0;
    }
};

/// OOK decoder for FS20 type KS devices.
class KSxDecoder : public DecodeOOK {
public:
    KSxDecoder () {}

    // see also http://www.dc3yc.homepage.t-online.de/protocol.htm
    virtual int8_t decode (uint16_t width) {
        if (200 <= width && width < 1000) {
            uint8_t w = width >= 600;
            switch (state) {
            case UNKNOWN:
                gotBit(w);
                bits = pos = 0;
                if (data[0] != 0x95)
                    state = UNKNOWN;
                break;
            case OK:
                state = w == 0 ? T0 : T1;
                break;
            case T0:
                gotBit(1);
                if (!w)
                    return -1;
                break;
            case T1:
                gotBit(0);
                if (w)
                    return -1;
                break;
            }
        } else if (width >= 1500 && pos >= 6)
            return 1;
        else
            return -1;
        return 0;
    }
};

/// OOK decoder for FS20 type FS devices.
class FSxDecoder : public DecodeOOK {
public:
    FSxDecoder () {}

    // see also http://fhz4linux.info/tiki-index.php?page=FS20%20Protocol
    virtual int8_t decode (uint16_t width) {
        if (300 <= width && width < 775) {
            uint8_t w = width >= 500;
            switch (state) {
            case UNKNOWN:
                if (w == 0)
                    ++flip;
                else if (flip > 20)
                    state = T1;
                else
                    return -1;
                break;
            case OK:
                state = w == 0 ? T0 : T1;
                break;
            case T0:
                gotBit(0);
                if (w)
                    return -1;
                break;
            case T1:
                gotBit(1);
                if (!w)
                    return -1;
                break;
            }
        } else if (width >= 1500 && pos >= 5)
            return 1;
        else
            return -1;
        return 0;
    }
};
