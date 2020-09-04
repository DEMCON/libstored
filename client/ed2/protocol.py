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

import logging
import crcmod
import sys

class ProtocolLayer:
    name = 'layer'

    def __init__(self):
        self._down = None
        self._up = None
        self._down_callback = None
        self._up_callback = None
        self.logger = logging.getLogger(__name__)

    def wrap(self, layer):
        layer._down = self
        self._up = layer

    @property
    def up(self):
        return self._up

    @up.setter
    def up(self, cb):
        self._up_callback = cb

    @property
    def down(self):
        return self._down

    @down.setter
    def down(self, cb):
        self._down_callback = cb

    def encode(self, data):
        if self._down_callback != None:
            self._down_callback(data)
        if self.down != None:
            self.down.encode(data)

    def decode(self, data):
        if self._up_callback != None:
            self._up_callback(data)
        if self.up != None:
            self.up.decode(data)

    @property
    def mtu(self):
        if self.down != None:
            return self.down.mtu
        else:
            return 0

    def timeout(self):
        if self.down != None:
            self.down.timeout()

class AsciiEscapeLayer(ProtocolLayer):
    name = 'ascii'

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    def decode(self, data):
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

        super().decode(res)

    def encode(self, data):
        if isinstance(data, str):
            data = data.encode()

        res = bytearray()

        for b in data:
            if b < 0x20:
                res += bytearray([0x7f, b | 0x40])
            elif b == 0x7f:
                res += bytearray([0x7f, 0x7f])
            else:
                res.append(b)

        super().encode(res)

    @property
    def mtu(self):
        m = super().mtu
        if m == 0:
            return 0
        return max(1, int(m / 2))

class TerminalLayer(ProtocolLayer):

    name = 'term'
    start = b'\x1b_' # APC
    end = b'\x1b\\'  # ST

    def __init__(self, fdout=1, **kwargs):
        super().__init__(**kwargs)

        if isinstance(fdout, str):
            fdout = int(fdout)
        if isinstance(fdout, int):
            if fdout == 1:
                self.fdout = lambda x: sys.stdout.write(x.decode(errors="replace"))
            else:
                self.fdout = lambda x: sys.stderr.write(x.decode(errors="replace"))
        else:
            self.fdout = fdout

        self._data = bytearray()
        self._inMsg = False

    def nonDebugData(self, data):
        if len(data) > 0:
            self.fdout(data)

    def encode(self, data):
        if isinstance(data, str):
            data = data.encode()

        super().encode(self.start + data + self.end)

    def decode(self, data):
        if len(data) == 0:
            return

        self._data += data

        if data[-1] == self.start[0]:
            # Got partial escape code. Wait for more data.
            return

        while True:
            if not self._inMsg:
                c = self._data.split(self.start, 1)
                self.nonDebugData(c[0])

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
                    self.logger.debug('extracted ' + str(bytes(msg)))
                    super().decode(msg)
                    self._data = c[1]
                    self._inMsg = False

    @property
    def mtu(self):
        m = super().mtu
        if m == 0:
            return 0
        return max(1, m - len(start) - len(end))

class SegmentationLayer(ProtocolLayer):
    name = 'segment'
    end = b'E'
    cont = b'C'

    def __init__(self, mtu=None, **kwargs):
        super().__init__(**kwargs)
        self._mtu = None if mtu == None else int(mtu)
        self._buffer = bytearray()

    def decode(self, data):
        self._buffer += data[:-1]
        if data[-1:] == self.end:
            self.logger.debug('reassembled ' + str(bytes(self._buffer)))
            super().decode(self._buffer)
            self._buffer = bytearray()

    def encode(self, data):
        mtu = self._mtu
        if self._mtu == None:
            mtu = super().mtu
        if mtu == 0:
            super().encode(data + self.end)
        else:
            mtu = max(1, mtu - 1)
            for i in range(0, len(data), mtu):
                if i + mtu >= len(data):
                    super().encode(data[i:i+mtu] + self.end)
                else:
                    super().encode(data[i:i+mtu] + self.cont)

    def timeout(self):
        # A retransmit is pending. Clear partial data.
        self._buffer = bytearray()
        super().timeout()

    @property
    def mtu(self):
        return 0

