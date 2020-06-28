# vim:et

# libstored, a Store for Embedded Debugger.
# Copyright (C) 2020  Jochem Rutgers
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import zmq
import time
import datetime
import struct

from PySide2.QtCore import QObject, Signal, Slot, Property, QTimer, Qt
from .zmq_server import ZmqServer

##
# \brief A variable or function as handled by a ZmqClient
#
# Do not instantiate directly, but as a ZmqClient for its objects.
#
# \ingroup libstored_client
class Object(QObject):
    valueChanged = Signal()
    valueUpdated = Signal()
    pollingChanged = Signal()
    aliasChanged = Signal()
    formatChanged = Signal()
    tUpdated = Signal()

    def __init__(self, name, type, size, client=None):
        super().__init__(parent=client)
        self._name = name
        self._type = type
        self._size = size
        self._client = client
        self._value = None
        self._t = None
        self._alias = None
        self._polling = False
        self._pollTimer = None
        self._format = None
        self._format_set(self.formats[0])

    @property
    def type(self):
        return self._type

    @property
    def size(self):
        return self._size

    @property
    def client(self):
        return self._client

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
        return self._type & 0x80 == 0

    def isFunction(self):
        return self._type & self.FlagFunction != 0

    def isFixed(self):
        return self._type & self.FlagFixed != 0

    def isInt(self):
        return self.isFixed() and self._type & self.FlagInt != 0

    def isSigned(self):
        return self.isFixed() and self._type & self.FlagSigned != 0

    def isSpecial(self):
        return self._type & 0x78 == 0

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
            }.get(self._type & ~self.FlagFunction, '?')
        return f'({t})' if self.isFunction() else t

    @Property(str, notify=aliasChanged)
    def alias(self):
        return self._alias

    def _alias_set(self, a):
        if a == self._alias:
            return

        self._alias = a
        self.aliasChanged.emit()

    # Return the alias or the normal name, if no alias was set.
    def shortName(self):
        if self._alias != None:
            return self._alias

        if self._client != None:
            # We don't have an alias, try to get one.
            self._client.acquireAlias(self)

        if self._alias != None:
            # We may have got one. Use it.
            return self._alias

        # Still not alias, return name instead.
        return self._name

    @staticmethod
    def sign_extend(value, bits):
        sign_bit = 1 << (bits - 1)
        return (value & (sign_bit - 1)) - (value & sign_bit)

    # Read value from server.
    @Slot(result='QVariant')
    def read(self):
        return self._read()

    def _read(self):
        if self._client == None:
            return None
        
        rep = self._client.req(b'r' + self.shortName().encode())
        return self.decodeReadRep(rep)

    # Decode a read reply.
    def decodeReadRep(self, rep, t = None):
        if rep == b'?':
            return None
        value = self._decode(rep)
        if value != None:
            self.set(value, t)
        return value

    def _decodeHex(self, data):
        data = data.decode()
        if len(data) % 2 == 1:
            data = '0' + data
        res = bytearray()
        for i in range(0, len(data), 2):
            res.append(int(data[i:i+2], 16))
        return res

    def _decode(self, rep):
        dtype = self._type & ~self.FlagFunction
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
                    return struct.unpack('<f', struct.pack('<I', binint))[0]
                elif dtype == self.Double:
                    return struct.unpack('<d', struct.pack('<Q', binint))[0]
                elif dtype == self.Bool:
                    return binint != 0
                elif dtype == self.Pointer32 or dtype == self.Pointer64:
                    return binint
                else:
                    return None
            elif dtype == self.Void:
                return b''
            elif dtype == self.Blob:
                return self._decodeHex(rep)
            elif dtype == self.String:
                return self._decodeHex(rep.partition(b'0')[0]).decode()
            else:
                return None
        except:
            return None

    # Write value to server.
    @Slot(object, result=bool)
    def write(self, value = None):
        if value != None:
            self.set(value)
        return self._write(value)

    def _write(self, value):
        if self._client == None:
            return False

        dtype = self.type & ~self.FlagFunction

        data = self._encode(value)
        if data == None:
            return False

        rep = self._client.req(b'w' + data + self.shortName().encode())
        return rep == b'!'
    

    def _encodeHex(self, data, zerostrip = False):
        s = ''.join(['%02x' % b for b in data])
        if zerostrip:
            s = s.lstrip('0')
            if s == '':
                s = '0'
        return s.encode()

    def _encode(self, value):
        dtype = self._type & ~self.FlagFunction

        try:
            if dtype == self.Void:
                return b''
            elif dtype == self.Blob:
                return self._encodeHex(value)
            elif dtype == self.String:
                return self._encodeHex(value.encode()) + b'00'
            elif dtype == self.Pointer32:
                return ('%x' % value).encode()
            elif dtype == self.Pointer64:
                return ('%x' % value).encode()
            elif dtype == self.Bool:
                return b'1' if value else b'0'
            elif dtype == self.Float:
                return self._encodeHex(struct.pack('>f', value))
            elif dtype == self.Double:
                return self._encodeHex(struct.pack('>d', value))
            elif not self.isInt():
                return None
            elif self.isSigned():
                return self._encodeHex(struct.pack('>q', value)[-self._size:], True)
            else:
                return self._encodeHex(struct.pack('>Q', value)[-self._size:], True)
        except:
            return None

    # Locally set value, but do not actually write it to the server.
    def set(self, value, t = None):
        if t == None:
            t = time.time()

        self._t = t
        self.tUpdated.emit()

        if value != self._value:
            self._value = value
            self.valueChanged.emit()

        self.valueUpdated.emit()

    @Property(float, notify=tUpdated)
    def t(self):
        return self._t

    @Property(str, notify=tUpdated)
    def tString(self):
        if self._t == None:
            return None
        else:
            return datetime.datetime.fromtimestamp(self._t).strftime('%Y-%m-%d %H:%M:%S.%f')

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
            }.get(self._type & ~self.FlagFunction, lambda x: x)(value)
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
        v = self._value
        if v == None:
            return ""
        else:
            try:
                return self._formatter(v)
            except:
                return "?"

    @valueString.setter
    def _valueString_set(self, v):
        return self._value_set(v)

    @Property(str, notify=formatChanged)
    def format(self):
        return self._format

    @format.setter
    def _format_set(self, f):
        if self._format == f:
            return

        self._format = f

        if f == 'hex':
            self._formatter = hex
        elif f == 'bin':
            self._formatter = bin
        elif f == 'bytes':
            self._formatter = self._formatBytes
        else:
            self._formatter = str

        self.formatChanged.emit()
        self.valueChanged.emit()

    def _formatBytes(self, value):
        value = self._encode(value).decode()
        value = '0' * (self._size * 2 - len(value)) + value
        res = ''
        for i in range(0, len(value), 2):
            if res != []:
                res += ' '
            res += value[i:i+2]
        return res

    @Property('QVariant', constant=True)
    def formats(self):
        if self._type == self.Blob:
            return ['bytes']

        f = ['default', 'bytes']
        if self.isFixed():
            f += ['hex', 'bin']
        return f

    @Property(bool, notify=pollingChanged)
    def polling(self):
        return self._polling

    @polling.setter
    def _polling_set(self, enable):
        if self._polling != enable:
            if enable:
                self.poll()
            else:
                self.poll(None)

    def poll(self, interval_s=0):
        if self._client != None:
            self._client.poll(self, interval_s)

    def _pollStop(self):
        self._pollSetFlag(False)
        if self._pollTimer != None:
            self._pollTimer.stop()

    def _pollSlow(self, interval_s):
        self._pollSetFlag(True)

        self._read()

        if self._pollTimer == None:
            self._pollTimer = QTimer(parent=self)
            self._pollTimer.timeout.connect(self._read)
            self._pollTimer.setSingleShot(False)

        self._pollTimer.setInterval(interval_s * 1000)
        if interval_s < 2:
            self._pollTimer.setTimerType(Qt.CoarseTimer)
        else:
            self._pollTimer.setTimerType(Qt.VeryCoarseTimer)

        self._pollTimer.start()

    def _pollFast(self, interval_s):
        self._pollSetFlag(True)
        if self._pollTimer != None:
            self._pollTimer.stop()

    def _pollSetFlag(self, enable):
        if self._polling != enable:
            self._polling = enable
            self.pollingChanged.emit()

