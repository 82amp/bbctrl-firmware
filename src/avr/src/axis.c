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


#include "axis.h"
#include "motor.h"
#include "plan/planner.h"

#include <math.h>
#include <string.h>
#include <ctype.h>


int motor_map[AXES] = {-1, -1, -1, -1, -1, -1};


typedef struct {
  float velocity_max;    // max velocity in mm/min or deg/min
  float travel_max;      // max work envelope for soft limits
  float travel_min;      // min work envelope for soft limits
  float jerk_max;        // max jerk (Jm) in km/min^3
  float radius;          // radius in mm for rotary axes
  float search_velocity; // homing search velocity
  float latch_velocity;  // homing latch velocity
  float latch_backoff;   // backoff from switches prior to homing latch movement
  float zero_backoff;    // backoff from switches for machine zero
  homing_mode_t homing_mode;
  bool homed;
} axis_t;


axis_t axes[MOTORS] = {{0}};


bool axis_is_enabled(int axis) {
  int motor = axis_get_motor(axis);
  return motor != -1 && motor_is_enabled(motor) &&
    !fp_ZERO(axis_get_velocity_max(axis));
}


char axis_get_char(int axis) {
  return (axis < 0 || AXES <= axis) ? '?' : "XYZABCUVW"[axis];
}


int axis_get_id(char axis) {
  const char *axes = "XYZABCUVW";
  char *ptr = strchr(axes, toupper(axis));
  return ptr == 0 ? -1 : (ptr - axes);
}


int axis_get_motor(int axis) {return motor_map[axis];}


// Map axes to first matching motor
void axis_map_motors() {
  for (int axis = 0; axis < AXES; axis++)
    for (int motor = 0; motor < MOTORS; motor++)
      if (motor_get_axis(motor) == axis) {
        motor_map[axis] = motor;
        break;
      }
}


float axis_get_vector_length(const float a[], const float b[]) {
  return sqrt(square(a[AXIS_X] - b[AXIS_X]) + square(a[AXIS_Y] - b[AXIS_Y]) +
              square(a[AXIS_Z] - b[AXIS_Z]) + square(a[AXIS_A] - b[AXIS_A]) +
              square(a[AXIS_B] - b[AXIS_B]) + square(a[AXIS_C] - b[AXIS_C]));
}


#define AXIS_VAR_GET(NAME, TYPE)                        \
  TYPE get_##NAME(int axis) {return axes[axis].NAME;}


#define AXIS_VAR_SET(NAME, TYPE)                        \
  void set_##NAME(int axis, TYPE value) {axes[axis].NAME = value;}


#define AXIS_GET(NAME, TYPE, DEFAULT)                   \
  TYPE axis_get_##NAME(int axis) {                      \
    int motor = axis_get_motor(axis);                   \
    return motor == -1 ? DEFAULT : axes[motor].NAME;    \
  }                                                     \
  AXIS_VAR_GET(NAME, TYPE)


#define AXIS_SET(NAME, TYPE)                            \
  void axis_set_##NAME(int axis, TYPE value) {          \
    int motor = axis_get_motor(axis);                   \
    if (motor != -1) axes[motor].NAME = value;          \
  }                                                     \
  AXIS_VAR_SET(NAME, TYPE)


AXIS_SET(homed, bool)

AXIS_GET(velocity_max, float, 0)
AXIS_GET(homed, bool, false)
AXIS_GET(homing_mode, homing_mode_t, HOMING_MANUAL)
AXIS_GET(radius, float, 0)
AXIS_GET(travel_min, float, 0)
AXIS_GET(travel_max, float, 0)
AXIS_GET(search_velocity, float, 0)
AXIS_GET(latch_velocity, float, 0)
AXIS_GET(zero_backoff, float, 0)
AXIS_GET(latch_backoff, float, 0)

/* Note on jerk functions
 *
 * Jerk values can be rather large.  Jerk values are stored in the system in
 * truncated format; values are divided by 1,000,000 then multiplied before use.
 *
 * The axis_jerk() functions expect the jerk in divided by 1,000,000 form.
 */
AXIS_GET(jerk_max, float, 0)


AXIS_VAR_SET(velocity_max, float)
AXIS_VAR_SET(radius, float)
AXIS_VAR_SET(travel_min, float)
AXIS_VAR_SET(travel_max, float)
AXIS_VAR_SET(homing_mode, homing_mode_t)
AXIS_VAR_SET(search_velocity, float)
AXIS_VAR_SET(latch_velocity, float)
AXIS_VAR_SET(zero_backoff, float)
AXIS_VAR_SET(latch_backoff, float)
AXIS_VAR_SET(jerk_max, float)


float get_homing_dir(int axis) {
  switch (axes[axis].homing_mode) {
  case HOMING_MANUAL: break;
  case HOMING_STALL_MIN: case HOMING_SWITCH_MIN: return -1;
  case HOMING_STALL_MAX: case HOMING_SWITCH_MAX: return 1;
  }
  return 0;
}


float get_home(int axis) {
  switch (axes[axis].homing_mode) {
  case HOMING_MANUAL: break;
  case HOMING_STALL_MIN: case HOMING_SWITCH_MIN: return get_travel_min(axis);
  case HOMING_STALL_MAX: case HOMING_SWITCH_MAX: return get_travel_max(axis);
  }
  return NAN;
}


static int _get_homing_switch(int axis) {
  switch (axes[axis].homing_mode) {
  case HOMING_MANUAL: break;

  case HOMING_STALL_MIN: case HOMING_SWITCH_MIN:
    switch (axis) {
    case AXIS_X: return SW_MIN_X;
    case AXIS_Y: return SW_MIN_Y;
    case AXIS_Z: return SW_MIN_Z;
    case AXIS_A: return SW_MIN_A;
    }
    break;

  case HOMING_STALL_MAX: case HOMING_SWITCH_MAX:
    switch (axis) {
    case AXIS_X: return SW_MAX_X;
    case AXIS_Y: return SW_MAX_Y;
    case AXIS_Z: return SW_MAX_Z;
    case AXIS_A: return SW_MAX_A;
    }
    break;
  }

  return -1;
}


bool get_axis_can_home(int axis) {
  return axis_is_enabled(axis) && axes[axis].homing_mode != HOMING_MANUAL &&
    switch_is_enabled(_get_homing_switch(axis));
}
