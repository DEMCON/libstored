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

from .event import Event, Value, ValueWrapper
from ..zmq_server import ZmqServer
from .worker import Work, run_sync

class Object(Work):
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

    def __init__(self, name : str, type : int, size : int, client : ZmqClient):
        super().__init__(worker=client.worker)
        self._name = name
        self._type = type
        self._size = size
        self._client = client
        self._format = None
        self._formatter = None

        self.alias = Value(str)
        self.value = Value(self.value_type())
        self.t = Value(float)
        self.value_str = ValueWrapper(str, self._value_str_get, self._value_str_set)

        self.format = 'default'

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
        if not self.alive():
            raise RuntimeError('Object destroyed, client connection closed')
        assert self._client is not None
        return self._client

    @property
    def name(self):
        return self._name



    #################################################
    # Type

    @property
    def type(self):
        return self._type

    @property
    def size(self):
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
        return self._type & 0x80 == 0

    def is_function(self) -> bool:
        return self._type & self.FlagFunction != 0

    def is_fixed(self) -> bool:
        return self._type & self.FlagFixed != 0

    def is_int(self) -> bool:
        return self.is_fixed() and self._type & self.FlagInt != 0

    def is_signed(self) -> bool:
        return self.is_fixed() and self._type & self.FlagSigned != 0

    def is_special(self) -> bool:
        return self._type & 0x78 == 0

    def type_name(self):
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
        return f'({t})' if self.is_function() else t

    def value_type(self) -> typing.Type:
        dtype = self._type & ~self.FlagFunction
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

    # Return the alias or the normal name, if no alias was set.
    @run_sync
    async def short_name(self, acquire = True) -> str:
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
        name = await self.short_name(acquire_alias)
        rep = await self.client.req(b'r' + name.encode())
        return self.handle_read(rep)

    # Decode a read reply.
    def handle_read(self, rep : bytes, t=None) -> typing.Any:
        if rep == b'?':
            return None
        try:
            self.set(self._decode(rep), t)
        except ValueError:
            pass
        return self.value.value

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
        dtype = self._type & ~self.FlagFunction
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



    ###############################################
    # Write

    @run_sync
    async def write(self, value : typing.Any = None):
        if not value is None:
            self.set(value)

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
        dtype = self._type & ~self.FlagFunction

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
        if type(value) != self.value_type():
            raise TypeError(f'Expected value of type {self.value_type()}, got {type(value)}')

        if t is None:
            t = time.time()

        if not self.t.value is None and t < self.t.value:
            # Old value.
            return

        if not self.is_fixed():
            if isinstance(value, str) or isinstance(value, bytes):
                value = value[0:self.size]

        if isinstance(value, float) and math.isnan(value) and self.value.type == float and math.isnan(self.value.value):
            # Not updated, even though value != self._value would be True
            pass
        elif value != self.value.value:
            self.t.pause()
            self.t.value = t
            self.value.value = value
            self.value_str.trigger()
            self.t.resume()



    ###############################################
    # String conversion

    def _interpret_int(self, value):
        # Remove all group separators. They are irrelevant, but prevent
        # parsing.
        gs = locale.localeconv().get('thousand_sep', '')
        if gs != '':
            value = value.replace(gs, '')

        return locale.atoi(value)

    def _interpret_float(self, value):
        # Remove all group separators. They are irrelevant, but prevent
        # parsing when not at the right place.
        gs = locale.localeconv().get('thousands_sep', '')
        if gs != '':
            value = value.replace(gs, '')

        return locale.atof(value)

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
                self.Void: lambda x: bytes(),
            }.get(self._type & ~self.FlagFunction, lambda x: x)(value)
        return value

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
        return self._format

    @format.setter
    def format(self, f):
        if self._format == f:
            return
        if not f in self.formats():
            raise ValueError(f'Invalid format "{f}"')

        self._format = f

        if f == 'hex':
            self._formatter = lambda x: hex(x & (1 << self._size * 8) - 1)
        elif f == 'bin':
            self._formatter = bin
        elif f == 'bytes' or self._type & ~self.FlagFunction == self.Blob:
            self._formatter = self._format_bytes
        elif self._type & ~self.FlagFunction == self.Float:
            self._formatter = lambda x: self._format_float(x, 'g', 6)
        elif self._type & ~self.FlagFunction == self.Double:
            self._formatter = lambda x: self._format_float(x, 'g', 15)
        elif self._type & self.FlagInt:
            self._formatter = self._format_int
        else:
            self._formatter = str

        self.value_str.trigger()

    def formats(self) -> list[str]:
        f = ['default', 'bytes']
        if self._type & ~self.FlagFunction == self.Blob:
            return f
        if self.is_fixed():
            f += ['hex', 'bin']
        return f

    def _value_str_get(self) -> str:
        if self.value.value is None:
            return ''

        assert self._formatter is not None
        try:
            return self._formatter(self.value.value)
        except:
            return '?'

    def _value_str_set(self, s : str):
        if s == '':
            self.set(None)
        else:
            try:
                self.set(self.interpret(s))
            except:
                self.value_str.trigger()




