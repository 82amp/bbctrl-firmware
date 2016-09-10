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

#pragma once

#include "machine.h"
#include "config.h"

#include <stdbool.h>


typedef enum {
  MOVE_OFF,                // move inactive (must be zero)
  MOVE_NEW,                // initial value
  MOVE_INIT,               // first run
  MOVE_RUN,                // general run state (for non-acceleration moves)
  MOVE_RESTART,            // restart buffer when done
} run_state_t;


// All the enums that equal zero must be zero. Don't change this
typedef enum {
  MP_BUFFER_EMPTY,                // struct is available for use (MUST BE 0)
  MP_BUFFER_LOADING,              // being written ("checked out")
  MP_BUFFER_QUEUED,               // in queue
  MP_BUFFER_RUNNING,              // current running buffer
} buffer_state_t;


// Callbacks
typedef void (*mach_func_t)(float[], float[]);
struct mp_buffer_t;
typedef stat_t (*bf_func_t)(struct mp_buffer_t *bf);


typedef struct mp_buffer_t {      // See Planning Velocity Notes
  struct mp_buffer_t *pv;         // pointer to previous buffer
  struct mp_buffer_t *nx;         // pointer to next buffer

  uint32_t ts;                    // Time stamp
  bf_func_t bf_func;              // callback to buffer exec function
  mach_func_t mach_func;          // callback to machine

  buffer_state_t buffer_state;    // used to manage queuing/dequeuing
  run_state_t run_state;          // run state machine sequence
  bool replannable;               // true if move can be re-planned

  int32_t line;                   // gcode block line number

  float target[AXES];             // XYZABC where the move should go
  float unit[AXES];               // unit vector for axis scaling & planning

  float length;                   // total length of line or helix in mm
  float head_length;
  float body_length;
  float tail_length;

  // See notes on these variables, in aline()
  float entry_velocity;           // entry velocity requested for the move
  float cruise_velocity;          // cruise velocity requested & achieved
  float exit_velocity;            // exit velocity requested for the move
  float braking_velocity;         // current value for braking velocity

  float entry_vmax;               // max junction velocity at entry of this move
  float cruise_vmax;              // max cruise velocity requested for move
  float exit_vmax;                // max exit velocity possible (redundant)
  float delta_vmax;               // max velocity difference for this move

  float jerk;                     // maximum linear jerk term for this move
  float recip_jerk;               // 1/Jm used for planning (computed & cached)
  float cbrt_jerk;                // cube root of Jm (computed & cached)

  float dwell;
} mp_buffer_t;


void mp_init_buffers();
uint8_t mp_get_planner_buffer_room();
uint8_t mp_get_planner_buffer_fill();
void mp_wait_for_buffer();
bool mp_queue_empty();
mp_buffer_t *mp_get_write_buffer();
void mp_commit_write_buffer(uint32_t line);
mp_buffer_t *mp_get_run_buffer();
void mp_free_run_buffer();
mp_buffer_t *mp_get_last_buffer();
static inline mp_buffer_t *mp_buffer_prev(mp_buffer_t *bp) {return bp->pv;}
static inline mp_buffer_t *mp_buffer_next(mp_buffer_t *bp) {return bp->nx;}
void mp_clear_buffer(mp_buffer_t *bf);
void mp_copy_buffer(mp_buffer_t *bf, const mp_buffer_t *bp);