##
# \brief Macro object as returned by ZmqClient.acquireMacro()
#
# Do not instantiate directly, but let ZmqClient acquire one for you.
#
# \ingroup libstored_client
class Macro(object):
    def __init__(self, client, reqsep=b'\n', repsep=b' '):
        self._client = client

        self._macro = client.acquireMacro()
        if self._macro != None:
            self._macro = self._macro.encode()

        self._cmds = {}

        if isinstance(reqsep, str):
            reqsep = reqsep.encode()
        self._reqsep = reqsep

        if isinstance(repsep, str):
            repsep = repsep.encode()
        self._repsep = repsep

    @property
    def macro(self):
        return self._macro

    @property
    def client(self):
        return self._client

    def add(self, cmd, cb, key):
        if isinstance(cmd, str):
            cmd = cmd.encode()
        self._cmds[key] = (cmd, cb)
        self._update()

        # Check if it still works...
        if self.run():
            # Success.
            return True

        # Rollback.
        self.remove(key)
        return False

    def remove(self, key):
        if key in self._cmds:
            del self._cmds[key]
            self._update()

    def _update(self):
        if self._macro == None:
            return

        cmds = []
        for c in self._cmds.values():
            if cmds != []:
                cmds.append(b'e' + self._repsep)
            cmds.append(c[0])

        self._client.assignMacro(self._macro, cmds, self._reqsep)

    def run(self):
        if self._macro != None:
            rep = self._client.req(self._macro)
            cb = list(self._cmds.values())
            values = rep.split(self._repsep)
            if len(cb) != len(values):
                return False

            for i in range(0, len(values)):
                cb[i][1](values[i])
        else:
            for c in self._cmds.values():
                c[1](self._client.req(c[0]))

        return True

    def __len__(self):
        return len(self._cmds)


