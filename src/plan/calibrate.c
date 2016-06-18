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


#include "calibrate.h"

#include "buffer.h"
#include "motor.h"
#include "canonical_machine.h"
#include "planner.h"
#include "stepper.h"
#include "rtc.h"
#include "tmc2660.h"
#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define CAL_VELOCITIES 256
#define CAL_MIN_VELOCITY 1000 // mm/sec
#define CAL_TARGET_SG 100
#define CAL_MAX_DELTA_SG 75
#define CAL_WAIT_TIME 3 // ms


enum {
  CAL_START,
  CAL_ACCEL,
  CAL_MEASURE,
  CAL_DECEL,
};


typedef struct {
  bool busy;
  bool stall_valid;
  bool stalled;
  bool reverse;

  uint32_t wait;
  int state;
  int motor;
  int axis;

  float velocity;
  uint16_t stallguard;
} calibrate_t;

static calibrate_t cal = {};


static stat_t _exec_calibrate(mpBuf_t *bf) {
  if (bf->move_state == MOVE_NEW) bf->move_state = MOVE_RUN;

  const float time = MIN_SEGMENT_TIME; // In minutes
  const float maxDeltaV = JOG_ACCELERATION * time;

  if (rtc_expired(cal.wait))
    switch (cal.state) {
    case CAL_START: {
      cal.axis = motor_get_axis(cal.motor);
      cal.state = CAL_ACCEL;
      cal.velocity = 0;
      cal.stall_valid = false;
      cal.stalled = false;
      cal.reverse = false;

      tmc2660_set_stallguard_threshold(cal.motor, 8);
      cal.wait = rtc_get_time() + CAL_WAIT_TIME;

      break;
    }

    case CAL_ACCEL:
      if (CAL_MIN_VELOCITY < cal.velocity) cal.stall_valid = true;

      if (cal.velocity < CAL_MIN_VELOCITY || CAL_TARGET_SG < cal.stallguard)
        cal.velocity += maxDeltaV;

      if (cal.stalled) {
        if (cal.reverse) {
          int32_t steps = -motor_get_encoder(cal.motor);
          float mm = (float)steps / motor_get_steps_per_unit(cal.motor);
          printf("%"PRIi32" steps %0.2f mm\n", steps, mm);

          tmc2660_set_stallguard_threshold(cal.motor, 63);
          mp_free_run_buffer(); // Release buffer
          cal.busy = false;
          return STAT_OK;

        } else {
          motor_set_encoder(cal.motor, 0);

          cal.reverse = true;
          cal.velocity = 0;
          cal.stall_valid = false;
          cal.stalled = false;
        }
      }
      break;
    }

  if (!cal.velocity) return STAT_OK;

  // Compute travel
  float travel[AXES] = {}; // In mm
  travel[cal.axis] = time * cal.velocity * (cal.reverse ? -1 : 1);

  // Convert to steps
  float steps[MOTORS] = {0};
  mp_kinematics(travel, steps);

  // Queue segment
  float error[MOTORS] = {0};
  st_prep_line(steps, error, time);

  return STAT_OK;
}


bool calibrate_busy() {return cal.busy;}


void calibrate_set_stallguard(int motor, uint16_t sg) {
  if (cal.motor != motor) return;

  if (cal.stall_valid) {
    int16_t delta = sg - cal.stallguard;
    if (!sg || CAL_MAX_DELTA_SG < abs(delta)) {
      cal.stalled = true;
      motor_end_move(cal.motor);
    }
  }

  cal.stallguard = sg;
}


uint8_t command_calibrate(int argc, char *argv[]) {
  if (cal.busy) return 0;

  mpBuf_t *bf = mp_get_write_buffer();
  if (!bf) {
    cm_hard_alarm(STAT_BUFFER_FULL_FATAL);
    return 0;
  }

  // Start
  memset(&cal, 0, sizeof(cal));
  cal.busy = true;
  cal.motor = 1;

  bf->bf_func = _exec_calibrate; // register callback
  mp_commit_write_buffer(MOVE_TYPE_COMMAND);

  return 0;
}
