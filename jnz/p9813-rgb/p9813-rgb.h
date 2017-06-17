// P9813  driver.

#include <libopencm3/stm32/usart.h>
#include <stdio.h>

class P9813
{
  public:
    P9813(uint32_t, uint16_t, uint32_t, uint16_t);
    void begin(void);
    void end(void);
    void ClkRise(void);
    void Send32Zero(void);
    uint8_t TakeAntiCode(uint8_t dat);
    void DatSend(uint32_t dx);
    void SetColor(uint8_t Red,uint8_t Green,uint8_t Blue);
  private:
    uint32_t CKport;
    uint16_t CKpin;
    uint32_t DOport;
    uint16_t DOpin;
};

