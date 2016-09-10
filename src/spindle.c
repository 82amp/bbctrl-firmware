/******************************************************************************\

                This file is part of the Buildbotics firmware.

                  Copyright (c) 2015 - 2016 Buildbotics LLC
                  Copyright (c) 2010 - 2015 Alden S. Hart, Jr.
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

#include "spindle.h"

#include "config.h"
#include "pwm_spindle.h"
#include "huanyang.h"

#include "plan/command.h"


typedef enum {
  SPINDLE_TYPE_PWM,
  SPINDLE_TYPE_HUANYANG,
} spindle_type_t;


typedef struct {
  spindle_type_t type;
  spindle_mode_t mode;
  float speed;
} spindle_t;


static spindle_t spindle = {.type = SPINDLE_TYPE};


void spindle_init() {
  pwm_spindle_init();
  huanyang_init();
}


void spindle_set(spindle_mode_t mode, float speed) {
  spindle.mode = mode;
  spindle.speed = speed;

  switch (spindle.type) {
  case SPINDLE_TYPE_PWM: pwm_spindle_set(mode, speed); break;
  case SPINDLE_TYPE_HUANYANG: huanyang_set(mode, speed); break;
  }
}


spindle_mode_t spindle_get_mode() {return spindle.mode;}
float spindle_get_speed() {return spindle.speed;}


void spindle_estop() {
  switch (spindle.type) {
  case SPINDLE_TYPE_PWM: pwm_spindle_estop(); break;
  case SPINDLE_TYPE_HUANYANG: huanyang_estop(); break;
  }
}


uint8_t get_spindle_type(int index) {return spindle.type;}


void set_spindle_type(int index, uint8_t value) {
  if (value != spindle.type) {
    spindle_mode_t mode = spindle.mode;
    float speed = spindle.speed;

    spindle_set(SPINDLE_OFF, 0);
    spindle.type = value;
    spindle_set(mode, speed);
  }
}
