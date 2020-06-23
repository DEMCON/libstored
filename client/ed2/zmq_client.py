# vim:et

import zmq
import time

from PySide2.QtCore import QObject, Signal, Slot, Property, QTimer, Qt
from .zmq_server import ZmqServer

class Object(QObject):
    valueChanged = Signal()
    valueUpdated = Signal()
    pollingChanged = Signal()

    def __init__(self, name, type, size, client = None):
        super().__init__()
        self._name = name
        self.type = type
        self.size = size
        self.client = client
        self._value = None
        self.t = None
        self._alias = None
        self._polling = False
        self._pollTimer = None

    @Property(str, constant=True)
    def name(self):
        return self._name

    @staticmethod
    def listResponseDecode(s, client):
        split = s.split('/', 1)
        if len(split) < 2:
            return None
        if len(split[0]) < 3:
            return None
        try:
            return Object('/' + split[1], int(split[0][0:2], 16), int(split[0][2:], 16), client)
        except ValueError:
            return None

    FlagSigned = 0x8
    FlagInt = 0x10
    FlagFixed = 0x20
    FlagFunction = 0x40

    Int8 = FlagFixed | FlagInt | FlagSigned | 0
    Uint8 = FlagFixed | FlagInt | 0
    Int16 = FlagFixed | FlagInt | FlagSigned | 1
    Uint16 = FlagFixed | FlagInt | 1
    Int32 = FlagFixed | FlagInt | FlagSigned | 3
    Uint32 = FlagFixed | FlagInt | 3
    Int64 = FlagFixed | FlagInt | FlagSigned | 7
    Uint64 = FlagFixed | FlagInt | 7

    Float = FlagFixed | FlagSigned | 3
    Double = FlagFixed | FlagSigned | 7
    Bool = FlagFixed | 0
    Pointer32 = FlagFixed | 3
    Pointer64 = FlagFixed | 7

    Void = 0
    Blob = 1
    String = 2

    Invalid = 0xff

    def isValidType(self):
        return self.type & 0x80 == 0

    def isFunction(self):
        return self.type & self.FlagFunction != 0

    def isFixed(self):
        return self.type & self.FlagFixed != 0

    def isInt(self):
        return self.isFixed() and self.type & self.FlagInt != 0

    def isSigned(self):
        return self.isFixed() and self.type & self.FlagSigned != 0

    def isSpecial(self):
        return self.type & 0x78 == 0

    @Property(str, constant=True)
    def typeName(self):
        t = {
                self.Int8: 'int8',
                self.Uint8: 'uint8',
                self.Int16: 'int16',
                self.Uint16: 'uint16',
                self.Int32: 'int32',
                self.Uint32: 'uint32',
                self.Int64: 'int64',
                self.Uint64: 'uint64',
                self.Float: 'float',
                self.Double: 'double',
                self.Pointer32: 'ptr32',
                self.Pointer64: 'ptr64',
                self.Bool: 'bool',
                self.Blob: 'blob',
                self.String: 'string',
                self.Void: 'void',
            }.get(self.type & ~self.FlagFunction, '?')
        return f'({t})' if self.isFunction() else t

    @property
    def alias(self):
        return self._alias

    @alias.setter
    def alias(self, a):
        if a == self._alias:
            return

        if a != None and not (isinstance(a, str) and len(a) == 1):
            raise ValueError('Invalid alias ' + a)

        if self.client != None:
            # Drop prevous alias
            if self._alias != None:
                self.client.req(b'a' + self._alias.encode())

            if a != None:
                rep = self.client.req(b'a' + a.encode() + self.name.encode())
                if rep != b'!':
                    # Failed setting, drop alias all together.
                    a = None

        self._alias = a

    # Return the alias or the normal name, if no alias was set.
    def shortName(self):
        if self._alias != None:
            return self._alias
        else:
            return self.name

    @staticmethod
    def sign_extend(value, bits):
        sign_bit = 1 << (bits - 1)
        return (value & (sign_bit - 1)) - (value & sign_bit)

    # Read value from server.
    @Slot(result='QVariant')
    def read(self):
        return self._read()

    def _read(self):
        value = self.__read()
        self.set(value)
        return value

    def __read(self):
        if self.client == None:
            return None
        
        rep = self.client.req(b'r' + self.shortName().encode())
        return self._decodeReadRep(rep)

    # Decode a read reply.
    def decodeReadRep(self, rep, t = None):
        value = self._decodeReadRep(rep)
        if value != None:
            self.set(value, t)

    def _decodeReadRep(self, rep):
        if rep == b'?':
            return None

        dtype = self.type & ~self.FlagFunction
        try:
            if self.isFixed():
                binint = int(rep.decode(), 16)
                if self.isInt() and not self.isSigned():
                    return binint
                elif dtype == self.Int8:
                    return self.sign_extend(binint, 8)
                elif dtype == self.Int16:
                    return self.sign_extend(binint, 16)
                elif dtype == self.Int32:
                    return self.sign_extend(binint, 32)
                elif dtype == self.Int64:
                    return self.sign_extend(binint, 64)
                elif dtype == self.Float:
                    return struct.unpack('<f', struct.pack('<I', dint))[0]
                elif dtype == self.Double:
                    return struct.unpack('<d', struct.pack('<Q', dint))[0]
                elif dtype == self.Bool:
                    return binint != 0
                elif dtype == self.Pointer32 or dtype == self.Pointer64:
                    return binint
                else:
                    return None
            elif dtype == self.Void:
                return b''
            elif dtype == self.Blob:
                return rep
            elif dtype == self.String:
                return rep.decode()
            else:
                return None
        except ValueError:
            return None

    # Write value to server.
    @Slot(object, result=bool)
    def write(self, value = None):
        if value != None:
            self.set(value)
        return self._write(value)

    def _write(self, value):
        if self.client == None:
            return False

        dtype = self.type & ~self.FlagFunction

        if dtype == self.Void:
            data = b''
        elif dtype == self.Blob:
            data = value
        elif dtype == self.String:
            data = value.encode()
        elif dtype == self.Pointer32:
            data = ('%x' % value).encode()
        elif dtype == self.Pointer64:
            data = ('%x' % value).encode()
        elif dtype == self.Bool:
            data = b'1' if value else b'0'
        elif dtype == self.Float:
            data = struct.pack('<f', value)
        elif dtype == self.Double:
            data = struct.pack('<d', value)
        elif not self.isInt():
            return False
        elif self.isSigned():
            data = ('%x' % value).encode()
        elif dtype == self.Int8:
            data = ('%2x' % value)[-2:].encode()
        elif dtype == self.Int16:
            data = ('%4x' % value)[-4:].encode()
        elif dtype == self.Int32:
            data = ('%8x' % value)[-8:].encode()
        elif dtype == self.Int64:
            data = ('%16x' % value)[-16:].encode()
        else:
            return False

        rep = self.client.req(b'w' + data + self.shortName().encode())
        return rep == b'!'

    # Locally set value, but do not actually write it to the server.
    def set(self, value, t = None):
        if t == None:
            t = time.time()

        self.t = t

        if value != self._value:
            self._value = value
            self.valueChanged.emit()

        self.valueUpdated.emit()

    def interpret(self, value):
        if isinstance(value,str):
            value = {
                self.Int8: lambda x: int(x,0),
                self.Uint8: lambda x: int(x,0),
                self.Int16: lambda x: int(x,0),
                self.Uint16: lambda x: int(x,0),
                self.Int32: lambda x: int(x,0),
                self.Uint32: lambda x: int(x,0),
                self.Int64: lambda x: int(x,0),
                self.Uint64: lambda x: int(x,0),
                self.Float: lambda x: float(x),
                self.Double: lambda x: float(x),
                self.Pointer32: lambda x: int(x,0),
                self.Pointer64: lambda x: int(x,0),
                self.Bool: lambda x: x.lower() in ['true', '1'],
                self.Blob: lambda x: x.encode(),
                self.String: lambda x: x,
                self.Void: lambda x: bytearray(),
            }.get(self.type & ~self.FlagFunction, lambda x: x)(value)
        return value

    # Returns the currently known value (without an actual read())
    @Property('QVariant', notify=valueChanged)
    def value(self):
        return self._value

    # Writes the value to the server.
    @value.setter
    def _value_set(self, v):
        try:
            return self.write(self.interpret(v))
        except ValueError:
            return False

    @Property(str, notify=valueChanged)
    def valueString(self):
        v = self.value
        if v == None:
            return ""
        else:
            return str(v)

    @valueString.setter
    def _valueString_set(self, v):
        return self._value_set(v)

    @Property(bool,notify=pollingChanged)
    def polling(self):
        return self._polling

    @polling.setter
    def _polling_set(self, enable):
        if self._polling != enable:
            self._polling = enable
            self.pollingChanged.emit()
            if enable:
                self.poll(self.client.defaultPollInterval)
            elif self._pollTimer != None:
                self._pollTimer.stop()

    def poll(self, interval):
        self.polling = True
        self.read()
        if self._pollTimer == None:
            self._pollTimer = QTimer(parent=self)
            self._pollTimer.timeout.connect(self._read)
        self._pollTimer.setInterval(interval * 1000)
        self._pollTimer.setSingleShot(False)
        if interval < 0.01:
            self._pollTimer.setTimerType(Qt.PreciseTimer)
        elif interval < 2:
            self._pollTimer.setTimerType(Qt.CoarseTimer)
        else:
            self._pollTimer.setTimerType(Qt.VeryCoarseTimer)

        self._pollTimer.start()


