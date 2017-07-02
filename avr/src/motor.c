/******************************************************************************\

                This file is part of the Buildbotics firmware.

                  Copyright (c) 2015 - 2017 Buildbotics LLC
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

#include "motor.h"
#include "config.h"
#include "hardware.h"
#include "cpp_magic.h"
#include "rtc.h"
#include "report.h"
#include "stepper.h"
#include "drv8711.h"
#include "estop.h"
#include "gcode_state.h"
#include "axis.h"
#include "util.h"
#include "pgmspace.h"

#include "plan/runtime.h"
#include "plan/calibrate.h"

#include <util/delay.h>

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct {
  // Config
  uint8_t axis;                  // map motor to axis
  uint16_t microsteps;           // microsteps per full step
  bool reverse;
  motor_power_mode_t power_mode;
  float step_angle;              // degrees per whole step
  float travel_rev;              // mm or deg of travel per motor revolution
  uint8_t step_pin;
  uint8_t dir_pin;
  TC0_t *timer;
  DMA_CH_t *dma;
  uint8_t dma_trigger;

  // Computed
  float steps_per_unit;

  // Runtime state
  uint32_t power_timeout;
  int32_t commanded;
  int32_t encoder;
  int16_t error;
  bool last_negative;
  uint8_t last_clock;

  // Move prep
  bool prepped;
  uint8_t timer_clock;
  uint16_t timer_period;
  bool negative;
  int32_t position;
} motor_t;


static motor_t motors[MOTORS] = {
  {
    .axis            = AXIS_X,
    .step_pin        = STEP_X_PIN,
    .dir_pin         = DIR_X_PIN,
    .timer           = &M1_TIMER,
    .dma             = &M1_DMA_CH,
    .dma_trigger     = M1_DMA_TRIGGER,
  }, {
    .axis            = AXIS_Y,
    .step_pin        = STEP_Y_PIN,
    .dir_pin         = DIR_Y_PIN,
    .timer           = &M2_TIMER,
    .dma             = &M2_DMA_CH,
    .dma_trigger     = M2_DMA_TRIGGER,
  }, {
    .axis            = AXIS_Z,
    .step_pin        = STEP_Z_PIN,
    .dir_pin         = DIR_Z_PIN,
    .timer           = &M3_TIMER,
    .dma             = &M3_DMA_CH,
    .dma_trigger     = M3_DMA_TRIGGER,
  }, {
    .axis            = AXIS_A,
    .step_pin        = STEP_A_PIN,
    .dir_pin         = DIR_A_PIN,
    .timer           = (TC0_t *)&M4_TIMER,
    .dma             = &M4_DMA_CH,
    .dma_trigger     = M4_DMA_TRIGGER,
    }
};


static uint8_t _dummy;


static void _update_config(int motor) {
  motor_t *m = &motors[motor];

  m->steps_per_unit = 360.0 * m->microsteps / m->travel_rev / m->step_angle;
}


void motor_init() {
  // Enable DMA
  DMA.CTRL = DMA_RESET_bm;
  DMA.CTRL = DMA_ENABLE_bm;
  DMA.INTFLAGS = 0xff; // clear all pending interrupts

  for (int motor = 0; motor < MOTORS; motor++) {
    motor_t *m = &motors[motor];

    _update_config(motor);
    axis_set_motor(m->axis, motor);

    // IO pins
    DIRSET_PIN(m->step_pin); // Output
    DIRSET_PIN(m->dir_pin);  // Output

    // Setup motor timer
    m->timer->CTRLB = TC_WGMODE_FRQ_gc | TC1_CCAEN_bm;

    // Setup DMA channel as timer event counter
    m->dma->ADDRCTRL = DMA_CH_SRCDIR_FIXED_gc | DMA_CH_DESTDIR_FIXED_gc;
    m->dma->TRIGSRC = m->dma_trigger;

    // Note, the DMA transfer must read CCA to clear the trigger
    m->dma->SRCADDR0 = (((uintptr_t)&m->timer->CCA) >> 0) & 0xff;
    m->dma->SRCADDR1 = (((uintptr_t)&m->timer->CCA) >> 8) & 0xff;
    m->dma->SRCADDR2 = 0;

    m->dma->DESTADDR0 = (((uintptr_t)&_dummy) >> 0) & 0xff;
    m->dma->DESTADDR1 = (((uintptr_t)&_dummy) >> 8) & 0xff;
    m->dma->DESTADDR2 = 0;

    m->dma->TRFCNT = 0xffff;
    m->dma->REPCNT = 0;
    m->dma->CTRLB = 0;
    m->dma->CTRLA = DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc;

    drv8711_set_microsteps(motor, m->microsteps);
  }
}


bool motor_is_enabled(int motor) {
  return motors[motor].power_mode != MOTOR_DISABLED;
}


int motor_get_axis(int motor) {return motors[motor].axis;}


void motor_set_axis(int motor, uint8_t axis) {
  if (MOTORS <= motor || AXES <= axis || axis == motors[motor].axis) return;
  axis_set_motor(motors[motor].axis, -1);
  motors[motor].axis = axis;
  axis_set_motor(axis, motor);
}


float motor_get_steps_per_unit(int motor) {return motors[motor].steps_per_unit;}
uint16_t motor_get_microsteps(int motor) {return motors[motor].microsteps;}


void motor_set_microsteps(int motor, uint16_t microsteps) {
  switch (microsteps) {
  case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128: case 256:
    break;
  default: return;
  }

  motors[motor].microsteps = microsteps;
  _update_config(motor);
  drv8711_set_microsteps(motor, microsteps);
}


void motor_set_position(int motor, int32_t position) {
  //if (st_is_busy()) ALARM(STAT_INTERNAL_ERROR); TODO

  motor_t *m = &motors[motor];

  m->commanded = m->encoder = m->position = position << 1; // We use half steps
  m->error = 0;
}


int32_t motor_get_position(int motor) {
  return motors[motor].position >> 1; // Convert from half to full steps
}


static void _update_power(int motor) {
  motor_t *m = &motors[motor];

  switch (m->power_mode) {
  case MOTOR_POWERED_ONLY_WHEN_MOVING:
  case MOTOR_POWERED_IN_CYCLE:
    if (rtc_expired(m->power_timeout)) {
      drv8711_set_state(motor, DRV8711_IDLE);
      break;
    }
    // Fall through

  case MOTOR_ALWAYS_POWERED:
    // TODO is ~5ms enough time to enable the motor?
    drv8711_set_state(motor, DRV8711_ACTIVE);
    break;

  default: // Disabled
    drv8711_set_state(motor, DRV8711_DISABLED);
  }
}


/// Callback to manage motor power sequencing and power-down timing.
stat_t motor_rtc_callback() { // called by controller
  for (int motor = 0; motor < MOTORS; motor++)
    _update_power(motor);

  return STAT_OK;
}


void motor_end_move(int motor) {
  motor_t *m = &motors[motor];

  if (!m->timer->CTRLA) return;

  // Stop clock
  m->timer->CTRLA = 0;

  // Get actual step count from DMA channel
  const int24_t half_steps = 0xffff - m->dma->TRFCNT;

  // Accumulate encoder
  m->encoder += m->last_negative ? -half_steps : half_steps;

  // Compute error
  m->error = m->commanded - m->encoder;
}


void motor_load_move(int motor) {
  motor_t *m = &motors[motor];

  ASSERT(m->prepped);

  motor_end_move(motor);

  // Set direction, compensating for polarity
  const bool counterclockwise = m->negative ^ m->reverse;
  SET_PIN(m->dir_pin, counterclockwise);

  // Adjust clock count
  if (m->last_clock) {
    uint24_t count = m->timer->CNT;
    int8_t freq_change = m->last_clock - m->timer_clock;

    count <<= freq_change; // Adjust count

    if (m->timer_period <= count) count -= m->timer_period;
    if (m->timer_period <= count) count -= m->timer_period;
    if (m->timer_period <= count) count = m->timer_period >> 1;

    m->timer->CNT = count;

  } else m->timer->CNT = m->timer_period >> 1;

  // Reset DMA channel counter
  m->dma->CTRLA &= ~DMA_CH_ENABLE_bm;
  m->dma->TRFCNT = 0xffff;
  m->dma->CTRLA |= DMA_CH_ENABLE_bm;

  // Set clock and period
  m->timer->CCA = m->timer_period;     // Set frequency
  m->timer->CTRLA = m->timer_clock;    // Start or stop
  m->last_clock = m->timer_clock;      // Save clock value
  m->timer_clock = 0;                  // Clear clock
  m->last_negative = m->negative;
  m->commanded = m->position;

  // Clear move
  m->prepped = false;
}


void motor_prep_move(int motor, float time, int32_t target) {
  motor_t *m = &motors[motor];

  // Validate input
  ASSERT(0 <= motor && motor < MOTORS);
  ASSERT(!m->prepped);

  // We count in half steps
  target = target << 1;

  // Compute travel in steps
  int24_t half_steps = target - m->position;
  m->position = target;

  // Error correction
  int16_t correction = abs(m->error);
  if (MIN_HALF_STEP_CORRECTION <= correction) {
    // Allowed step correction is proportional to velocity
    int24_t positive_half_steps = half_steps < 0 ? -half_steps : half_steps;
    int16_t max_correction = (positive_half_steps >> 5) + 1;
    if (max_correction < correction) correction = max_correction;

    if (m->error < 0) correction = -correction;

    half_steps += correction;
    m->error -= correction;
  }

  // Positive steps from here on
  m->negative = half_steps < 0;
  if (m->negative) half_steps = -half_steps;

  // Find the fastest clock rate that will fit the required number of steps.
  // Note, clock toggles step line so we need two clocks per step.
  uint24_t seg_clocks = time * F_CPU * 60;
  uint24_t ticks_per_step = seg_clocks / half_steps + 1; // Round up
  if (ticks_per_step < 0xffff) m->timer_clock = TC_CLKSEL_DIV1_gc;
  else if (ticks_per_step < 0x1ffff) m->timer_clock = TC_CLKSEL_DIV2_gc;
  else if (ticks_per_step < 0x3ffff) m->timer_clock = TC_CLKSEL_DIV4_gc;
  else if (ticks_per_step < 0x7ffff) m->timer_clock = TC_CLKSEL_DIV8_gc;
  else m->timer_clock = 0; // Clock off, too slow

  // Note, we rely on the fact that TC_CLKSEL_DIV1_gc through TC_CLKSEL_DIV8_gc
  // equal 1, 2, 3 & 4 respectively.
  m->timer_period = (ticks_per_step >> (m->timer_clock - 1)) + 1; // Round up

  if (!m->timer_period || !half_steps) m->timer_clock = 0;

  // Power motor
  switch (m->power_mode) {
  case MOTOR_POWERED_ONLY_WHEN_MOVING:
    if (!m->timer_clock) break; // Not moving
    // Fall through

  case MOTOR_ALWAYS_POWERED: case MOTOR_POWERED_IN_CYCLE:
    // Reset timeout
    m->power_timeout = rtc_get_time() + MOTOR_IDLE_TIMEOUT * 1000;
    break;

  default: break;
  }
  _update_power(motor);

  // Queue move
  m->prepped = true;
}


// Var callbacks
float get_step_angle(int motor) {return motors[motor].step_angle;}


void set_step_angle(int motor, float value) {
  motors[motor].step_angle = value;
  _update_config(motor);
}


float get_travel(int motor) {return motors[motor].travel_rev;}


void set_travel(int motor, float value) {
  motors[motor].travel_rev = value;
  _update_config(motor);
}


uint16_t get_microstep(int motor) {return motors[motor].microsteps;}


void set_microstep(int motor, uint16_t value) {
  if (motor < 0 || MOTORS <= motor) return;
  motor_set_microsteps(motor, value);
}


bool get_reverse(int motor) {
  if (motor < 0 || MOTORS <= motor) return 0;
  return motors[motor].reverse;
}


void set_reverse(int motor, bool value) {motors[motor].reverse = value;}
char get_motor_axis(int motor) {return motors[motor].axis;}
void set_motor_axis(int motor, uint8_t axis) {motor_set_axis(motor, axis);}


uint8_t get_power_mode(int motor) {return motors[motor].power_mode;}


void set_power_mode(int motor, uint8_t value) {
  if (value <= MOTOR_POWERED_ONLY_WHEN_MOVING)
    motors[motor].power_mode = value;
  else motors[motor].power_mode = MOTOR_DISABLED;
}


int32_t get_encoder(int m) {return motors[m].encoder;}
int32_t get_error(int m) {return motors[m].error;}
