#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#include "spi.h"
#include "rf69.h"

RF69<SpiDev0> rf;

uint8_t rxBuf[64];

int main () {
  wiringPiSetup();
  int myFd = wiringPiSPISetup (0, 4000000);

  if (myFd < 0) {
    printf("Can't open the SPI bus: %d\n", errno);
    return 1;
  }

  printf("\n[rf69try]\n");

  rf.init(1, 42, 8686);
  //rf.encrypt("mysecret");
  rf.txPower(15); // 0 = min .. 31 = max

  uint16_t cnt = 0;
  uint8_t txBuf[62];
  for (int i = 0; i < sizeof txBuf; ++i)
    txBuf[i] = i;

  while (true) {
    if (++cnt % 1024 == 0) {
      int txLen = ++txBuf[0] % (sizeof txBuf + 1);
      printf(" > #%d, %db\n", txBuf[0], txLen);
      rf.send(0, txBuf, txLen);
    }

    int len = rf.receive(rxBuf, sizeof rxBuf);
    if (len >= 0) {
      printf("OK ");
      for (int i = 0; i < len; ++i)
        printf("%02x", rxBuf[i]);
      printf(" (%d%s%d:%d)\n",
          rf.rssi, rf.afc < 0 ? "" : "+", rf.afc, rf.lna);
    }

    chThdYield();
  }
}
