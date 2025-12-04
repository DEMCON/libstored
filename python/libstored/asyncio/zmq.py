# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations
import logging

import aiofiles
import asyncio
import concurrent.futures
import datetime
import filelock
import json
import keyword
import locale
import math
import os
import platformdirs
import re
import struct
import time
import typing
from typing import overload
import zmq
import zmq.asyncio
import zmq.utils.monitor

from .. import libstored_version
from .event import Event, Value, ValueWrapper
from .. import protocol as lprot
from .worker import Work
from ..heatshrink import HeatshrinkDecoder
from .. import exceptions as lexc

class ZmqClientWork(Work):
    def __init__(self, client : ZmqClient, *args, **kwargs):
        super().__init__(worker=client.worker, *args, **kwargs)
        self._client : ZmqClient | None = client

    def alive(self) -> bool:
        '''Check if this object is still alive, i.e. the client connection is still active.'''
        return self._client is not None

    def destroy(self):
        '''Destroy this object, as the client connection is closed.'''
        self._client = None

    def __del__(self):
        self.destroy()

    @property
    def client(self) -> ZmqClient:
        '''The ZmqClient this object belongs to.'''
        if not self.alive():
            raise lexc.Disconnected('Object destroyed, client connection closed')
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
        self.polling = ValueWrapper(float, lambda: self.poll_interval, self._poll_set, event_name=f'{name}/polling')
        self.format = ValueWrapper(str, self._format_get, self._format_set, event_name=f'{name}/format')
        self.format.value = 'default'

    @property
    def name(self) -> str:
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

    @overload
    async def short_name(self, acquire : bool=True) -> str: ...
    @overload
    def short_name(self, acquire : bool=True, *, block : typing.Literal[False]) -> asyncio.Future[str]: ...
    @overload
    def short_name(self, acquire : bool=True, *, sync : typing.Literal[True]) -> str: ...
    @overload
    def short_name(self, acquire : bool=True, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str]: ...

    @ZmqClientWork.run_sync
    async def short_name(self, acquire : bool=True) -> str:
        '''
        Get the alias of this object, or its full name if no alias is set.

        **Arguments**
        * `acquire : bool = True`: try to acquire the alias if it is not set
        * `block : bool = True`: perform a blocking call

        **Result**
        * `str`: the alias of the object, or its full name if no alias is set, when `block = True`
        * otherwise a future with this `str`

        **Raises**
        * `OperationFailed`: when the Alias command failed
        '''

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

    @overload
    async def read(self, acquire_alias : bool=True) -> typing.Any: ...
    @overload
    def read(self, acquire_alias : bool=True, *, block : typing.Literal[False]) -> asyncio.Future[typing.Any]: ...
    @overload
    def read(self, acquire_alias : bool=True, *, sync : typing.Literal[True]) -> typing.Any: ...
    @overload
    def read(self, acquire_alias : bool=True, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[typing.Any]: ...

    @ZmqClientWork.run_sync
    async def read(self, acquire_alias : bool=True) -> typing.Any:
        '''
        Read the value of this object from the server.

        **Arguments**
        * `acquire_alias : bool = True`: try to acquire the alias if it is not set
        * `block : bool = True`: perform a blocking call

        **Result**
        * `Any`: the value of this object when `block = True`
        * otherwise a future with this `Any`

        **Raises**
        * `OperationFailed`: when the read operation failed
        '''
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

    @overload
    async def write(self, value : typing.Any=None) -> None: ...
    @overload
    def write(self, value : typing.Any=None, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def write(self, value : typing.Any=None, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def write(self, value : typing.Any=None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @ZmqClientWork.run_sync
    async def write(self, value : typing.Any=None) -> None:
        '''
        Write a value to this object on the server.

        **Arguments**
        * `value : typing.Any = None`: the value to write
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future

        **Raises**
        * `OperationFailed`: when the write operation failed
        * `ValueError`: when the value cannot be encoded
        '''

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
            raise lexc.OperationFailed('Write failed')

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
            raise TypeError('Invalid type for encoding')
        elif self.is_signed():
            return self._encode_hex(struct.pack('>q', value)[-self._size:], True)
        else:
            if value < 0:
                value += 1 << 64
            return self._encode_hex(struct.pack('>Q', value)[-self._size:], True)

    @overload
    def set(self, value : typing.Any, t : float | None=None) -> None: ...
    @overload
    def set(self, value : typing.Any, t : float | None=None, *, block : typing.Literal[False]) -> concurrent.futures.Future[None] | None: ...

    @ZmqClientWork.thread_safe
    def set(self, value : typing.Any, t : float | None=None) -> None:
        '''
        Set the value of this object, without actually writing it yet to the server.

        **Arguments**
        * `value : typing.Any`: the value to set
        * `t : float | None = None`: the timestamp to set, or None to use the current time
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''

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

        self.t.pause()
        self.t.value = t

        if isinstance(value, float) and math.isnan(value) and self.type == float and self.value is not None and math.isnan(self.value):
            # Not updated, even though value != self._value would be True
            pass
        elif value != self.value:
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

    def _format_get(self):
        '''Get or set the format used to convert the value to a string.'''
        return self._format

    def _format_set(self, f : str):
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
        self.format.trigger()

    def formats(self) -> list[str]:
        '''Get the list of supported formats for this object.'''

        f = ['default', 'bytes']
        if self._type_id & ~self.FlagFunction == self.Blob:
            return f
        if self.is_int():
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
            self.set(self.type(), block=False)
        else:
            try:
                self.set(self.interpret(s), block=False)
            except:
                self.value_str.trigger()



    ###############################################
    # Polling

    @overload
    async def poll(self, interval_s : float | None=None) -> None: ...
    @overload
    def poll(self, interval_s : float | None=None, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def poll(self, interval_s : float | None=None, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def poll(self, interval_s : float | None=None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @ZmqClientWork.run_sync
    async def poll(self, interval_s : float | None=None):
        '''Set up polling of this object.

        If interval_s is None (the default), stop polling.
        If interval_s is 0, poll as fast as possible.
        If interval_s > 0, poll at that interval in seconds.

        **Arguments**
        * `interval_s : float | None`: the polling interval in seconds, or None to stop polling
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''

        if not self.alive():
            raise lexc.InvalidState('Object destroyed, client connection closed')

        if interval_s is not None and interval_s < 0:
            raise ValueError('interval_s must be None or >= 0')

        if interval_s is None:
            pass
        elif isinstance(interval_s, int):
            # Good enough.
            interval_s = float(interval_s)
        elif not isinstance(interval_s, float):
            raise ValueError('interval_s must be None or a float')

        self._poll_slow_stop()
        self._poll_interval_s = interval_s
        await self.client._poll(self, interval_s)
        self.polling.trigger()

    def _poll_set(self, interval_s : float | None):
        self.poll(interval_s, block=False)

    def _poll_slow_stop(self):
        if self._poller is not None:
            self._poller.cancel()
            self._poller = None

    async def _poll_slow(self, interval_s : float):
        self._poll_slow_stop()
        self._poller = self.client.periodic(interval_s, self._read, name=f'poll {self.name}')

    @property
    def poll_interval(self) -> float | None:
        '''Get the current polling interval, or None if not polling.'''
        return self._poll_interval_s



    ###############################################
    # State

    def state(self) -> dict[str, dict[str, typing.Any]]:
        '''Get the state of this object as a JSON-serializable dictionary.'''

        default = True

        s : dict[str, typing.Any] = {}

        if self.format.value != 'default':
            s['format'] = self.format.value
            default = False

        p = self.poll_interval
        if not p is None:
            s['poll_interval'] = p
            default = False

        return {} if default else { self.name: s }

    @overload
    async def restore_state(self, state : dict[str, dict[str, typing.Any]]) -> None: ...
    @overload
    def restore_state(self, state : dict[str, dict[str, typing.Any]], *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def restore_state(self, state : dict[str, dict[str, typing.Any]], *, sync : typing.Literal[True]) -> None: ...
    @overload
    def restore_state(self, state : dict[str, dict[str, typing.Any]], *, block : typing.Literal[True]) -> asyncio.Future[None]: ...

    @ZmqClientWork.run_sync
    async def restore_state(self, state : dict):
        '''
        Restore the state of this object from a dictionary as returned by state().

        **Arguments**
        * `state : dict`: the state dictionary to restore from
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''

        if not self.name in state:
            return

        if not self.alive():
            raise lexc.InvalidState('Object not connected')

        s = state[self.name]

        try:
            if 'format' in s:
                self.format.value = s['format']
        except ValueError:
            pass

        try:
            if 'poll_interval' in s:
                await self.poll(float(s['poll_interval']))
        except ValueError:
            pass



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
            raise lexc.NotSupported('Stream capability missing')

        self._compressed = 'f' in cap
        self._initialized = True
        await self.reset()

    @overload
    async def poll(self, suffix : str='') -> str | bytes | bytearray: ...
    @overload
    def poll(self, suffix : str='', *, block : typing.Literal[False]) -> asyncio.Future[str | bytes | bytearray]: ...
    @overload
    def poll(self, suffix : str='', *, sync : typing.Literal[True]) -> str | bytes | bytearray: ...
    @overload
    def poll(self, suffix : str='', *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str | bytes | bytearray]: ...

    @ZmqClientWork.run_sync
    async def poll(self, suffix : str='') -> str | bytes | bytearray:
        '''
        Poll the stream for new data.

        **Arguments**
        * `suffix : str = ''`: optional suffix to the stream data
        * `block : bool = True`: perform a blocking call

        **Result**
        * `str | bytes | bytearray`: the new data when `block = True`
        * otherwise a future with this `str | bytes | bytearray`
        '''
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

    @overload
    async def flush(self) -> None: ...
    @overload
    def flush(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def flush(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def flush(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @ZmqClientWork.run_sync
    async def flush(self) -> None:
        '''
        Flush the stream, to finalize the compression, if any.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''
        if self._compressed and not self._flushing and not self._finishing:
            self._flushing = True
            await self.client.req(b'f' + self.name.encode())
            self._flushing = False
            self._finishing = True

    @overload
    async def reset(self) -> None: ...
    @overload
    def reset(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def reset(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def reset(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @ZmqClientWork.run_sync
    async def reset(self) -> None:
        '''
        Reset the compressed stream, when compression is enabled.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''
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
        if self.alive() and self._macro is not None:
            self.client.release_macro(self._macro, sync=True, block=False)

        super().__del__()

    @property
    def macro(self) -> bytes | None:
        return self._macro

    @overload
    async def add(self, cmd : str, cb : typing.Callable[[bytes, float | None], None] | None=None, key : typing.Hashable | None=None) -> None: ...
    @overload
    def add(self, cmd : str, cb : typing.Callable[[bytes, float | None], None] | None=None, key : typing.Hashable | None=None, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def add(self, cmd : str, cb : typing.Callable[[bytes, float | None], None] | None=None, key : typing.Hashable | None=None, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def add(self, cmd : str, cb : typing.Callable[[bytes, float | None], None] | None=None, key : typing.Hashable | None=None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @ZmqClientWork.run_sync
    @ZmqClientWork.locked
    async def add(self, cmd : str, cb : typing.Callable[[bytes, float | None], None] | None=None, key : typing.Hashable | None=None):
        '''
        Add a command to this macro.

        **Arguments**
        * `cmd : str`: the command to add
        * `cb : Callable[[bytes, float | None], None] | None = None`: optional callback to handle the response
        * `key : Hashable | None = None`: optional key to identify this command, otherwise an integer key is assigned
        * `block : bool = True`: perform a blocking call

        **Result**
        * `bool`: True if the command was added successfully, False otherwise, when `block = True`
        * otherwise a future with this `bool`
        '''

        if key is None:
            key = self._key
            self._key += 1

        self._cmds[key] = (cmd.encode(), cb)
        try:
            await self._update()
        except lexc.OperationFailed:
            # Update failed, remove command again.
            del self._cmds[key]
            raise

        # Check if it still works...
        if cb is None:
            # No response expected
            return

        try:
            await self._run()
            # Success.
        except RuntimeError:
            # Rollback.
            await self._remove(key)
            raise lexc.OperationFailed('Cannot add to macro')

    @overload
    async def remove(self, key : typing.Hashable) -> bool: ...
    @overload
    def remove(self, key : typing.Hashable, *, block : typing.Literal[False]) -> asyncio.Future[bool]: ...
    @overload
    def remove(self, key : typing.Hashable, *, sync : typing.Literal[True]) -> bool: ...
    @overload
    def remove(self, key : typing.Hashable, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[bool]: ...

    @ZmqClientWork.run_sync
    @ZmqClientWork.locked
    async def remove(self, key : typing.Hashable) -> bool:
        '''
        Remove a command from this macro.

        **Arguments**
        * `key : Hashable`: the key of the command to remove
        * `block : bool = True`: perform a blocking call

        **Result**
        * `bool`: True if the command was removed successfully, when `block = True`
        * otherwise a future with this `bool`
        '''
        return await self._remove(key)

    async def _remove(self, key : typing.Hashable) -> bool:
        if key in self._cmds:
            del self._cmds[key]
            await self._update()
            return True
        else:
            return False

    @overload
    async def clear(self) -> None: ...
    @overload
    def clear(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def clear(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def clear(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @ZmqClientWork.run_sync
    @ZmqClientWork.locked
    async def clear(self) -> None:
        '''
        Clear all commands from this macro.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''
        await self._clear()

    async def _clear(self):
        self._cmds.clear()
        if self.client.is_connected():
            await self._update()

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
        if await self.client.req(definition) != b'!':
            raise lexc.OperationFailed('Macro definition failed')

    @overload
    async def run(self) -> None: ...
    @overload
    def run(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def run(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def run(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @ZmqClientWork.run_sync
    @ZmqClientWork.locked
    async def run(self):
        '''
        Run this macro.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future with this `bool`
        '''
        await self._run()

    async def _run(self):
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
            raise lexc.InvalidResponse('Unexpected number of responses')

        for i in range(0, len(values)):
            f = cb[i + skip]
            if f is not None:
                f(values[i], t)

        return True

    def __len__(self):
        return len(self._cmds)

    def __getitem__(self, key: typing.Hashable):
        return self._cmds[key]

    def __iter__(self):
        return iter(self._cmds)



class Tracing(Macro):
    """Tracing command handling"""

    def __init__(self, client : ZmqClient, stream : str='t', poll_interval_s : float=0, *args, **kwargs):
        super().__init__(client=client, reqsep=b'\r', repsep=b';', *args, **kwargs)

        self._poll_interval_s : float = poll_interval_s
        self._stream : Stream | str = stream
        self._enabled : bool | None = None
        self._decimate : int = 1
        self._partial : bytearray = bytearray()
        self._task : asyncio.Task | None = None

    async def _init(self):
        if self._enabled is not None:
            return

        self._enabled = False

        if not self.client.is_connected():
            raise lexc.InvalidState('Client not connected')

        try:
            cap = await self.client.capabilities()
            if 't' not in cap:
                raise lexc.NotSupported('Tracing capability missing')
            if 'm' not in cap:
                raise lexc.NotSupported('Macro capability missing')
            if 'e' not in cap:
                raise lexc.NotSupported('Echo capability missing')
            if 's' not in cap:
                raise lexc.NotSupported('Stream capability missing')

            if isinstance(self._stream, str):
                self._stream = self.client.stream(self._stream, raw=True)

            assert isinstance(self._stream, Stream)

            # Start with sample separator.
            try:
                await self.add('e\n', None, 'e')
            except lexc.OperationFailed:
                raise lexc.NotSupported('Cannot add echo command for tracing')

            # We must have a macro, not a simulated Macro instance.
            if self.macro is None:
                raise lexc.NotSupported('Cannot get macro for tracing')

            t = self.client.time()
            if t is None:
                raise lexc.NotSupported('Cannot determine time stamp variable')

            try:
                await self.add(f'r{await t.short_name()}', None, 't')
            except lexc.OperationFailed:
                raise lexc.NotSupported('Cannot add time stamp command for tracing')

            await self._update_tracing(True)
        except:
            self._enabled = False
            raise

    def __del__(self):
        try:
            self._enabled = False
            if self.client.is_connected():
                self.client.req(b't', sync=True, block=False)
        except:
            pass

    async def _update(self):
        await super()._update()

        # Remove existing samples from buffer, as the layout is changing.
        if self._enabled:
            assert isinstance(self._stream, Stream)
            await self._stream.reset()
            self._partial = bytearray()

        await self._update_tracing()

    async def _update_tracing(self, force=False):
        await self._init()

        enable = len(self) > 0

        if (force or self._enabled) and not enable:
            self._enabled = False
            await self.client.req(b't')
            if self._task is not None:
                self._task.cancel()
                self._task = None
        elif (force or not self._enabled) and enable:
            macro = self.macro
            assert macro is not None
            assert isinstance(self._stream, Stream)

            rep = await self.client.req(b't' + macro + self._stream.name.encode() + ('%x' % self.decimate).encode())
            if rep != b'!':
                raise lexc.NotSupported('Cannot configure tracing')

            await self._stream.reset()
            self._partial = bytearray()
            self._enabled = True

        if self._enabled and (self._task is None or self._task.done()):
            if self._task is not None:
                self._task.cancel()
                self._task = None

            self._task = self.client.periodic(self._poll_interval_s, self._process, name='tracing')

    async def _clear(self):
        await super()._clear()
        await self._update_tracing()

    @property
    def enabled(self) -> bool:
        return self._enabled is True

    @property
    def decimate(self) -> int:
        return self._decimate

    @Macro.run_sync
    @Macro.locked
    async def set_decimate(self, decimate: int):
        if decimate < 1:
            decimate = 1
        elif decimate > 0x7fffffff:
            # Limit it somewhat to stay within 32 bit
            decimate = 0x7fffffff

        self._decimate = decimate
        await self._update_tracing(True)

    @property
    def stream(self) -> Stream | None:
        return self._stream if isinstance(self._stream, Stream) else None

    @overload
    async def process(self) -> None: ...
    @overload
    def process(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def process(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def process(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Macro.run_sync
    @Macro.locked
    async def process(self):
        '''Process new samples from the stream.

        This function is called automatically when polling is enabled.
        It can also be called manually to process samples immediately.
        '''
        await self._process()

    @Macro.locked
    async def _process(self):
        if not self.enabled:
            return
        assert isinstance(self._stream, Stream)
        x = await self._stream.poll()
        assert not isinstance(x, str)
        self._process_data(x)

    def _process_data(self, s : bytes | bytearray):
        samples = (self._partial + s).split(b'\n;')
        self._partial = samples[-1]
        time = self.client.time()
        assert time is not None

        for sample in samples[0:-1]:
            # The first value is the time stamp.
            t_data = sample.split(b';', 1)
            if len(t_data) < 2:
                # Empty sample.
                continue
            t = time._decode(t_data[0])
            if t is None:
                continue
            ts = self.client.timestamp_to_time(t)
            time.set(t, ts)
            super().decode(t_data[1], ts, skip=2)

    def __len__(self):
        # Don't count sample separator and time stamp.
        return max(0, super().__len__() - 2)



class ZmqClient(Work):
    '''
    Asynchronous ZMQ client.

    This client can connect to both the libstored.zmq_server.ZmqServer and stored::DebugZmqLayer.
    '''

    def __init__(self, host : str='localhost', port : int=lprot.default_port,
                multi : bool=False, timeout : float | None=None, context : None | zmq.asyncio.Context=None,
                t : str | None = None, use_state : str | None=None,
                stack : str | lprot.ProtocolLayer | None=None,
                *args, **kwargs):

        super().__init__(*args, **kwargs)
        self._context = context or zmq.asyncio.Context.instance()
        self._host = host
        self._port = port
        self._multi = multi
        self._timeout = timeout if timeout is None or timeout > 0 else None
        self._socket = None
        self._alias_lock = lexc.DeadlockChecker(asyncio.Lock())
        self._t : str | Object | None | bool = t
        self._t0 : float = 0
        self._timestamp_to_time = lambda t: float(t)
        self._use_state = use_state

        if isinstance(stack, str):
            self._stack = lprot.build_stack(stack)
        elif isinstance(stack, lprot.ProtocolLayer):
            self._stack = stack
        else:
            self._stack = lprot.ProtocolLayer()
        self._stack_encoded : bytearray | None = None
        self._stack_decoded : bytearray | None = None
        self._stack.up = self._stack_up
        self._stack.down = self._stack_down


        self._reset()

        # Events
        self.connecting = Event('connecting')
        self.connected = Event('connected')
        self.disconnecting = Event('disconnecting')
        self.disconnected = Event('disconnected')



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
        self._capabilities : str | None = None
        self._identification : str | None = None
        self._version : str | None = None

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
        self._monitor : asyncio.Task | None = None

        if hasattr(self, '_req_task'):
            if self._req_task is not None:
                self._req_task.cancel()
        self._req_task : asyncio.Task | None = None

        if hasattr(self, '_fast_poll_task'):
            if self._fast_poll_task is not None:
                self._fast_poll_task.cancel()
        self._fast_poll_task : asyncio.Task | None = None
        self._fast_poll_macro : Macro | None = None
        self._fast_poll_interval_s : float = self.fast_poll_threshold_s

        self._tracing : Tracing | bool | None = None

    @overload
    async def connect(self, host : str | None=None, port : int | None=None, \
                      multi : bool | None=None, default_state : bool=False) -> None: ...
    @overload
    def connect(self, host : str | None=None, port : int | None=None, \
                multi : bool | None=None, default_state : bool=False, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def connect(self, host : str | None=None, port : int | None=None, \
                multi : bool | None=None, default_state : bool=False, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def connect(self, host : str | None=None, port : int | None=None, \
                multi : bool | None=None, default_state : bool=False, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Work.run_sync
    async def connect(self, host : str | None=None, port : int | None=None, \
                      multi : bool | None=None, default_state : bool=False):
        '''
        Connect to the ZMQ server.

        **Arguments**
        * `host : str | None = None`: the host to connect to, or None to use the configured host
        * `port : int | None = None`: the port to connect to, or None to use the configured port
        * `multi : bool | None = None`: whether to use multi-client safe commands, or None to use the configured value
        * `default_state : bool = False`: if True, do not restore the state from the state file
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''

        await self._connect(host, port, multi)

        if 'l' in await self.capabilities():
            await self.list()
            await self.find_time()

        if 'm' in await self.capabilities():
            # Clear all existing macros.
            macros = await self.req('m')
            if macros != '?':
                for m in macros:
                    await self.req(f'm{m}')

        self.connected.trigger()

        if not default_state:
            await self.restore_state()

    @Work.locked
    async def _connect(self, host : str | None=None, port : int | None=None, multi : bool | None=None):
        if self.is_connected():
            raise lexc.InvalidState('Already connected')

        if host is not None:
            self._host = host
        if port is not None:
            self._port = port
        if multi is not None:
            self._multi = multi

        self._reset()

        self._socket = self._context.socket(zmq.REQ)
        self.connecting.trigger()

        try:
            if self._timeout is not None:
                self.logger.debug(f'using a timeout of {self._timeout} s')
                self._socket.setsockopt(zmq.CONNECT_TIMEOUT, int(self._timeout * 1000))
                self._socket.setsockopt(zmq.RCVTIMEO, int(self._timeout * 1000))
                self._socket.setsockopt(zmq.SNDTIMEO, int(self._timeout * 1000))

            self.logger.debug(f'connect to tcp://{self._host}:{self._port}')
            self._socket.connect(f'tcp://{self._host}:{self._port}')
            self._monitor = asyncio.create_task(self._monitor_socket(), name=f'{self.__class__.__name__} monitor')
        except:
            s = self._socket
            self._socket = None
            if s is not None:
                s.close(0)
            raise

    @overload
    async def disconnect(self) -> None: ...
    @overload
    def disconnect(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def disconnect(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def disconnect(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Work.run_sync
    async def disconnect(self):
        '''
        Disconnect from the ZMQ server.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''

        s = self._socket

        if s is None:
            # Not connected
            return

        self.logger.debug('disconnect')
        self.disconnecting.trigger()

        await self.save_state()
        self._socket = None
        try:
            s.close(0)
        except asyncio.CancelledError:
            pass
        except:
            pass

        async with self.lock:
            self._reset()

        self.disconnected.trigger()

    @overload
    async def close(self) -> None: ...
    @overload
    def close(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def close(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def close(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Work.run_sync
    async def close(self):
        '''Alias for disconnect().'''
        await self.disconnect()

    def __del__(self):
        if self.is_connected():
            self.disconnect(sync=True)

    def sync(self) -> SyncZmqClient:
        s = SyncZmqClient(self)
        s.connect()
        return s

    def __enter__(self):
        return self.sync()

    def __exit__(self, *args):
        if self.is_connected():
            self.disconnect(sync=True)

    async def __aenter__(self):
        await self.connect()
        return self

    async def __aexit__(self, *args):
        await self.disconnect()



    ##############################################
    # Low-level req

    @overload
    async def req(self, msg : bytes) -> bytes: ...
    @overload
    async def req(self, msg : str) -> str: ...
    @overload
    def req(self, msg : bytes, *, block : typing.Literal[False]) -> asyncio.Future[bytes]: ...
    @overload
    def req(self, msg : str, *, block : typing.Literal[False]) -> asyncio.Future[str]: ...
    @overload
    def req(self, msg : bytes, *, sync : typing.Literal[True]) -> bytes: ...
    @overload
    def req(self, msg : str, *, sync : typing.Literal[True]) -> str: ...
    @overload
    def req(self, msg : bytes, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[bytes]: ...
    @overload
    def req(self, msg : str, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str]: ...

    @Work.run_sync
    @Work.locked
    async def req(self, msg : bytes | str) -> bytes | str:
        '''
        Send a request to the ZMQ server and wait for a reply.

        **Arguments**
        * `msg : bytes | str`: the request message to send
        * `block : bool = True`: perform a blocking call

        **Result**
        * `bytes`: the reply message when the type of `msg` is `bytes` and `block = True`
        * `str`: the reply message when the type of `msg` is `str` and `block = True`
        * otherwise a future with this `bytes | str`

        **Raises**
        * `InvalidState`: when not connected
        * `Disconnected`: when the connection was lost during the request
        * `OperationFailed`: when the request failed or interrupted
        '''

        if len(msg) == 0:
            raise ValueError('Empty request')

        try:
            if isinstance(msg, str):
                self._req_task = asyncio.create_task(self._req(msg.encode()), name=f'{self.__class__.__name__} req')
                return (await self._req_task).decode()
            else:
                self._req_task = asyncio.create_task(self._req(msg), name=f'{self.__class__.__name__} req')
                return await self._req_task
        except asyncio.CancelledError:
            if self._req_task is not None:
                t = asyncio.current_task()
                assert t is not None

                if t.cancelled():
                    # We are cancelled, so cancel the req task too.
                    self._req_task.cancel()
                elif self._req_task.cancelled():
                    # The exception is not due to us being cancelled.  Someone
                    # just aborted the req.  Raise another exception instead.
                    if not self.is_connected():
                        raise lexc.Disconnected('Request aborted')
                    else:
                        raise lexc.OperationFailed('Request aborted')

            raise
        finally:
            self._req_task = None

    def _stack_clear(self) -> None:
        self._stack_encoded = None
        self._stack_decoded = None

    def _stack_up(self, data : lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if self._stack_decoded is None:
            self._stack_decoded = bytearray(data)
        else:
            self._stack_decoded.extend(data)

    def _stack_down(self, data : lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if self._stack_encoded is None:
            self._stack_encoded = bytearray(data)
        else:
            self._stack_encoded.extend(data)

    async def _req(self, msg : bytes) -> bytes:
        if not self.is_connected():
            raise lexc.InvalidState('Not connected')

        assert self._socket is not None

        self._stack_clear()
        await self._stack.encode(msg)
        if self._stack_encoded is None:
            raise lexc.OperationFailed('Stack did not produce data')

        if self.logger.getEffectiveLevel() <= logging.DEBUG:
            if self._stack_encoded != msg:
                self.logger.debug('req %s -> %s', msg, bytes(self._stack_encoded))
            else:
                self.logger.debug('req %s', msg)

        await self._socket.send(self._stack_encoded)

        rep = b''.join(await self._socket.recv_multipart())
        await self._stack.decode(rep)
        if rep and self._stack_decoded is None:
            raise lexc.InvalidResponse('Stack did not decode data')

        decoded = bytes(self._stack_decoded) if self._stack_decoded is not None else b''

        if self.logger.getEffectiveLevel() <= logging.DEBUG:
            if self._stack_decoded != rep:
                self.logger.debug('rep %s <- %s', decoded, rep)
            else:
                self.logger.debug('rep %s', decoded)

        return decoded



    ##############################################
    # Simple commands

    @overload
    async def capabilities(self) -> str: ...
    @overload
    def capabilities(self, *, block : typing.Literal[False]) -> asyncio.Future[str]: ...
    @overload
    def capabilities(self, *, sync : typing.Literal[True]) -> str: ...
    @overload
    def capabilities(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str]: ...

    @Work.run_sync
    async def capabilities(self) -> str:
        '''
        Get the capabilities of the connected ZMQ server.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `str` with the capabilities when `block = True`
        * otherwise a future
        '''

        if self._capabilities is None:
            self._capabilities = await self.req('?')
            assert self._capabilities is not None
            if self._multi:
                # Remove capabilities that are stateful at the embedded side.
                self._capabilities = re.sub(r'[amstf]', '', self._capabilities)

        return self._capabilities

    @overload
    async def echo(self, msg : str) -> str: ...
    @overload
    def echo(self, msg : str, *, block : typing.Literal[False]) -> asyncio.Future[str]: ...
    @overload
    def echo(self, msg : str, *, sync : typing.Literal[True]) -> str: ...
    @overload
    def echo(self, msg : str, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str]: ...

    @Work.run_sync
    async def echo(self, msg : str) -> str:
        '''
        Echo a message via the ZMQ server.

        **Arguments**
        * `msg : str`: the message to echo
        * `block : bool = True`: perform a blocking call

        **Result**
        * `str`: the echoed message when `block = True`
        * otherwise a future with this `str`

        **Raises**
        * `NotSupported`: when the echo command is not supported by the server
        '''
        if 'e' not in await self.capabilities():
            raise lexc.NotSupported('Echo command not supported')

        return (await self.req(b'e' + msg.encode())).decode()

    @overload
    async def identification(self) -> str: ...
    @overload
    def identification(self, *, block : typing.Literal[False]) -> asyncio.Future[str]: ...
    @overload
    def identification(self, *, sync : typing.Literal[True]) -> str: ...
    @overload
    def identification(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str]: ...

    @Work.run_sync
    async def identification(self) -> str:
        '''
        Get the identification string.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `str`: the identification string, which is empty when not supported, when `block = True`
        * otherwise a future with this `str`
        '''

        if self._identification is not None:
            return self._identification

        if not 'i' in await self.capabilities():
            self._identification = ''
            return self._identification

        try:
            self._identification = (await self.req(b'i')).decode()
        except ValueError:
            self._identification = ''

        assert self._identification is not None
        return self._identification

    @overload
    async def version(self) -> str: ...
    @overload
    def version(self, *, block : typing.Literal[False]) -> asyncio.Future[str]: ...
    @overload
    def version(self, *, sync : typing.Literal[True]) -> str: ...
    @overload
    def version(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str]: ...

    @Work.run_sync
    async def version(self) -> str:
        '''
        Get the version string.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `str`: the version string, which is empty when not supported, when `block = True`
        * otherwise a future with this `str`
        '''
        if self._version is not None:
            return self._version

        try:
            self._version = (await self.req(b'v')).decode()
        except ValueError:
            self._version = ''

        assert self._version is not None
        return self._version

    @overload
    async def read_mem(self, pointer : int, size : int) -> bytearray: ...
    @overload
    def read_mem(self, pointer : int, size : int, *, block : typing.Literal[False]) -> asyncio.Future[bytearray]: ...
    @overload
    def read_mem(self, pointer : int, size : int, *, sync : typing.Literal[True]) -> bytearray: ...
    @overload
    def read_mem(self, pointer : int, size : int, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[bytearray]: ...

    @Work.run_sync
    async def read_mem(self, pointer : int, size : int) -> bytearray:
        '''
        Read memory from the connected device.

        **Arguments**
        * `pointer : int`: the memory address to read from
        * `size : int`: the number of bytes to read
        * `block : bool = True`: perform a blocking call

        **Result**
        * `bytearray`: the read data when `block = True`
        * otherwise a future with this `bytearray`

        **Raises**
        * `NotSupported`: when the ReadMem command is not supported by the server
        * `OperationFailed`: when the ReadMem command failed
        '''

        if 'R' not in await self.capabilities():
            raise lexc.NotSupported('ReadMem command not supported')

        rep = await self.req(f'R{pointer:x} {size}')

        if rep == '?':
            raise lexc.OperationFailed('ReadMem command failed')

        if len(rep) & 1:
            # Odd number of bytes.
            raise lexc.OperationFailed('Invalid ReadMem response')

        res = bytearray()
        for i in range(0, len(rep), 2):
            res.append(int(rep[i:i+2], 16))
        return res

    @overload
    async def write_mem(self, pointer : int, data : bytearray) -> None: ...
    @overload
    def write_mem(self, pointer : int, data : bytearray, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def write_mem(self, pointer : int, data : bytearray, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def write_mem(self, pointer : int, data : bytearray, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Work.run_sync
    async def write_mem(self, pointer : int, data : bytearray):
        '''
        Write memory to the connected device.

        **Arguments**
        * `pointer : int`: the memory address to write to
        * `data : bytearray`: the data to write
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future

        **Raises**
        * `NotSupported`: when the WriteMem command is not supported by the server
        * `OperationFailed`: when the WriteMem command failed
        '''
        if 'W' not in await self.capabilities():
            raise lexc.NotSupported('WriteMem command not supported')

        req = f'W{pointer:x} '
        for i in range(0, len(data)):
            req += f'{data[i]:02x}'
        rep = await self.req(req)
        if rep != '!':
            raise lexc.OperationFailed('WriteMem command failed')



    ##############################################
    # Objects

    @property
    def objects(self) -> typing.List[Object]:
        if self._objects is None:
            return list()
        else:
            return list(self._objects)

    @overload
    async def list(self) -> typing.List[Object]: ...
    @overload
    def list(self, *, block : typing.Literal[False]) -> asyncio.Future[typing.List[Object]]: ...
    @overload
    def list(self, *, sync : typing.Literal[True]) -> typing.List[Object]: ...
    @overload
    def list(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[typing.List[Object]]: ...

    @Work.run_sync
    async def list(self) -> typing.List[Object]:
        '''
        List the objects available.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `List[Object]`: the list of objects when `block = True`
        * otherwise a future with this `List[Object]`

        **Raises**
        * `NotSupported`: when the List command is not supported by the server
        * `InvalidResponse`: when the List command returned an invalid response
        '''

        if not self._objects is None:
            return self.objects

        if 'l' not in await self.capabilities():
            raise lexc.NotSupported('List command not supported')

        res = []
        for o in (await self.req('l')).split('\n'):
            if o == '':
                continue
            obj = Object.create(o, self)
            if obj is None:
                raise lexc.InvalidResponse(f'Invalid List response: "{o}"')

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

    @overload
    async def find_time(self) -> Object | None: ...
    @overload
    def find_time(self, *, block : typing.Literal[False]) -> asyncio.Future[Object | None]: ...
    @overload
    def find_time(self, *, sync : typing.Literal[True]) -> Object | None: ...
    @overload
    def find_time(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[Object | None]: ...

    @Work.run_sync
    async def find_time(self) -> Object | None:
        '''
        Find the time object.
        It should start with `/t`, and have a unit between parentheses, like `/t (s)`.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `Object | None`: the time object when found, or None when not found, when `block = True`
        * otherwise a future with this `Object | None`
        '''
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
                else:
                    continue

                if not o.is_fixed():
                    # Not a compatible type for time.
                    continue
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
            self._timestamp_to_time = lambda t: float(t - t0)

        # Make alias permanent.
        await self.alias(t, t.alias.value, False)

        # All set.
        self._t = t
        self.logger.info('time object: %s', t.name)
        return self._t

    def timestamp_to_time(self, t : float | None=None) -> float:
        if not isinstance(self._t, Object):
            # No time object found.
            return time.time()
        else:
            # Override to implement arbitrary conversion.
            return self._timestamp_to_time(t if t is not None else self._t.value)



    ##############################################
    # Streams

    @overload
    async def streams(self) -> typing.List[str]: ...
    @overload
    def streams(self, *, block : typing.Literal[False]) -> asyncio.Future[typing.List[str]]: ...
    @overload
    def streams(self, *, sync : typing.Literal[True]) -> typing.List[str]: ...
    @overload
    def streams(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[typing.List[str]]: ...

    @Work.run_sync
    async def streams(self) -> typing.List[str]:
        '''
        Get the list of available streams.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `List[str]`: the list of stream names when `block = True`
        * otherwise a future with this `List[str]`
        '''

        if 's' not in await self.capabilities():
            return []

        rep = await self.req(b's')
        if rep == b'?':
            return []
        else:
            return list(map(lambda b: chr(b), rep))

    @Work.run_sync
    async def other_streams(self):
        streams = await self.streams()
        if isinstance(self._tracing, Tracing):
            s = self._tracing.stream
            if s:
                try:
                    streams.remove(s.name)
                except ValueError:
                    pass

        return streams

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

    @overload
    async def alias(self, obj : str | Object, prefer : str | None=None,
                    temporary : bool=True, permanentRef : typing.Any=None) -> str | None: ...
    @overload
    def alias(self, obj : str | Object, prefer : str | None=None,
              temporary : bool=True, permanentRef : typing.Any=None, *, block : typing.Literal[False]) -> asyncio.Future[str | None]: ...
    @overload
    def alias(self, obj : str | Object, prefer : str | None=None,
              temporary : bool=True, permanentRef : typing.Any=None, *, sync : typing.Literal[True]) -> str | None: ...
    @overload
    def alias(self, obj : str | Object, prefer : str | None=None,
              temporary : bool=True, permanentRef : typing.Any=None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str | None]: ...

    @Work.run_sync
    async def alias(self, obj : str | Object, prefer : str | None=None,
                    temporary : bool=True, permanentRef : typing.Any=None) -> str | None:

        '''
        Assign an alias to an object.

        **Arguments**
        * `obj : str | Object`: the object to assign an alias to, or its name
        * `prefer : str | None = None`: the preferred alias, or None to get any available one
        * `temporary : bool = True`: if True, assign a temporary alias, otherwise a permanent one
        * `permanentRef : Any = None`: when assigning a permanent alias, a reference to the user of this alias.

        **Result**
        * `str | None`: the assigned alias, or None when no alias could be assigned, when `block = True`
        * otherwise a future with this `str | None`
        '''

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

        if not self._release_alias(a, permanentRef):
            # Not allowed, still is use as permanent alias.
            self.logger.debug(f'cannot release alias {a}; still in use')
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

    @overload
    async def release_alias(self, alias : str, permanentRef=None) -> None: ...
    @overload
    def release_alias(self, alias : str, permanentRef=None, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def release_alias(self, alias : str, permanentRef=None, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def release_alias(self, alias : str, permanentRef=None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Work.run_sync
    async def release_alias(self, alias : str, permanentRef=None):
        '''
        Release an alias.

        **Arguments**
        * `alias : str`: the alias to release
        * `permanentRef : Any = None`: when releasing a permanent alias, a reference to the user of this alias.

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''
        if self._release_alias(alias, permanentRef):
            await self.req(b'a' + alias.encode())

    @Work.run_sync
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

    @overload
    async def acquire_macro(self) -> str | None: ...
    @overload
    def acquire_macro(self, *, block : typing.Literal[False]) -> asyncio.Future[str | None]: ...
    @overload
    def acquire_macro(self, *, sync : typing.Literal[True]) -> str | None: ...
    @overload
    def acquire_macro(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[str | None]: ...

    @Work.run_sync
    async def acquire_macro(self) -> str | None:
        '''
        Get a free macro name.

        In case there is no available macro name, `None` is returned.  This can
        still be used to create a Macro object, although it is not that
        efficient.

        **Arguments**
        * `block : bool = True`: perform a blocking call

        **Result**
        * `str | None`: the macro name when `block = True`
        * otherwise a future with this `str | None`
        '''

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
            return None
        else:
            m = self._available_macros.pop()
            self._used_macros.append(m)
            return m

    def macro(self, name : str | None, *args, **kwargs) -> Macro:
        '''
        Create a macro object for the given macro name.
        '''

        mo = Macro(self, name, *args, **kwargs)
        self._macros.append(mo)
        return mo

    @overload
    async def release_macro(self, m : str | bytes | Macro) -> None: ...
    @overload
    def release_macro(self, m : str | bytes | Macro, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def release_macro(self, m : str | bytes | Macro, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def release_macro(self, m : str | bytes | Macro, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Work.run_sync
    async def release_macro(self, m : str | bytes | Macro):
        '''
        Release a macro.

        **Arguments**
        * `m : str | bytes | Macro`: the macro to release, or its name
        * `block : bool = True`: perform a blocking call

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''

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

    @overload
    def periodic(self, interval_s : float, f : typing.Callable, *args, name : str | None=None) -> asyncio.Task: ...
    @overload
    def periodic(self, interval_s : float, f : typing.Callable, *args, name : str | None=None, block : typing.Literal[False]) -> concurrent.futures.Future[asyncio.Task] | asyncio.Task: ...

    @Work.thread_safe
    def periodic(self, interval_s : float, f : typing.Callable, *args, name : str | None=None) -> asyncio.Task:
        '''
        Run a function periodically while the client is alive.

        **Arguments**
        * `interval_s : float`: the interval in seconds between calls; if 0, run as often as possible
        * `f : Callable`: the function to call periodically; can be a coroutine or a normal function
        * `name : str | None = None`: the name of the periodic task
        * `*args`: arguments to pass to `f`
        * `block : bool = True`: perform a blocking call

        **Result**
        * `asyncio.Task`: the created periodic task when `block = True`
        * otherwise a future with this `asyncio.Task`
        '''

        if not interval_s >= 0:
            raise ValueError('interval_s must be non-negative')

        if not asyncio.iscoroutinefunction(f):
            async def _f(*args):
                f(*args)
            coro = _f
        else:
            coro = f

        task = asyncio.create_task(self._periodic(interval_s, coro, *args), name=name)
        self._periodic_tasks.add(task)
        return task

    async def _periodic(self, interval_s : float, coro : typing.Callable, *args, **kwargs):
        name = ''
        try:
            task = asyncio.current_task()
            assert task is not None
            name = task.get_name()
            if name != '':
                name = ' ' + name

            t = time.time()
            while self.is_connected():
                self.logger.debug('periodic task%s', name)
                await coro(*args, **kwargs)

                if interval_s == 0:
                    await asyncio.sleep(0)
                else:
                    t += interval_s
                    now = time.time()
                    rem = t - now
                    if rem <= 0:
                        t = now
                        await asyncio.sleep(0)
                    else:
                        await asyncio.sleep(rem)
        except asyncio.CancelledError:
            pass
        except lexc.Disconnected as e:
            self.logger.debug('periodic task%s stopped; %s', name, e)
            pass
        except lexc.InvalidState as e:
            if self.is_connected():
                self.logger.exception('exception in periodic task%s: %s', name, e)
        except Exception as e:
            self.logger.exception('exception in periodic task%s: %s', name, e)
        finally:
            t = asyncio.current_task()
            try:
                if t is not None:
                    self._periodic_tasks.remove(t)
            except:
                pass

    trace_threshold_s = 0.1
    trace_poll_interval_s = 0.5
    fast_poll_threshold_s = 0.9
    slow_poll_threshold_s = 1.0

    async def _poll(self, o : Object, interval_s : float | None):
        if o not in self.objects:
            raise ValueError('Object not managed by this client')

        if interval_s is None:
            # Stop slow polling, if any.
            o._poll_slow_stop()

            # Stop fast polling, if any.
            if self._fast_poll_macro is not None and o in self._fast_poll_macro:
                await self._fast_poll_macro.remove(o)
                await self.alias(o, temporary=True, permanentRef=self._fast_poll_macro)

                if len(self._fast_poll_macro) == 0:
                    await self._poll_fast_stop()

            # Stop trace, if any
            if isinstance(self._tracing, Tracing) and o in self._tracing:
                await self._tracing.remove(o)
                await self.alias(o, temporary=True, permanentRef=self._tracing)

            # All stopped.
            return

        if interval_s < 0:
            interval_s = 0

        if interval_s <= self.trace_threshold_s:
            if await self._trace(o, interval_s):
                # Success.
                return

        if interval_s <= self.fast_poll_threshold_s:
            if await self._poll_fast(o, interval_s):
                # Success.
                return

        # Fallback to slow polling.
        await o._poll_slow(max(interval_s, self.slow_poll_threshold_s))

    async def _poll_fast_stop(self):
        if self._fast_poll_task is not None:
            t = self._fast_poll_task
            self._fast_poll_task = None
            t.cancel()

    async def _poll_fast(self, o : Object, interval_s : float):
        t = self.time()
        if t is None:
            # Cannot do fast polling without time object.
            return False

        if self._fast_poll_macro is None:
            self._fast_poll_macro = self.macro(await self.acquire_macro())
            await self._fast_poll_macro.add(cmd=f'r{await t.short_name()}', cb=t.handle_read, key=self)

        assert self._fast_poll_macro is not None
        if o not in self._fast_poll_macro:
            await self.alias(o, temporary=False, permanentRef=self._fast_poll_macro)
            a = await o.short_name()
            try:
                await self._fast_poll_macro.add(cmd=f'r{a}',
                    cb=lambda x, _, t=t: o.handle_read(x, self.timestamp_to_time(t.value)), key=o)
            except lexc.NotSupported:
                # Cannot do a fast poll.
                return False

        if interval_s < self._fast_poll_interval_s:
            self._fast_poll_interval_s = interval_s
            await self._poll_fast_stop()

        if self._fast_poll_task is None or self._fast_poll_task.done():
            self._fast_poll_task = self.periodic(self._fast_poll_interval_s, self._poll_fast_task, name='poll fast')

        return True

    async def _poll_fast_task(self):
        if self._fast_poll_macro is None:
            await self._poll_fast_stop()
            return

        await self._fast_poll_macro.run()

    async def _trace(self, o : Object, interval_s : float):
        if self._tracing is False:
            # Not supported
            return False

        if self._tracing is None:
            m = await self.acquire_macro()
            if m is None:
                # No macro available.
                # This might be a temporary issue.
                return False

            self._tracing = Tracing(self, poll_interval_s=self.trace_poll_interval_s, macro=m)
            try:
                await self._tracing._init()
            except BaseException as e:
                self.logger.info('cannot initialize tracing; %s', e)
                self._tracing = False
                await self.release_macro(m)
                return False

        assert isinstance(self._tracing, Tracing)
        if o not in self._tracing:
            await self.alias(o, temporary=False, permanentRef=self._tracing)
            a = await o.short_name()
            try:
                await self._tracing.add(cmd=f'r{a}', cb=o.handle_read, key=o)
            except lexc.NotSupported:
                # Cannot do a trace.
                return False

        return True



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
                self.logger.debug(f'socket event: {repr(evt["event"])}')
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

    def state(self) -> dict:
        '''Get the current state of the client.'''

        if not self.is_connected:
            return {}

        id = self._identification
        if not id:
            return {}

        objs = {}
        for o in self.objects:
            objs.update(o.state())

        s = {
            'identification': id,
            'version': self._version,
            'last': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'objects': objs
        }

        return {id: s}

    @overload
    async def save_state(self, state_name : str | None=None) -> None: ...
    @overload
    def save_state(self, state_name : str | None=None, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def save_state(self, state_name : str | None=None, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def save_state(self, state_name : str | None=None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Work.run_sync
    async def save_state(self, state_name : str | None=None) -> None:
        '''
        Save the current state to a file.

        **Arguments**
        * `state_name : str | None`: the name of the state file to save to

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''

        if not self.is_connected():
            raise lexc.InvalidState('Not connected')

        filename = self.state_file(state_name)
        if filename is None:
            return

        s = self.state()

        async with lexc.DeadlockChecker(filelock.AsyncFileLock(f'{filename}.lock')):
            state = {}
            try:
                async with aiofiles.open(filename, 'r') as f:
                    state = json.loads(await f.read())
            except FileNotFoundError:
                pass
            except json.JSONDecodeError:
                pass

            if not s:
                # Nothing to save, remove entry
                if self._identification in state:
                    del state[self._identification]
            else:
                state.update(s)

            state['_version'] = libstored_version

            os.makedirs(os.path.dirname(filename), exist_ok=True)
            async with aiofiles.open(filename, 'w') as f:
                await f.write(json.dumps(state, indent=4, sort_keys=True))

        self.logger.debug('saved state to %s', filename)

    @overload
    async def restore_state(self, state_name : str | None=None) -> None: ...
    @overload
    def restore_state(self, state_name : str | None=None, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def restore_state(self, state_name : str | None=None, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def restore_state(self, state_name : str | None=None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @Work.run_sync
    async def restore_state(self, state_name : str | None=None):
        '''
        Restore the state from a file.

        **Arguments**
        * `state_name : str | None`: the name of the state file to restore from

        **Result**
        * `None`: when `block = True`
        * otherwise a future
        '''

        if not self.is_connected():
            raise lexc.InvalidState('Not connected')

        filename = self.state_file(state_name)
        if not filename:
            return

        id = await self.identification()
        if not id:
            return

        obj = await self.list()
        if not obj:
            return

        async with lexc.DeadlockChecker(filelock.AsyncFileLock(f'{filename}.lock')):
            try:
                async with aiofiles.open(filename, 'r') as f:
                    state = json.loads(await f.read())
            except FileNotFoundError:
                self.logger.debug('cannot restore state from %s; not found', filename)
                return
            except json.JSONDecodeError as e:
                self.logger.warning('cannot restore state from %s; invalid JSON: %s', filename, e)
                return

            if not id in state:
                return

            s = state[id]
            if not 'objects' in s:
                return

            for o in obj:
                await o.restore_state(s['objects'])

        self.logger.debug('restored state from %s', filename)

    def state_file(self, state_name : str | None=None) -> str | None:
        '''Get the state file name.'''

        if not state_name:
            state_name = self._use_state

        if not state_name:
            return None

        return os.path.join(platformdirs.user_config_dir('libstored'), state_name + '.json')



class SyncObject:
    '''
    A synchronous ZeroMQ client object.

    This class wraps the AsyncZmqClientObject to provide a synchronous interface.
    '''

    def __init__(self, obj : Object):
        self._obj = obj

    def __getattr__(self, name):
        return getattr(self._obj, name)

    def read(self, acquire_alias : bool=True) -> typing.Any:
        '''Read the value of the object.'''
        return self._obj.read(acquire_alias, sync=True)

    def write(self, value : typing.Any = None) -> None:
        '''Write a value to the object.'''
        return self._obj.write(value, sync=True)



class SyncZmqClient:
    '''
    A synchronous ZeroMQ client.

    This class wraps the ZmqClient to provide a synchronous interface that is
    understood by static code analyzers.
    '''

    def __init__(self, client : ZmqClient):
        self._client = client

    def __getattr__(self, name):
        return getattr(self._client, name)

    def __getitem__(self, x : str) -> SyncObject:
        return SyncObject(self.obj(x))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        # Don't disconnect.  This object only exists, as it has been created
        # using ZmqClient.__enter__().  Its corresponding __exit__() will
        # disconnect the client.
        pass
