# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import asyncio
import crcmod
import inspect
import logging
import struct
import sys
import time
import typing
import zmq
import zmq.asyncio

from .. import protocol as lprot

class ProtocolLayer:
    '''
    Base class for all protocol layers.

    Layers can be stacked (aka wrapped); the top (inner) layer is the
    application layer, the bottom (outer) layer is the physical layer.

    Encoding means sending data down the stack (towards the physical layer),
    decoding means receiving data up the stack (towards the application layer).
    '''

    name = 'layer'
    Packet : typing.TypeAlias = bytes | bytearray | memoryview | str
    Callback : typing.TypeAlias = typing.Callable[[Packet], typing.Any]
    AsyncCallback : typing.TypeAlias = typing.Callable[[Packet], typing.Coroutine[typing.Any, typing.Any, None]]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._down : ProtocolLayer | None = None
        self._up : ProtocolLayer | None = None
        self._down_callback : ProtocolLayer.AsyncCallback = self._callback_factory(None)
        self._up_callback : ProtocolLayer.AsyncCallback = self._callback_factory(None)
        self._activity : float = 0
        self.logger = logging.getLogger(self.__class__.__name__)

    def wrap(self, layer : ProtocolLayer) -> None:
        '''
        Wrap this layer around another layer.
        '''
        layer._down = self
        self._up = layer

    @property
    def up(self) -> ProtocolLayer | None:
        return self._up

    @up.setter
    def up(self, cb : ProtocolLayer.Callback | None) -> None:
        '''
        Set a callback to be called when data is received from the lower layer.
        '''
        self._up_callback = self._callback_factory(cb)

    @property
    def down(self) -> ProtocolLayer | None:
        return self._down

    @down.setter
    def down(self, cb : ProtocolLayer.Callback | None) -> None:
        '''
        Set a callback to be called when data is received from the upper layer.
        '''
        self._down_callback = self._callback_factory(cb)

    @staticmethod
    def _callback_factory(f : ProtocolLayer.Callback | None) -> ProtocolLayer.AsyncCallback:
        if f is None:
            async def no_callback(data : ProtocolLayer.Packet) -> None:
                pass
            return no_callback
        elif inspect.iscoroutinefunction(f):
            return f
        else:
            async def callback(data : ProtocolLayer.Packet) -> None:
                f(data)
            return callback

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        '''
        Encode data for transmission.
        '''
        self.activity()

        await self._down_callback(data)

        if self.down is not None:
            await self.down.encode(data)

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        '''
        Decode data received from the lower layer.
        '''
        self.activity()

        await self._up_callback(data)

        if self.up is not None:
            await self.up.decode(data)

    @property
    def mtu(self) -> int | None:
        '''
        Maximum Transmission Unit (MTU) for this layer.
        Return None when there is no limit.
        '''
        if self.down is not None:
            return self.down.mtu
        else:
            return None

    async def timeout(self) -> None:
        '''
        Trigger maintenance actions when a timeout occurs.
        '''
        if self.down is not None:
            await self.down.timeout()

    def activity(self) -> None:
        '''
        Mark that there was activity on this layer.
        '''
        self._activity = time.time()

    def last_activity(self) -> float:
        '''
        Get the time of the last activity on this layer or any lower layer.
        '''
        a = 0
        if self.down is not None:
            a = self.down.last_activity()

        return max(a, self._activity)

    async def close(self) -> None:
        '''
        Close the layer and release resources.
        '''
        if self.down is not None:
            await self.down.close()

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.close()



class AsciiEscapeLayer(ProtocolLayer):
    '''
    Layer that escapes non-printable ASCII characters.
    '''

    name = 'ascii'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        res = bytearray()
        esc = False
        for b in data:
            if esc:
                if b == 0x7f:
                    res.append(0x7f)
                else:
                    res.append(b & 0x3f)
                esc = False
            elif b == 0x7f:
                esc = True
            else:
                res.append(b)

        await super().decode(res)

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        res = bytearray()

        for b in data:
            if b < 0x20:
                res += bytearray([0x7f, b | 0x40])
            elif b == 0x7f:
                res += bytearray([0x7f, 0x7f])
            else:
                res.append(b)

        await super().encode(res)

    @property
    def mtu(self) -> int | None:
        m = super().mtu
        if m is None or m <= 0:
            return None
        return max(1, int(m / 2))



