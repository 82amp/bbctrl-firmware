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

#include "canonical_machine.h"

#include <stdbool.h>

/* PLANNER_BUFFER_POOL_SIZE
 *  Should be at least the number of buffers requires to support optimal
 *  planning in the case of very short lines or arc segments.
 *  Suggest 12 min. Limit is 255
 */
#define PLANNER_BUFFER_POOL_SIZE 32

/// Buffers to reserve in planner before processing new input line
#define PLANNER_BUFFER_HEADROOM 4


typedef enum {             // bf->move_type values
  MOVE_TYPE_NULL,          // null move - does a no-op
  MOVE_TYPE_ALINE,         // acceleration planned line
  MOVE_TYPE_DWELL,         // delay with no movement
  MOVE_TYPE_COMMAND,       // general command
  MOVE_TYPE_JOG,           // interactive jogging
} moveType_t;

typedef enum {
  MOVE_OFF,               // move inactive (MUST BE ZERO)
  MOVE_NEW,               // general value if you need an initialization
  MOVE_RUN,               // general run state (for non-acceleration moves)
  MOVE_SKIP_BLOCK         // mark a skipped block
} moveState_t;

typedef enum {
  SECTION_OFF,            // section inactive
  SECTION_NEW,            // uninitialized section
  SECTION_1st_HALF,       // first half of S curve
  SECTION_2nd_HALF        // second half of S curve or running a BODY (cruise)
} sectionState_t;

// All the enums that equal zero must be zero. Don't change this
typedef enum {                    // bf->buffer_state values
  MP_BUFFER_EMPTY,                // struct is available for use (MUST BE 0)
  MP_BUFFER_LOADING,              // being written ("checked out")
  MP_BUFFER_QUEUED,               // in queue
  MP_BUFFER_PENDING,              // marked as the next buffer to run
  MP_BUFFER_RUNNING               // current running buffer
} mpBufferState_t;


// Callbacks
typedef void (*cm_exec_t)(float[], float[]);
struct mpBuffer;
typedef stat_t (*bf_func_t)(struct mpBuffer *bf);


typedef struct mpBuffer {         // See Planning Velocity Notes
  struct mpBuffer *pv;            // pointer to previous buffer
  struct mpBuffer *nx;            // pointer to next buffer

  bf_func_t bf_func;              // callback to buffer exec function
  cm_exec_t cm_func;              // callback to canonical machine

  float naive_move_time;

  mpBufferState_t buffer_state;   // used to manage queuing/dequeuing
  moveType_t move_type;           // used to dispatch to run routine
  uint8_t move_code;              // byte used by used exec functions
  moveState_t move_state;         // move state machine sequence
  bool replannable;               // true if move can be re-planned

  float unit[AXES];               // unit vector for axis scaling & planning

  float length;                   // total length of line or helix in mm
  float head_length;
  float body_length;
  float tail_length;
  // See notes on these variables, in aline()
  float entry_velocity;           // entry velocity requested for the move
  float cruise_velocity;          // cruise velocity requested & achieved
  float exit_velocity;            // exit velocity requested for the move

  float entry_vmax;               // max junction velocity at entry of this move
  float cruise_vmax;              // max cruise velocity requested for move
  float exit_vmax;                // max exit velocity possible (redundant)
  float delta_vmax;               // max velocity difference for this move
  float braking_velocity;         // current value for braking velocity

  uint8_t jerk_axis;              // rate limiting axis used to compute jerk
  float jerk;                     // maximum linear jerk term for this move
  float recip_jerk;               // 1/Jm used for planning (computed & cached)
  float cbrt_jerk;                // cube root of Jm (computed & cached)

  MoveState_t ms;
} mpBuf_t;


uint8_t mp_get_planner_buffers_available();
void mp_init_buffers();
mpBuf_t *mp_get_write_buffer();
void mp_unget_write_buffer();
void mp_commit_write_buffer(const uint8_t move_type);
mpBuf_t *mp_get_run_buffer();
uint8_t mp_free_run_buffer();
mpBuf_t *mp_get_first_buffer();
mpBuf_t *mp_get_last_buffer();
/// Returns pointer to prev buffer in linked list
#define mp_get_prev_buffer(b) ((mpBuf_t *)(b->pv))
/// Returns pointer to next buffer in linked list
#define mp_get_next_buffer(b) ((mpBuf_t *)(b->nx))
void mp_clear_buffer(mpBuf_t *bf);
void mp_copy_buffer(mpBuf_t *bf, const mpBuf_t *bp);
