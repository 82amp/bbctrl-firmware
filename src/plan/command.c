/******************************************************************************\

                This file is part of the Buildbotics firmware.

                  Copyright (c) 2015 - 2016 Buildbotics LLC
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

/* How this works:
 *   - A command is called by the Gcode interpreter (cm_<command>, e.g. M code)
 *   - cm_ function calls mp_queue_command which puts it in the planning queue
 *     (bf buffer) which sets some parameters and registers a callback to the
 *     execution function in the canonical machine.
 *   - When the planning queue gets to the function it calls _exec_command()
 *     which loads a pointer to the bf buffer in stepper.c's next move.
 *   - When the runtime gets to the end of the current activity (sending steps,
 *     counting a dwell) it executes mp_runtime_command which uses the callback
 *     function in the bf and the saved parameters in the vectors.
 *   - To finish up mp_runtime_command() frees the bf buffer.
 *
 * Doing it this way instead of synchronizing on queue empty simplifies the
 * handling of feedholds, feed overrides, buffer flushes, and thread blocking,
 * and makes keeping the queue full and avoiding starvation much easier.
 */

#include "command.h"

#include "buffer.h"
#include "canonical_machine.h"
#include "stepper.h"


/// Callback to execute command
static stat_t _exec_command(mpBuf_t *bf) {
  st_prep_command(bf);
  return STAT_OK;
}


/// Queue a synchronous Mcode, program control, or other command
void mp_queue_command(cm_exec_t cm_exec, float *value, float *flag) {
  mpBuf_t *bf = mp_get_write_buffer();

  if (!bf) {
    cm_hard_alarm(STAT_BUFFER_FULL_FATAL);
    return; // Shouldn't happen, buffer availability was checked upstream.
  }

  bf->move_type = MOVE_TYPE_COMMAND;
  bf->bf_func = _exec_command;    // callback to planner queue exec function
  bf->cm_func = cm_exec;          // callback to canonical machine exec function

  // Store values and flags in planner buffer
  for (int axis = 0; axis < AXES; axis++) {
    bf->ms.target[axis] = value[axis];
    bf->unit[axis] = flag[axis]; // flag vector in unit
  }

  // Must be final operation before exit
  mp_commit_write_buffer(MOVE_TYPE_COMMAND);
}


void mp_runtime_command(mpBuf_t *bf) {
  // Use values & flags stored in mp_queue_command()
  bf->cm_func(bf->ms.target, bf->unit);

  // Free buffer & perform cycle_end if planner is empty
  if (mp_free_run_buffer()) cm_cycle_end();
}