##
# \brief A ZMQ client.
#
# This client can connect to either the ed2.zmq_server.ZmqServer and stored::ZmqLayer.
#
# Instantiate as ed2.ZmqClient().
#
# \ingroup libstored_client
class ZmqClient(QObject):
    
    fastPollThreshold_s = 0.9
    slowPollInterval_s = 2.0
    defaultPollIntervalChanged = Signal()

    def __init__(self, address='localhost', port=ZmqServer.default_port, parent=None):
        super().__init__(parent=parent)
        self._context = zmq.Context()
        self._socket = self._context.socket(zmq.REQ)
        self._socket.connect(f'tcp://{address}:{port}')
        self._defaultPollInterval = 1
        self._availableAliases = None
        self._temporaryAliases = {}
        self._permanentAliases = {}
        self._availableMacros = None
        self._usedMacros = []
        self._objects = None
        self._fastPollMacro = None
        self._fastPollTimer = None
    
    @property
    def context(self):
        return self._context

    @property
    def objects(self):
        return list(self._objects)

    @property
    def socket(self):
        return self._socket

    @Slot(str,result=str)
    def req(self, message):
        if isinstance(message,str):
            return self.req(message.encode()).decode()
#        print(message)

        self._socket.send(message)
        return b''.join(self._socket.recv_multipart())

    def timestampToTime(self, t = None):
        if t == None:
            return time.time()
        else:
            # Override to implement arbitrary conversion.
            return t

    def close(self):
        self._socket.close()

    @Slot(result=str)
    def capabilities(self):
        return self.req(b'?').decode()

    @Slot(result=str)
    def echo(self, s):
        return self.req(b'e' + s.encode()).decode()

    # Returns a list of Objects, registered to this client.
    def list(self):
        self._list_init()
        return self.objects

    def _list_init(self):
        if self._objects != None:
            return

        res = []
        for o in self.req(b'l').decode().split('\n'):
            obj = Object.listResponseDecode(o, self)
            if obj != None:
                res.append(obj)

        self._objects = res

    def find(self, name):
        chunks = name.split('/')
        obj = []
        self._list_init()
        for o in self._objects:
            ochunks = o.name.split('/')
            if len(chunks) != len(ochunks):
                continue
            match = True
            for i in range(0, len(ochunks)):
                if not (chunks[i].startswith(ochunks[i]) or ochunks[i].startswith(chunks[i])):
                    match = False
            if match:
                obj.append(o)

        if obj == []:
            return None
        elif len(obj) == 1:
            return obj[0]
        else:
            return obj
    
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

    @Property(float, notify=defaultPollIntervalChanged)
    def defaultPollInterval(self):
        return self._defaultPollInterval

    @defaultPollInterval.setter
    def _defaultPollInterval_set(self, interval):
        interval = max(0.0001, float(interval))
        if interval != self._defaultPollInterval:
            self._defaultPollInterval = interval
            self.defaultPollIntervalChanged.emit()

    def acquireAlias(self, obj, prefer=None, temporary=True):
        if prefer == None and obj.alias != None:
            if temporary != self._isTemporaryAlias(obj.alias):
                # Switch type
                return self._reassignAlias(obj.alias, obj, temporary)
            else:
                # Already assigned one.
                return obj.alias
        if prefer != None:
            if obj.alias != prefer:
                # Go assign one.
                pass
            elif temporary != self._isTemporaryAlias(prefer):
                # Switch type
                return self._reassignAlias(prefer, obj, temporary)
            else:
                # Already assigned preferred one.
                return prefer

        if self._availableAliases == None:
            # Not yet initialized
            if 'a' in self.capabilities():
                self._availableAliases = list(map(chr, range(0x20, 0x7f)))
                self._availableAliases.remove('/')
            else:
                self._availableAliases = []
        
        if prefer != None:
            if self._isAliasAvailable(prefer):
                return self._acquireAlias(prefer, obj, temporary)
            elif self._isTemporaryAlias(prefer):
                return self._reassignAlias(prefer, obj, temporary)
            else:
                # Cannot reassign permanent alias.
                return None
        else:
            a = self._getFreeAlias()
            if a == None:
                a = self._getTemporaryAlias()
            if a == None:
                # Nothing free.
                return None
            return self._acquireAlias(a, obj, temporary)

    def _isAliasAvailable(self, a):
        return a in self._availableAliases
    
    def _isTemporaryAlias(self, a):
        return a in self._temporaryAliases

    def _isAliasInUse(self, a):
        return a in self._temporaryAliases or a  in self._permanentAliases

    def _acquireAlias(self, a, obj, temporary):
        assert not self._isAliasInUse(a)

        if not (isinstance(a, str) and len(a) == 1):
            raise ValueError('Invalid alias ' + a)

        if not self._setAlias(a, obj.name):
            # Too many aliases, apparently. Drop a temporary one.
            tmp = self._getTemporaryAlias()
            if tmp == None:
                # Nothing to drop.
                return None

            self.releaseAlias(tmp)

            # OK, we should have some more space now. Retry.
            if not self._setAlias(a, obj.name):
                # Still failing, give up.
                return None

        # Success!
        if a in self._availableAliases:
            self._availableAliases.remove(a)
        if temporary:
            self._temporaryAliases[a] = obj
        else:
            self._permanentAliases[a] = obj
        obj._alias_set(a)
        return a

    def _setAlias(self, a, name):
        rep = self.req(b'a' + a.encode() + name.encode())
        return rep == b'!'

    def _reassignAlias(self, a, obj, temporary):
        assert a in self._temporaryAliases or a in self._permanentAliases
        assert not self._isAliasAvailable(a)

        if not self._setAlias(a, obj.name):
            return None

        self._releaseAlias(a)
        if a in self._availableAliases:
            self._availableAliases.remove(a)
        if temporary:
            self._temporaryAliases[a] = obj
        else:
            self._permanentAliases[a] = obj
        obj._alias_set(a)
        return a
    
    def _releaseAlias(self, alias):
        obj = None
        if alias in self._temporaryAliases:
            obj = self._temporaryAliases[alias]
            del self._temporaryAliases[alias]
        elif alias in self._permanentAliases:
            obj = self._permanentAliases[alias]
            del self._permanentAliases[alias]

        if obj != None:
            obj._alias_set(None)
            self._availableAliases.append(alias)

        return obj

    def _getFreeAlias(self):
        if self._availableAliases == []:
            return None
        else:
            return self._availableAliases.pop()

    def _getTemporaryAlias(self):
        keys = list(self._temporaryAliases.keys())
        if keys == []:
            return None
        a = keys[0] # pick oldest one
        self._releaseAlias(a)
        return a

    def releaseAlias(self, alias):
        self._releaseAlias(alias)
        self.req(b'a' + alias.encode())

    def _printAliasMap(self):
        if self._availableAliases == None:
            print("Not initialized")
        else:
            print("Available aliases: " + ''.join(self._availableAliases))

            if len(self._temporaryAliases) == 0:
                print("No temporary aliases")
            else:
                print("Temporary aliases:\n\t" + '\n\t'.join([f'{a}: {o.name}' for a,o in self._temporaryAliases.items()]))

            if len(self._permanentAliases) == 0:
                print("No permanent aliases")
            else:
                print("Permanent aliases: \n\t" + '\n\t'.join([f'{a}: {o.name}' for a,o in self._permanentAliases.items()]))

    def acquireMacro(self, cmds = None, sep='\n'):
        if self._availableMacros == None:
            # Not initialized yet.
            capabilities = self.capabilities()
            if not 'm' in capabilities:
                # Not supported.
                self._availableMacros = []
            else:
                self._availableMacros = list(map(chr, range(0x20, 0x7f)))
                for c in capabilities:
                    self._availableMacros.remove(c)

        if self._availableMacros == []:
            return None

        m = self._availableMacros.pop()
        if cmds != None:
            if not self.assignMacro(m, cmds, sep):
                # Setting macro failed. Rollback.
                self._availableMacros.append(m)
                return None

        self._usedMacros.append(m)
        return m

    def assignMacro(self, m, cmds, sep=b'\n'):
        if isinstance(m, str):
            m = m.encode()
        if isinstance(sep, str):
            sep = sep.encode()

        definition = b'm' + m

        for cmd in cmds:
            if isinstance(cmd, str):
                cmd = cmd.encode()
            definition += sep + cmd

        return self.req(definition) == b'!'

    def releaseMacro(self, m):
        if m in self._usedMacros:
            self._usedMacros.remove(m)
            self._availableMacros.append(m)
            self.req(b'm' + m.encode())

    def poll(self, obj, interval_s=0):
        if interval_s == None:
            self._pollStop(obj)
            return

        if interval_s <= 0:
            interval_s = self.defaultPollInterval

        if interval_s < self.fastPollThreshold_s:
            self._pollFast(obj, interval_s)
        else:
            self._pollSlow(obj, interval_s)

    def _pollFast(self, obj, interval_s):
        if self._fastPollMacro == None:
            self._fastPollMacro = Macro(self)
            self._fastPollTimer = QTimer(parent=self)
            self._fastPollTimer.timeout.connect(lambda: self._fastPollMacro.run())
            self._fastPollTimer.setInterval(self.fastPollThreshold_s * 1000)
            self._fastPollTimer.setSingleShot(False)
            self._fastPollTimer.setTimerType(Qt.PreciseTimer)

        if not self._fastPollMacro.add(b'r' + obj.shortName().encode(), obj.decodeReadRep, obj):
            self._pollSlow(obj, interval_s)
        else:
            obj._pollFast(interval_s)
            self._fastPollTimer.setInterval(min(self._fastPollTimer.interval(), interval_s * 1000))
            self._fastPollTimer.start()
        
    def _pollSlow(self, obj, interval_s):
        obj._pollSlow(max(self.slowPollInterval_s, interval_s))
    
    def _pollStop(self, obj):
        if self._fastPollMacro != None:
            self._fastPollMacro.remove(obj)
            if len(self._fastPollMacro) == 0:
                self._fastPollTimer.stop()
                self._fastPollTimer.setInterval(self.fastPollThreshold_s * 1000)
        obj._pollStop()
        