class TerminalLayer(ProtocolLayer):
    '''
    A ProtocolLayer that encodes debug messages in terminal escape codes.
    Non-debug messages are passed through unchanged.
    '''

    name = 'term'
    start = b'\x1b_' # APC
    end = b'\x1b\\'  # ST

    def __init__(self, fdout : str | int | ProtocolLayer.Callback | None=1, ignoreEscapesTillFirstEncode : bool=True, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.fdout : ProtocolLayer.Callback | None = None

        if isinstance(fdout, str):
            fdout = int(fdout)

        if isinstance(fdout, int):
            if fdout == 1:
                def fdout_stdout(x):
                    if isinstance(x, (bytes, bytearray)):
                        x = x.decode(errors='replace')
                    elif isinstance(x, memoryview):
                        x = x.tobytes().decode(errors='replace')
                    sys.stdout.write(x)
                    sys.stdout.flush()
                self.fdout = fdout_stdout
            else:
                def fdout_stderr(x):
                    if isinstance(x, (bytes, bytearray)):
                        x = x.decode(errors='replace')
                    elif isinstance(x, memoryview):
                        x = x.tobytes().decode(errors='replace')
                    sys.stderr.write(x)
                    sys.stderr.flush()
                self.fdout = fdout_stderr
        else:
            self.fdout = fdout

        self._data : bytearray = bytearray()
        self._inMsg : bool = False
        self._ignoreEscape : bool = ignoreEscapesTillFirstEncode

    async def non_debug_data(self, data : ProtocolLayer.Packet) -> None:
        if len(data) > 0:
            if self.fdout is not None:
                self.fdout(data)

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()

        self._ignoreEscape = False
        await super().encode(self.start + bytes(data) + self.end)

    async def inject(self, data : ProtocolLayer.Packet) -> None:
        '''
        Inject non-debug data down the stack.
        '''
        if isinstance(data, str):
            data = data.encode()

        await super().encode(data)

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        if data == b'':
            return
        if self._ignoreEscape and not self._inMsg:
            await self.non_debug_data(data)
            return
        if isinstance(data, str):
            data = data.encode()

        self._data += data

        if data[-1] == self.start[0]:
            # Got partial escape code. Wait for more data.
            return

        while True:
            if not self._inMsg:
                c = self._data.split(self.start, 1)
                if c[0] != b'':
                    self.logger.debug('non-debug %s', bytes(c[0]))
                    await self.non_debug_data(c[0])

                if len(c) == 1:
                    # No start of message in here.
                    self._data = bytearray()
                    return
                else:
                    # Keep rest of data, and continue decoding.
                    self._data = c[1]
                    self._inMsg = True
            else:
                c = self._data.split(self.end, 1)
                if len(c) == 1:
                    # No end of message in here. Wait for more.
                    return
                else:
                    # Got a full message.
                    # Remove \r as they can be inserted automatically by Windows.
                    # If \r is meant to be sent, escape it.
                    msg = c[0].replace(b'\r', b'')
                    self.logger.debug('extracted %s', bytes(msg))
                    self._data = c[1]
                    self._inMsg = False
                    await super().decode(msg)

    @property
    def mtu(self) -> int | None:
        m = super().mtu
        if m is None or m <= 0:
            return None
        return max(1, m - len(self.start) - len(self.end))



class PubTerminalLayer(TerminalLayer):
    """
    A TerminalLayer (term), that also forwards all non-debug data over a PUB socket.
    """

    name = 'pubterm'
    default_port = lprot.default_port + 1

    def __init__(self, bind : str=f'*:{default_port}', *args, context : zmq.asyncio.Context | None=None, **kwargs):
        super().__init__(*args, **kwargs)
        self._context : zmq.asyncio.Context = context or zmq.asyncio.Context.instance()
        self._socket : zmq.asyncio.Socket | None = self.context.socket(zmq.PUB)
        assert self._socket is not None
        self._socket.bind(f'tcp://{bind}')

    @property
    def context(self) -> zmq.asyncio.Context:
        return self._context

    @property
    def socket(self) -> zmq.asyncio.Socket | None:
        return self._socket

    async def close(self) -> None:
        if self._socket is not None:
            self._socket.close()
            self._socket = None

        await super().close()

    async def non_debug_data(self, data : ProtocolLayer.Packet) -> None:
        await super().non_debug_data(data)
        if len(data) > 0 and self._socket is not None:
            await self._socket.send(data)


class RepReqCheckLayer(ProtocolLayer):
    '''
    A ProtocolLayer that checks that requests and replies are matched.
    '''

    name = 'repreqcheck'

    def __init__(self, timeout_s : float = 1, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._req : bool = False
        self._timeout_s : float = timeout_s
        self._retransmit_time : float = 0
        self._retransmitter : asyncio.Task | None = asyncio.create_task(self._retransmitter_task())

    @property
    def timeout_s(self) -> float:
        return self._timeout_s

    @timeout_s.setter
    def timeout_s(self, value : float) -> None:
        if not self._req:
            self._retransmit_time = time.time() + value
        self._timeout_s = value

    @property
    def req(self) -> bool:
        '''
        Return if we are currently waiting for a reply to a request.
        '''
        return self._req

    async def _retransmitter_task(self) -> None:
        try:
            dt_s = self._timeout_s
            while True:
                await asyncio.sleep(dt_s)
                if not self._req:
                    continue
                now = time.time()
                dt_s = self._retransmit_time - now
                if dt_s <= 0:
                    self._retransmit_time = now + self._timeout_s
                    dt_s = self._timeout_s
                    await super().timeout()
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.exception(f'Retransmitter task error: {e}')
            raise

    async def timeout(self) -> None:
        # Ignore timeouts from above, we are checking for retransmissions ourselves.
        pass

    async def close(self) -> None:
        if self._retransmitter is not None:
            self._retransmitter.cancel()
            try:
                await self._retransmitter
            except asyncio.CancelledError:
                pass
            self._retransmitter = None

        await super().close()

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        if self._req:
            raise RuntimeError('RepReqCheckLayer encode called while previous request not yet handled')

        self._req = True
        self._retransmit_time = time.time() + self._timeout_s
        await super().encode(data)

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        if not self._req:
            self.logger.debug('Ignoring unexpected rep %s', data)
            return

        self._req = False
        await super().decode(data)



class SegmentationLayer(ProtocolLayer):
    '''
    A ProtocolLayer that segments and reassembles data according to a simple
    protocol with 'E' (end) and 'C' (continue) markers.
    '''

    name = 'segment'
    end = b'E'
    cont = b'C'

    def __init__(self, mtu : str | int | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        if isinstance(mtu, str):
                mtu = int(mtu)
        self._mtu = mtu if mtu is not None and mtu > 0 else None
        self._buffer = bytearray()

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        self._buffer += data[:-1]
        if data[-1:] == self.end:
            self.logger.debug('reassembled %s', bytes(self._buffer))
            await super().decode(self._buffer)
            self._buffer = bytearray()

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = bytearray(data.cast('B'))

        mtu = self._mtu
        if self._mtu is None:
            mtu = super().mtu
        if mtu is None:
            await super().encode(data + self.end)
        else:
            mtu = max(1, mtu - 1)
            for i in range(0, len(data), mtu):
                if i + mtu >= len(data):
                    await super().encode(data[i:i+mtu] + self.end)
                else:
                    await super().encode(data[i:i+mtu] + self.cont)

    @property
    def mtu(self) -> int | None:
        return None

    async def timeout(self) -> None:
        '''
        A DebugArqLayer below us is going to do a retransmit. Clear the buffer.
        '''
        self._buffer = bytearray()
        await super().timeout()



class DebugArqLayer(ProtocolLayer):
    '''
    A ProtocolLayer that implements a simple ARQ protocol for debugging.
    '''

    name = 'arq'
    reset_flag = 0x80

    def __init__(self, timeout_s : float = 1, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._req : bool = False
        self._request : list[bytes] = []
        self._reset : bool = True
        self._syncing : bool = False
        self._decode_seq : int = 1
        self._decode_seq_start : int = self._decode_seq
        self._encode_lock : asyncio.Lock = asyncio.Lock()

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if len(data) == 0:
            return

        (seq, msg) = self.decode_seq(data)
        if data[0] & self.reset_flag:
            self._decode_seq = seq

        if self._req:
            # We got a response, so the request should be finished now.
            self._req = False
            # This is the first part of the response.
            self._decode_seq_start = self._decode_seq

        if seq == self._decode_seq:
            self._decode_seq = self.next_seq(self._decode_seq)
            if len(msg) > 0:
                await super().decode(msg)
        else:
            self.logger.debug(f'unexpected seq {seq} instead of {self._decode_seq}; dropped')

        if self._syncing and data[0] == self.reset_flag:
            self._syncing = False
            for r in self._request:
                await self._encode(r)

    @staticmethod
    def decode_seq(data : bytes | bytearray | memoryview) -> tuple[int, memoryview[int]]:
        if isinstance(data, memoryview):
            data = data.cast('B')

        seq = 0
        if len(data) == 0:
            raise ValueError
        seq = data[0] & 0x3f
        if data[0] & 0x40:
            if len(data) == 1:
                raise ValueError
            seq = (seq << 7) | data[1] & 0x7f
            if data[1] & 0x80:
                if len(data) == 2:
                    raise ValueError
                seq = (seq << 7) | data[2] & 0x7f
                if data[2] & 0x80:
                    if len(data) == 3:
                        raise ValueError
                    seq = (seq << 7) | data[3] & 0x7f
                    if data[3] & 0x80:
                        raise ValueError
                    return (seq, memoryview(data)[4:])
                else:
                    return (seq, memoryview(data)[3:])
            else:
                return (seq, memoryview(data)[2:])
        else:
            return (seq, memoryview(data)[1:])

    @staticmethod
    def encode_seq(seq : int) -> bytes:
        if seq < 0x40:
            return bytes([seq & 0x3f])
        if seq < 0x2000:
            return bytes([
                0x40 | ((seq >> 7) & 0x3f),
                seq & 0x7f])
        if seq < 0x100000:
            return bytes([
                0x40 | ((seq >> 14) & 0x3f),
                0x80 | ((seq >> 7) & 0x7f),
                seq & 0x7f])
        if seq < 0x8000000:
            return bytes([
                0x40 | ((seq >> 21) & 0x3f),
                0x80 | ((seq >> 14) & 0x7f),
                0x80 | ((seq >> 7) & 0x7f),
                seq & 0x7f])
        return DebugArqLayer.encode_seq(seq % 0x8000000)

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if self._reset:
            self._reset = False
            await self._sync()

        if not self._req:
            self._request = []
            self._encode_seq_start = self._encode_seq

        self._req = True
        self._encode_seq = self.next_seq(self._encode_seq)

        # We cannot handle a wrap around of the seq, as a retransmit will be bogus.
        # Make sure the MTU is big enough to transmit the full request in 255 chunks.
        assert self._encode_seq != self._encode_seq_start

        request = self.encode_seq(self._encode_seq) + data
        self._request.append(request)
        if not self._syncing:
            await self._encode(request)

    async def _encode(self, data : ProtocolLayer.Packet) -> None:
        async with self._encode_lock:
            await super().encode(data)

    async def _sync(self) -> None:
        if self._syncing:
            return
        self._syncing = True
        self._encode_seq = 0
        await self._encode(bytes([self.reset_flag]))

    def reset(self) -> None:
        self._reset = True
        self._request = []

    async def retransmit(self) -> None:
        self.logger.debug('retransmit')
        if not self._req:
            self._decode_seq = self._decode_seq_start

        if self._syncing:
            await self._encode(bytes([self.reset_flag]))
        else:
            for r in self._request:
                await self._encode(r)

    async def timeout(self) -> None:
        await self.retransmit()

    @staticmethod
    def next_seq(seq : int) -> int:
        seq = (seq + 1) % 0x8000000
        return 1 if seq == 0 else seq

    @property
    def mtu(self) -> int | None:
        m = super().mtu
        if m is None or m <= 0:
            return None
        return max(1, m - 4)



class Crc8Layer(ProtocolLayer):
    '''
    ProtocolLayer to add and check integrity using a CRC8.
    '''

    name = 'crc8'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._crc = crcmod.mkCrcFun(0x1a6, 0xff, False, 0)

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()

        await super().encode(bytearray(data) + bytes([self.crc(data)]))

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if len(data) == 0:
            return

        if self.crc(data[0:-1]) != data[-1]:
#            self.logger.debug('invalid CRC, dropped ' + str(bytes(data)))
            return

        self.logger.debug('valid CRC %s', bytes(data))
        await super().decode(data[0:-1])

    def crc(self, data : ProtocolLayer.Packet) -> int:
        return self._crc(data)

    @property
    def mtu(self) -> int | None:
        # Limit MTU to 256 bytes to ensure proper 2 bit error detection.
        m = super().mtu
        if m is None or m <= 0:
            return 256
        return min(256, max(1, m - 1))



class Crc16Layer(ProtocolLayer):
    '''
    ProtocolLayer to add and check integrity using a CRC16.
    '''

    name = 'crc16'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._crc = crcmod.mkCrcFun(0x1baad, 0xffff, False, 0)

    async def encode(self, data: ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()

        await super().encode(bytearray(data) + struct.pack('>H', self.crc(data)))

    async def decode(self, data: ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if len(data) < 2:
            return

        if self.crc(data[0:-2]) != struct.unpack('>H', data[-2:])[0]:
#            self.logger.debug('invalid CRC, dropped ' + str(bytes(data)))
            return

        self.logger.debug('valid CRC %s', bytes(data))
        await super().decode(data[0:-2])

    def crc(self, data : ProtocolLayer.Packet) -> int:
        return self._crc(data)

    @property
    def mtu(self) -> int | None:
        # Limit MTU to 256 bytes to ensure proper 4 bit error detection.
        m = super().mtu
        if m is None or m <= 0:
            return 256
        return min(256, max(1, m - 1))


class Crc32Layer(ProtocolLayer):
    '''
    ProtocolLayer to add and check integrity using a CRC32.
    '''

    name = 'crc32'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._crc = crcmod.mkCrcFun(0x104c11db7, 0xffffffff, True, 0xffffffff)

    async def encode(self, data: ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()

        await super().encode(bytearray(data) + struct.pack('>I', self.crc(data)))

    async def decode(self, data: ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if len(data) < 4:
            return

        if self.crc(data[0:-4]) != struct.unpack('>I', data[-4:])[0]:
#            self.logger.debug('invalid CRC, dropped ' + str(bytes(data)))
            return

        self.logger.debug('valid CRC %s', bytes(data))
        await super().decode(data[0:-4])

    def crc(self, data : ProtocolLayer.Packet) -> int:
        return self._crc(data)

    @property
    def mtu(self) -> int | None:
        m = super().mtu
        if m is None or m <= 0:
            return None
        return max(1, m - 1)



class ProtocolStack(ProtocolLayer):
    '''
    Composition of a stack of layers.

    The given stack assumes that the layers are already wrapped.  At index 0,
    the application layer is expected, at index -1 the physical layer.
    '''

    name = 'stack'

    def __init__(self, layers : list[ProtocolLayer], *args, **kwargs):
        super().__init__(*args, **kwargs)
        if layers == []:
            layers = [ProtocolLayer()]
        self._layers = layers
        self._layers[-1].down = super().encode
        self._layers[0].up = super().decode

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        await self._layers[0].encode(data)

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        await self._layers[-1].decode(data)

    def last_activity(self) -> float:
        return max(super().last_activity(), self._layers[0].last_activity())

    async def close(self) -> None:
        await self._layers[0].close()
        await super().close()

    @property
    def mtu(self) -> int | None:
        return self._layers[0].mtu

    class Iterator:
        def __init__(self, stack):
            self._stack = stack
            self._index = 0
            self._it = None

        def __next__(self):
            if self._it is not None:
                try:
                    return next(self._it)
                except StopIteration:
                    self._it = None

            if self._index >= len(self._stack._layers):
                raise StopIteration

            l = self._stack._layers[self._index]
            self._index += 1
            if isinstance(l, ProtocolStack):
                self._it = iter(l)
                return next(self)

            return l

    def __iter__(self):
        return self.Iterator(self)



class LoopbackLayer(ProtocolLayer):
    '''
    A ProtocolLayer that loops back all data.
    '''

    name = 'loop'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        await self.decode(data)
        await super().encode(data)



class RawLayer(ProtocolLayer):
    '''
    A ProtocolLayer that just forwards raw data as bytes.
    '''
    name = 'raw'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    async def encode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        await super().encode(data)

    async def decode(self, data : ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        await super().decode(data)



layer_types : list[typing.Type[ProtocolLayer]] = [
    AsciiEscapeLayer,
    TerminalLayer,
    PubTerminalLayer,
    RepReqCheckLayer,
    SegmentationLayer,
    DebugArqLayer,
    Crc8Layer,
    Crc16Layer,
    Crc32Layer,
    LoopbackLayer,
    RawLayer,
]

def register_layer_type(layer_type : typing.Type[ProtocolLayer]) -> None:
    '''
    Register a new protocol layer type.
    The layer type must be a subclass of ProtocolLayer with a unique name.
    '''
    for lt in layer_types:
        if layer_type.name == lt.name:
            raise ValueError(f'Layer type {layer_type.name} already registered')

    layer_types.append(layer_type)

def unregister_layer_type(layer_type : typing.Type[ProtocolLayer]) -> None:
    '''
    Unregister a protocol layer type.
    '''
    for i, lt in enumerate(layer_types):
        if layer_type.name == lt.name:
            del layer_types[i]
            return

    raise ValueError(f'Layer type {layer_type.name} not registered')

def get_layer_type(name : str) -> typing.Type[ProtocolLayer]:
    '''
    Get a protocol layer type by name.
    '''
    for lt in layer_types:
        if name == lt.name:
            return lt

    raise ValueError(f'Unknown layer type {name}')

def get_layer_types() -> list[typing.Type[ProtocolLayer]]:
    '''
    Get the list of registered protocol layer types.
    '''
    return layer_types.copy()

def build_stack(description : str) -> ProtocolLayer:
    '''
    Construct the protocol stack from a description.

    The description is a comma-separated string with layer ids.  If the layer has
    a parameter, it can be specified.  The stack is constructed top-down in order
    of the specified layers.

    Grammar: ( <name> ( ``=`` <value> ) ? ) (``,`` <name> ( ``=`` <value> ) ? ) *
    '''

    layers = description.split(',')

    if layers == []:
        # Dummy layer
        return ProtocolLayer()

    stack = []

    for l in layers:
        name_arg = l.split('=')
        if name_arg[0] == '':
            raise ValueError(f'Missing layer type')

        layer_type = get_layer_type(name_arg[0])

        if len(name_arg) == 2:
            layer = layer_type(name_arg[1])
        else:
            layer = layer_type()

        if stack != []:
            layer.wrap(stack[-1])

        stack.append(layer)

    return ProtocolStack(stack)
