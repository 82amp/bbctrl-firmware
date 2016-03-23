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

#include "motor.h"
#include "config.h"
#include "hardware.h"
#include "cpp_magic.h"
#include "rtc.h"
#include "report.h"
#include "stepper.h"
#include "encoder.h"
#include "tmc2660.h"

#include "plan/planner.h"

#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


#define TIMER_CC_BM(x) CAT3(TC1_, x, EN_bm)


typedef enum {
  MOTOR_IDLE,                   // motor stopped and may be partially energized
  MOTOR_ENERGIZING,
  MOTOR_ACTIVE
} motorPowerState_t;


typedef enum {
  MOTOR_DISABLED,                 // motor enable is deactivated
  MOTOR_ALWAYS_POWERED,           // motor is always powered while machine is ON
  MOTOR_POWERED_IN_CYCLE,         // motor fully powered during cycles,
                                  // de-powered out of cycle
  MOTOR_POWERED_ONLY_WHEN_MOVING, // idles shortly after stopped, even in cycle
  MOTOR_POWER_MODE_MAX_VALUE      // for input range checking
} cmMotorPowerMode_t;


typedef enum {
  MOTOR_POLARITY_NORMAL,
  MOTOR_POLARITY_REVERSED
} cmMotorPolarity_t;


typedef struct {
  // Config
  uint8_t motor_map;             // map motor to axis
  uint16_t microsteps;           // microsteps to apply for each axis (ex: 8)
  cmMotorPolarity_t polarity;
  cmMotorPowerMode_t power_mode;
  float step_angle;              // degrees per whole step (ex: 1.8)
  float travel_rev;              // mm or deg of travel per motor revolution
  TC0_t *timer;

  // Runtime state
  motorPowerState_t power_state; // state machine for managing motor power
  uint32_t timeout;
  cmMotorFlags_t flags;

  // Move prep
  uint8_t timer_clock;           // clock divisor setting or zero for off
  uint16_t timer_period;         // clock period counter
  uint32_t steps;                // expected steps

  // direction and direction change
  cmDirection_t direction;       // travel direction corrected for polarity
  cmDirection_t prev_direction;  // travel direction from previous segment run
  int8_t step_sign;              // set to +1 or -1 for encoders

  // step error correction
  int32_t correction_holdoff;    // count down segments between corrections
  float corrected_steps;         // accumulated for cycle (diagnostic)
} motor_t;


static motor_t motors[MOTORS] = {
  {
    .motor_map  = M1_MOTOR_MAP,
    .step_angle = M1_STEP_ANGLE,
    .travel_rev = M1_TRAVEL_PER_REV,
    .microsteps = M1_MICROSTEPS,
    .polarity   = M1_POLARITY,
    .power_mode = M1_POWER_MODE,
    .timer      = (TC0_t *)&M1_TIMER,
    .prev_direction = STEP_INITIAL_DIRECTION
  }, {
    .motor_map  = M2_MOTOR_MAP,
    .step_angle = M2_STEP_ANGLE,
    .travel_rev = M2_TRAVEL_PER_REV,
    .microsteps = M2_MICROSTEPS,
    .polarity   = M2_POLARITY,
    .power_mode = M2_POWER_MODE,
    .timer      = &M2_TIMER,
    .prev_direction = STEP_INITIAL_DIRECTION
  }, {
    .motor_map  = M3_MOTOR_MAP,
    .step_angle = M3_STEP_ANGLE,
    .travel_rev = M3_TRAVEL_PER_REV,
    .microsteps = M3_MICROSTEPS,
    .polarity   = M3_POLARITY,
    .power_mode = M3_POWER_MODE,
    .timer      = &M3_TIMER,
    .prev_direction = STEP_INITIAL_DIRECTION
  }, {
    .motor_map  = M4_MOTOR_MAP,
    .step_angle = M4_STEP_ANGLE,
    .travel_rev = M4_TRAVEL_PER_REV,
    .microsteps = M4_MICROSTEPS,
    .polarity   = M4_POLARITY,
    .power_mode = M4_POWER_MODE,
    .timer      = &M4_TIMER,
    .prev_direction = STEP_INITIAL_DIRECTION
  }
};


/// Special interrupt for X-axis
ISR(TCE1_CCA_vect) {
  PORT_MOTOR_1.OUTTGL = STEP_BIT_bm;
}


void motor_init() {
  // Reset position
  mp_set_steps_to_runtime_position();

  // Setup motor timers
  M1_TIMER.CTRLB = TC_WGMODE_FRQ_gc | TIMER_CC_BM(M1_TIMER_CC);
  M2_TIMER.CTRLB = TC_WGMODE_FRQ_gc | TIMER_CC_BM(M2_TIMER_CC);
  M3_TIMER.CTRLB = TC_WGMODE_FRQ_gc | TIMER_CC_BM(M3_TIMER_CC);
  M4_TIMER.CTRLB = TC_WGMODE_FRQ_gc | TIMER_CC_BM(M4_TIMER_CC);

  // Setup special interrupt for X-axis mapping
  M1_TIMER.INTCTRLB = TC_CCAINTLVL_HI_gc;
}


