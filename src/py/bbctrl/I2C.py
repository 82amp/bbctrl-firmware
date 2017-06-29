import logging

try:
    import smbus
except:
    import smbus2 as smbus

log = logging.getLogger('I2C')


class I2C(object):
    def __init__(self, port):
        self.port = port
        self.i2c_bus = None


    def connect(self):
        if self.i2c_bus is None:
            try:
                self.i2c_bus = smbus.SMBus(self.port)

            except OSError as e:
                self.i2c_bus = None
                log.warning('Failed to open device: %s', e)
                raise e


    def read_word(self, addr):
        self.connect()

        try:
            return self.i2c_bus.read_word_data(addr, 0)

        except IOError as e:
            log.warning('I2C read word failed: %s' % e)
            self.i2c_bus.close()
            self.i2c_bus = None
            raise e


    def write(self, addr, cmd, byte = None, word = None):
        self.connect()

        try:
            if byte is not None:
                self.i2c_bus.write_byte_data(addr, cmd, byte)

            elif word is not None:
                self.i2c_bus.write_word_data(addr, cmd, word)

            else: self.i2c_bus.write_byte(addr, cmd)

        except IOError as e:
            log.warning('I2C write failed: %s' % e)
            self.i2c_bus.close()
            self.i2c_bus = None
            raise e