class ArqLayer(ProtocolLayer):
    name = 'arq'
    reset_seq = 32

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._req = False
        self._request = []
        self._reset = True
        self._decode_seq = self.nextSeq(self.reset_seq)

    def decode(self, data):
        if len(data) == 0:
            return

        # We got a response, so the request should be finished now.
        self._req = False
        if data[0] == self.reset_seq:
            self._decode_seq = self.reset_seq
        if data[0] == self._decode_seq:
            self._decode_seq = self.nextSeq(self._decode_seq)
            if len(data) > 1:
                super().decode(data[1:])
        else:
            self.logger.debug(f'unexpected seq {data[0]} instead of {self._decode_seq}; dropped')

    def encode(self, data):
        if self._reset:
            super().encode(bytes([self.reset_seq]))
            self._reset = False
            self._encode_seq = self.reset_seq

        if not self._req:
            self._request = []
            self._encode_seq_start = self._encode_seq

        self._req = True
        self._encode_seq = self.nextSeq(self._encode_seq)

        # We cannot handle a wrap around of the seq, as a retransmit will be bogus.
        # Make sure the MTU is big enough to transmit the full request in 255 chunks.
        assert self._encode_seq != self._encode_seq_start

        request = bytes([self._encode_seq]) + data
        self._request.append(request)
        super().encode(request)

    def timeout(self):
        super().timeout()
        self.retransmit()

    def reset(self):
        self._reset = True
        self._request = []

    def retransmit(self):
        self.logger.debug('retransmit')
        for r in self._request:
            super().encode(r)

    def nextSeq(self, seq):
        seq = (seq + 1) % 256
        if seq == self.reset_seq:
            return nextSeq(seq)
        else:
            return seq

    @property
    def mtu(self):
        m = super().mtu
        if m == 0:
            return 0
        return max(1, m - 1)

class CrcLayer(ProtocolLayer):
    name = 'crc'

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._crc = crcmod.mkCrcFun(0x1a6, 0xff, False, 0)

    def encode(self, data):
        super().encode(data + bytes([self.crc(data)]))

    def decode(self, data):
        if len(data) == 0:
            return

        if self.crc(data[0:-1]) != data[-1]:
#            self.logger.debug('invalid CRC, dropped ' + str(bytes(data)))
            return

        self.logger.debug('valid CRC ' + str(bytes(data)))
        super().decode(data[0:-1])

    def crc(self, data):
        return self._crc(data)

    @property
    def mtu(self):
        m = super().mtu
        if m == 0:
            return 0
        return max(1, m - 1)

class ProtocolStack(ProtocolLayer):
    name = 'stack'

    def __init__(self, layers):
        super().__init__()
        if layers == []:
            layers = ProtocolLayer()
        self._layers = layers
        self._layers[-1].down = super().encode
        self._layers[0].up = super().decode

    def encode(self, data):
        self._layers[0].encode(data)

    def decode(self, data):
        self._layers[-1].decode(data)

    def timeout(self):
        self._layers[0].timeout()

    @property
    def mtu(self):
        return self._layers[0].mtu

    class Iterator:
        def __init__(self, stack):
            self._stack = stack
            self._index = 0
            self._it = None

        def __next__(self):
            if self._it != None:
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
    name = 'loop'

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    def encode(self, data):
        if self._down_callback:
            self._down_callback(data)
        self.decode(data)

class RawLayer(ProtocolLayer):
    name = 'raw'

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

layer_types = [
    AsciiEscapeLayer,
    TerminalLayer,
    SegmentationLayer,
    ArqLayer,
    CrcLayer,
    LoopbackLayer,
    RawLayer,
]

def registerLayerType(layer_type):
    layer_types.append(layer_type)

##
# \brief Construct the protocol stack from a description.
#
# The description is a comma-separated string with layer ids.  If the layer has
# a parameter, it can be specified.  The stack is constructed top-down in order
# of the specified layers.
#
# Grammar: ( name ( '=' value ) ? ) (',' name ( '=' value ) ? ) *
#
def buildStack(description):
    layers = description.split(',')

    if layers == []:
        # Dummy layer
        return ProtocolLayer()

    stack = []

    for l in layers:
        name_arg = l.split('=')
        if name_arg[0] == '':
            raise ValueError(f'Missing layer type')

        for lt in layer_types:
            if name_arg[0] == lt.name:
                layer_type = lt
                break
        else:
            raise ValueError(f'Unknown layer type {name_arg[0]}')

        if len(name_arg) == 2:
            layer = layer_type(name_arg[1])
        else:
            layer = layer_type()

        if stack != []:
            layer.wrap(stack[-1])

        stack.append(layer)

    return ProtocolStack(stack)

