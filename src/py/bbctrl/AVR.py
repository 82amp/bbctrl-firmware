import re
import serial
import json
import time
import logging
import traceback
from collections import deque

import bbctrl
import bbctrl.Cmd as Cmd

log = logging.getLogger('AVR')


# Axis homing procedure:
#
#   Mark axis unhomed
#   Seek closed (home_dir * (travel_max - travel_min) * 1.5) at search_velocity
#   Seek open (home_dir * -latch_backoff) at latch_vel
#   Seek closed (home_dir * latch_backoff * 1.5) at latch_vel
#   Rapid to (home_dir * -zero_backoff + position)
#   Mark axis homed and set absolute position

axis_homing_procedure = '''
  G28.2 %(axis)s0 F[#<_%(axis)s_sv>]
  G38.6 %(axis)s[#<_%(axis)s_hd> * [#<_%(axis)s_tm> - #<_%(axis)s_tn>] * 1.5]
  G38.8 %(axis)s[#<_%(axis)s_hd> * -#<_%(axis)s_lb>] F[#<_%(axis)s_lv>]
  G38.6 %(axis)s[#<_%(axis)s_hd> * #<_%(axis)s_lb> * 1.5]
  G91 G0 G53 %(axis)s[#<_%(axis)s_hd> * -#<_%(axis)s_zb>]
  G90 G28.3 %(axis)s[#<_%(axis)s_hp>]
'''