class ZmqClient(QObject):

    def __init__(self, address='localhost', port=ZmqServer.default_port):
        super().__init__()
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect(f'tcp://{address}:{port}')
        self._defaultPollInterval = 1
    
    def req(self, message):
        self.socket.send(message)
        return self.socket.recv()

    def timestampToTime(self, t = None):
        if t == None:
            return time.time()
        else:
            # Override to implement arbitrary conversion.
            return t

    def close(self):
        self.socket.close()

    @Slot(result=str)
    def capabilities(self):
        return self.req(b'?').decode()

    @Slot(result=str)
    def echo(self, s):
        return self.req(b'e' + s.encode()).decode()

    # Returns a list of Objects, registered to this client.
    def list(self):
        res = []
        for o in self.req(b'l').decode().split('\n'):
            obj = Object.listResponseDecode(o, self)
            if obj != None:
                res.append(obj)
        return res
    
    @Slot(result=str)
    def identification(self):
        return self.req(b'i').decode()

    @Slot(result=str)
    def version(self):
        return self.req(b'v').decode()

    def readMem(self, pointer, size):
        rep = self.req(('R%x %d' % (pointer, size)).encode()).decode()

        if rep == '?':
            return None

        if len(rep) & 1:
            # Odd number of bytes.
            return None

        res = bytearray()
        for i in range(0, len(rep), 2):
            res.append(int(rep[i:i+2], 16))
        return res

    def writeMem(self, pointer, data):
        req = 'W%x ' % pointer
        for i in range(0, len(data)):
            req += '%02x' % data[i]
        rep = self.req(req.encode())
        return rep == b'!'

    def streams(self):
        rep = self.req(b's')
        if rep == b'?':
            return []
        else:
            return list(map(lambda b: chr(b), rep))

    def stream(self, s, suffix=''):
        if not isinstance(s, str) or len(s) != 1:
            raise ValueError('Invalid stream name ' + s)
        return self.req(b's' + (s + suffix).encode()).decode()

    @Property(float)
    def defaultPollInterval(self):
        return self._defaultPollInterval

    @defaultPollInterval.setter
    def _defaultPollInterval_set(self, interval):
        self._defaultPollInterval = interval

