# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import asyncio
import functools
import keyword
import locale
import math
import re
import struct
import time
import typing
import zmq
import zmq.asyncio
import zmq.utils.monitor

from .event import Event, Value, ValueWrapper
from ..zmq_server import ZmqServer
from .worker import Work, run_sync, run_async
from ..heatshrink import HeatshrinkDecoder

class ZmqClientWork(Work):
    def __init__(self, client : ZmqClient, *args, **kwargs):
        super().__init__(worker=client.worker, *args, **kwargs)
        self._client : ZmqClient | None = client

    def alive(self) -> bool:
        '''Check if this object is still alive, i.e. the client connection is still active.'''
        return not self._client is None

    def destroy(self):
        '''Destroy this object, as the client connection is closed.'''
        self._client = None

    def __del__(self):
        self.destroy()

    @property
    def client(self) -> ZmqClient:
        '''The ZmqClient this object belongs to.'''
        if not self.alive():
            raise RuntimeError('Object destroyed, client connection closed')
        assert self._client is not None
        return self._client

class Object(ZmqClientWork, Value):
    """A variable or function as handled by a ZmqClient

    Do not instantiate directly, but as a ZmqClient for its objects.
    """

    @staticmethod
    def create(s : str, client : ZmqClient) -> Object | None:
        '''
        Create an Object from a List response line.
        Return None if the line is invalid.
        '''

        split = s.split('/', 1)
        if len(split) < 2:
            return None
        if len(split[0]) < 3:
            return None
        try:
            return Object('/' + split[1], int(split[0][0:2], 16), int(split[0][2:], 16), client)
        except ValueError:
            return None

    def __init__(self, name : str, type : int, size : int, client : ZmqClient, *args, **kwargs):
        self._name = name
        self._type_id = type
        self._size = size
        super().__init__(client=client, type=self.value_type, event_name=f'{name}/value', *args, **kwargs)

        self._format : str = ''
        self._formatter : typing.Callable[..., str] | None = None
        self._poller : asyncio.Task | None = None
        self._poll_interval_s : float | None = None

        self.alias = Value(str, event_name=f'{name}/alias')
        self.t = Value(float, event_name=f'{name}/t')
        self.value_str = ValueWrapper(str, self._value_str_get, self._value_str_set, event_name=f'{name}/value_str')
        self.polling = ValueWrapper(float, lambda: self.poll_interval, self.poll, event_name=f'{name}/polling')

        self.format = 'default'

    @property
    def name(self):
        '''The full name of this object.'''
        return self._name

    def __str__(self) -> str:
        return f'{self.name} = {repr(self.value)}'

    def destroy(self):
        super().destroy()

        if self._poller is not None:
            self._poller.cancel()
            self._poller = None

        self.value = None



    #################################################
    # Type

    @property
    def type_id(self):
        '''The type code of this object.'''
        return self._type_id

    @property
    def size(self):
        '''The size of this object.'''
        return self._size

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

    def is_valid_type(self) -> bool:
        return self._type_id & 0x80 == 0

    def is_function(self) -> bool:
        return self._type_id & self.FlagFunction != 0

    def is_fixed(self) -> bool:
        return self._type_id & self.FlagFixed != 0

    def is_int(self) -> bool:
        return self.is_fixed() and self._type_id & self.FlagInt != 0

    def is_signed(self) -> bool:
        return self.is_fixed() and self._type_id & self.FlagSigned != 0

    def is_special(self) -> bool:
        return self._type_id & 0x78 == 0

    @property
    def type_name(self) -> str:
        '''Get the type name as used in the store definition.'''

        dtype = self._type_id & ~self.FlagFunction
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
        return f'({t})' if self.is_function() else t

    @property
    def value_type(self) -> typing.Type:
        '''Get the Python type used for the value of this object.'''

        dtype = self._type_id & ~self.FlagFunction
        t = {
                self.Int8: int,
                self.Uint8: int,
                self.Int16: int,
                self.Uint16: int,
                self.Int32: int,
                self.Uint32: int,
                self.Int64: int,
                self.Uint64: int,
                self.Float: float,
                self.Double: float,
                self.Pointer32: int,
                self.Pointer64: int,
                self.Bool: bool,
                self.Blob: bytearray,
                self.String: str,
                self.Void: type(None),
            }.get(dtype, type(None))
        return t



    ###############################################
    # Read

    @run_sync
    async def short_name(self, acquire = True) -> str:
        '''Get the alias of this object, or its full name if no alias is set.'''

        if not self.alias.value is None:
            return self.alias.value

        if acquire:
            # We don't have an alias, try to get one.
            self.alias.value = await self.client.alias(self)

            if not self.alias.value is None:
                # We have got one. Use it.
                return self.alias.value

        # Still no alias, return name instead.
        return self.name

    @run_sync
    async def read(self, acquire_alias : bool=True) -> typing.Any:
        '''Read the value of this object from the server.'''
        return await self._read(acquire_alias)

    async def _read(self, acquire_alias : bool=True) -> typing.Any:
        name = await self.short_name(acquire_alias)
        t = time.time()
        rep = await self.client.req(b'r' + name.encode())
        return self.handle_read(rep, t)

    def handle_read(self, rep : bytes, t=None) -> typing.Any:
        '''Handle a read reply.'''

        if rep == b'?':
            return None
        try:
            self.set(self._decode(rep), t)
        except ValueError:
            pass
        return self.value

    def _decode_hex(self, data : bytes) -> bytearray:
        if len(data) % 2 == 1:
            data = b'0' + data
        res = bytearray()
        for i in range(0, len(data), 2):
            res.append(int(data[i:i+2], 16))
        return res

    @staticmethod
    def _sign_extend(value : int, bits : int) -> int:
        sign_bit = 1 << (bits - 1)
        return (value & (sign_bit - 1)) - (value & sign_bit)

    def _decode(self, rep : bytes) -> typing.Any:
        dtype = self._type_id & ~self.FlagFunction
        if self.is_fixed():
            binint = int(rep.decode(), 16)
            if self.is_int() and not self.is_signed():
                return binint
            elif dtype == self.Int8:
                return self._sign_extend(binint, 8)
            elif dtype == self.Int16:
                return self._sign_extend(binint, 16)
            elif dtype == self.Int32:
                return self._sign_extend(binint, 32)
            elif dtype == self.Int64:
                return self._sign_extend(binint, 64)
            elif dtype == self.Float:
                return struct.unpack('<f', struct.pack('<I', binint))[0]
            elif dtype == self.Double:
                return struct.unpack('<d', struct.pack('<Q', binint))[0]
            elif dtype == self.Bool:
                return binint != 0
            elif dtype == self.Pointer32 or dtype == self.Pointer64:
                return binint
            else:
                raise ValueError()
        elif dtype == self.Void:
            return b''
        elif dtype == self.Blob:
            return self._decode_hex(rep)
        elif dtype == self.String:
            return self._decode_hex(rep).partition(b'\x00')[0].decode()
        else:
            raise ValueError()

    def get(self) -> typing.Any:
        '''Get the locally cached value of this object.'''
        return self.value



    ###############################################
    # Write

    @run_sync
    async def write(self, value : typing.Any = None):
        '''Write a value to this object on the server.'''

        if value is not None:
            self.set(value)
        else:
            value = self.value

        if value is None:
            return

        data = self._encode(value)
        name = await self.short_name()
        req = b'w' + data + name.encode()
        rep = await self.client.req(req)
        if rep != b'!':
            raise RuntimeError('Write failed')

    def _encode_hex(self, data, zerostrip = False) -> bytes:
        s = b''.join([b'%02x' % b for b in data])
        if zerostrip:
            s = s.lstrip(b'0')
            if s == b'':
                s = b'0'
        return s

    def _encode(self, value : typing.Any) -> bytes:
        dtype = self._type_id & ~self.FlagFunction

        if dtype == self.Void:
            return b''
        elif dtype == self.Blob:
            return self._encode_hex(value)
        elif dtype == self.String:
            return self._encode_hex(value.encode()) + b'00'
        elif dtype == self.Pointer32:
            return ('%x' % value).encode()
        elif dtype == self.Pointer64:
            return ('%x' % value).encode()
        elif dtype == self.Bool:
            return b'1' if value else b'0'
        elif dtype == self.Float:
            return self._encode_hex(struct.pack('>f', value))
        elif dtype == self.Double:
            return self._encode_hex(struct.pack('>d', value))
        elif not self.is_int():
            raise ValueError()
        elif self.is_signed():
            return self._encode_hex(struct.pack('>q', value)[-self._size:], True)
        else:
            if value < 0:
                value += 1 << 64
            return self._encode_hex(struct.pack('>Q', value)[-self._size:], True)

    def set(self, value : typing.Any, t = None):
        '''Set the value of this object, without actually writing it yet to the server.'''

        if type(value) != self.value_type:
            raise TypeError(f'Expected value of type {self.value_type}, got {type(value)}')

        if t is None:
            t = time.time()

        if not self.t.value is None and t < self.t.value:
            # Old value.
            return

        if not self.is_fixed():
            if isinstance(value, str) or isinstance(value, bytes):
                value = value[0:self.size]

        if isinstance(value, float) and math.isnan(value) and self.type == float and math.isnan(self.value):
            # Not updated, even though value != self._value would be True
            pass
        elif value != self.value:
            self.t.pause()
            self.t.value = t
            self.value = value
            self.value_str.trigger()
            self.t.resume()



    ###############################################
    # String conversion

    def _interpret_int(self, value):
        return int(locale.delocalize(value), 0)

    def _interpret_float(self, value):
        try:
            return float(locale.delocalize(value))
        except ValueError:
            return float(self._interpret_int(value))

    def interpret(self, value : str) -> typing.Any:
        '''Interpret a string as a value of the appropriate type for this object.'''

        value = value.strip().replace(' ', '')

        if not hasattr(self, '_interpret_map'):
            self._interpret_map = {
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
                self.Void: lambda x: bytes(),
            }

        return self._interpret_map.get(self._type_id & ~self.FlagFunction, lambda x: x)(value)

    def _format_int(self, x : int) -> str:
        return locale.format_string('%d', x, True)

    def _format_float(self, x : float, f : str, prec : int) -> str:
        return locale.format_string(f'%.{prec}{f}', x, True)

    def _format_bytes(self, value : typing.Any) -> str:
        value = self._encode(value).decode()
        value = '0' * (self._size * 2 - len(value)) + value
        res = ''
        for i in range(0, len(value), 2):
            if res != []:
                res += ' '
            res += value[i:i+2]
        return res

    @property
    def format(self):
        '''Get or set the format used to convert the value to a string.'''
        return self._format

    @format.setter
    def format(self, f : str):
        if self._format == f:
            return
        if not f in self.formats():
            raise ValueError(f'Invalid format "{f}"')

        self._format = f

        if f == 'hex':
            self._formatter = lambda x: hex(x & (1 << self._size * 8) - 1)
        elif f == 'bin':
            self._formatter = bin
        elif f == 'bytes' or self._type_id & ~self.FlagFunction == self.Blob:
            self._formatter = self._format_bytes
        elif self._type_id & ~self.FlagFunction == self.Float:
            self._formatter = lambda x: self._format_float(x, 'g', 6)
        elif self._type_id & ~self.FlagFunction == self.Double:
            self._formatter = lambda x: self._format_float(x, 'g', 15)
        elif self._type_id & self.FlagInt:
            self._formatter = self._format_int
        else:
            self._formatter = str

        self.value_str.trigger()

    def formats(self) -> list[str]:
        '''Get the list of supported formats for this object.'''

        f = ['default', 'bytes']
        if self._type_id & ~self.FlagFunction == self.Blob:
            return f
        if self.is_fixed():
            f += ['hex', 'bin']
        return f

    def _value_str_get(self) -> str:
        '''Get the string representation of the value of this object.'''

        x = self.value
        if x is None:
            return ''

        assert self._formatter is not None
        try:
            return self._formatter(x)
        except:
            return '?'

    def _value_str_set(self, s : str):
        '''Set the value of this object from a string representation.'''

        if s == '':
            self.set(None)
        else:
            try:
                self.set(self.interpret(s))
            except:
                self.value_str.trigger()



    ###############################################
    # Polling

    @run_async
    async def poll(self, interval_s : float | None=None):
        '''Set up polling of this object.

        If interval_s is None (the default), stop polling.
        If interval_s is 0, poll as fast as possible.
        If interval_s > 0, poll at that interval in seconds.
        '''

        if not self.alive():
            raise RuntimeError('Object destroyed, client connection closed')

        if not interval_s is None and interval_s < 0:
            raise ValueError('interval_s must be None or >= 0')

        if self._poller is not None:
            self._poller.cancel()
            self._poller = None
            self._poll_interval_s = None

        if interval_s is not None:
            self._poll_interval_s = interval_s
            self._poller = await self.client.periodic(interval_s, self._read)

        self.polling.trigger()

    @property
    def poll_interval(self) -> float | None:
        '''Get the current polling interval, or None if not polling.'''
        return self._poll_interval_s



