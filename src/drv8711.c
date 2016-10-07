/******************************************************************************\

                This file is part of the Buildbotics firmware.

                  Copyright (c) 2015 - 2016 Buildbotics LLC
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

#include "drv8711.h"
#include "status.h"
#include "motor.h"

#include <avr/interrupt.h>
#include <util/delay.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define DRIVERS 1
#define COMMANDS 4


#define DRV8711_WORD_BYTE_PTR(WORD, LOW) \
  (((uint8_t *)&(WORD)) + ((LOW) ? 0 : 1))


typedef struct {
  uint8_t status;

  bool active;
  float idle_current;
  float drive_current;
  float stall_threshold;

  uint8_t mode; // microstepping mode

  uint8_t cs_pin;
  uint8_t fault_pin;
} drv8711_driver_t;


static drv8711_driver_t drivers[MOTORS] = {
  {.cs_pin = SPI_CS_X_PIN, .fault_pin = FAULT_X_PIN},
  {.cs_pin = SPI_CS_Y_PIN, .fault_pin = FAULT_Y_PIN},
  {.cs_pin = SPI_CS_Z_PIN, .fault_pin = FAULT_Z_PIN},
  {.cs_pin = SPI_CS_A_PIN, .fault_pin = FAULT_A_PIN},
};


typedef struct {
  uint8_t *read;
  bool callback;
  uint8_t disable_cs_pin;

  uint8_t cmd;
  uint8_t driver;
  bool low_byte;

  uint8_t ncmds;
  uint16_t commands[DRIVERS][COMMANDS];
  uint16_t responses[DRIVERS];
} spi_t;

static spi_t spi = {0};


static void _driver_check_status(int driver) {
  uint8_t status = drivers[driver].status;
  uint8_t mflags = 0;

  if (status & DRV8711_STATUS_OTS_bm)    mflags |= MOTOR_FLAG_OVER_TEMP_bm;
  if (status & DRV8711_STATUS_AOCP_bm)   mflags |= MOTOR_FLAG_OVER_CURRENT_bm;
  if (status & DRV8711_STATUS_BOCP_bm)   mflags |= MOTOR_FLAG_OVER_CURRENT_bm;
  if (status & DRV8711_STATUS_APDF_bm)   mflags |= MOTOR_FLAG_DRIVER_FAULT_bm;
  if (status & DRV8711_STATUS_BPDF_bm)   mflags |= MOTOR_FLAG_DRIVER_FAULT_bm;
  if (status & DRV8711_STATUS_UVLO_bm)   mflags |= MOTOR_FLAG_UNDER_VOLTAGE_bm;
  if (status & DRV8711_STATUS_STD_bm)    mflags |= MOTOR_FLAG_STALLED_bm;
  if (status & DRV8711_STATUS_STDLAT_bm) mflags |= MOTOR_FLAG_STALLED_bm;

  if (mflags) motor_error_callback(driver, mflags);
}


static float _driver_get_current(int driver) {
  drv8711_driver_t *drv = &drivers[driver];
  return drv->active ? drv->drive_current : drv->idle_current;
}


static uint8_t _spi_next_command(uint8_t cmd) {
  // Process status responses
  for (int driver = 0; driver < DRIVERS; driver++) {
    uint16_t command = spi.commands[driver][cmd];

    if (DRV8711_CMD_IS_READ(command) &&
        DRV8711_CMD_ADDR(command) == DRV8711_STATUS_REG) {
      uint8_t status = spi.responses[driver];

      if (status != drivers[driver].status) {
        drivers[driver].status = status;
        _driver_check_status(driver);
      }
    }
  }

  // Next command
  if (++cmd == spi.ncmds) {
    cmd = 0; // Wrap around

    for (int driver = 0; driver < DRIVERS; driver++)
      motor_driver_callback(driver);
  }

  // Prep next command
  for (int driver = 0; driver < DRIVERS; driver++) {
    uint16_t *command = &spi.commands[driver][cmd];

    switch (DRV8711_CMD_ADDR(*command)) {
    case DRV8711_TORQUE_REG: // Update motor current setting
      *command = (*command & 0xff00) |
        (uint8_t)round(0xff * _driver_get_current(driver));
      break;

    case DRV8711_CTRL_REG: // Set microsteps
      *command = (*command & 0xff87) | (drivers[driver].mode << 3);
      break;

    default: break;
    }
  }

  return cmd;
}


static void _spi_send() {
  // Flush any status errors (TODO check for errors)
  uint8_t x = SPIC.STATUS;
  x = x;

  // Disable CS
  if (spi.disable_cs_pin) {
    OUTCLR_PIN(spi.disable_cs_pin); // Set low (inactive)
    _delay_us(1);
    spi.disable_cs_pin = 0;
  }

  // Schedule next CS disable or enable next CS now
  if (spi.low_byte) spi.disable_cs_pin = drivers[spi.driver].cs_pin;
  else {
    OUTSET_PIN(drivers[spi.driver].cs_pin); // Set high (active)
    _delay_us(1);
  }

  // Read
  if (spi.read) {
    *spi.read = SPIC.DATA;
    spi.read = 0;
  }

  // Callback, passing current command index, and get next command index
  if (spi.callback) {
    spi.cmd = _spi_next_command(spi.cmd);
    spi.callback = false;
  }

  // Write byte and prep next read
  SPIC.DATA =
    *DRV8711_WORD_BYTE_PTR(spi.commands[spi.driver][spi.cmd], spi.low_byte);
  spi.read = DRV8711_WORD_BYTE_PTR(spi.responses[spi.driver], spi.low_byte);

  // Check if WORD complete, go to next driver & check if command finished
  if (spi.low_byte && ++spi.driver == DRIVERS) {
    spi.driver = 0;      // Wrap around
    spi.callback = true; // Call back after last byte is read
  }

  // Next byte
  spi.low_byte = !spi.low_byte;
}


static void _init_spi_commands() {
  // Setup SPI command sequence
  for (int driver = 0; driver < DRIVERS; driver++) {
    uint16_t *commands = spi.commands[driver];
    spi.ncmds = 0;

    // Enable motor
    commands[spi.ncmds++] =
      DRV8711_WRITE(DRV8711_CTRL_REG, DRV8711_CTRL_ENBL_bm |
                    DRV8711_CTRL_EXSTALL_bm);

    // Set current
    commands[spi.ncmds++] =
      DRV8711_WRITE(DRV8711_TORQUE_REG, DRV8711_TORQUE_SMPLTH_100);

    // Read status
    commands[spi.ncmds++] = DRV8711_READ(DRV8711_STATUS_REG);

    // Clear status
    commands[spi.ncmds++] = DRV8711_WRITE(DRV8711_STATUS_REG, 0);
  }

  if (COMMANDS < spi.ncmds)
    STATUS_ERROR(STAT_INTERNAL_ERROR,
                 "SPI command buffer overflow increase COMMANDS in %s",
                 __FILE__);

  _spi_send(); // Kick it off
}


ISR(SPIC_INT_vect) {
  _spi_send();
}


void _fault_isr(int motor) {}


ISR(PORT_1_FAULT_ISR_vect) {_fault_isr(0);}
ISR(PORT_2_FAULT_ISR_vect) {_fault_isr(1);}
ISR(PORT_3_FAULT_ISR_vect) {_fault_isr(2);}
ISR(PORT_4_FAULT_ISR_vect) {_fault_isr(3);}


void drv8711_init() {
  // Configure motors
  for (int i = 0; i < MOTORS; i++) {
    drivers[i].idle_current = MOTOR_IDLE_CURRENT;
    drivers[i].drive_current = MOTOR_CURRENT;
    drivers[i].stall_threshold = MOTOR_STALL_THRESHOLD;

    drv8711_disable(i);
  }

  // Setup pins
  // Must set the SS pin either in/high or any/output for master mode to work
  // Note, this pin is also used by the USART as the CTS line
  DIRSET_PIN(SPI_SS_PIN); // Output
  OUTSET_PIN(SPI_CLK_PIN); // High
  DIRSET_PIN(SPI_CLK_PIN); // Output
  DIRCLR_PIN(SPI_MISO_PIN); // Input
  OUTSET_PIN(SPI_MOSI_PIN); // High
  DIRSET_PIN(SPI_MOSI_PIN); // Output

  for (int i = 0; i < DRIVERS; i++) {
    uint8_t cs_pin = drivers[i].cs_pin;
    uint8_t fault_pin = drivers[i].fault_pin;

    OUTSET_PIN(cs_pin);     // High
    DIRSET_PIN(cs_pin);     // Output
    OUTCLR_PIN(fault_pin);  // Input

    // Fault interrupt
    //PINCTRL_PIN(fault_pin) = PORT_ISC_RISING_gc;
    //PORT(fault_pin)->INT1MASK = BM(fault_pin);      // INT1
    //PORT(fault_pin)->INTCTRL |= PORT_INT1LVL_HI_gc;
  }

  // Configure SPI
  PR.PRPC &= ~PR_SPI_bm; // Disable power reduction
  SPIC.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc |
    SPI_PRESCALER_DIV16_gc; // enable, big endian, master, mode, clock div
  PORT(SPI_CLK_PIN)->REMAP = PORT_SPI_bm; // Swap SCK and MOSI
  SPIC.INTCTRL = SPI_INTLVL_LO_gc; // interupt level

  _init_spi_commands();
}


void drv8711_enable(int driver) {
  if (driver < 0 || DRIVERS <= driver) return;
  drivers[driver].active = true;
}


void drv8711_disable(int driver) {
  if (driver < 0 || DRIVERS <= driver) return;
  drivers[driver].active = false;
}


void drv8711_set_microsteps(int driver, uint16_t msteps) {
  if (driver < 0 || DRIVERS <= driver) return;
  switch (msteps) {
  case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128: case 256:
    break;
  default: return; // Invalid
  }

  drivers[driver].mode = round(logf(msteps) / logf(2));
}


float get_drive_power(int driver) {
  if (driver < 0 || DRIVERS <= driver) return 0;
  return drivers[driver].drive_current;
}


void set_drive_power(int driver, float value) {
  if (driver < 0 || DRIVERS <= driver || value < 0 || 1 < value) return;
  drivers[driver].drive_current = value;
}


float get_idle_power(int driver) {
  if (driver < 0 || DRIVERS <= driver) return 0;
  return drivers[driver].idle_current;
}


void set_idle_power(int driver, float value) {
  if (driver < 0 || DRIVERS <= driver || value < 0 || 1 < value) return;
  drivers[driver].idle_current = value;
}


float get_current_power(int driver) {
  if (driver < 0 || DRIVERS <= driver) return 0;
  return _driver_get_current(driver);
}
