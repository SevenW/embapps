//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//  Modified Record:
/***************************************************************************/
#include "p9813-rgb.h"
#include <libopencm3/stm32/gpio.h>

void delay_us(uint32_t);

P9813::P9813(uint32_t ClkPort, uint16_t ClkPin, uint32_t DataPort, uint16_t DataPin)
{
	CKport = ClkPort;
	CKpin = ClkPin;
	DOport = DataPort;
	DOpin = DataPin;
    gpio_mode_setup(CKport, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, CKpin);    // CLK
    gpio_set_output_options(CKport, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CKpin);
    gpio_mode_setup(DOport, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DOpin);    // DO
    gpio_set_output_options(DOport, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DOpin);
}

void P9813::begin(void)
{
  Send32Zero();
}

void P9813::end(void)
{
  Send32Zero();
}

void P9813::ClkRise(void)
{
  gpio_clear(CKport, CKpin);
  delay_us(20);
  gpio_set(CKport, CKpin);
  delay_us(20);
}

void P9813::Send32Zero(void)
{
  unsigned char i;

  for (i=0; i<32; i++)
  {
	gpio_clear(DOport, DOpin);
	ClkRise();
  }
}

uint8_t P9813::TakeAntiCode(uint8_t dat)
{
  uint8_t tmp = 0;

  if ((dat & 0x80) == 0)
  {
    tmp |= 0x02;
  }

  if ((dat & 0x40) == 0)
  {
    tmp |= 0x01;
  }

  return tmp;
}

// gray data
void P9813::DatSend(uint32_t dx)
{
  uint8_t i;

  for (i=0; i<32; i++)
  {
    if ((dx & 0x80000000) != 0)
    {
    	gpio_set(DOport, DOpin);
    }
    else
    {
    	gpio_clear(DOport, DOpin);
    }

    dx <<= 1;
    ClkRise();
  }
}

// Set color
void P9813::SetColor(uint8_t Red,uint8_t Green,uint8_t Blue)
{
  uint32_t dx = 0;

  dx |= (uint32_t)0x03 << 30;             // highest two bits 1，flag bits
  dx |= (uint32_t)TakeAntiCode(Blue) << 28;
  dx |= (uint32_t)TakeAntiCode(Green) << 26;
  dx |= (uint32_t)TakeAntiCode(Red) << 24;

  dx |= (uint32_t)Blue << 16;
  dx |= (uint32_t)Green << 8;
  dx |= Red;

  DatSend(dx);
}
