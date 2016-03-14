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

#include "planner.h"
#include "canonical_machine.h"
#include "stepper.h"


// Dwells are performed by passing a dwell move to the stepper drivers.
// When the stepper driver sees a dwell it times the dwell on a separate
// timer than the stepper pulse timer.


/// Dwell execution
static stat_t _exec_dwell(mpBuf_t *bf) {
  st_prep_dwell(bf->gm.move_time); // in seconds
  // free buffer & perform cycle_end if planner is empty
  if (mp_free_run_buffer()) cm_cycle_end();

  return STAT_OK;
}


/// Queue a dwell
stat_t mp_dwell(float seconds) {
  mpBuf_t *bf;

  if (!(bf = mp_get_write_buffer())) // get write buffer
    return cm_hard_alarm(STAT_BUFFER_FULL_FATAL); // never supposed to fail

  bf->bf_func = _exec_dwell;  // register callback to dwell start
  bf->gm.move_time = seconds; // in seconds, not minutes
  bf->move_state = MOVE_NEW;
  // must be final operation before exit
  mp_commit_write_buffer(MOVE_TYPE_DWELL);

  return STAT_OK;
}
