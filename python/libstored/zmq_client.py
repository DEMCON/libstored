# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
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
import re
import sys
import os
import math
import logging
import heatshrink2
import keyword
import weakref
import random
import locale

from PySide6.QtCore import QObject, Signal, Slot, Property, QTimer, Qt, QLocale, \
    QEvent, QCoreApplication, QStandardPaths, QSocketNotifier, QEventLoop, SIGNAL
from PySide6.QtGui import QKeyEvent

from .zmq_server import ZmqServer
from .csv import CsvExport

# Wrapper to keep sphinx happy...
class _Property(Property):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class SignalRateLimiter(QObject):
    def __init__(self, src, dst, window_s=0.2, parent=None):
        super().__init__(parent=parent)
        self._timer = QTimer(parent=self)
        self._timer.timeout.connect(self._emit)
        self._timer.setSingleShot(False)
        self._timer.setInterval(window_s * 1000)
        self._dst = dst
        self._idle = True
        self._suppressed = False
        src.connect(self.receive)

    @Slot()
    def receive(self):
        if self._idle:
            self._idle = False
            self._dst.emit()
            self._timer.start()
        else:
            self._suppressed = True

    def _emit(self):
        if self._suppressed:
            self._dst.emit()
            self._suppressed = False
        else:
            self._timer.stop()
            self._idle = True;