class Stream(ZmqClientWork):
    def __init__(self, client : ZmqClient, name : str, raw : bool=False, *args, **kwargs):
        super().__init__(client=client, *args, **kwargs)
        self._raw = raw

        if not isinstance(name, str) or len(name) != 1:
            raise ValueError('Invalid stream name ' + name)

        self._name = name
        self._finishing = False
        self._flushing = False
        self._decoder : HeatshrinkDecoder | None = None
        self._initialized = False
        self._compressed = False

    @property
    def name(self) -> str:
        return self._name

    @property
    def raw(self) -> bool:
        return self._raw

    async def _init(self):
        if self._initialized:
            return

        cap = await self.client.capabilities()
        if not 's' in cap:
            raise ValueError('Stream capability missing')

        self._compressed = 'f' in cap
        self._initialized = True
        self.reset()

    @run_sync
    async def poll(self, suffix : str=''):
        await self._init()
        req = b's' + (self.name + suffix).encode()
        return self._decode(await self.client.req(req))

    def _decode(self, x : bytes) -> str | bytes | bytearray:
        if self._decoder is not None:
            x = self._decoder.fill(x)
            if self._finishing:
                x += self._decoder.finish()
                self._reset()

        if self.raw:
            return x
        else:
            return x.decode(errors='backslashreplace')

    @run_sync
    async def flush(self):
        if self._compressed and not self._flushing and not self._finishing:
            self._flushing = True
            await self.client.req(b'f' + self.name.encode())
            self._flushing = False
            self._finishing = True

    @run_sync
    async def reset(self):
        if self._compressed:
            await self.client.req(b'f' + self.name.encode())
            # Drop old data, as we missed the start of the stream.
            await self.client.req(b's' + self.name.encode())
            self._reset()

    def _reset(self):
        if self._compressed:
            self._decoder = HeatshrinkDecoder()
            self._finishing = False
            self._flushing = False



