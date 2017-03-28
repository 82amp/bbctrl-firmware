/******************************************************************************\

                This file is part of the Buildbotics firmware.

                  Copyright (c) 2015 - 2017 Buildbotics LLC
                  Copyright (c) 2010 - 2015 Alden S. Hart, Jr.
                  Copyright (c) 2012 - 2015 Rob Giseburt
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

#include "planner.h"
#include "buffer.h"
#include "stepper.h"
#include "motor.h"
#include "util.h"
#include "report.h"
#include "state.h"
#include "config.h"

#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>


typedef struct {
  bool busy;               // True if a move is running
  float position[AXES];    // Current move position
  float work_offset[AXES]; // Current move work offset
  float velocity;          // Current move velocity

  int32_t line;            // Current move GCode line number
  uint8_t tool;            // Active tool

  float feed;
  feed_mode_t feed_mode;
  float feed_override;
  float spindle_override;

  plane_t plane;
  units_t units;
  coord_system_t coord_system;
  bool absolute_mode;
  path_mode_t path_mode;
  distance_mode_t distance_mode;
  distance_mode_t arc_distance_mode;

  float previous_error[MOTORS];
} mp_runtime_t;

static mp_runtime_t rt = {0};


bool mp_runtime_is_busy() {return rt.busy;}
void mp_runtime_set_busy(bool busy) {rt.busy = busy;}
int32_t mp_runtime_get_line() {return rt.line;}
void mp_runtime_set_line(int32_t line) {rt.line = line; report_request();}
uint8_t mp_runtime_get_tool() {return rt.tool;}
void mp_runtime_set_tool(uint8_t tool) {rt.tool = tool; report_request();}


/// Returns current segment velocity
float mp_runtime_get_velocity() {return rt.velocity;}


void mp_runtime_set_velocity(float velocity) {
  rt.velocity = velocity;
  report_request();
}


/// Set encoder counts to the runtime position
void mp_runtime_set_steps_from_position() {
  // Convert lengths to steps in floating point
  float steps[MOTORS];
  mp_kinematics(rt.position, steps);

  for (int motor = 0; motor < MOTORS; motor++)
    // Write steps to encoder register
    motor_set_encoder(motor, steps[motor]);
}


/* Since steps are in motor space you have to run the position vector
 * through inverse kinematics to get the right numbers.  This means
 * that in a non-Cartesian robot changing any position can result in
 * changes to multiple step values.  So this operation is provided as a
 * single function and always uses the new position vector as an
 * input.
 *
 * Keeping track of position is complicated by the fact that moves
 * exist in several reference frames.  The scheme to keep this
 * straight is:
 *
 *   - mp_position    - start and end position for planning
 *   - rt.position    - current position of runtime segment
 *   - rt.steps.*     - position in steps
 *
 * Note that position is set immediately when called and may not be
 * an accurate representation of the tool position.  The motors
 * are still processing the action and the real tool position is
 * still close to the starting point.
 */


/// Set runtime position for a single axis
void mp_runtime_set_axis_position(uint8_t axis, const float position) {
  rt.position[axis] = position;
  report_request();
}


/// Returns current axis position in machine coordinates
float mp_runtime_get_axis_position(uint8_t axis) {return rt.position[axis];}
float *mp_runtime_get_position() {return rt.position;}


void mp_runtime_set_position(float position[]) {
  copy_vector(rt.position, position);
  report_request();
}


/// Returns axis position in work coordinates that were in effect at plan time
float mp_runtime_get_work_position(uint8_t axis) {
  return rt.position[axis] - rt.work_offset[axis];
}


/// Set offsets
void mp_runtime_set_work_offsets(float offset[]) {
  copy_vector(rt.work_offset, offset);
}


static void _step_correction(const float steps[], float time, int32_t error[]) {
#ifdef STEP_CORRECTION
  float travel[MOTORS];
  float new_length_sqr = 0;
  float old_length_sqr = 0;

  for (int motor = 0; motor < MOTORS; motor++) {
    error[motor] = motor_get_error(motor);
    travel[motor] = steps[motor] - motor_get_position(motor);

    if (fp_ZERO(travel[motor])) {
      motor[travel] = 0;
      motor[error] = 0;
    }

    error[motor] = 0.5 * (error[motor] - rt.previous_error[motor]);
    rt.previous_error[motor] = error[motor];

    if (error[motor] < -MAX_STEP_CORRECTION)
      error[motor] = -MAX_STEP_CORRECTION;
    else if (MAX_STEP_CORRECTION < error[motor])
      error[motor] = MAX_STEP_CORRECTION;

    old_length_sqr += square(travel[motor]);
    new_length_sqr += square(travel[motor] - error[motor]);
  }

  bool use_error = false;
  if (!fp_ZERO(new_length_sqr)) {
    float new_time = time * invsqrt(old_length_sqr / new_length_sqr);

    if (!isnan(new_time) && !isinf(new_time) &&
        EPSILON <= new_time && new_time <= MAX_SEGMENT_TIME) {
      time = new_time;
      use_error = true;
    }
  }

  if (!use_error)
    for (int motor = 0; motor < MOTORS; motor++)
      error[motor] = 0;

#endif // STEP_CORRECTION
}


/// Segment runner
stat_t mp_runtime_move_to_target(float target[], float time) {
  ASSERT(isfinite(time));

  // Convert target position to steps.
  float steps[MOTORS];
  mp_kinematics(target, steps);

  int32_t error[MOTORS] = {0};
  _step_correction(steps, time, error);

  // Call the stepper prep function
  RITORNO(st_prep_line(time, steps, error));

  // Update positions
  mp_runtime_set_position(target);

  return STAT_OK;
}