class Object(QObject):
    """A variable or function as handled by a ZmqClient

    Do not instantiate directly, but as a ZmqClient for its objects.
    """
    valueChanged = Signal()
    valueStringChanged = Signal()
    valueUpdated = Signal()
    pollingChanged = Signal()
    aliasChanged = Signal()
    formatChanged = Signal()
    tUpdated = Signal()
    tStringChanged = Signal()
    locale = QLocale()

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
        self._pollTimerRunning = False
        self._pollInterval_s = None
        self._format = None
        self._format_set(self.formats[0])
        self._autoCsv = False
        self._valueChangedRateLimiter = SignalRateLimiter(self.valueChanged, self.valueStringChanged, parent=self)
        self._tUpdatedChangedRateLimiter = SignalRateLimiter(self.tUpdated, self.tStringChanged, parent=self)
        self._suppressSetSignals = False
        self._asyncReadPending = False

    def optimizeSignals(self):
        self._suppressSetSignals = True
        if self.receivers(SIGNAL("tUpdated()")) > 1: # ignore rate limiter
            self._suppressSetSignals = False
        elif self.receivers(SIGNAL("valueChanged()")) > 1: # ignore rate limiter
            self._suppressSetSignals = False
        elif self.receivers(SIGNAL("valueUpdated()")) > 0:
            self._suppressSetSignals = False

    @property
    def type(self):
        return self._type

    @property
    def size(self):
        return self._size

    @property
    def client(self):
        return self._client

    def _name_get(self):
        return self._name

    name = _Property(str, _name_get, constant=True)

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

    def _typeName_get(self):
        dtype = self._type & ~self.FlagFunction
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
            }.get(dtype, '?')
        if dtype in [self.Blob, self.String]:
            t = f'{t}:{self.size}'
        return f'({t})' if self.isFunction() else t

    typeName = _Property(str, _typeName_get, constant=True)

    def _alias_get(self):
        return self._alias

    def _alias_set(self, a):
        if a == self._alias:
            return

        self._alias = a
        self.aliasChanged.emit()

    # alias is read-only as there may exist macros that use them too.
    alias = _Property(str, _alias_get, notify=aliasChanged)

    # Return the alias or the normal name, if no alias was set.
    def shortName(self, tryToGetAlias = True):
        if not self._alias is None:
            return self._alias

        if not self._client is None and tryToGetAlias:
            # We don't have an alias, try to get one.
            self._client.acquireAlias(self)

            if not self._alias is None:
                # We have got one. Use it.
                return self._alias

        # Still no alias, return name instead.
        return self._name

    @staticmethod
    def sign_extend(value, bits):
        sign_bit = 1 << (bits - 1)
        return (value & (sign_bit - 1)) - (value & sign_bit)

    # Read value from server.
    @Slot(result='QVariant')
    @Slot(bool,result='QVariant')
    def read(self, tryToGetAlias=True):
        return self._read(tryToGetAlias)

    AsyncReadEvent = QEvent.User

    @Slot()
    def asyncRead(self):
        event = QEvent(self.AsyncReadEvent)
        QCoreApplication.postEvent(self, event, Qt.LowEventPriority - 1)

    def _asyncRead(self):
        if self._asyncReadPending:
            pass
        elif not self._client is None:
            self._asyncReadPending = True
            self._client.reqAsync(b'r' + self.shortName(False).encode(), self._asyncReadRep)

    def _asyncReadRep(self, rep):
        self._asyncReadPending = False
        self.decodeReadRep(rep)

    def event(self, event):
        if event.type() == self.AsyncReadEvent:
            self._asyncRead()
            return True
        else:
            return super().event(event)

    def _read(self, tryToGetAlias=True):
        if self._client is None:
            return None

        rep = self._client.req(b'r' + self.shortName(tryToGetAlias).encode())
        return self.decodeReadRep(rep)

    # Decode a read reply.
    def decodeReadRep(self, rep, t=None):
        if rep == b'?':
            return None
        value = self._decode(rep)
        if not value is None:
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
                if sys.version_info.major <= 3 and sys.version_info.minor < 9:
                    # If binint >= 2^63 for Uint64, we run into problems in libshiboken.
                    # See https://bugreports.qt.io/browse/PYSIDE-648
                    # Force to Int64 in that case.
                    if binint >= 1 << 63:
                        binint -= 1 << 64
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
                return self._decodeHex(rep).partition(b'\x00')[0].decode()
            else:
                return None
        except:
            return None

    # Write value to server.
    @Slot(object, result=bool)
    def write(self, value = None):
        if not value is None:
            self.set(value)
        return self._write(value)

    @Slot(object)
    def asyncWrite(self, value = None):
        if not value is None:
            self.set(value)
        self._write(value, lambda rep, value=value: self._asyncWriteCallback(value, rep))

    def _asyncWriteCallback(self, value, rep):
        if rep == b'!':
            # A concurrent async read may have overwritten our value,
            # which was just set. Set it here again.
            if not value is None:
                self.set(value)

    def _write(self, value, asyncCallback=None):
        if self._client is None:
            return False

        dtype = self.type & ~self.FlagFunction

        data = self._encode(value)
        if data is None:
            return False

        req = b'w' + data + self.shortName(asyncCallback is None).encode()

        if not asyncCallback is None:
            self._client.reqAsync(req, asyncCallback)
            # Assume it was successful.
            return True
        else:
            rep = self._client.req(req)
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
                if value < 0:
                    value += 1 << 64
                return self._encodeHex(struct.pack('>Q', value)[-self._size:], True)
        except:
            return None

    # Locally set value, but do not actually write it to the server.
    def set(self, value, t = None):
        if t is None:
            t = time.time()

        if not self._t is None and t < self._t:
            # Old value.
            return

        updated = False

        if self._t != t:
            self._t = t
            if not self._suppressSetSignals:
                self.tUpdated.emit()
            else:
                self._tUpdatedChangedRateLimiter.receive()
            updated = True

        if not self.isFixed():
            if isinstance(value, str) or isinstance(value, bytes):
                value = value[0:self.size]

        if isinstance(value, float) and math.isnan(value) and isinstance(self._value, float) and math.isnan(self._value):
            # Not updated, even though value != self._value would be True
            pass
        elif value != self._value:
            self._value = value
            if not self._suppressSetSignals:
                self.valueChanged.emit()
            else:
                self._valueChangedRateLimiter.receive()
            if self._autoCsv and self._polling and not self._client.csv is None and (self._client._tracing is None or not self._client._tracing.enabled):
                self._client.csv.write(t)
            updated = True

        if updated and not self._suppressSetSignals:
            self.valueUpdated.emit()

    def _t_get(self):
        return self._t

    t = _Property(float, _t_get, notify=tUpdated)

    def _tString_get(self):
        if self._t is None:
            return None
        else:
            return datetime.datetime.fromtimestamp(self._t).strftime('%Y-%m-%d %H:%M:%S.%f')

    tString = _Property(str, _tString_get, notify=tStringChanged)

    @Slot('QVariant')
    def injectDecimalPoint(self, recv):
        p = self.locale.decimalPoint()
        ev = QKeyEvent(QKeyEvent.KeyPress,
            Qt.Key_Comma if p == ',' else Qt.Key_Period, Qt.NoModifier,
            ',' if p == ',' else '.')
        QCoreApplication.sendEvent(recv, ev)

    def _interpret_int(self, value):
        # Remove all group separators. They are irrelevant, but prevent
        # parsing.
        gs = self.locale.groupSeparator()
        if gs != '':
            value = value.replace(gs, '')

        x = int(value, 0)
        return x

    def _interpret_float(self, value):
        # Remove all group separators. They are irrelevant, but prevent
        # parsing when not at the right place.
        gs = self.locale.groupSeparator()
        if gs != '':
            value = value.replace(gs, '')

        x, ok = self.locale.toDouble(value)
        if not ok:
            raise ValueError(f'Cannot convert "{value}" to float')
        return x

    def interpret(self, value):
        if isinstance(value,str):
            value = {
                self.Int8: self._interpret_int,
                self.Uint8: self._interpret_int,
                self.Int16: self._interpret_int,
                self.Uint16: self._interpret_int,
                self.Int32: self._interpret_int,
                self.Uint32: self._interpret_int,
                self.Int64: self._interpret_int,
                self.Uint64: self._interpret_int,
                self.Float: self._interpret_float,
                self.Double: self._interpret_float,
                self.Pointer32: lambda x: int(x,0),
                self.Pointer64: lambda x: int(x,0),
                self.Bool: lambda x: x.lower() in ['true', '1'],
                self.Blob: lambda x: x.encode(),
                self.String: lambda x: x,
                self.Void: lambda x: bytearray(),
            }.get(self._type & ~self.FlagFunction, lambda x: x)(value)
        return value

    # Returns the currently known value (without an actual read())
    def _value_get(self):
        return self._value

    # Writes the value to the server.
    def _value_set(self, v):
        try:
            v = self.interpret(v)
            if self._client and self._client.useEventLoop:
                self.asyncWrite(v)
                return True
            else:
                return self.write(v)
        except ValueError:
            return False

    value = _Property('QVariant', _value_get, _value_set, notify=valueChanged)

    def _valueString_get(self):
        v = self._value
        if v is None:
            return ""
        else:
            try:
                return self._formatter(v)
            except:
                return "?"

    def _valueString_set(self, v):
        return self._value_set(v)

    valueString = _Property(str, _valueString_get, _valueString_set, notify=valueStringChanged)

    def _format_int(self, x):
        # QLocale does not support ints larger than 32-bit, so fallback to
        # python locale formatting.  It should be the same, though.
        locale.setlocale(locale.LC_NUMERIC, '')
        return f'{x:n}'

    def _format_float(self, x, f, prec):
        # QLocale seems to pick toString(float) instead of toString(double),
        # resulting in rounding errors. Use python locale formatting instead.
        locale.setlocale(locale.LC_NUMERIC, '')
        return locale.format_string(f'%.{prec}{f}', x, True)

    def _format_get(self):
        return self._format

    def _format_set(self, f):
        if self._format == f:
            return

        self._format = f

        if f == 'hex':
            self._formatter = lambda x: hex(x & (1 << self._size * 8) - 1)
        elif f == 'bin':
            self._formatter = bin
        elif f == 'bytes':
            self._formatter = self._formatBytes
        elif self._type & ~self.FlagFunction == self.Float:
            self._formatter = lambda x: self._format_float(x, 'g', 6)
        elif self._type & ~self.FlagFunction == self.Double:
            self._formatter = lambda x: self._format_float(x, 'g', 15)
        elif self._type & self.FlagInt:
            self._formatter = self._format_int
        else:
            self._formatter = str

        self.formatChanged.emit()
        self.valueChanged.emit()
        if not self._client is None:
            self._client._autoSaveStateNow()

    format = _Property(str, _format_get, _format_set, notify=formatChanged)

    def _formatBytes(self, value):
        value = self._encode(value).decode()
        value = '0' * (self._size * 2 - len(value)) + value
        res = ''
        for i in range(0, len(value), 2):
            if res != []:
                res += ' '
            res += value[i:i+2]
        return res

    def _formats_get(self):
        if self._type == self.Blob:
            return ['bytes']

        f = ['default', 'bytes']
        if self.isFixed():
            f += ['hex', 'bin']
        return f

    formats = _Property('QVariant', _formats_get, constant=True)

    def _polling_get(self):
        return self._polling

    def _polling_set(self, enable):
        if self._polling != enable:
            if enable:
                self.poll()
            else:
                self.poll(None)

    polling = _Property(bool, _polling_get, _polling_set, notify=pollingChanged)

    def _pollInterval_get(self):
        if self._polling:
            return self._pollInterval_s
        else:
            return float('nan')

    pollIntervalChanged = Signal()

    @Slot(float)
    def poll(self, interval_s=0):
        if not self._client is None:
            if interval_s is not None and math.isnan(interval_s):
                interval_s = None
            self._client.poll(self, interval_s)

    def _pollStop(self):
        self._pollSetFlag(False)
        if not self._pollTimer is None:
            self._pollTimerRunning = False
            self._pollTimer.stop()

    def _pollTimerChangeOffset(self, offset, start, tries=10):
        if not self._pollTimerRunning or self._pollTimer is None or tries < 1:
            return

        now = time.time()
        precision = max(0.05, self._pollInterval_s / 20)
        if abs(now - start - offset) < precision:
            # Good enough. Restart to set the new offset.
            try:
                self._pollTimer.start()
            except:
                # Give up.
                pass
        else:
            # Timer overran quite a lot. Try again.
            QTimer.singleShot(int(offset * 1000), lambda: self._pollTimerChangeOffset(offset, now, tries - 1))

    def _pollSlow(self, interval_s):
        self._pollSetFlag(True)
        self._autoCsv = True
        self._pollInterval_s = interval_s
        self.pollIntervalChanged.emit()

        self._read()

        if self._pollTimer is None:
            self._pollTimer = QTimer(parent=self)
            self._pollTimer.timeout.connect(self._pollRead)
            self._pollTimer.setSingleShot(False)

        self._pollTimer.setInterval(interval_s * 1000)
        if interval_s < 2:
            self._pollTimer.setTimerType(Qt.CoarseTimer)
        else:
            self._pollTimer.setTimerType(Qt.VeryCoarseTimer)

        # Randomize start times of the timer to distribute multiple polling
        # objects, which are initialized at the same times, somewhat over time.
        self._pollTimerRunning = True
        self._pollTimer.start()
        self._pollTimerChangeOffset(random.random() * interval_s, time.time())

    def _pollRead(self):
        try:
            if self.alias is None:
                # Do a sequential read to get an alias.
                self._read(True)
            else:
                # Now we have an alias, do async reads.
                self._asyncRead()
        except zmq.ZMQError as e:
            if not self._client.socket is None:
                # Only reraise error when the socket wasn't closed meanwhile.
                raise e

    def _pollFast(self, interval_s):
        if interval_s < 0.01:
            self.optimizeSignals()
        else:
            self._suppressSetSignals = False
        self._pollSetFlag(True)
        self._autoCsv = interval_s > 0.01
        self._pollInterval_s = interval_s
        self.pollIntervalChanged.emit()
        if not self._pollTimer is None:
            self._pollTimerRunning = False
            self._pollTimer.stop()

    def _pollSetFlag(self, enable):
        if self._polling != enable:
            if not enable:
                self._suppressSetSignals = False
            self._polling = enable
            self.pollingChanged.emit()
            if self._client.csv:
                if enable:
                    self._client.csv.add(self)
                else:
                    self._client.csv.remove(self)

    pollInterval = _Property(float, _pollInterval_get, poll, notify=pollIntervalChanged)

    def _state(self):
        res = ''
        if self.polling:
            res += f'    o.poll({repr(self._pollInterval_s)})\n'
        if self._format != 'default':
            res += f'    o.format = {repr(self._format)}\n'

        if len(res) > 0:
            return \
                f'o = client.find({repr(self.name)})\n' + \
                f'try:\n' + \
                res + \
                f'except:\n    pass\n'
        else:
            return ''

