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

/* Planner buffers are used to queue and operate on Gcode blocks. Each
 * buffer contains one Gcode block which may be a move, and M code, or
 * other command that must be executed synchronously with movement.
 *
 * Buffers are in a circularly linked list managed by a WRITE pointer
 * and a RUN pointer.  New blocks are populated by (1) getting a write
 * buffer, (2) populating the buffer, then (3) placing it in the queue
 * (queue write buffer). If an exception occurs during population you
 * can unget the write buffer before queuing it, which returns it to
 * the pool of available buffers.
 *
 * The RUN buffer is the buffer currently executing. It may be
 * retrieved once for simple commands, or multiple times for
 * long-running commands like moves. When the command is complete the
 * run buffer is returned to the pool by freeing it.
 *
 * Notes:
 *    The write buffer pointer only moves forward on _queue_write_buffer, and
 *    the read buffer pointer only moves forward on free_read calls.
 *    (test, get and unget have no effect)
 */

#include "buffer.h"
#include "state.h"
#include "report.h"

#include <string.h>


typedef struct {                            // ring buffer for sub-moves
  volatile uint8_t buffers_available;       // count of available buffers
  mp_buffer_t *w;                           // get_write_buffer pointer
  mp_buffer_t *q;                           // queue_write_buffer pointer
  mp_buffer_t *r;                           // get/end_run_buffer pointer
  mp_buffer_t bf[PLANNER_BUFFER_POOL_SIZE]; // buffer storage
} buffer_pool_t;


buffer_pool_t mb; // move buffer queue


uint8_t mp_get_planner_buffer_room() {
  uint16_t n = mb.buffers_available;
  return n < PLANNER_BUFFER_HEADROOM ? 0 : n - PLANNER_BUFFER_HEADROOM;
}


void mp_wait_for_buffer() {
  while (!mb.buffers_available) continue;
}


/// buffer incr & wrap
#define _bump(a) ((a < PLANNER_BUFFER_POOL_SIZE - 1) ? a + 1 : 0)


/// Initializes or resets buffers
void mp_init_buffers() {
  mp_buffer_t *pv;

  memset(&mb, 0, sizeof(mb));      // clear all values, pointers and status

  mb.w = mb.q = mb.r = &mb.bf[0];  // init write and read buffer pointers
  pv = &mb.bf[PLANNER_BUFFER_POOL_SIZE - 1];

  // setup ring pointers
  for (int i = 0; i < PLANNER_BUFFER_POOL_SIZE; i++) {
    mb.bf[i].nx = &mb.bf[_bump(i)];
    mb.bf[i].pv = pv;
    pv = &mb.bf[i];
  }

  mb.buffers_available = PLANNER_BUFFER_POOL_SIZE;

  mp_state_idle();
}


bool mp_queue_empty() {return mb.w == mb.r;}


/// Get pointer to next available write buffer
/// Returns pointer or 0 if no buffer available.
mp_buffer_t *mp_get_write_buffer() {
  // get & clear a buffer
  if (mb.w->buffer_state == MP_BUFFER_EMPTY) {
    mp_buffer_t *w = mb.w;
    mp_buffer_t *nx = mb.w->nx;               // save linked list pointers
    mp_buffer_t *pv = mb.w->pv;
    memset(mb.w, 0, sizeof(mp_buffer_t));     // clear all values
    w->nx = nx;                               // restore pointers
    w->pv = pv;
    w->buffer_state = MP_BUFFER_LOADING;
    mb.w = w->nx;

    mb.buffers_available--;
    report_request();

    return w;
  }

  return 0;
}


/* Commit the next write buffer to the queue
 * Advances write pointer & changes buffer state
 *
 * WARNING: The routine calling mp_commit_write_buffer() must not use the write
 * buffer once it has been queued. Action may start on the buffer immediately,
 * invalidating its contents
 */
void mp_commit_write_buffer(uint32_t line, move_type_t type) {
  mp_state_running();

  mb.q->ms.line = line;
  mb.q->move_type = type;
  mb.q->run_state = MOVE_NEW;
  mb.q->buffer_state = MP_BUFFER_QUEUED;
  mb.q = mb.q->nx; // advance the queued buffer pointer
}


/* Get pointer to the next or current run buffer
 * Returns a new run buffer if prev buf was ENDed
 * Returns same buf if called again before ENDing
 * Returns 0 if no buffer available
 * The behavior supports continuations (iteration)
 */
mp_buffer_t *mp_get_run_buffer() {
  switch (mb.r->buffer_state) {
  case MP_BUFFER_QUEUED: // fresh buffer; becomes running if queued or pending
    mb.r->buffer_state = MP_BUFFER_RUNNING;
    // Fall through

  case MP_BUFFER_RUNNING: // asking for the same run buffer for the Nth time
    return mb.r; // return same buffer

  default: return 0; // no queued buffers
  }
}


/// Release the run buffer & return to buffer pool.
void mp_free_run_buffer() {           // EMPTY current run buf & adv to next
  mp_clear_buffer(mb.r);              // clear it out (& reset replannable)
  mb.r = mb.r->nx;                    // advance to next run buffer
  mb.buffers_available++;
  report_request();

  if (mp_queue_empty()) mp_state_idle();  // if queue empty
}


/// Returns pointer to last buffer, i.e. last block (zero)
mp_buffer_t *mp_get_last_buffer() {
  mp_buffer_t *bf = mp_get_run_buffer();
  mp_buffer_t *bp;

  for (bp = bf; bp && bp->nx != bf; bp = mp_get_next_buffer(bp))
    if (bp->nx->run_state == MOVE_OFF) break;

  return bp;
}


/// Zeroes the contents of the buffer
void mp_clear_buffer(mp_buffer_t *bf) {
  mp_buffer_t *nx = bf->nx;            // save pointers
  mp_buffer_t *pv = bf->pv;
  memset(bf, 0, sizeof(mp_buffer_t));
  bf->nx = nx;                         // restore pointers
  bf->pv = pv;
}


///  Copies the contents of bp into bf - preserves links
void mp_copy_buffer(mp_buffer_t *bf, const mp_buffer_t *bp) {
  mp_buffer_t *nx = bf->nx;            // save pointers
  mp_buffer_t *pv = bf->pv;
  memcpy(bf, bp, sizeof(mp_buffer_t));
  bf->nx = nx;                         // restore pointers
  bf->pv = pv;
}
