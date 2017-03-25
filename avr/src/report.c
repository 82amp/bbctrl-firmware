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

#include "report.h"
#include "config.h"
#include "usart.h"
#include "rtc.h"
#include "vars.h"
#include "pgmspace.h"

#include <stdio.h>
#include <stdbool.h>


static bool report_requested = false;
static bool report_full = false;
static uint32_t last_report = 0;


void report_request() {
  report_requested = true;
}


void report_request_full() {
  report_requested = report_full = true;
}


void report_callback() {
  if (usart_tx_full()) return; // Wait for buffer space

  if (report_requested && usart_tx_empty()) {
    uint32_t now = rtc_get_time();
    if (now - last_report < 100) return;
    last_report = now;

    vars_report(report_full);
    report_requested = report_full = false;
  }
}