class Stream(object):
    def __init__(self, client, name, raw=False):
        self._client = client
        self._raw = raw

        if not isinstance(name, str) or len(name) != 1:
            raise ValueError('Invalid stream name ' + s)

        self._name = name
        self._finishing = False
        self._flushing = False
        self._decoder = None

        cap = self._client.capabilities()
        if not 's' in cap:
            raise ValueError('Stream capability missing')

        self._compressed = 'f' in self._client.capabilities()
        self.reset()

    @property
    def client(self):
        return self._client

    @property
    def name(self):
        return self._name

    @property
    def raw(self):
        return self._raw

    def poll(self, suffix='', callback=None):
        req = b's' + (self.name + suffix).encode()
        if callback is None:
            return self._decode(self.client.req(req))
        else:
            return self.client.reqAsync(req, lambda x: callback(self._decode(x)))

    def _decode(self, x):
        if not self._decoder is None:
            x = self._decoder.fill(x)
            if self._finishing:
                x += self._decoder.finish()
                self._reset()

        if not self.raw:
            x = x.decode()
        return x

    def flush(self):
        if self._compressed and not self._flushing and not self._finishing:
            self._flushing = True
            self.client.reqAsync(b'f' + self.name.encode(), callback=lambda x: self._finish())

    def _finish(self):
        if self._flushing:
            self._flushing = False
            self._finishing = True

    def reset(self):
        if self._compressed:
            self.client.reqAsync(b'f' + self.name.encode(), lambda x: None)
            # Drop old data, as we missed the start of the stream.
            self.client.reqAsync(b's' + self.name.encode(), lambda x: None)
            self._reset()

    def _reset(self):
        if self._compressed:
            self._decoder = heatshrink2.core.Encoder(heatshrink2.core.Reader(window_sz2=8, lookahead_sz2=4))
            self._finishing = False
            self._flushing = False