class Macro(ZmqClientWork):
    """Macro object as returned by ZmqClient.macro()

    Do not instantiate directly, but let ZmqClient acquire one for you.
    """

    def __init__(self, client : ZmqClient, macro : str | None=None, reqsep : bytes=b'\n', repsep : bytes=b' ', *args, **kwargs):
        super().__init__(client=client, *args, **kwargs)

        if macro is not None and len(macro) != 1:
            raise ValueError('Invalid macro name ' + macro)
        self._macro = None if macro is None else macro.encode()

        self._cmds : dict[typing.Hashable, tuple[bytes, typing.Callable[[bytes, float | None], None] | None]] = {}
        self._key = 0

        if len(reqsep) != 1:
            raise ValueError('Invalid request separator')
        self._reqsep = reqsep

        self._repsep = repsep

    def __del__(self):
        if self.alive():
            self.client.release_macro(self)

        super().__del__()

    @property
    def macro(self) -> bytes | None:
        return self._macro

    @run_sync
    async def add(self, cmd : str, cb : typing.Callable[[bytes, float | None], None] | None=None, key : typing.Hashable | None=None) -> bool:
        '''Add a command to this macro.'''

        if key is None:
            key = self._key
            self._key += 1

        self._cmds[key] = (cmd.encode(), cb)
        if not await self._update():
            # Update failed, remove command again.
            del self._cmds[key]
            return False

        # Check if it still works...
        if cb is None:
            # No response expected
            return True

        try:
            self.run()
            # Success.
            return True
        except RuntimeError:
            pass

        # Rollback.
        await self.remove(key)
        return False

    @run_sync
    async def remove(self, key : typing.Hashable) -> bool:
        if key in self._cmds:
            del self._cmds[key]
            await self._update()
            return True
        else:
            return False

    async def _update(self):
        m = self.macro
        if m is None:
            return

        cmds = [b'm' + m]
        first = True
        for c in self._cmds.values():
            if not first:
                cmds.append(b'e' + self._repsep)
            cmds.append(c[0])
            first = False

        definition = self._reqsep.join(cmds)
        return await self.client.req(definition) == b'!'

    @run_sync
    async def run(self):
        m = self.macro
        if m is not None:
            self.decode(await self.client.req(m))
        else:
            for c in self._cmds.values():
                if c[1] is not None:
                    c[1](await self.client.req(c[0]), None)
                else:
                    await self.client.req(c[0])

    def decode(self, rep : bytes, t : float | None=None, skip : int=0):
        cb = [x[1] for x in self._cmds.values()]
        values = rep.split(self._repsep)
        if len(cb) != len(values) + skip:
            raise RuntimeError('Unexpected number of responses')

        for i in range(0, len(values)):
            f = cb[i + skip]
            if f is not None:
                f(values[i], t)

        return True

    def __len__(self):
        return len(self._cmds)



