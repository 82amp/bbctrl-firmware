################################################################################
#                                                                              #
#                This file is part of the Buildbotics firmware.                #
#                                                                              #
#                  Copyright (c) 2015 - 2018, Buildbotics LLC                  #
#                             All rights reserved.                             #
#                                                                              #
#     This file ("the software") is free software: you can redistribute it     #
#     and/or modify it under the terms of the GNU General Public License,      #
#      version 2 as published by the Free Software Foundation. You should      #
#      have received a copy of the GNU General Public License, version 2       #
#     along with the software. If not, see <http://www.gnu.org/licenses/>.     #
#                                                                              #
#     The software is distributed in the hope that it will be useful, but      #
#          WITHOUT ANY WARRANTY; without even the implied warranty of          #
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       #
#               Lesser General Public License for more details.                #
#                                                                              #
#       You should have received a copy of the GNU Lesser General Public       #
#                License along with the software.  If not, see                 #
#                       <http://www.gnu.org/licenses/>.                        #
#                                                                              #
#                For information regarding this software email:                #
#                  "Joseph Coffland" <joseph@buildbotics.com>                  #
#                                                                              #
################################################################################

import logging
from tornado.ioloop import PeriodicCallback

import bbctrl

log = logging.getLogger('PWR')


# Must match regs in pwr firmware
TEMP_REG        = 0
VIN_REG         = 1
VOUT_REG        = 2
MOTOR_REG       = 3
LOAD1_REG       = 4
LOAD2_REG       = 5
VDD_REG         = 6
FLAGS_REG       = 7

# Must be kept in sync with pwr firmware
UNDER_VOLTAGE_FLAG  = 1 << 0
OVER_VOLTAGE_FLAG   = 1 << 1
OVER_CURRENT_FLAG   = 1 << 2
SENSE_ERROR_FLAG    = 1 << 3
SHUNT_OVERLOAD_FLAG = 1 << 4
MOTOR_OVERLOAD_FLAG = 1 << 5
LOAD1_SHUTDOWN_FLAG = 1 << 6
LOAD2_SHUTDOWN_FLAG = 1 << 7

reg_names = 'temp vin vout motor load1 load2 vdd pwr_flags'.split()


class Pwr():
    def __init__(self, ctrl):
        self.ctrl = ctrl

        self.i2c_addr = ctrl.args.pwr_addr
        self.regs = [-1] * 8
        self.lcd_page = ctrl.lcd.add_new_page()
        self.failures = 0

        PeriodicCallback(self._update, 1000, ctrl.ioloop).start()


    def get_reg(self, i): return self.regs[i]


    def check_fault(self, var, status):
        status = bool(status)

        if not self.ctrl.state.has(var) or status != self.ctrl.state.get(var):
            self.ctrl.state.set(var, status)
            if status: return True

        return False


    def check_faults(self):
        flags = self.regs[FLAGS_REG]

        if self.check_fault('under_voltage', flags & UNDER_VOLTAGE_FLAG):
            log.error('Device under voltage')

        if self.check_fault('over_voltage', flags & OVER_VOLTAGE_FLAG):
            log.error('Device over voltage')

        if self.check_fault('over_current', flags & OVER_CURRENT_FLAG):
            log.error('Device total current limit exceeded')

        if self.check_fault('sense_error', flags & SENSE_ERROR_FLAG):
            log.error('Power sense error')

        if self.check_fault('shunt_overload', flags & SHUNT_OVERLOAD_FLAG):
            log.error('Power shunt overload')

        if self.check_fault('motor_overload', flags & MOTOR_OVERLOAD_FLAG):
            log.error('Motor power overload')

        if self.check_fault('load1_shutdown', flags & LOAD1_SHUTDOWN_FLAG):
            log.error('Load 1 over temperature shutdown')

        if self.check_fault('load2_shutdown', flags & LOAD2_SHUTDOWN_FLAG):
            log.error('Load 2 over temperature shutdown')


    def _update(self):
        update = {}

        try:
            for i in range(len(self.regs)):
                value = self.ctrl.i2c.read_word(self.i2c_addr + i)

                if i == TEMP_REG: value -= 273
                elif i == FLAGS_REG: pass
                else: value /= 100.0

                key = reg_names[i]
                self.ctrl.state.set(key, value)

                if self.regs[i] != value:
                    update[key] = value
                    self.regs[i] = value

                if i == FLAGS_REG: self.check_faults()

        except Exception as e:
            if i < 6: # Older pwr firmware does not have regs > 5
                self.failures += 1
                msg = 'Pwr communication failed: %s' % e
                if self.failures != 5: log.info(msg)
                else:
                    log.warning(msg)
                    self.failures = 0
                return

        self.lcd_page.text('%3dC   Tmp' % self.regs[TEMP_REG],  0, 0)
        self.lcd_page.text('%5.1fV  In' % self.regs[VIN_REG],   0, 1)
        self.lcd_page.text('%5.1fV Out' % self.regs[VOUT_REG],  0, 2)
        self.lcd_page.text(' %04x  Flg' % self.regs[FLAGS_REG], 0, 3)

        self.lcd_page.text('%5.1fA Mot' % self.regs[MOTOR_REG], 10, 0)
        self.lcd_page.text('%5.1fA Ld1' % self.regs[LOAD1_REG], 10, 1)
        self.lcd_page.text('%5.1fA Ld2' % self.regs[LOAD2_REG], 10, 2)
        self.lcd_page.text('%5.1fA Vdd' % self.regs[VDD_REG],   10, 3)

        if len(update): self.ctrl.state.update(update)

        self.failures = 0
