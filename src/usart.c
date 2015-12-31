/******************************************************************************\

                   This file is part of the TinyG firmware.

                     Copyright (c) 2015, Buildbotics LLC
                             All rights reserved.

        The C! library is free software: you can redistribute it and/or
        modify it under the terms of the GNU Lesser General Public License
        as published by the Free Software Foundation, either version 2.1 of
        the License, or (at your option) any later version.

        The C! library is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
        Lesser General Public License for more details.

        You should have received a copy of the GNU Lesser General Public
        License along with the C! library.  If not, see
        <http://www.gnu.org/licenses/>.

        In addition, BSD licensing may be granted on a case by case basis
        by written permission from at least one of the copyright holders.
        You may request written permission by emailing the authors.

                For information regarding this software email:
                               Joseph Coffland
                            joseph@buildbotics.com

\******************************************************************************/

#include "usart.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include <stdio.h>

// Ring buffers
#define RING_BUF_NAME tx_buf
#define RING_BUF_SIZE USART_TX_RING_BUF_SIZE
#include "ringbuf.def"

#define RING_BUF_NAME rx_buf
#define RING_BUF_SIZE USART_RX_RING_BUF_SIZE
#include "ringbuf.def"

int usart_flags = USART_CRLF | USART_ECHO;


static void set_dre_interrupt(int enable) {
  if (enable) USARTC0.CTRLA |= USART_DREINTLVL_HI_gc;
  else USARTC0.CTRLA &= ~USART_DREINTLVL_HI_gc;
}


static void set_rxc_interrupt(int enable) {
  if (enable) USARTC0.CTRLA |= USART_RXCINTLVL_HI_gc;
  else USARTC0.CTRLA &= ~USART_RXCINTLVL_HI_gc;
}


// Data register empty interrupt vector
ISR(USARTC0_DRE_vect) {
  if (tx_buf_empty()) set_dre_interrupt(0); // Disable interrupt
  else {
    USARTC0.DATA = tx_buf_peek();
    tx_buf_pop();
  }
}


// Data received interrupt vector
ISR(USARTC0_RXC_vect) {
  if (rx_buf_full()) set_rxc_interrupt(0); // Disable interrupt

  else {
    uint8_t data = USARTC0.DATA;
    rx_buf_push(data);

    if ((usart_flags & USART_ECHO) && !tx_buf_full()) {
      set_dre_interrupt(1); // Enable interrupt
      usart_putc(data);
    }
  }
}


static int usart_putchar(char c, FILE *f) {
  usart_putc(c);
  return 0;
}

static FILE _stdout = FDEV_SETUP_STREAM(usart_putchar, 0, _FDEV_SETUP_WRITE);


void usart_init(void) {
  // Setup ring buffer
  tx_buf_init();
  rx_buf_init();

  PR.PRPC &= ~PR_USART0_bm; // Disable power reduction

  // Setup pins
  PORTC.OUTSET = 1 << 3; // High
  PORTC.DIRSET = 1 << 3; // Output
  PORTC.DIRCLR = 1 << 2; // Input

  // Set baud rate
  usart_set_baud(USART_BAUD_115200);

  // No parity, 8 data bits, 1 stop bit
  USARTC0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc |
    USART_CHSIZE_8BIT_gc;

  // Configure receiver and transmitter
  USARTC0.CTRLA = USART_RXCINTLVL_HI_gc;
  USARTC0.CTRLB = USART_RXEN_bm | USART_TXEN_bm | USART_CLK2X_bm;

  PMIC.CTRL |= PMIC_HILVLEN_bm; // Lowlevel interrupt on

  // Connect IO
  stdout = &_stdout;
  stderr = &_stdout;
}


static void set_baud(uint8_t bsel, uint8_t bscale) {
  USARTC0.BAUDCTRLB = (uint8_t)((bscale << 4) | (bsel >> 8));
  USARTC0.BAUDCTRLA = bsel;
}


void usart_set_baud(int baud) {
  // The BSEL / BSCALE values provided below assume a 32 Mhz clock
  // Assumes CTRLB CLK2X bit (0x04) is not enabled

  switch (baud) {
  case USART_BAUD_9600:    set_baud(207, 0);      break;
  case USART_BAUD_19200:   set_baud(103, 0);      break;
  case USART_BAUD_38400:   set_baud(51, 0);       break;
  case USART_BAUD_57600:   set_baud(34, 0);       break;
  case USART_BAUD_115200:  set_baud(33, -1 << 4); break;
  case USART_BAUD_230400:  set_baud(31, -2 << 4); break;
  case USART_BAUD_460800:  set_baud(27, -3 << 4); break;
  case USART_BAUD_921600:  set_baud(19, -4 << 4); break;
  case USART_BAUD_500000:  set_baud(1, 1 << 4);   break;
  case USART_BAUD_1000000: set_baud(1, 0);        break;
  }
}


void usart_ctrl(int flag, int enable) {
  if (enable) usart_flags |= flag;
  else usart_flags &= ~flag;
}


static void usart_sleep() {
  cli();
  SLEEP.CTRL = SLEEP_SMODE_IDLE_gc | SLEEP_SEN_bm;
  sei();
  sleep_cpu();
}


void usart_putc(char c) {
  while (tx_buf_full()) usart_sleep();

  tx_buf_push(c);

  set_dre_interrupt(1); // Enable interrupt

  if ((usart_flags & USART_CRLF) && c == '\n') usart_putc('\r');
}


void usart_puts(const char *s) {
  while (*s) usart_putc(*s++);
}


int8_t usart_getc() {
  while (rx_buf_empty()) usart_sleep();

  uint8_t data = rx_buf_peek();
  rx_buf_pop();

  set_rxc_interrupt(1); // Enable interrupt

  return data;
}


int usart_gets(char *buf, int size) {
  int fill = rx_buf_fill();

  for (int i = 0; i < fill; i++) {
    uint8_t data = rx_buf_get(i);

    if (data == '\r' || data == '\n' || i == size) {
      for (int j = 0; j < i; j++) {
        buf[j] = rx_buf_peek();
        rx_buf_pop();
      }

      buf[i] = 0;
      if (i != size) rx_buf_pop();
      set_rxc_interrupt(1); // Enable interrupt
      return 0; // OK
    }
  }

  return 2; // EAGAIN
}


int16_t usart_peek() {
  return rx_buf_empty() ? -1 : rx_buf_peek();
}


void usart_rx_flush() {
  rx_buf_init();
}


int16_t usart_rx_space() {
  return rx_buf_space();
}


int16_t usart_rx_fill() {
  return rx_buf_fill();
}


int16_t usart_tx_space() {
  return tx_buf_space();
}


int16_t usart_tx_fill() {
  return tx_buf_fill();
}