class ZmqClient(Work):
    '''
    Asynchronous ZMQ client.

    This client can connect to both the libstored.zmq_server.ZmqServer and stored::DebugZmqLayer.
    '''

    def __init__(self, host : str='localhost', port : int=ZmqServer.default_port,
                multi : bool=False, timeout : float | None=None, context : None | zmq.asyncio.Context=None,
                **kwargs):

        super().__init__(**kwargs)
        self._context = context or zmq.asyncio.Context.instance()
        self._host = host
        self._port = port
        self._multi = multi
        self._timeout = timeout if timeout is None or timeout > 0 else None
        self._socket = None
        self._lock = asyncio.Lock()

        self._reset()

        # Events
        self.connected = Event()
        self.disconnected = Event()



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

    def isConnected(self) -> bool:
        '''Check if connected to the ZMQ server.'''
        return self.socket is not None

    def _reset(self):
        self._capabilities = None
        self._identification = None
        self._version = None

        if hasattr(self, '_objects'):
            if not self._objects is None:
                for o in self._objects:
                    o.destroy()
        self._objects = None

        if hasattr(self, '_objects_attr'):
            for o in self._objects_attr:
                if hasattr(self, o):
                    delattr(self, o)
        self._objects_attr = set()

    @run_sync
    async def connect(self, host : str | None=None, port : int | None=None, multi : bool | None=None):
        '''Connect to the ZMQ server.'''

        await self._connect(host, port, multi)

        if 'l' in await self.capabilities():
            await self.list()

        self.connected.trigger()

    @locked
    async def _connect(self, host : str | None=None, port : int | None=None, multi : bool | None=None):
        if self.isConnected():
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
    @locked
    async def disconnect(self):
        '''Disconnect from the ZMQ server.'''

        if not self.isConnected():
            return

        self.logger.debug('disconnect')

        try:
            assert self._socket is not None
            s = self._socket
            self._socket = None
            s.close(0)
        finally:
            self._socket = None
            self._reset()
            self.disconnected.trigger()

    @run_sync
    async def close(self):
        '''Alias for disconnect().'''
        await self.disconnect()

    def __del__(self):
        if self.isConnected():
            self.disconnect()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.disconnect()



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
        if not self.isConnected():
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

    def time(self):
        pass

    def timestampToTime(self, t = None):
        pass



    ##############################################
    # Streams

    def streams(self):
        pass

    def otherStreams(self):
        pass

    def stream(self, s, raw=False):
        pass



    ##############################################
    # Alias

    async def alias(self, o : str | Object) -> str | None:
        if isinstance(o, Object):
            o = o.name

        return o

    ##############################################
    # Macro

    ##############################################
    # Poll

    ##############################################
    # State
