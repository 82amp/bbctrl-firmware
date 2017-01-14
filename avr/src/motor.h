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

#pragma once

#include "status.h"

#include <stdint.h>
#include <stdbool.h>


typedef enum {
  MOTOR_FLAG_ENABLED_bm       = 1 << 0,
  MOTOR_FLAG_STALLED_bm       = 1 << 1,
  MOTOR_FLAG_OVER_TEMP_bm     = 1 << 2,
  MOTOR_FLAG_OVER_CURRENT_bm  = 1 << 3,
  MOTOR_FLAG_DRIVER_FAULT_bm  = 1 << 4,
  MOTOR_FLAG_UNDER_VOLTAGE_bm = 1 << 5,
  MOTOR_FLAG_ERROR_bm         = (//MOTOR_FLAG_STALLED_bm |
                                 MOTOR_FLAG_OVER_TEMP_bm |
                                 MOTOR_FLAG_OVER_CURRENT_bm |
                                 MOTOR_FLAG_DRIVER_FAULT_bm |
                                 MOTOR_FLAG_UNDER_VOLTAGE_bm)
} motor_flags_t;


typedef enum {
  MOTOR_DISABLED,                 // motor enable is deactivated
  MOTOR_ALWAYS_POWERED,           // motor is always powered while machine is ON
  MOTOR_POWERED_IN_CYCLE,         // motor fully powered during cycles,
                                  // de-powered out of cycle
  MOTOR_POWERED_ONLY_WHEN_MOVING, // idles shortly after stopped, even in cycle
  MOTOR_POWER_MODE_MAX_VALUE      // for input range checking
} motor_power_mode_t;


typedef void (*stall_callback_t)(int motor);


void motor_init();
void motor_enable(int motor, bool enable);

bool motor_is_enabled(int motor);
int motor_get_axis(int motor);
void motor_set_stall_callback(int motor, stall_callback_t cb);
float motor_get_stall_homing_velocity(int motor);
float motor_get_steps_per_unit(int motor);
float motor_get_units_per_step(int motor);
uint16_t motor_get_microsteps(int motor);
void motor_set_microsteps(int motor, uint16_t microsteps);
int32_t motor_get_encoder(int motor);
void motor_set_encoder(int motor, float encoder);
int32_t motor_get_error(int motor);
int32_t motor_get_position(int motor);

bool motor_error(int motor);
bool motor_stalled(int motor);
void motor_reset(int motor);

bool motor_energizing();

void motor_driver_callback(int motor);
stat_t motor_rtc_callback();
void motor_error_callback(int motor, motor_flags_t errors);

void motor_load_move(int motor);
void motor_end_move(int motor);
stat_t motor_prep_move(int motor, int32_t clocks, float target, int32_t error,
                       float time);