int motor_get_axis(int motor) {
  return motors[motor].motor_map;
}


int motor_get_steps_per_unit(int motor) {
  return (360 * motors[motor].microsteps) /
    (motors[motor].travel_rev * motors[motor].step_angle);
}


/// returns true if motor is in an error state
static bool _error(int motor) {
  return motors[motor].flags & MOTOR_FLAG_ERROR_bm;
}


/// Remove power from a motor
static void _deenergize(int motor) {
  if (motors[motor].power_state == MOTOR_ACTIVE) {
    motors[motor].power_state = MOTOR_IDLE;
    tmc2660_disable(motor);
  }
}


/// Apply power to a motor
static void _energize(int motor) {
  if (motors[motor].power_state == MOTOR_IDLE && !_error(motor)) {
    motors[motor].power_state = MOTOR_ENERGIZING;
    tmc2660_enable(motor);
  }

  // Reset timeout, regardless
  motors[motor].timeout = rtc_get_time() + MOTOR_IDLE_TIMEOUT * 1000;
}


bool motor_energizing() {
  for (int motor = 0; motor < MOTORS; motor++)
    if (motors[motor].power_state == MOTOR_ENERGIZING)
      return true;

  return false;
}


void motor_driver_callback(int motor) {
  motor_t *m = &motors[motor];

  if (m->power_state == MOTOR_IDLE) m->flags &= ~MOTOR_FLAG_ENABLED_bm;
  else {
    m->power_state = MOTOR_ACTIVE;
    m->flags |= MOTOR_FLAG_ENABLED_bm;
  }

  st_request_load_move();
  report_request();
}


/// Callback to manage motor power sequencing and power-down timing.
stat_t motor_power_callback() { // called by controller
  for (int motor = 0; motor < MOTORS; motor++)
    // Deenergize motor if disabled, in error or after timeout when not holding
    if (motors[motor].power_mode == MOTOR_DISABLED || _error(motor) ||
        (cm_get_combined_state() != COMBINED_HOLD &&
         motors[motor].timeout < rtc_get_time()))
      _deenergize(motor);

  return STAT_OK;
}


void motor_error_callback(int motor, cmMotorFlags_t errors) {
  if (motors[motor].power_state != MOTOR_ACTIVE) return;

  motors[motor].flags |= errors;
  report_request();

  if (_error(motor)) {
    _deenergize(motor);

    // Stop and flush motion
    cm_request_feedhold();
    cm_request_queue_flush();
  }
}


void motor_prep_move(int motor, uint32_t seg_clocks, float travel_steps,
                     float error) {
  motor_t *m = &motors[motor];

  if (fp_ZERO(travel_steps)) {
    m->timer_clock = 0; // Motor clock off
    return;
  }

  // Setup the direction, compensating for polarity.
  // Set the step_sign which is used by the stepper ISR to accumulate step
  // position
  if (0 <= travel_steps) { // positive direction
    m->direction = DIRECTION_CW ^ m->polarity;
    m->step_sign = 1;

  } else {
    m->direction = DIRECTION_CCW ^ m->polarity;
    m->step_sign = -1;
  }

#ifdef __STEP_CORRECTION
  float correction;

  // 'Nudge' correction strategy. Inject a single, scaled correction value
  // then hold off
  if (--m->correction_holdoff < 0 &&
      STEP_CORRECTION_THRESHOLD < fabs(error)) {

    m->correction_holdoff = STEP_CORRECTION_HOLDOFF;
    correction = error * STEP_CORRECTION_FACTOR;

    if (0 < correction)
      correction = min3(correction, fabs(travel_steps), STEP_CORRECTION_MAX);
    else correction =
           max3(correction, -fabs(travel_steps), -STEP_CORRECTION_MAX);

    m->corrected_steps += correction;
    travel_steps -= correction;
  }
#endif

  // Compute motor timer clock and period. Rounding is performed to eliminate
  // a negative bias in the uint32_t conversion that results in long-term
  // negative drift.
  uint16_t steps = round(fabs(travel_steps));
  uint32_t ticks_per_step = seg_clocks / (steps + 0.5);

  // Find the right clock rate
  if (ticks_per_step & 0xffff0000UL) {
    ticks_per_step /= 2;
    seg_clocks /= 2;

    if (ticks_per_step & 0xffff0000UL) {
      ticks_per_step /= 2;
      seg_clocks /= 2;

      if (ticks_per_step & 0xffff0000UL) {
        ticks_per_step /= 2;
        seg_clocks /= 2;

        if (ticks_per_step & 0xffff0000UL) m->timer_clock = 0; // Off
        else m->timer_clock = TC_CLKSEL_DIV8_gc;
      } else m->timer_clock = TC_CLKSEL_DIV4_gc;
    } else m->timer_clock = TC_CLKSEL_DIV2_gc;
  } else m->timer_clock = TC_CLKSEL_DIV1_gc;

  m->timer_period = ticks_per_step * 2; // TODO why do we need *2 here?
  m->steps = seg_clocks / ticks_per_step;
}