class Macro(object):
    """Macro object as returned by ZmqClient.acquireMacro()

    Do not instantiate directly, but let ZmqClient acquire one for you.
    """
    def __init__(self, client, reqsep=b'\n', repsep=b' '):
        self._client = client

        self._macro = client.acquireMacro()
        if not self._macro is None:
            self._macro = self._macro.encode()

        self._cmds = {}

        if isinstance(reqsep, str):
            reqsep = reqsep.encode()
        self._reqsep = reqsep

        if isinstance(repsep, str):
            repsep = repsep.encode()
        self._repsep = repsep

        self._pending = False

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
        if cb is None:
            # No response expected
            return True

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
            return True
        else:
            return False

    def _update(self):
        if self._macro is None:
            return

        cmds = []
        for c in self._cmds.values():
            if cmds != []:
                cmds.append(b'e' + self._repsep)
            cmds.append(c[0])

        self._client.assignMacro(self._macro, cmds, self._reqsep)

    def run(self, asyncDecode=False):
        if not self._macro is None:
            if asyncDecode:
                if not self._pending:
                    self._pending = True
                    self._client.reqAsync(self._macro, self.decode)
                return True
            else:
                return self.decode(self._client.req(self._macro))
        else:
            for c in self._cmds.values():
                if not c[1] is None:
                    c[1](self._client.req(c[0]))
                else:
                    self._client.req(c[0])

        return True

    def decode(self, rep, t=None, skip=0):
        self._pending = False
        cb = [x[1] for x in self._cmds.values()]
        values = rep.split(self._repsep)
        if len(cb) != len(values) + skip:
            return False

        for i in range(0, len(values)):
            if not cb[i + skip] is None:
                cb[i + skip](values[i], t)

        if self._client.csv is not None and (t is not None or self._client._tracing is None or not self._client._tracing.enabled):
            self._client.csv.write(t)

        return True

    def __len__(self):
        return len(self._cmds)

class Tracing(Macro):
    """Tracing command handling"""
    def __init__(self, client, t=None, stream='t'):
        super().__init__(client=client, reqsep=b'\r', repsep=';')

        self._enabled = None
        self._decimate = 1
        self._streamPending = False
        self._streamQueued = False
        self._partial = b''

        cap = self.client.capabilities()
        if not 't' in cap:
            raise ValueError('Tracing capability missing')
        if not 'm' in cap:
            raise ValueError('Macro capability missing')
        if not 'e' in cap:
            raise ValueError('Echo capability missing')
        if not 's' in cap:
            raise ValueError('Stream capability missing')

        self._stream = self._client.stream(stream, raw=True)

        # Start with sample separator.
        self.add('e\n', None, 'e')

        # We must have a macro, not a simulated Macro instance.
        if self.macro is None:
            raise ValueError('Cannot get macro for tracing')

        t = self.client.time()
        if t is None:
            raise ValueError('Cannot determine time stamp variable')

        self.add(f'r{t.shortName()}', None, 't')
        self._enabled = False
        self._updateTracing(True);

    def __del__(self):
        try:
            self._enabled = False
            self.client.reqAsync(b't')
        except:
            pass

    def _update(self):
        super()._update()
        # Remove existing samples from buffer, as the layout is changing.
        self._stream.reset()
        self._updateTracing()

    def _updateTracing(self, force=False):
        if self._enabled is None:
            # Initializing
            return

        if (force or self._enabled) and len(self) == 0:
            self._enabled = False
            self.client.req(b't')
        elif force or not self._enabled:
            rep = self.client.req(b't' + self.macro + self.stream.name.encode() + ('%x' % self.decimate).encode()).decode()
            if rep != '!':
                raise ValueError('Cannot configure tracing')
            self._enabled = True

    @property
    def enabled(self):
        return self._enabled

    @property
    def decimate(self):
        return self._decimate

    @decimate.setter
    def _decimate_set(self, decimate):
        if not isinstance(decimate, int):
            return

        if decimate < 1:
            decimate = 1
        elif decimate > 0x7fffffff:
            # Limit it somewhat to stay within 32 bit
            decimate = 0x7fffffff

        self._decimate = decimate
        self._updateTracing(True)

    @property
    def stream(self):
        return self._stream

    def process(self):
        if self._streamPending:
            self._streamQueued = True
            return
        self._streamPending = True
        self._stream.poll(callback=self._process)

    def _process(self, s):
        if self._streamQueued and self.client.useEventLoop:
            assert self._streamPending
            # Immediately send out another request.
            self._streamQueued = False
            self._stream.poll(callback=self._process)
        else:
            self._streamPending = False

        samples = (self._partial + s).split(b'\n;')
        self._partial = samples[-1]
        time = self.client.time()
        for sample in samples[0:-1]:
            # The first value is the time stamp.
            t_data = sample.split(b';', 1)
            if len(t_data) < 2:
                # Empty sample.
                continue
            t = time._decode(t_data[0])
            if t is None:
                continue
            ts = self.client.timestampToTime(t)
            time.set(t, ts)
            super().decode(t_data[1], ts, skip=2)

    def __len__(self):
        # Don't count sample separator and time stamp.
        return max(0, super().__len__() - 2)

