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


#include "config.h"
#include "status.h"

#include <stdint.h>
#include <stdbool.h>


void tmc2660_init();
uint8_t tmc2660_status(int driver);
void tmc2660_reset(int driver);
bool tmc2660_ready(int driver);
stat_t tmc2660_sync();
void tmc2660_enable(int driver);
void tmc2660_disable(int driver);


#define TMC2660_DRVCTRL             0
#define TMC2660_DRVCTRL_ADDR        (0UL << 18)
#define TMC2660_DRVCTRL_PHA         (1UL << 17)
#define TMC2660_DRVCTRL_CA(x)       (((int32_t)x & 0xff) << 9)
#define TMC2660_DRVCTRL_PHB         (1UL << 8)
#define TMC2660_DRVCTRL_CB(x)       (((int32_t)x & 0xff) << 0)
#define TMC2660_DRVCTRL_INTPOL      (1UL << 9)
#define TMC2660_DRVCTRL_DEDGE       (1UL << 8)
#define TMC2660_DRVCTRL_MRES_256    (0UL << 0)
#define TMC2660_DRVCTRL_MRES_128    (1UL << 0)
#define TMC2660_DRVCTRL_MRES_64     (2UL << 0)
#define TMC2660_DRVCTRL_MRES_32     (3UL << 0)
#define TMC2660_DRVCTRL_MRES_16     (4UL << 0)
#define TMC2660_DRVCTRL_MRES_8      (5UL << 0)
#define TMC2660_DRVCTRL_MRES_4      (6UL << 0)
#define TMC2660_DRVCTRL_MRES_2      (7UL << 0)
#define TMC2660_DRVCTRL_MRES_1      (8UL << 0)

#define TMC2660_CHOPCONF            1
#define TMC2660_CHOPCONF_ADDR       (4UL << 17)
#define TMC2660_CHOPCONF_TBL_16     (0UL << 15)
#define TMC2660_CHOPCONF_TBL_24     (1UL << 15)
#define TMC2660_CHOPCONF_TBL_36     (2UL << 15)
#define TMC2660_CHOPCONF_TBL_54     (3UL << 15)
#define TMC2660_CHOPCONF_CHM        (1UL << 14)
#define TMC2660_CHOPCONF_RNDTF      (1UL << 13)
#define TMC2660_CHOPCONF_FDM_COMP   (0UL << 12)
#define TMC2660_CHOPCONF_FDM_TIMER  (1UL << 12)
#define TMC2660_CHOPCONF_HDEC_16    (0UL << 11)
#define TMC2660_CHOPCONF_HDEC_32    (1UL << 11)
#define TMC2660_CHOPCONF_HDEC_48    (2UL << 11)
#define TMC2660_CHOPCONF_HDEC_64    (3UL << 11)
#define TMC2660_CHOPCONF_HEND(x)    ((((int32_t)x + 3) & 0xf) << 7)
#define TMC2660_CHOPCONF_SWO(x)     ((((int32_t)x + 3) & 0xf) << 7)
#define TMC2660_CHOPCONF_HSTART(x)  ((((int32_t)x - 1) & 7) << 4)
#define TMC2660_CHOPCONF_FASTD(x)   ((((int32_t)x & 8) << 11) | ((x & 7) << 4))
#define TMC2660_CHOPCONF_TOFF_TBL   (1 << 0)
#define TMC2660_CHOPCONF_TOFF(x)    (((int32_t)x & 0xf) << 0)

#define TMC2660_SMARTEN             2
#define TMC2660_SMARTEN_ADDR        (5UL << 17)
#define TMC2660_SMARTEN_SEIMIN      (1UL << 15)
#define TMC2660_SMARTEN_SEDN_32     (0UL << 13)
#define TMC2660_SMARTEN_SEDN_8      (1UL << 13)
#define TMC2660_SMARTEN_SEDN_2      (2UL << 13)
#define TMC2660_SMARTEN_SEDN_1      (3UL << 13)
#define TMC2660_SMARTEN_SEUP_1      (0UL << 5)
#define TMC2660_SMARTEN_SEUP_2      (1UL << 5)
#define TMC2660_SMARTEN_SEUP_4      (2UL << 5)
#define TMC2660_SMARTEN_SEUP_8      (3UL << 5)
#define TMC2660_SMARTEN_SE(MIN, MAX)                                    \
  (((uint32_t)(MIN / 32) & 0xf) |                                       \
   (((uint32_t)(MAX / 32 - MIN / 32 - 1) & 0xf) << 8))

#define TMC2660_SGCSCONF            3
#define TMC2660_SGCSCONF_ADDR       (6UL << 17)
#define TMC2660_SGCSCONF_SFILT      (1UL << 16)
#define TMC2660_SGCSCONF_THRESH(x)  (((int32_t)x & 0x7f) << 8)
#define TMC2660_SGCSCONF_CS(x)      (((int32_t)x & 0x1f) << 0)
#define TMC2660_SGCSCONF_CS_NONE    (31UL << 0)

#define TMC2660_DRVCONF             4
#define TMC2660_DRVCONF_ADDR        (7UL << 17)
#define TMC2660_DRVCONF_TST         (1UL << 16)
#define TMC2660_DRVCONF_SLPH_MIN    (0UL << 14)
#define TMC2660_DRVCONF_SLPH_MIN_TC (1UL << 14)
#define TMC2660_DRVCONF_SLPH_MED_TC (2UL << 14)
#define TMC2660_DRVCONF_SLPH_MAX    (3UL << 14)
#define TMC2660_DRVCONF_SLPL_MIN    (0UL << 12)
#define TMC2660_DRVCONF_SLPL_MED    (2UL << 12)
#define TMC2660_DRVCONF_SLPL_MAX    (3UL << 12)
#define TMC2660_DRVCONF_DISS2G      (1UL << 10)
#define TMC2660_DRVCONF_TS2G_3_2    (0UL << 8)
#define TMC2660_DRVCONF_TS2G_1_6    (1UL << 8)
#define TMC2660_DRVCONF_TS2G_1_2    (2UL << 8)
#define TMC2660_DRVCONF_TS2G_0_8    (3UL << 8)
#define TMC2660_DRVCONF_SDOFF       (1UL << 7)
#define TMC2660_DRVCONF_VSENSE      (1UL << 6)
#define TMC2660_DRVCONF_RDSEL_MSTEP (0UL << 4)
#define TMC2660_DRVCONF_RDSEL_SG    (1UL << 4)
#define TMC2660_DRVCONF_RDSEL_SGCS  (2UL << 4)

#define TMC2660_DRVSTATUS_STANDSTILL     (1UL << 7)
#define TMC2660_DRVSTATUS_OPEN_LOAD_B    (1UL << 6)
#define TMC2660_DRVSTATUS_OPEN_LOAD_A    (1UL << 5)
#define TMC2660_DRVSTATUS_SHORT_TO_GND_B (1UL << 4)
#define TMC2660_DRVSTATUS_SHORT_TO_GND_A (1UL << 3)
#define TMC2660_DRVSTATUS_OVERTEMP_WARN  (1UL << 2)
#define TMC2660_DRVSTATUS_OVERTEMP       (1UL << 1)
#define TMC2660_DRVSTATUS_STALLED        (1UL << 0)
