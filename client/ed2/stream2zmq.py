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

import sys
import zmq

from .zmq_server import ZmqServer

##
# \brief A generic out-of-band frame grabber for ASCII streams.
# \ingroup libstored_client
class Stream2Zmq(ZmqServer):
    def __init__(self, args, port=ZmqServer.default_port, **kwargs):
        super().__init__(port)
        self.stdout_buffer = bytearray()
        self.stdout_msg = bytearray()
        self.rep_queue = []

    def req(self, message, rep):
        self.rep_queue.append(rep)
        self.sendToApp(self.encode(message))

    def sendToApp(self, data):
        pass

    def stdout(self, data):
        sys.stdout.write(data)

    # Extract Zmq response from the application's stdout, and forward the rest
    # to self.stdout.
    def recvFromApp(self, data):
        if self.rep_queue == []:
            # Can't contain a response, as we don't expect one.
            self.stdout(data.decode())
            return

        # Can't be processing both.
        assert len(self.stdout_msg) == 0 or len(self.stdout_buffer) == 0

        inMsg = len(self.stdout_msg) > 0
        for b in data:
            if inMsg:
                self.stdout_msg.append(b)
                if self.stdout_msg[-2:] == b'\x1b\\':
                    # Found end of message
                    self.rep_queue.pop(0)(self.decode(self.stdout_msg))
                    self.stdout_msg = bytearray()
                    inMsg = False
            elif len(self.stdout_buffer) > 0 and self.stdout_buffer[-1] == 0x1b and b == ord('_'):
                # Found start of message
                self.stdout_buffer.pop()
                self.stdout_msg = bytearray(b'\x1b_')
                inMsg = True
            else:
                # Got more stdout
                self.stdout_buffer.append(b)

        if not inMsg:
            l = len(self.stdout_buffer)
            if l > 1 and self.stdout_buffer[-1] == 0x1b:
                # Pass everything to stdout except for the last escape byte.
                self.stdout(self.stdout_buffer[:-1])
                self.stdout_buffer = self.stdout_buffer[-1:]
            elif l > 0 and self.stdout_buffer[-1] != 0x1b:
                # Pass everything to stdout
                self.stdout(self.stdout_buffer)
                self.stdout_buffer = bytearray()

    @staticmethod
    def encode(message):
        res = bytearray(b'\x1b_')

        for b in message:
            if b < 0x20:
                res += bytearray([0x7f, b | 0x40])
            elif b == 0x7f:
                res += bytearray([0x7f, 0x7f])
            else:
                res.append(b)

        res += b'\x1b\\'
        return res

    def decode(self, message):
        # Strip APC and ST
        data = message[2:-2]

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

        return res

