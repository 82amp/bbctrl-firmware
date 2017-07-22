/******************************************************************************\

                This file is part of the Buildbotics firmware.

                  Copyright (c) 2015 - 2017 Buildbotics LLC
          Copyright (c) 2010 Alex Forencich <alex@alexforencich.com>
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

#include "lcd.h"

#include <avr/io.h>
#include <util/delay.h>

#include <stdbool.h>


void lcd_init(uint8_t addr) {
  // Enable I2C master
  TWIC.MASTER.BAUD = 0x9b; // 100 KHz with 32MHz clock
  TWIC.MASTER.CTRLA = TWI_MASTER_ENABLE_bm;
  TWIC.MASTER.CTRLB = TWI_MASTER_TIMEOUT_DISABLED_gc;
  TWIC.MASTER.STATUS |= 1; // Force idle

  _delay_ms(50);
  lcd_nibble(addr, 3 << 4); // Home
  _delay_ms(50);
  lcd_nibble(addr, 3 << 4); // Home
  _delay_ms(50);
  lcd_nibble(addr, 3 << 4); // Home
  lcd_nibble(addr, 2 << 4); // 4-bit

  lcd_write(addr,
            LCD_FUNCTION_SET | LCD_2_LINE | LCD_5x8_DOTS | LCD_4_BIT_MODE, 0);
  lcd_write(addr, LCD_DISPLAY_CONTROL | LCD_DISPLAY_ON, 0);
  lcd_write(addr, LCD_ENTRY_MODE_SET | LCD_ENTRY_SHIFT_INC, 0);

  lcd_write(addr, LCD_CLEAR_DISPLAY, 0);
  lcd_write(addr, LCD_RETURN_HOME, 0);
}


static void _write_i2c(uint8_t addr, uint8_t data) {
  data |= BACKLIGHT_BIT;

  TWIC.MASTER.ADDR = addr << 1;
  while (!(TWIC.MASTER.STATUS & TWI_MASTER_WIF_bm)) continue;

  TWIC.MASTER.DATA = data;
  while (!(TWIC.MASTER.STATUS & TWI_MASTER_WIF_bm)) continue;

  TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

  _delay_us(100);
}


void lcd_nibble(uint8_t addr, uint8_t data) {
  _write_i2c(addr, data);
  _write_i2c(addr, data | ENABLE_BIT);
  _delay_us(500);
  _write_i2c(addr, data & ~ENABLE_BIT);
  _delay_us(100);
}


void lcd_write(uint8_t addr, uint8_t cmd, uint8_t flags) {
  lcd_nibble(addr, flags | (cmd & 0xf0));
  lcd_nibble(addr, flags | ((cmd << 4) & 0xf0));
}


void lcd_goto(uint8_t addr, uint8_t x, uint8_t y) {
  static uint8_t row[] = {0, 64, 20, 84};
  lcd_write(addr, LCD_SET_DDRAM_ADDR | (row[y] + x), 0);
}


void lcd_putchar(uint8_t addr, uint8_t c) {
  lcd_write(addr, c, REG_SELECT_BIT);
}


void lcd_pgmstr(uint8_t addr, const char *s) {
  while (*s) lcd_putchar(addr, *s++);
}
