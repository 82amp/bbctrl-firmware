/******************************************************************************\

                This file is part of the Buildbotics firmware.

                  Copyright (c) 2015 - 2016 Buildbotics LLC
                  Copyright (c) 2010 - 2015 Alden S. Hart, Jr.
                            All rights reserved.

     This file ("the software") is free software: you can redistribute it
     and/or modify it under the terms of the GNU General Public License,
      version 2 as published by the Free Software Foundation. You should
      have received a copy of the GNU General Public License, version 2
     along with the software. If not, see <http://www.gnu.org/licenses/>.

     The software is distributed in the hope that it will be useful, but
          WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
               Lesser General Public License for more details.

       You should have received a copy of the GNU Lesser General Public
                License along with the software.  If not, see
                       <http://www.gnu.org/licenses/>.

                For information regarding this software email:
                  "Joseph Coffland" <joseph@buildbotics.com>

\******************************************************************************/

/* This GPIO file is where all parallel port bits are managed that are
 * not already taken up by steppers, serial ports, SPI or PDI programming
 *
 * There are 2 GPIO ports:
 *
 *   gpio1   Located on 5x2 header next to the PDI programming plugs
 *           Four (4) output bits capable of driving 3.3v or 5v logic
 *
 *           Note: On v6 and earlier boards there are also 4 inputs:
 *           Four (4) level converted input bits capable of being driven
 *           by 3.3v or 5v logic - connected to B0 - B3 (now used for SPI)
 *
 *   gpio2   Located on 9x2 header on "bottom" edge of the board
 *           Eight (8) non-level converted input bits
 *           Eight (8) ground pins - one each "under" each input pin
 *           Two   (2) 3.3v power pins (on left edge of connector)
 *           Inputs can be used as switch contact inputs or
 *           3.3v input bits depending on port configuration
 *           **** These bits CANNOT be used as 5v inputs ****
 */

#include "gpio.h"
#include "hardware.h"


void indicator_led_set() {gpio_led_on(INDICATOR_LED);}
void indicator_led_clear() {gpio_led_off(INDICATOR_LED);}
void indicator_led_toggle() {gpio_led_toggle(INDICATOR_LED);}
void gpio_led_on(uint8_t led) {gpio_set_bit_on(8 >> led);}
void gpio_led_off(uint8_t led) {gpio_set_bit_off(8 >> led);}
void gpio_led_toggle(uint8_t led) {gpio_set_bit_toggle(8 >> led);}


uint8_t gpio_read_bit(uint8_t b) {
  if (b & 0x08) return hw.out_port[0]->IN & GPIO1_OUT_BIT_bm;
  if (b & 0x04) return hw.out_port[1]->IN & GPIO1_OUT_BIT_bm;
  if (b & 0x02) return hw.out_port[2]->IN & GPIO1_OUT_BIT_bm;
  if (b & 0x01) return hw.out_port[3]->IN & GPIO1_OUT_BIT_bm;

  return 0;
}


void gpio_set_bit_on(uint8_t b) {
  if (b & 0x08) hw.out_port[0]->OUTSET = GPIO1_OUT_BIT_bm;
  if (b & 0x04) hw.out_port[1]->OUTSET = GPIO1_OUT_BIT_bm;
  if (b & 0x02) hw.out_port[2]->OUTSET = GPIO1_OUT_BIT_bm;
  if (b & 0x01) hw.out_port[3]->OUTSET = GPIO1_OUT_BIT_bm;
}


void gpio_set_bit_off(uint8_t b) {
  if (b & 0x08) hw.out_port[0]->OUTCLR = GPIO1_OUT_BIT_bm;
  if (b & 0x04) hw.out_port[1]->OUTCLR = GPIO1_OUT_BIT_bm;
  if (b & 0x02) hw.out_port[2]->OUTCLR = GPIO1_OUT_BIT_bm;
  if (b & 0x01) hw.out_port[3]->OUTCLR = GPIO1_OUT_BIT_bm;
}


void gpio_set_bit_toggle(uint8_t b) {
  if (b & 0x08) hw.out_port[0]->OUTTGL = GPIO1_OUT_BIT_bm;
  if (b & 0x04) hw.out_port[1]->OUTTGL = GPIO1_OUT_BIT_bm;
  if (b & 0x02) hw.out_port[2]->OUTTGL = GPIO1_OUT_BIT_bm;
  if (b & 0x01) hw.out_port[3]->OUTTGL = GPIO1_OUT_BIT_bm;
}