void motor_begin_move(int motor) {
  motor_t *m = &motors[motor];

  // Energize motor
  switch (m->power_mode) {
  case MOTOR_DISABLED: return;

  case MOTOR_POWERED_ONLY_WHEN_MOVING:
    if (!m->timer_clock) return; // Not moving
    // Fall through

  case MOTOR_ALWAYS_POWERED: case MOTOR_POWERED_IN_CYCLE:
    _energize(motor);
    break;

  case MOTOR_POWER_MODE_MAX_VALUE: break; // Shouldn't get here
  }
}


void motor_load_move(int motor) {
  motor_t *m = &motors[motor];

  // Set or zero runtime clock and period
  m->timer->CTRLFCLR = TC0_DIR_bm; // Count up
  m->timer->CNT = 0; // Start at zero
  m->timer->CCA = m->timer_period;  // Set frequency
  m->timer->CTRLA = m->timer_clock; // Start or stop

  // If motor has 0 steps the following is all skipped. This ensures that
  // state comparisons always operate on the last segment actually run by
  // this motor, regardless of how many segments it may have been inactive
  // in between.
  if (m->timer_clock) {
    // Detect direction change and set the direction bit in hardware.
    if (m->direction != m->prev_direction) {
      m->prev_direction = m->direction;

      if (m->direction == DIRECTION_CW)
        hw.st_port[motor]->OUTCLR = DIRECTION_BIT_bm;
      else hw.st_port[motor]->OUTSET = DIRECTION_BIT_bm;
    }

    // Accumulate encoder
    en[motor].encoder_steps += m->steps * m->step_sign;
    m->steps = 0;
  }
}


void motor_end_move(int motor) {
  // Disable motor clock
  motors[motor].timer->CTRLA = 0;
}


// Var callbacks
float get_step_angle(int motor) {
  return motors[motor].step_angle;
}


void set_step_angle(int motor, float value) {
  motors[motor].step_angle = value;
}


float get_travel(int motor) {
  return motors[motor].travel_rev;
}


void set_travel(int motor, float value) {
  motors[motor].travel_rev = value;
}


uint16_t get_microstep(int motor) {
  return motors[motor].microsteps;
}


void set_microstep(int motor, uint16_t value) {
  switch (value) {
  case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128: case 256:
    break;
  default: return;
  }

  motors[motor].microsteps = value;
}


uint8_t get_polarity(int motor) {
  if (motor < 0 || MOTORS <= motor) return 0;
  return motors[motor].polarity;
}


void set_polarity(int motor, uint8_t value) {
  motors[motor].polarity = value;
}


uint8_t get_motor_map(int motor) {
  return motors[motor].motor_map;
}


void set_motor_map(int motor, uint16_t value) {
  if (value < AXES) motors[motor].motor_map = value;
}


uint8_t get_power_mode(int motor) {
  return motors[motor].power_mode;
}


void set_power_mode(int motor, uint16_t value) {
  if (value < MOTOR_POWER_MODE_MAX_VALUE)
    motors[motor].power_mode = value;
}



uint8_t get_status_flags(int motor) {
  return motors[motor].flags;
}


void print_status_flags(uint8_t flags) {
  bool first = true;

  putchar('"');

  if (MOTOR_FLAG_ENABLED_bm & flags) {
    printf_P(PSTR("enable"));
    first = false;
  }

  if (MOTOR_FLAG_STALLED_bm & flags) {
    if (!first) printf_P(PSTR(", "));
    printf_P(PSTR("stall"));
    first = false;
  }

  if (MOTOR_FLAG_OVERTEMP_WARN_bm & flags) {
    if (!first) printf_P(PSTR(", "));
    printf_P(PSTR("temp warn"));
    first = false;
  }

  if (MOTOR_FLAG_OVERTEMP_bm & flags) {
    if (!first) printf_P(PSTR(", "));
    printf_P(PSTR("over temp"));
    first = false;
  }

  if (MOTOR_FLAG_SHORTED_bm & flags) {
    if (!first) printf_P(PSTR(", "));
    printf_P(PSTR("short"));
    first = false;
  }

  putchar('"');
}


uint8_t get_status_strings(int motor) {
  return get_status_flags(motor);
}


// Command callback
void command_mreset(int argc, char *argv[]) {
  if (argc == 1)
    for (int motor = 0; motor < MOTORS; motor++)
      motors[motor].flags = 0;

  else {
    int motor = atoi(argv[1]);
    if (motor < MOTORS) motors[motor].flags = 0;
  }

  report_request();
}
