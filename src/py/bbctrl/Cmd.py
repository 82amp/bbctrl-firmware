#!/usr/bin/env python3

import struct
import base64
import json
import logging

log = logging.getLogger('Cmd')

# TODO, sync this up with AVR code
SET      = '$'
SET_SYNC = '#'
SEEK     = 's'
LINE     = 'l'
REPORT   = 'r'
PAUSE    = 'P'
UNPAUSE  = 'U'
ESTOP    = 'E'
CLEAR    = 'C'
FLUSH    = 'F'
STEP     = 'S'
RESUME   = 'c'

SEEK_ACTIVE = 1 << 0
SEEK_ERROR  = 1 << 1


def encode_float(x):
    return base64.b64encode(struct.pack('<f', x))[:-2].decode("utf-8")


def decode_float(s):
    return struct.unpack('<f', base64.b64decode(s + '=='))[0]


def encode_axes(axes):
    data = ''
    for axis in 'xyzabc':
        if axis in axes:
            data += axis + encode_float(axes[axis])

    return data


def seek(switch, open, error):
    flags = (SEEK_OPEN if open else 0) | (SEEK_ERROR if error else 0)
    return '%c%x%x' % (SEEK, switch, flags)


def line_number(line): return '#ln=%d' % line


def line(id, target, exitVel, maxAccel, maxJerk, times):
    cmd = '#id=%u\n%c' % (id, LINE)

    cmd += encode_float(exitVel)
    cmd += encode_float(maxAccel)
    cmd += encode_float(maxJerk)
    cmd += encode_axes(target)

    # S-Curve time parameters
    for i in range(7):
        if times[i]:
            cmd += str(i) + encode_float(times[i] / 60000) # to mins

    return cmd


def tool(tool): return '#t=%d' % tool
def speed(speed): return '#s=:' + encode_float(speed)
def set_position(axis, value): return '#%sp=:%s' % (axis, encode_float(value))

def output(port, value):
    if port == 'mist':  return '#1oa=' + ('1' if value else '0')
    if port == 'flood': return '#2oa=' + ('1' if value else '0')
    raise Exception('Unsupported output "%s"' % port)


def dwell(seconds): return 'd' + encode_float(seconds)
def pause(optional = False): 'P' + ('1' if optional else '0')
def jog(axes): return 'j' + encode_axes(axes)


def seek(switch, active, error):
    cmd = SEEK

    if switch == 'probe': cmd += '1'
    elif switch == 'x-min': cmd += '2'
    elif switch == 'x-max': cmd += '3'
    elif switch == 'y-min': cmd += '4'
    elif switch == 'y-max': cmd += '5'
    elif switch == 'z-min': cmd += '6'
    elif switch == 'z-max': cmd += '7'
    elif switch == 'a-min': cmd += '8'
    elif switch == 'a-max': cmd += '9'
    else: raise Exception('Unsupported switch "%s"' % switch)

    flags = 0
    if active: flags |= SEEK_ACTIVE
    if error:  flags |= SEEK_ERROR
    cmd += chr(flags + ord('0'))

    return cmd


def decode_command(cmd):
    if not len(cmd): return

    data = {}

    if cmd[0] == SET or cmd[0] == SET_SYNC:
        data['type'] = 'set'
        if cmd[0] == SET_SYNC: data['sync'] = True

        equal = cmd.find('=')
        data['name'] = cmd[1:equal]

        value = cmd[equal + 1:]

        if value.lower() == 'true': value = True
        elif value.lower() == 'false': value = False
        elif value.find('.') == -1: data['value'] = int(value)
        else: data['value'] = float(value)

    elif cmd[0] == SEEK:
        data['type'] = 'seek'

        data['port'] = int(cmd[2], 16)
        flags = int(cmd[2], 16)

        data['active'] = bool(flags & SEEK_ACTIVE)
        data['error'] = bool(flags & SEEK_ERROR)

    elif cmd[0] == LINE:
        data['type'] = 'line'
        data['exit-vel']  = decode_float(cmd[1:7])
        data['max-accel'] = decode_float(cmd[7:13])
        data['max-jerk']  = decode_float(cmd[13:19])

        data['target'] = {}
        data['times'] = [0] * 7
        cmd = cmd[19:]

        while len(cmd):
            name = cmd[0]
            value = decode_float(cmd[1:7])
            cmd = cmd[7:]

            if name in 'xyzabcuvw': data['target'][name] = value
            else: data['times'][int(name)] = value

    elif cmd[0] == REPORT:  data['type'] = 'report'
    elif cmd[0] == PAUSE:   data['type'] = 'pause'
    elif cmd[0] == UNPAUSE: data['type'] = 'unpause'
    elif cmd[0] == ESTOP:   data['type'] = 'estop'
    elif cmd[0] == CLEAR:   data['type'] = 'clear'
    elif cmd[0] == FLUSH:   data['type'] = 'flush'
    elif cmd[0] == STEP:    data['type'] = 'step'
    elif cmd[0] == RESUME:  data['type'] = 'resume'

    print(json.dumps(data))


def decode(cmd):
    for line in cmd.split('\n'):
        decode_command(line.strip())


if __name__ == "__main__":
    import sys

    if 1 < len(sys.argv):
        for arg in sys.argv[1:]:
            decode(arg)

    else:
        for line in sys.stdin:
            decode(line)