class AVR():
    def __init__(self, ctrl):
        self.ctrl = ctrl

        self.queue = deque()
        self.in_buf = ''
        self.command = None

        self.lcd_page = ctrl.lcd.add_new_page()
        self.install_page = True

        ctrl.state.add_listener(lambda x: self._update_state(x))

        try:
            self.sp = serial.Serial(ctrl.args.serial, ctrl.args.baud,
                                    rtscts = 1, timeout = 0, write_timeout = 0)
            self.sp.nonblocking()

        except Exception as e:
            self.sp = None
            log.warning('Failed to open serial port: %s', e)

        if self.sp is not None:
            ctrl.ioloop.add_handler(self.sp, self._serial_handler,
                                    ctrl.ioloop.READ)

        self.i2c_addr = ctrl.args.avr_addr


    def _is_busy(self): return self.ctrl.planner.is_running()


    def _i2c_command(self, cmd, byte = None, word = None):
        log.info('I2C: ' + cmd)
        retry = 5
        cmd = ord(cmd[0])

        while True:
            try:
                self.ctrl.i2c.write(self.i2c_addr, cmd, byte, word)
                break

            except Exception as e:
                retry -= 1

                if retry:
                    log.error('AVR I2C communication failed, retrying: %s' % e)
                    time.sleep(0.1)
                    continue

                else:
                    log.error('AVR I2C communication failed: %s' % e)
                    raise


    def _start_sending_gcode(self, path):
        self.ctrl.planner.load(path)
        self._set_write(True)


    def _stop_sending_gcode(self): self.ctrl.planner.reset()


    def _set_write(self, enable):
        if self.sp is None: return

        flags = self.ctrl.ioloop.READ
        if enable: flags |= self.ctrl.ioloop.WRITE
        self.ctrl.ioloop.update_handler(self.sp, flags)


    def _load_next_command(self, cmd):
        log.info('< ' + json.dumps(cmd).strip('"'))
        self.command = bytes(cmd.strip() + '\n', 'utf-8')


    def _queue_command(self, cmd):
        self.queue.append(cmd)
        self._set_write(True)


    def _serial_handler(self, fd, events):
        try:
            if self.ctrl.ioloop.READ & events: self._serial_read()
            if self.ctrl.ioloop.WRITE & events: self._serial_write()
        except Exception as e:
            log.error('Serial handler error: %s', traceback.format_exc())


    def _serial_write(self):
        # Finish writing current command
        if self.command is not None:
            try:
                count = self.sp.write(self.command)

            except Exception as e:
                self._set_write(False)
                raise e

            self.command = self.command[count:]
            if len(self.command): return # There's more
            self.command = None

        # Load next command from queue
        if len(self.queue): self._load_next_command(self.queue.popleft())

        # Load next GCode command, if running or paused
        elif self._is_busy():
            cmd = self.ctrl.planner.next()

            if cmd is None: self._set_write(False)
            else: self._load_next_command(cmd)

        # Else stop writing
        else: self._set_write(False)


    def _serial_read(self):
        try:
            data = self.sp.read(self.sp.in_waiting)
            self.in_buf += data.decode('utf-8')

        except Exception as e:
            log.warning('%s: %s', e, data)

        update = {}

        # Parse incoming serial data into lines
        while True:
            i = self.in_buf.find('\n')
            if i == -1: break
            line = self.in_buf[0:i].strip()
            self.in_buf = self.in_buf[i + 1:]

            if line:
                log.info('> ' + line)

                try:
                    msg = json.loads(line)

                except Exception as e:
                    log.error('%s, data: %s', e, line)
                    continue

                if 'variables' in msg:
                    try:
                        self.ctrl.state.machine_cmds_and_vars(msg)
                        self._queue_command('D') # Refresh all vars

                    except Exception as e:
                        log.warning('AVR reload failed: %s',
                                    traceback.format_exc())
                        self.ctrl.ioloop.call_later(1, self.connect)

                    continue

                update.update(msg)

        if update:
            if 'firmware' in update:
                log.error('AVR rebooted')
                self.connect()

            self.ctrl.state.update(update)

            # Must be after AVR vars have loaded
            if self.install_page:
                self.install_page = False
                self.ctrl.lcd.set_current_page(self.lcd_page.id)


    def _update_state(self, update):
        if 'x' in update and update['x'] == 'ESTOPPED':
            self._stop_sending_gcode()

        self._update_lcd(update)


    def _update_lcd(self, update):
        if 'x' in update:
            self.lcd_page.text('%-9s' % self.ctrl.state.get('x'), 0, 0)

        # Show enabled axes
        row = 0
        for axis in 'xyzabc':
            motor = self.ctrl.state.find_motor(axis)
            if motor is not None:
                if (axis + 'p') in update:
                    self.lcd_page.text('% 10.3f%s' % (
                            update[axis + 'p'], axis.upper()), 9, row)

                row += 1

        # Show tool, units, feed and speed
        # TODO Units not in state
        if 't' in update: self.lcd_page.text('%2uT' % update['t'], 6, 1)
        if 'u' in update: self.lcd_page.text('%-6s' % update['u'], 0, 1)
        if 'f' in update: self.lcd_page.text('%8uF' % update['f'], 0, 2)
        if 's' in update: self.lcd_page.text('%8dS' % update['s'], 0, 3)


    def connect(self):
        try:
            # Reset AVR communication
            self.stop();
            self._queue_command('h') # Load AVR commands and variables

        except Exception as e:
            log.warning('Connect failed: %s', e)
            self.ctrl.ioloop.call_later(1, self.connect)


    def set(self, code, value):
        self._queue_command('${}={}'.format(code, value))


    def mdi(self, cmd):
        if len(cmd) and cmd[0] == '$':
            equal = cmd.find('=')
            if equal == -1:
                log.info('%s=%s' % (cmd, self.ctrl.state.get(cmd[1:])))

            else:
                name, value = cmd[1:equal], cmd[equal + 1:]

                if value.lower() == 'true': value = True
                elif value.lower() == 'false': value = False
                else:
                    try:
                        value = float(value)
                    except: pass

                self.ctrl.state.config(name, value)

        elif len(cmd) and cmd[0] == '\\': self._queue_command(cmd[1:])

        else:
            self.ctrl.planner.mdi(cmd)
            self._set_write(True)


    def jog(self, axes):
        if self._is_busy(): raise Exception('Busy, cannot jog')

        _axes = {}
        for i in range(len(axes)): _axes["xyzabc"[i]] = axes[i]

        self._queue_command(Cmd.jog(_axes))


    def home(self, axis, position = None):
        if self._is_busy(): raise Exception('Busy, cannot home')

        if position is not None:
            self.ctrl.planner.mdi('G28.3 %c%f' % (axis, position))

        else:
            if axis is None: axes = 'zxyabc' # TODO This should be configurable
            else: axes = '%c' % axis

            for axis in axes:
                if not self.ctrl.state.axis_can_home(axis): continue

                log.info('Homing %s axis' % axis)
                gcode = axis_homing_procedure % {'axis': axis}
                self.ctrl.planner.mdi(gcode)
                self._set_write(True)


    def estop(self): self._i2c_command(Cmd.ESTOP)
    def clear(self): self._i2c_command(Cmd.CLEAR)


    def start(self, path):
        if path: self._start_sending_gcode(path)


    def step(self, path):
        self._i2c_command(Cmd.STEP)
        if not self._is_busy() and path and \
                self.ctrl.state.get('x', '') == 'READY':
            self._start_sending_gcode(path)


    def stop(self):
        self._i2c_command(Cmd.FLUSH)
        self._stop_sending_gcode()
        # Resume processing once current queue of GCode commands has flushed
        self._queue_command(Cmd.RESUME)


    def pause(self): self._i2c_command(Cmd.PAUSE, byte = 0)


    def unpause(self):
        if self.ctrl.state.get('x', '') != 'HOLDING' or not self._is_busy():
            return

        self._i2c_command(Cmd.FLUSH)
        self._queue_command(Cmd.RESUME)
        self.ctrl.planner.restart()
        self._set_write(True)
        self._i2c_command(Cmd.UNPAUSE)


    def optional_pause(self): self._i2c_command(Cmd.PAUSE, byte = 1)


    def set_position(self, axis, position):
        if self._is_busy(): raise Exception('Busy, cannot set position')

        if self.ctrl.state.is_axis_homed('%c' % axis):
            self.ctrl.planner.mdi('G92 %c%f' % (axis, position))
        else: self._queue_command('$%cp=%f' % (axis, position))
