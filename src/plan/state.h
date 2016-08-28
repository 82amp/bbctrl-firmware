/******************************************************************************\

                This file is part of the Buildbotics firmware.

                  Copyright (c) 2015 - 2016 Buildbotics LLC
                  Copyright (c) 2013 - 2015 Alden S. Hart, Jr.
                  Copyright (c) 2013 - 2015 Robert Giseburt
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

#include <avr/pgmspace.h>

#include <stdbool.h>


typedef enum {
  STATE_READY,
  STATE_ESTOPPED,
  STATE_RUNNING,
  STATE_STOPPING,
  STATE_HOLDING,
} plannerState_t;


typedef enum {
  CYCLE_MACHINING,
  CYCLE_HOMING,
  CYCLE_PROBING,
  CYCLE_CALIBRATING,
  CYCLE_JOGGING,
} plannerCycle_t;


typedef enum {          // feedhold state machine
  FEEDHOLD_OFF,         // no feedhold in effect
  FEEDHOLD_SYNC,        // start hold - sync to latest aline segment
  FEEDHOLD_PLAN,        // replan blocks for feedhold
  FEEDHOLD_DECEL,       // decelerate to hold point
  FEEDHOLD_HOLD,        // holding
} holdState_t;


plannerState_t mp_get_state();
plannerCycle_t mp_get_cycle();
holdState_t mp_get_hold_state();

void mp_set_state(plannerState_t state);
void mp_set_cycle(plannerCycle_t cycle);
void mp_set_hold_state(holdState_t hold);

PGM_P mp_get_state_pgmstr(plannerState_t state);
PGM_P mp_get_cycle_pgmstr(plannerCycle_t cycle);

void mp_state_running();
void mp_state_idle();
void mp_state_estop();
void mp_state_hold_callback(bool done);

void mp_request_hold();
void mp_request_flush();
void mp_request_start();

void mp_state_callback();