class ZmqClient(QObject):
    """A ZMQ client.

    This client can connect to either the libstored.zmq_server.ZmqServer and stored::DebugZmqLayer.

    Instantiate as libstored.ZmqClient().
    """

    traceThreshold_s = 0.1
    fastPollThreshold_s = 0.9
    slowPollInterval_s = 1.0
    defaultPollIntervalChanged = Signal()
    closed = Signal()

    def __init__(self, address='localhost', port=ZmqServer.default_port, csv=None, multi=False, parent=None, t=None, timeout=None):
        super().__init__(parent=parent)
        self.logger = logging.getLogger(__name__)
        self._multi = multi
        self._context = zmq.Context()
        self._socket = self._context.socket(zmq.REQ)
        if timeout is not None and timeout <= 0:
            timeout = None
        self._timeout = timeout
        if timeout is not None:
            self.logger.debug(f'Using a timeout of {timeout} s')
            self._socket.setsockopt(zmq.CONNECT_TIMEOUT, int(timeout * 1000))
            self._socket.setsockopt(zmq.RCVTIMEO, int(timeout * 1000))
            self._socket.setsockopt(zmq.SNDTIMEO, int(timeout * 1000))
        self.logger.debug('Connecting to %s:%d...', address, port)
        self._socket.connect(f'tcp://{address}:{port}')
        self.logger.debug('Connected')
        self._defaultPollInterval = 1
        self._capabilities = None
        self._availableAliases = None
        self._temporaryAliases = {}
        self._permanentAliases = {}
        self._availableMacros = None
        self._usedMacros = []
        self._objects = None
        self._fastPollMacro = None
        self._fastPollTimer = None
        if csv is None:
            self.csv = None
        else:
            self.csv = CsvExport(filename=csv, parent=self)
        self._t = t
        self._timestampToTime = lambda x: x
        self._t0 = 0
        self._tracingTimer = None
        self._autoSaveState = False
        self._identification = None
        self._reqQueue = []
        self._socketNotifier = QSocketNotifier(self._socket.fileno(), QSocketNotifier.Read, parent=self)
        self._socketNotifier.setEnabled(False)
        self._socketNotifier.activated.connect(self._reqAsyncCheckResponse)
        self._useEventLoop = False
        QTimer.singleShot(0, self._haveEventLoop)
        app = QCoreApplication.instance()
        if not app is None:
            app.aboutToQuit.connect(self._aboutToQuit)

        if 'f' in self.capabilities():
            self.logger.debug('Streams are compressed')

        try:
            self._tracing = Tracing(self)
        except Exception as e:
            self.logger.info('Tracing not available; %s', e)
            # Not available.
            self._tracing = None

    def _haveEventLoop(self):
        self.logger.debug('event loop running')
        self._useEventLoop = True

    @property
    def useEventLoop(self):
        return self._useEventLoop

    @property
    def context(self):
        return self._context

    @property
    def objects(self):
        if self._objects is None:
            return list()
        else:
            return list(self._objects)

    @property
    def socket(self):
        return self._socket

    @Slot(str,result=str)
    def req(self, message):
        if isinstance(message,str):
            return self.req(message.encode()).decode()

        if message == b'':
            return b''

        # Wait for all outstanding requests first.
        self._reqAsyncFlush()

        if self._socket is None:
            return None

        self.logger.debug('req %s', message)
        self._socket.send(message)

        # Block till we have some message.
        start = time.time()
        while True:
            if self._timeout is not None and time.time() - start > self._timeout:
                self.logger.error('Closing because of communication timeout')
                self.close()
                raise TimeoutError()

            try:
                rep = b''.join(self._socket.recv_multipart(zmq.NOBLOCK))
                break
            except zmq.ZMQError as e:
                if e.errno != zmq.EAGAIN:
                    raise
            self._socket.poll(1000)

        self.logger.debug('rep %s', rep)
        return rep

    def reqAsync(self, message, callback=None):
        if isinstance(message,str):
            message = message.encode()
            if callback is None:
                self.reqAsync(message)
            else:
                self.reqAsync(message, lambda rep: callback(rep.decode()))
            return

        if message == b'':
            if not callback is None:
                callback(b'')
            return

        if not self.useEventLoop:
            # Without event loop, async does not work.
            # Forward to blocking req instead.
            rep = self.req(message)
            if not callback is None:
                callback(rep)
            elif rep == b'?':
                self.logger.warning('Async req returned an error, which was not handled')
            return

        self._reqQueue.append((message, callback))
        if len(self._reqQueue) == 1:
            self._socketNotifier.setEnabled(True)
            self._reqAsyncSendNext()
        else:
            self.logger.debug('req async queued %s', message)

    def _reqAsyncSendNext(self):
        if not self._socket is None and self._reqQueue != []:
            req, _ = self._reqQueue[0]
            self.logger.debug('req async send %s', req)
            self._socket.send(req)
        else:
            self._socketNotifier.setEnabled(False)

    @Slot()
    def _reqAsyncCheckResponse(self):
        res = False

        try:
            while not self._socket is None:
                resp = b''.join(self._socket.recv_multipart(zmq.NOBLOCK))
                self._reqAsyncHandleResponse(resp)
                res = True
        except zmq.ZMQError as e:
            pass
        return res

    def _reqAsyncHandleResponse(self, resp):
        self.logger.debug('req async recv %s', resp)
        assert(self._reqQueue != [])
        req, callback = self._reqQueue.pop(0)
        self._reqAsyncSendNext()
        if not callback is None:
            callback(resp)
        elif resp == b'?':
            # We got an error back, but no callback was specified. Report it anyway.
            self.logger.warning('Req %s returned an error, which was not handled', req)

    def _reqAsyncFlush(self, timeout=None):
        start = time.time()
        lastMsg = start

        pollInterval = 1000
        if timeout is not None:
            pollInterval = min(timeout, pollInterval)

        while self._reqQueue != []:
            if not timeout is None and time.time() - start > timeout:
                raise TimeoutError()

            if self._socket is None:
                self._reqAsyncHandleResponse(b'')
                continue

            try:
                self._reqAsyncHandleResponse(b''.join(self._socket.recv_multipart(zmq.NOBLOCK)))
                lastMsg = time.time()
            except zmq.ZMQError as e:
                if e.errno != zmq.EAGAIN:
                    raise
                else:
                    if self._timeout is not None and time.time() - lastMsg > self._timeout:
                        self.logger.error('Closing because of communication timeout')
                        self.close()
                        raise TimeoutError()

                    self._socket.poll(pollInterval)

    @Slot()
    def _aboutToQuit(self):
        self.logger.debug('aboutToQuit')
        self._useEventLoop = False
        try:
            self._reqAsyncFlush(10)
        except:
            pass

        try:
            app = QCoreApplication.instance()
            if not app is None:
                app.aboutToQuit.disconnect(self._aboutToQuit)
        except:
            pass

    def time(self):
        if self._t == False:
            # Not found
            return None
        elif not self._t is None:
            return self._t

        # Not initialized.
        self._t = False

        if isinstance(self._t, str):
            # Take the given time variable
            t = self.find(self._t)
        else:
            # Try finding /t (unit)
            t = self.find('/t (')

        if t is None:
            # Not found, try the first /store/t (unit)
            for o in self.list():
                chunks = o.name.split('/', 4)
                if len(chunks) != 3:
                    # Strange name
                    continue
                elif chunks[2].startswith('t ('):
                    # Got some
                    t = o
                    break;
            if t is None:
                # Still not found. Give up.
                return None
        elif isinstance(t, list):
            # Multiple found. Just take first one.
            t = t[0]
        else:
            # One found
            pass

        try:
            t0 = t.read()
            self._t0 = time.time()
        except:
            # Cannot read. Give up.
            return None

        # Try parse the unit
        unit = re.sub(r'.*/t \((.*)\)$', r'\1', t.name)
        if unit == 's':
            self._timestampToType = lambda t: float(t - t0) + self._t0
        elif unit == 'ms':
            self._timestampToType = lambda t: float(t - t0) / 1e3 + self._t0
        elif unit == 'us':
            self._timestampToType = lambda t: float(t - t0) / 1e6 + self._t0
        elif unit == 'ns':
            self._timestampToType = lambda t: float(t - t0) / 1e9 + self._t0
        else:
            # Don't know a conversion, just use the raw value.
            self._timestampToType = lambda t: t - t0

        # Make alias permanent.
        self.acquireAlias(t, t.alias, False)
        # All set.
        self._t = t
        self.logger.info('time object: %s', t.name)
        return self._t

    def timestampToTime(self, t = None):
        if t is None:
            return time.time()
        else:
            # Override to implement arbitrary conversion.
            return self._timestampToType(t)

    def close(self):
        self.logger.debug('closing')
        if self._fastPollTimer:
            self._fastPollTimer.stop()
        if self._tracingTimer:
            self._tracingTimer.stop()
        s = self._socket
        self._socket = None
        if not s is None:
            s.close(0)
        self._aboutToQuit()

        # Break all references to Objects for gc
        self._temporaryAliases = {}
        self._permanentAliases = {}

        self._fastPollMacro = None
        self._tracing = None
        self._t = None

        if not self._objects is None:
            for o in self._objects:
                o.setParent(None)

            self._objects = None

        if not self.csv is None:
            self.csv.close()
            self.csv = None

        self.closed.emit()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def __del__(self):
        self.logger.debug('del')
        s = self._socket
        self._socket = None
        if not s is None:
            s.close(0)
        if not self.csv is None:
            self.csv.close()
            self.csv = None

    @Slot(result=str)
    def capabilities(self):
        if self._capabilities is None:
            self._capabilities = self.req('?')
            if self._multi:
                # Remove capabilities that are stateful at the embedded side.
                self._capabilities = re.sub(r'[amstf]', '', self._capabilities)

        return self._capabilities

    @Slot(result=str)
    def echo(self, s):
        return self.req(b'e' + s.encode()).decode()

    # Returns a list of Objects, registered to this client.
    def list(self):
        self._list_init()
        return self.objects

    def pyname(self, name):
        n = re.sub(r'[^A-Za-z0-9/]+', '_', name)
        n = re.sub(r'_*/+', '__', n)
        n = re.sub(r'^__', '', n)
        n = re.sub(r'^[^A-Za-z]_*', '_', n)
        n = re.sub(r'_+$', '', n)

        if n == '':
            n = 'obj'

        if keyword.iskeyword(n):
            n += '_obj'

        if hasattr(self, n):
            i = 1
            while hasattr(self, f'{n}_{i}'):
                i += 1
            n = f'{n}_{i}'

        return n

    def _list_init(self):
        if not self._objects is None:
            return

        res = []
        for o in self.req(b'l').decode().split('\n'):
            obj = Object.listResponseDecode(o, self)
            if not obj is None:
                res.append(obj)
                pyname = self.pyname(obj.name)
                wobj = weakref.ref(obj)
                setattr(ZmqClient, pyname, _Property(Object, lambda s, wobj=wobj: obj, constant=True))

        self._objects = res

    def find(self, name, all=False):
        chunks = name.split('/')
        obj1 = set()
        obj2 = set()
        obj3 = set()
        obj4 = set()
        self._list_init()
        for o in self._objects:
            ochunks = o.name.split('/')
            if len(chunks) != len(ochunks):
                continue

            # There are several cases:
            # 1. The given name is an unambiguous full name, and the target has full names too. Expect exact match.
            # 2. The given name is an unambiguous full name, while the target has abbreviated names.
            # 3. The given name matches multiple objects, having full names, as it was ambiguous.
            # 4. The object names are abbreviated, and the given name was ambiguous.

            # Case 1.
            match = True
            for i in range(0, len(ochunks)):
                # Assume abbreviated names.
                if ochunks[i] != chunks[i]:
                    match = False
                    break
            if match:
                obj1.add(o)

            # Case 2.
            match = True
            for i in range(0, len(ochunks)):
                if re.fullmatch(re.sub(r'\\\?', '.', re.escape(ochunks[i])) + r'.*', chunks[i]) is None:
                    match = False
                    break
                # It seems to match. Additional check: the object's chunk should not be longer, as it makes name ambiguous.
                elif len(ochunks[i]) > len(chunks[i]):
                    match = False
                    break
            if match:
                obj2.add(o)

            # Case 3.
            # Prefer names that have an exact prefix, even when ambiguous.
            match = True
            exact = True
            exactLen = 0
            for i in range(0, len(ochunks)):
                if not ochunks[i].startswith(chunks[i]):
                    match = False
                    break
                if exact:
                    if ochunks[i] == chunks[i]:
                        exactLen += 1
                    else:
                        exact = False
            if match:
                obj3 = {(x,e) for x,e in obj3 if e >= exactLen}
                best = max(obj3, key=lambda x: x[1], default=(None,0))[1]
                if exactLen >= best:
                    obj3.add((o,exactLen))

            # Case 4.
            match = True
            for i in range(0, len(ochunks)):
                if re.fullmatch(re.sub(r'\\\?', '.', re.escape(ochunks[i])) + r'.*', chunks[i]) is None:
                    match = False
                    break
            if match:
                obj4.add(o)

        obj = obj1 | obj2 | {x for x,e in obj3} | obj4
        if all:
            return obj
        if len(obj1) == 1:
            # Best result.
            return obj1.pop()
        elif len(obj) == 0:
            return None
        elif len(obj) == 1:
            return obj.pop()
        else:
            return obj

    @Slot(str, result=Object)
    def obj(self, x):
        try:
            return getattr(self, x)
        except:
            pass

        obj = self.find(x)
        if isinstance(obj, Object):
            return obj
        elif obj is None:
            raise ValueError(f'Cannot find object with name "{x}"')
        else:
            raise ValueError(f'Object name "{x}" is ambiguous')

    def __getitem__(self, x):
        return self.obj(x)

    @Slot(result=str)
    def identification(self):
        try:
            self._identification = self.req(b'i').decode()
        except:
            self._identification = None
        return self._identification

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

    def stream(self, s, raw=False):
        return Stream(self, s, raw)

    def _defaultPollInterval_get(self):
        return self._defaultPollInterval

    def _defaultPollInterval_set(self, interval):
        interval = max(0.0001, float(interval))
        if interval != self._defaultPollInterval:
            self._defaultPollInterval = interval
            self.defaultPollIntervalChanged.emit()

    defaultPollInterval = _Property(float, _defaultPollInterval_get, _defaultPollInterval_set, notify=defaultPollIntervalChanged)

    def acquireAlias(self, obj, prefer=None, temporary=True, permanentRef=None):
        if prefer is None and not obj.alias is None:
            if temporary != self._isTemporaryAlias(obj.alias):
                # Switch type
                return self._reassignAlias(obj.alias, obj, temporary, permanentRef)
            else:
                if not temporary:
                    self._incPermanentAlias(obj.alias, permanentRef)
                # Already assigned one.
                return obj.alias
        if not prefer is None:
            if obj.alias != prefer:
                # Go assign one.
                pass
            elif temporary != self._isTemporaryAlias(prefer):
                # Switch type
                return self._reassignAlias(prefer, obj, temporary, permanentRef)
            else:
                if not temporary:
                    self._incPermanentAlias(prefer, permanentRef)
                # Already assigned preferred one.
                return prefer

        if self._availableAliases is None:
            # Not yet initialized
            if 'a' in self.capabilities():
                self._availableAliases = list(map(chr, range(0x20, 0x7f)))
                self._availableAliases.remove('/')
            else:
                self._availableAliases = []

        if not prefer is None:
            if self._isAliasAvailable(prefer):
                return self._acquireAlias(prefer, obj, temporary, permanentRef)
            elif self._isTemporaryAlias(prefer):
                return self._reassignAlias(prefer, obj, temporary, permanentRef)
            else:
                # Cannot reassign permanent alias.
                return None
        else:
            a = self._getFreeAlias()
            if a is None:
                a = self._getTemporaryAlias()
            if a is None:
                # Nothing free.
                return None
            return self._acquireAlias(a, obj, temporary, permanentRef)

    def _isAliasAvailable(self, a):
        return a in self._availableAliases

    def _isTemporaryAlias(self, a):
        return a in self._temporaryAliases

    def _isAliasInUse(self, a):
        return a in self._temporaryAliases or a in self._permanentAliases

    def _incPermanentAlias(self, a, permanentRef):
        assert a in self._permanentAliases
        self.logger.debug(f'increment permanent alias {a} use')
        self._permanentAliases[a][1].append(permanentRef)

    def _decPermanentAlias(self, a, permanentRef):
        assert a in self._permanentAliases
        if permanentRef is None:
            self.logger.debug(f'ignored decrement permanent alias {a} use')
            return False

        try:
            self._permanentAliases[a][1].remove(permanentRef)
            self.logger.debug(f'decrement permanent alias {a} use')
        except ValueError:
            # Unknown ref.
            pass

        return self._permanentAliases[a][1] == []

    def _acquireAlias(self, a, obj, temporary, permanentRef):
        assert not self._isAliasInUse(a)

        if not (isinstance(a, str) and len(a) == 1):
            raise ValueError('Invalid alias ' + a)

        availableUponRollback = False
        if a in self._availableAliases:
            self.logger.debug('available: ' + ''.join(self._availableAliases))
            self._availableAliases.remove(a)
            availableUponRollback = True

        if not self._setAlias(a, obj.name):
            # Too many aliases, apparently. Drop a temporary one.
            tmp = self._getTemporaryAlias()
            if tmp is None:
                # Nothing to drop.
                if availableUponRollback:
                    self._availableAliases.append(a)
                return None

            self.releaseAlias(tmp)

            # OK, we should have some more space now. Retry.
            if not self._setAlias(a, obj.name):
                # Still failing, give up.
                if availableUponRollback:
                    self._availableAliases.append(a)
                return None

        # Success!
        if temporary:
            self.logger.debug(f'new temporary alias {a} for {obj.name}')
            self._temporaryAliases[a] = obj
        else:
            self.logger.debug(f'new permanent alias {a} for {obj.name}')
            self._permanentAliases[a] = (obj, [permanentRef])
        obj._alias_set(a)
        return a

    def _setAlias(self, a, name):
        rep = self.req(b'a' + a.encode() + name.encode())
        return rep == b'!'

    def _reassignAlias(self, a, obj, temporary, permanentRef):
        assert a in self._temporaryAliases or a in self._permanentAliases
        assert not self._isAliasAvailable(a)

        if not self._setAlias(a, obj.name):
            return None

        if not self._releaseAlias(a, permanentRef):
            # Not allowed, still is use as permanent alias.
            self.logger.debug(f'cannot release alias {a}; still in use')
            pass
        else:
            if a in self._availableAliases:
                self._availableAliases.remove(a)
            if temporary:
                self.logger.debug(f'reassigned temporary alias {a} to {obj.name}')
                self._temporaryAliases[a] = obj
            else:
                self.logger.debug(f'reassigned permanent alias {a} to {obj.name}')
                self._permanentAliases[a] = (obj, [permanentRef])

        obj._alias_set(a)
        return a

    def _releaseAlias(self, alias, permanentRef=None):
        obj = None
        if alias in self._temporaryAliases:
            obj = self._temporaryAliases[alias]
            del self._temporaryAliases[alias]
            self.logger.debug(f'released temporary alias {alias}')
        elif alias in self._permanentAliases:
            if not self._decPermanentAlias(alias, permanentRef):
                # Do not release (yet).
                return False
            obj = self._permanentAliases[alias][0]
            del self._permanentAliases[alias]
            self.logger.debug(f'released permanent alias {alias}')
        else:
            self.logger.debug(f'released unused alias {alias}')

        if not obj is None:
            obj._alias_set(None)
            self._availableAliases.append(alias)

        return True

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
        self.logger.debug(f'stealing temporary alias {a}')
        self._releaseAlias(a)
        return a

    def releaseAlias(self, alias, permanentRef=None):
        if self._releaseAlias(alias, permanentRef):
            if alias is not None:
                self.req(b'a' + alias.encode())

    def _printAliasMap(self):
        if self._availableAliases is None:
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
                print("Permanent aliases: \n\t" + '\n\t'.join([f'{a}: {o[0].name} ({len(o[1])})' for a,o in self._permanentAliases.items()]))

    def acquireMacro(self, cmds = None, sep='\n'):
        if self._availableMacros is None:
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
        if not cmds is None:
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
        self._pollStop(obj)

        if interval_s is None:
            self._autoSaveStateNow()
            return

        if interval_s <= 0:
            interval_s = self.defaultPollInterval

        if interval_s < self.traceThreshold_s:
            self._trace(obj, interval_s)
        elif interval_s < self.fastPollThreshold_s:
            self._pollFast(obj, interval_s)
        else:
            self._pollSlow(obj, interval_s)

        self._autoSaveStateNow()

    def _pollFast(self, obj, interval_s):
        if self._fastPollMacro is None:
            self._fastPollMacro = Macro(self)
            self._fastPollTimer = QTimer(parent=self)
            self._fastPollTimer.timeout.connect(lambda: self._fastPollMacro.run(True))
            self._fastPollTimer.setInterval(self.fastPollThreshold_s * 1000)
            self._fastPollTimer.setSingleShot(False)
            self._fastPollTimer.setTimerType(Qt.PreciseTimer)

        if not self._fastPollMacro.add(b'r' + obj.shortName().encode(), obj.decodeReadRep, obj):
            self._pollSlow(obj, interval_s)
        else:
            a = obj.alias
            if not a is None:
                # Make alias permanent.
                self.acquireAlias(obj, a, False, self._fastPollMacro)

            obj._pollFast(interval_s)
            self._fastPollTimer.setInterval(min(self._fastPollTimer.interval(), interval_s * 1000))
            self._fastPollTimer.start()

    def _pollSlow(self, obj, interval_s):
        obj._pollSlow(max(self.slowPollInterval_s, interval_s))

    def _pollStop(self, obj):
        if not self._fastPollMacro is None:
            if self._fastPollMacro.remove(obj):
                # Did remove, so release permanent alias.
                self.releaseAlias(obj.alias, self._fastPollMacro)

                if len(self._fastPollMacro) == 0:
                    self._fastPollTimer.stop()
                    self._fastPollTimer.setInterval(self.fastPollThreshold_s * 1000)

        if not self._tracing is None:
            if self._tracing.remove(obj):
                self.releaseAlias(obj.alias, self._tracing)

                if not self._tracing.enabled:
                    self._tracingTimer.stop()

        obj._pollStop()

    def _trace(self, obj, interval_s):
        if self._tracing is None:
            self._pollFast(obj, interval_s)
            return

        if self._tracingTimer is None:
            self._tracingTimer = QTimer(parent=self)
            self._tracingTimer.timeout.connect(self.traceProcess)
            self._tracingTimer.setInterval(self.traceThreshold_s * 1000)
            self._tracingTimer.setSingleShot(False)
            self._tracingTimer.setTimerType(Qt.PreciseTimer)

        if not self._tracing.add(b'r' + obj.shortName().encode(), obj.decodeReadRep, obj):
            self.logger.debug('Cannot add %s for tracing, use polling instead', obj.name)
            self._pollFast(obj, interval_s)
            return
        else:
            a = obj.alias
            if not a is None:
                # Make alias permanent.
                self.acquireAlias(obj, a, False, self._tracing)

        obj._pollFast(interval_s)

        if self._tracing.enabled:
            self._tracingTimer.start()

    def traceProcess(self):
        """
        In case there is no Qt event loop running, call this function to poll
        and process data that is expected via the trace stream.
        """
        if self._tracing is None:
            return

        self._tracing.process()

    @Slot(int)
    def traceDecimate(self, decimate):
        if self._tracing:
            self._tracing.decimate = decimate

    def autoSaveState(self, enable=True):
        if not self._autoSaveState and enable:
            self.saveState()
        self._autoSaveState = enable

    def _autoSaveStateNow(self):
        if self._autoSaveState:
            self.saveState()

    @Slot()
    @Slot(str)
    def saveState(self, f=None):
        if f is None:
            f = self.defaultStateFile()

        os.makedirs(os.path.dirname(f), exist_ok=True)

        with open(f, 'w') as f:
            res = ''
            f.write('# This file is auto-generated.\n')
            if self._identification is None:
                self.identification()
            if not self._identification is None:
                f.write(f'if client.identification() != {repr(self._identification)}:\n   raise NameError()\n')

            if not self._objects is None:
                for o in self._objects:
                    f.write(o._state())

    @Slot()
    @Slot(str)
    def restoreState(self, f=None):
        if f is None:
            f = self.defaultStateFile()

        autoSaveState = self._autoSaveState
        self._autoSaveState = False

        try:
            exec(open(f).read(), {'client': self})
        except:
            # Ignore all errors, restoring is best-effort.
            pass

        if autoSaveState:
            self.autoSaveState()

    def defaultStateFile(self):
        return os.path.join(QStandardPaths.standardLocations(QStandardPaths.AppDataLocation)[0], "state.conf")