class ZmqClient(Work):
    '''
    Asynchronous ZMQ client.

    This client can connect to both the libstored.zmq_server.ZmqServer and stored::DebugZmqLayer.
    '''

    def __init__(self, host : str='localhost', port : int=ZmqServer.default_port,
                multi : bool=False, timeout : float | None=None, context : None | zmq.asyncio.Context=None,
                t : str | None = None,
                *args, **kwargs):

        super().__init__(*args, **kwargs)
        self._context = context or zmq.asyncio.Context.instance()
        self._host = host
        self._port = port
        self._multi = multi
        self._timeout = timeout if timeout is None or timeout > 0 else None
        self._socket = None
        self._lock = asyncio.Lock()
        self._alias_lock = asyncio.Lock()
        self._t : str | Object | None | bool = t
        self._t0 : float = 0
        self._timestamp_to_time = lambda t: t

        self._reset()

        # Events
        self.connected = Event('connected')
        self.disconnected = Event('disconnected')



    ##############################################
    # asyncio support

    @staticmethod
    def locked(f : typing.Callable) -> typing.Callable:
        '''Decorator to lock a method with the instance's lock.'''

        @functools.wraps(f)
        async def wrapper(self, *args, **kwargs):
            async with self._lock:
                return await f(self, *args, **kwargs)
        return wrapper



    ##############################################
    # ZMQ connection handling

    @property
    def host(self) -> str:
        '''Configured or currently connected host.'''
        return self._host

    @property
    def port(self) -> int:
        '''Configured or currently connected port.'''
        return self._port

    @property
    def multi(self) -> bool:
        '''
        Return whether the client uses a subset of the commands that are safe
        when multiple connections to the same ZMQ server are made.
        '''
        return self._multi

    @property
    def context(self) -> zmq.asyncio.Context:
        '''The ZMQ context used by this client.'''
        return self._context

    @property
    def socket(self) -> zmq.asyncio.Socket | None:
        '''The ZMQ socket used by this client, or None if not connected.'''
        return self._socket

    def is_connected(self) -> bool:
        '''Check if connected to the ZMQ server.'''
        return self.socket is not None

    def _reset(self):
        self._capabilities = None
        self._identification = None
        self._version = None

        self._available_aliases : list[str] | None = None
        self._temporary_aliases : dict[str, Object] = {}
        self._permanent_aliases : dict[str, tuple[Object, list[typing.Any]]] = {}

        self._available_macros : list[str] | None = None
        self._used_macros : list[str] = []
        if hasattr(self, '_macros'):
            if not self._macros is None:
                for m in self._macros:
                    m.destroy()
        self._macros : list[Macro] = []

        self._t = None

        if hasattr(self, '_objects'):
            if not self._objects is None:
                for o in self._objects:
                    o.destroy()
        self._objects : typing.List[Object] | None = None

        if hasattr(self, '_objects_attr'):
            for o in self._objects_attr:
                if hasattr(self, o):
                    delattr(self, o)
        self._objects_attr : set[str] = set()

        if hasattr(self, '_streams'):
            for s, o in self._streams.items():
                o.destroy()
        self._streams : typing.Dict[str, Stream] = {}

        if hasattr(self, '_periodic_tasks'):
            for t in self._periodic_tasks:
                t.cancel()
        self._periodic_tasks : set[asyncio.Task] = set()

        if hasattr(self, '_monitor'):
            if self._monitor is not None:
                self._monitor.cancel()
        self._monitor = None

    @run_sync
    async def connect(self, host : str | None=None, port : int | None=None, multi : bool | None=None):
        '''Connect to the ZMQ server.'''

        await self._connect(host, port, multi)

        if 'l' in await self.capabilities():
            await self.list()
            await self.find_time()

        self._monitor = asyncio.create_task(self._monitor_socket())

        self.connected.trigger()

    @locked
    async def _connect(self, host : str | None=None, port : int | None=None, multi : bool | None=None):
        if self.is_connected():
            raise RuntimeError('Already connected')

        if host is not None:
            self._host = host
        if port is not None:
            self._port = port
        if multi is not None:
            self._multi = multi

        self._reset()

        self._socket = self._context.socket(zmq.REQ)

        try:
            if self._timeout is not None:
                self.logger.debug(f'using a timeout of {self._timeout} s')
                self._socket.setsockopt(zmq.CONNECT_TIMEOUT, int(self._timeout * 1000))
                self._socket.setsockopt(zmq.RCVTIMEO, int(self._timeout * 1000))
                self._socket.setsockopt(zmq.SNDTIMEO, int(self._timeout * 1000))

            self.logger.debug(f'connect to tcp://{self._host}:{self._port}')
            self._socket.connect(f'tcp://{self._host}:{self._port}')
        except:
            s = self._socket
            self._socket = None
            if s is not None:
                s.close(0)
            raise

    @run_sync
    async def disconnect(self):
        '''Disconnect from the ZMQ server.'''

        try:
            if await self._disconnect():
                self.disconnected.trigger()
        except asyncio.CancelledError:
            raise
        except:
            self.disconnected.trigger()
            raise

    @locked
    async def _disconnect(self) -> bool:
        if not self.is_connected():
            return False

        self.logger.debug('disconnect')

        try:
            assert self._socket is not None
            s = self._socket
            self._socket = None
            s.close(0)
            return True
        finally:
            self._socket = None
            self._reset()

    @run_sync
    async def close(self):
        '''Alias for disconnect().'''
        await self.disconnect()

    def __del__(self):
        if self.is_connected():
            self.disconnect()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        if self.is_connected():
            self.disconnect()

    async def __aenter__(self):
        await self.connect()
        return self

    async def __aexit__(self, *args):
        await self.disconnect()



    ##############################################
    # Low-level req

    @run_sync
    @locked
    async def req(self, msg : bytes | str) -> bytes | str:
        '''Send a request to the ZMQ server and wait for a reply.'''

        if isinstance(msg, str):
            return (await self._req(msg.encode())).decode()
        else:
            return await self._req(msg)

    async def _req(self, msg : bytes) -> bytes:
        if not self.is_connected():
            raise RuntimeError('Not connected')

        assert self._socket is not None
        self.logger.debug('req %s', msg)
        await self._socket.send(msg)
        rep = b''.join(await self._socket.recv_multipart())
        self.logger.debug('rep %s', rep)
        return rep



    ##############################################
    # Simple commands

    @run_sync
    async def capabilities(self) -> str:
        '''Get the capabilities of the connected ZMQ server.'''

        if self._capabilities is None:
            self._capabilities = await self.req('?')
            if self._multi:
                # Remove capabilities that are stateful at the embedded side.
                self._capabilities = re.sub(r'[amstf]', '', self._capabilities)

        return self._capabilities

    @run_sync
    async def echo(self, msg : str) -> str:
        '''Echo a message via the ZMQ server.'''
        if 'e' not in await self.capabilities():
            raise RuntimeError('Echo command not supported')

        return (await self.req(b'e' + msg.encode())).decode()

    @run_sync
    async def identification(self) -> str:
        '''Get the identification string.'''

        if self._identification is not None:
            return self._identification

        if not 'i' in await self.capabilities():
            self._identification = ''
            return self._identification

        try:
            self._identification = (await self.req(b'i')).decode()
        except ValueError:
            self._identification = ''
            pass

        return self._identification

    @run_sync
    async def version(self) -> str:
        '''Get the version string.'''
        if self._version is not None:
            return self._version

        try:
            self._version = (await self.req(b'v')).decode()
        except ValueError:
            self._version = ''
            pass

        return self._version

    @run_sync
    async def read_mem(self, pointer : int, size : int) -> bytearray | None:
        '''Read memory from the connected device.'''

        if 'R' not in await self.capabilities():
            raise RuntimeError('ReadMem command not supported')

        rep = await self.req(f'R{pointer:x} {size}')

        if rep == '?':
            raise RuntimeError('ReadMem command failed')

        if len(rep) & 1:
            # Odd number of bytes.
            raise ValueError('Invalid ReadMem response')

        res = bytearray()
        for i in range(0, len(rep), 2):
            res.append(int(rep[i:i+2], 16))
        return res

    @run_sync
    async def write_mem(self, pointer : int, data : bytearray):
        '''Write memory to the connected device.'''
        if 'W' not in await self.capabilities():
            raise RuntimeError('WriteMem command not supported')

        req = f'W{pointer:x} '
        for i in range(0, len(data)):
            req += f'{data[i]:02x}'
        rep = await self.req(req)
        if rep != '!':
            raise RuntimeError('WriteMem command failed')



    ##############################################
    # Objects

    @property
    def objects(self) -> typing.List[Object]:
        if self._objects is None:
            return list()
        else:
            return list(self._objects)

    @run_sync
    async def list(self) -> typing.List[Object]:
        '''List the objects available.'''

        if not self._objects is None:
            return self.objects

        if 'l' not in await self.capabilities():
            raise RuntimeError('List command not supported')

        res = []
        for o in (await self.req('l')).split('\n'):
            if o == '':
                continue
            obj = Object.create(o, self)
            if obj is None:
                raise ValueError(f'Invalid List response: "{o}"')

            res.append(obj)
            pyname = self._pyname(obj.name)
            setattr(self, pyname, obj)
            self._objects_attr.add(pyname)

        self._objects = res
        return self.objects

    def _pyname(self, name : str) -> str:
        '''Convert an object name to a valid Python attribute name.'''

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

    def find(self, name : str, all=False) -> Object | typing.Set[Object] | None:
        '''
        Find object(s) by name.

        This functions uses the previously retrieved list of objects.
        '''

        if self._objects is None:
            return None

        chunks = name.split('/')
        obj1 = set()
        obj2 = set()
        obj3 = set()
        obj4 = set()
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

    def obj(self, x : str) -> Object:
        '''Get an object by name.'''

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



    ##############################################
    # Time

    def time(self) -> Object | None:
        if isinstance(self._t, Object):
            # Already initialized
            return self._t
        else:
            # Not initialized yet, or could not find.
            return None

    @run_sync
    async def find_time(self) -> Object | None:
        if isinstance(self._t, str):
            # Take the given time variable
            t = self.find(self._t)
        else:
            # Try finding /t (unit)
            t = self.find('/t (')

        # Not initialized.
        self._t = False

        if t is None:
            # Not found, try the first /store/t (unit)
            for o in self.objects:
                chunks = o.name.split('/', 4)
                if len(chunks) != 3:
                    # Strange name
                    continue
                elif chunks[2].startswith('t ('):
                    # Got some
                    t = o
                    break
            if t is None:
                # Still not found. Give up.
                return None
        elif isinstance(t, set):
            # Multiple found. Just take first one.
            t = next(iter(t))
        else:
            # One found
            pass

        try:
            t0 = await t.read()
            self._t0 = time.time()
        except:
            # Cannot read. Give up.
            return None

        # Try parse the unit
        unit = re.sub(r'.*/t \((.*)\)$', r'\1', t.name)
        if unit == 's':
            self._timestamp_to_time = lambda t: float(t - t0) + self._t0
        elif unit == 'ms':
            self._timestamp_to_time = lambda t: float(t - t0) / 1e3 + self._t0
        elif unit == 'us':
            self._timestamp_to_time = lambda t: float(t - t0) / 1e6 + self._t0
        elif unit == 'ns':
            self._timestamp_to_time = lambda t: float(t - t0) / 1e9 + self._t0
        else:
            # Don't know a conversion, just use the raw value.
            self._timestamp_to_time = lambda t: t - t0

        # Make alias permanent.
        await self.alias(t, t.alias.value, False)

        # All set.
        self._t = t
        self.logger.info('time object: %s', t.name)
        return self._t

    def timestamp_to_time(self, t = None):
        if not isinstance(self._t, Object):
            # No time object found.
            return time.time()
        else:
            # Override to implement arbitrary conversion.
            return self._timestamp_to_time(t)



    ##############################################
    # Streams

    @run_sync
    async def streams(self) -> typing.List[str]:
        '''Get the list of available streams.'''

        if 's' not in await self.capabilities():
            return []

        rep = await self.req(b's')
        if rep == b'?':
            return []
        else:
            return list(map(lambda b: chr(b), rep))

    @run_sync
    async def otherStreams(self):
        # TODO
        pass

    def stream(self, s : str, raw : bool=False) -> Stream:
        '''Get a Stream object for the given stream name.'''

        if not isinstance(s, str) or len(s) != 1:
            raise ValueError('Invalid stream name ' + s)

        if s in self._streams:
            return self._streams[s]

        self._streams[s] = Stream(self, s, raw)
        return self._streams[s]



    ##############################################
    # Alias

    @run_sync
    async def alias(self, obj : str | Object, prefer : str | None=None,
                    temporary : bool=True, permanentRef : typing.Any=None) -> str | None:

        '''Assign an alias to an object.'''

        if isinstance(obj, str):
            obj = self.obj(obj)

        async with self._alias_lock:
            if self._available_aliases is None:
                # Not yet initialized
                if 'a' in await self.capabilities():
                    self._available_aliases = list(map(chr, range(0x20, 0x7f)))
                    self._available_aliases.remove('/')
                else:
                    self._available_aliases = []

            if prefer is None and not obj.alias.value is None:
                if temporary != self._is_temporary_alias(obj.alias.value):
                    # Switch type
                    return await self._reassign_alias(obj.alias.value, obj, temporary, permanentRef)
                else:
                    if not temporary:
                        self._inc_permanent_alias(obj.alias.value, permanentRef)
                    # Already assigned one.
                    return obj.alias.value

            if not prefer is None:
                if obj.alias != prefer:
                    # Go assign one.
                    pass
                elif temporary != self._is_temporary_alias(prefer):
                    # Switch type
                    return await self._reassign_alias(prefer, obj, temporary, permanentRef)
                else:
                    if not temporary:
                        self._inc_permanent_alias(prefer, permanentRef)
                    # Already assigned preferred one.
                    return prefer

            if not prefer is None:
                if self._is_alias_available(prefer):
                    return await self._acquire_alias(prefer, obj, temporary, permanentRef)
                elif self._is_temporary_alias(prefer):
                    return await self._reassign_alias(prefer, obj, temporary, permanentRef)
                else:
                    # Cannot reassign permanent alias.
                    return None
            else:
                a = self._get_free_alias()
                if a is None:
                    a = self._get_temporary_alias()
                if a is None:
                    # Nothing free.
                    return None
                return await self._acquire_alias(a, obj, temporary, permanentRef)

    def _is_alias_available(self, a : str) -> bool:
        return self._available_aliases is not None and a in self._available_aliases

    def _is_temporary_alias(self, a : str) -> bool:
        return a in self._temporary_aliases

    def _is_alias_in_use(self, a : str) -> bool:
        return a in self._temporary_aliases or a in self._permanent_aliases

    def _inc_permanent_alias(self, a : str, permanentRef : typing.Any):
        assert a in self._permanent_aliases
        self.logger.debug(f'increment permanent alias {a} use')
        self._permanent_aliases[a][1].append(permanentRef)

    def _dec_permanent_alias(self, a : str, permanentRef : typing.Any):
        assert a in self._permanent_aliases
        if permanentRef is None:
            self.logger.debug(f'ignored decrement permanent alias {a} use')
            return False

        try:
            self._permanent_aliases[a][1].remove(permanentRef)
            self.logger.debug(f'decrement permanent alias {a} use')
        except ValueError:
            # Unknown ref.
            pass

        return self._permanent_aliases[a][1] == []

    async def _acquire_alias(self, a : str, obj : Object, temporary : bool, permanentRef : typing.Any) -> str | None:
        assert not self._is_alias_in_use(a)
        assert self._available_aliases is not None

        if not (isinstance(a, str) and len(a) == 1):
            raise ValueError('Invalid alias ' + a)

        available_upon_rollback = False
        if a in self._available_aliases:
            self.logger.debug('available: ' + ''.join(self._available_aliases))
            self._available_aliases.remove(a)
            available_upon_rollback = True

        if not await self._set_alias(a, obj.name):
            # Too many aliases, apparently. Drop a temporary one.
            tmp = self._get_temporary_alias()
            if tmp is None:
                # Nothing to drop.
                if available_upon_rollback:
                    self._available_aliases.append(a)
                return None

            self._release_alias(tmp)

            # OK, we should have some more space now. Retry.
            if not await self._set_alias(a, obj.name):
                # Still failing, give up.
                if available_upon_rollback:
                    self._available_aliases.append(a)
                return None

        # Success!
        if temporary:
            self.logger.debug(f'new temporary alias {a} for {obj.name}')
            self._temporary_aliases[a] = obj
        else:
            self.logger.debug(f'new permanent alias {a} for {obj.name}')
            self._permanent_aliases[a] = (obj, [permanentRef])
        obj.alias.value = a
        return a

    async def _set_alias(self, a : str, name : str) -> bool:
        rep = await self.req(b'a' + a.encode() + name.encode())
        return rep == b'!'

    async def _reassign_alias(self, a : str, obj : Object, temporary : bool, permanentRef : typing.Any) -> str | None:
        assert a in self._temporary_aliases or a in self._permanent_aliases
        assert not self._is_alias_available(a)
        assert self._available_aliases is not None

        if not await self._set_alias(a, obj.name):
            return None

        if not self._release_alias(a, permanentRef):
            # Not allowed, still is use as permanent alias.
            self.logger.debug(f'cannot release alias {a}; still in use')
            pass
        else:
            if a in self._available_aliases:
                self._available_aliases.remove(a)
            if temporary:
                self.logger.debug(f'reassigned temporary alias {a} to {obj.name}')
                self._temporary_aliases[a] = obj
            else:
                self.logger.debug(f'reassigned permanent alias {a} to {obj.name}')
                self._permanent_aliases[a] = (obj, [permanentRef])

        obj.alias.value = a
        return a

    def _release_alias(self, alias : str, permanentRef : typing.Any = None) -> bool:
        assert self._available_aliases is not None

        obj = None
        if alias in self._temporary_aliases:
            obj = self._temporary_aliases[alias]
            del self._temporary_aliases[alias]
            self.logger.debug(f'released temporary alias {alias}')
        elif alias in self._permanent_aliases:
            if not self._dec_permanent_alias(alias, permanentRef):
                # Do not release (yet).
                return False
            obj = self._permanent_aliases[alias][0]
            del self._permanent_aliases[alias]
            self.logger.debug(f'released permanent alias {alias}')
        else:
            self.logger.debug(f'released unused alias {alias}')

        if not obj is None:
            obj.alias.value = None
            self._available_aliases.append(alias)

        return True

    def _get_free_alias(self) -> str | None:
        if not self._available_aliases:
            return None
        else:
            return self._available_aliases.pop()

    def _get_temporary_alias(self) -> str | None:
        keys = list(self._temporary_aliases.keys())
        if not keys:
            return None
        a = keys[0] # pick oldest one
        self.logger.debug(f'stealing temporary alias {a}')
        self._release_alias(a)
        return a

    @run_sync
    async def release_alias(self, alias : str, permanentRef=None):
        '''Release an alias.'''
        if self._release_alias(alias, permanentRef):
            await self.req(b'a' + alias.encode())

    @run_sync
    async def _print_alias_map(self):
        '''Print the current alias map.'''

        async with self._alias_lock:
            if self._available_aliases is None:
                print("Not initialized")
            else:
                print("Available aliases: " + ''.join(self._available_aliases))

                if len(self._temporary_aliases) == 0:
                    print("No temporary aliases")
                else:
                    print("Temporary aliases:\n\t" + '\n\t'.join([f'{a}: {o.name}' for a,o in self._temporary_aliases.items()]))

                if len(self._permanent_aliases) == 0:
                    print("No permanent aliases")
                else:
                    print("Permanent aliases: \n\t" + '\n\t'.join([f'{a}: {o[0].name} ({len(o[1])})' for a,o in self._permanent_aliases.items()]))



    ##############################################
    # Macro

    @run_sync
    async def macro(self, *args, **kwargs) -> Macro:
        '''Get a free macro, and optionally assign commands to it.'''

        if self._available_macros is None:
            # Not initialized yet.
            capabilities = await self.capabilities()
            if 'm' not in capabilities:
                # Not supported.
                self._available_macros = []
            else:
                self._available_macros = list(map(chr, range(0x20, 0x7f)))
                for c in capabilities:
                    self._available_macros.remove(c)

        if self._available_macros == []:
            mo = Macro(self, None, *args, **kwargs)
        else:
            m = self._available_macros.pop()
            self._used_macros.append(m)
            mo = Macro(self, m, *args, **kwargs)

        self._macros.append(mo)
        return mo

    @run_sync
    async def release_macro(self, m : str | bytes | Macro):
        '''Release a macro.'''

        macro = None
        mo = None
        if isinstance(m, Macro):
            mo = m
            macro = mo.macro
        if isinstance(m, bytes):
            macro = m.decode()

        assert macro is not None or mo is not None
        assert macro is None or isinstance(macro, str)

        if macro in self._used_macros:
            assert self._available_macros is not None
            self._used_macros.remove(macro)
            self._available_macros.append(macro)
            await self.req(b'm' + macro.encode())

        if mo is None:
            assert isinstance(macro, str)
            mb = macro.encode()
            for x in self._macros:
                if x.macro == mb:
                    mo = x
                    break

        if mo in self._macros:
            self._macros.remove(mo)
            mo.destroy()



    ##############################################
    # Poll

    @run_sync
    async def periodic(self, interval_s : float, f : typing.Callable, *args, **kwargs) -> asyncio.Task:
        '''
        Run a function periodically while the client is alive.
        '''

        if not interval_s >= 0:
            raise ValueError('interval_s must be non-negative')

        if not asyncio.iscoroutinefunction(f):
            async def _f(*args, **kwargs):
                f(*args, **kwargs)
            coro = _f
        else:
            coro = f

        task = asyncio.get_running_loop().create_task(self._periodic(interval_s, coro, *args, **kwargs))
        self._periodic_tasks.add(task)
        return task

    async def _periodic(self, interval_s : float, coro : typing.Callable, *args, **kwargs):
        try:
            while self.is_connected():
                t_start = time.time()
                self.logger.debug('periodic task')
                await coro(*args, **kwargs)

                if interval_s == 0:
                    await asyncio.sleep(0)
                else:
                    t_end = time.time()
                    dt = t_end - t_start
                    if dt < interval_s:
                        await asyncio.sleep(interval_s - dt)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.exception('Exception in periodic task: %s', e)
        finally:
            t = asyncio.current_task()
            try:
                if t is not None:
                    self._periodic_tasks.remove(t)
            except:
                pass



    ##############################################
    # Socket monitor

    async def _monitor_socket(self):
        if not self.is_connected():
            self.logger.debug('Not connected, not starting socket monitor')
            return

        assert self._socket is not None
        monitor = self._socket.get_monitor_socket()

        try:
            while self.is_connected():
                event = await monitor.recv_multipart()
                evt = zmq.utils.monitor.parse_monitor_message(event)
                self.logger.debug(f'socket event: {repr(evt['event'])}')
                if evt['event'] == zmq.EVENT_DISCONNECTED:
                    self.logger.info('socket disconnected')
                    await self.disconnect()
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.exception('exception in socket monitor: %s', e)
        finally:
            monitor.close(0)



    ##############################################
    # State
