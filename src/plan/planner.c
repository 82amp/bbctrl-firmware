/*
 * planner.c - Cartesian trajectory planning and motion execution
 * This file is part of the TinyG project
 *
 * Copyright (c) 2010 - 2015 Alden S. Hart, Jr.
 * Copyright (c) 2012 - 2015 Rob Giseburt
 *
 * This file ("the software") is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 as published by the Free Software
 * Foundation. You should have received a copy of the GNU General
 * Public License, version 2 along with the software.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * As a special exception, you may use this file as part of a software
 * library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with  other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU General
 * Public License.
 *
 * THE SOFTWARE IS DISTRIBUTED IN THE HOPE THAT IT WILL BE USEFUL, BUT
 * WITHOUT ANY WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Planner Notes
 *
 * The planner works below the canonical machine and above the
 * motor mapping and stepper execution layers. A rudimentary
 * multitasking capability is implemented for long-running commands
 * such as lines, arcs, and dwells.  These functions are coded as
 * non-blocking continuations - which are simple state machines
 * that are re-entered multiple times until a particular operation
 * is complete. These functions have 2 parts - the initial call,
 * which sets up the local context, and callbacks (continuations)
 * that are called from the main loop (in controller.c).
 *
 * One important concept is isolation of the three layers of the
 * data model - the Gcode model (gm), planner model (bf queue &
 * mm), and runtime model (mr).  These are designated as "model",
 * "planner" and "runtime" in function names.
 *
 * The Gcode model is owned by the canonical machine and should
 * only be accessed by cm_xxxx() functions. Data from the Gcode
 * model is transferred to the planner by the mp_xxx() functions
 * called by the canonical machine.
 *
 * The planner should only use data in the planner model. When a
 * move (block) is ready for execution the planner data is
 * transferred to the runtime model, which should also be isolated.
 *
 * Lower-level models should never use data from upper-level models
 * as the data may have changed and lead to unpredictable results.
 */

#include "planner.h"
#include "arc.h"
#include "canonical_machine.h"
#include "kinematics.h"
#include "stepper.h"
#include "encoder.h"

#include <string.h>
#include <stdbool.h>
#include <stdio.h>


mpBufferPool_t mb;              // move buffer queue
mpMoveMasterSingleton_t mm;     // context for line planning
mpMoveRuntimeSingleton_t mr;    // context for line runtime


void planner_init() {
  // If you know all memory has been zeroed by a hard reset you don't need
  // these next 2 lines
  memset(&mr, 0, sizeof(mr));    // clear all values, pointers and status
  memset(&mm, 0, sizeof(mm));    // clear all values, pointers and status
  mp_init_buffers();
}


/* Flush all moves in the planner and all arcs
 *
 * Does not affect the move currently running in mr.  Does not affect
 * mm or gm model positions.  This function is designed to be called
 * during a hold to reset the planner.  This function should not
 * generally be called; call cm_queue_flush() instead.
 */
void mp_flush_planner() {
  cm_abort_arc();
  mp_init_buffers();
  cm_set_motion_state(MOTION_STOP);
}


/* Since steps are in motor space you have to run the position vector
 * through inverse kinematics to get the right numbers. This means
 * that in a non-Cartesian robot changing any position can result in
 * changes to multiple step values. So this operation is provided as a
 * single function and always uses the new position vector as an
 * input.
 *
 * Keeping track of position is complicated by the fact that moves
 * exist in several reference frames. The scheme to keep this
 * straight is:
 *
 *   - mm.position    - start and end position for planning
 *   - mr.position    - current position of runtime segment
 *   - mr.target      - target position of runtime segment
 *   - mr.endpoint    - final target position of runtime segment
 *
 * Note that position is set immediately when called and may not be
 * not an accurate representation of the tool position. The motors
 * are still processing the action and the real tool position is
 * still close to the starting point.
 */

/// Set planner position for a single axis
void mp_set_planner_position(uint8_t axis, const float position) {
  mm.position[axis] = position;
}


/// Set runtime position for a single axis
void mp_set_runtime_position(uint8_t axis, const float position) {
  mr.position[axis] = position;
}


/// Set encoder counts to the runtime position
void mp_set_steps_to_runtime_position() {
  float step_position[MOTORS];

  // convert lengths to steps in floating point
  ik_kinematics(mr.position, step_position);

  for (uint8_t motor = 0; motor < MOTORS; motor++) {
    mr.target_steps[motor] = step_position[motor];
    mr.position_steps[motor] = step_position[motor];
    mr.commanded_steps[motor] = step_position[motor];

    // write steps to encoder register
    en_set_encoder_steps(motor, step_position[motor]);

    // These must be zero:
    mr.following_error[motor] = 0;
    st_pre.mot[motor].corrected_steps = 0;
  }
}


