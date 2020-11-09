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
import threading
import io
import logging
import serial

from . import protocol

##
# \brief A ZMQ Server
#
# This can be used to create a bridge from an arbitrary interface to ZMQ, which
# in turn can be used to connect a ed2.zmq_client.ZmqClient to.
#
# Instantiate as ed2.ZmqServer().
#
# \ingroup libstored_client
class ZmqServer(protocol.ProtocolLayer):
    default_port = 19026
    name = 'zmq'

    def __init__(self, port=default_port):
        super().__init__()
        self.logger = logging.getLogger(__name__)
        self.sockets = set()
        self.context = zmq.Context()
        self.poller = zmq.Poller()
        self.streams = 0
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind(f'tcp://*:{port}')
        self.register(self.socket, zmq.POLLIN)
        self.closing = False
        self._rep_queue = []

    def register(self, socket, flags):
        self.poller.register(socket, flags)
        if flags & zmq.POLLIN:
            self.sockets.add(socket)

    def unregister(self, socket):
        try:
            self.poller.unregister(socket)
            self.sockets.remove(socket)
        except:
            pass

    def poll(self, timeout_s = None):
        events = dict(self.poller.poll(None if timeout_s == None else timeout_s * 1000))
        if events.get(self.socket, 0) & zmq.POLLIN:
            self.req(self.socket.recv(), self.socket.send)
        return events

    def _forwardStream(self, stream, socket):
        try:
            if isinstance(stream, io.TextIOBase):
                r = lambda: stream.readline().encode()
            elif isinstance(stream, serial.Serial):
                r = lambda: stream.read(max(1, stream.inWaiting()))
            else:
                r = lambda: stream.read(1)

            data = r()
            while len(data) > 0:
                socket.send(data)
                data = r()

            # Send EOF
            socket.send(bytearray())
        except:
            if not self.closing:
                raise
        finally:
            socket.close()
            self.sockets.remove(socket)

    def registerStream(self, stream, f=True):
        reader = self.context.socket(zmq.PAIR)
        reader.bind(f'inproc://stream-{self.streams}')
        writer = self.context.socket(zmq.PAIR)
        writer.connect(f'inproc://stream-{self.streams}')
        self.streams += 1
        self.register(reader, flags=zmq.POLLIN)

        if f == True:
            self.sockets.add(writer)
            thread = threading.Thread(target=self._forwardStream, args=(stream, writer))
            thread.daemon = True
            thread.start()
            return reader
        else:
            return (reader, writer)

    def req(self, message, rep):
        self._rep_queue.append(rep)
        self.encode(message)

    def decode(self, data):
        if self._rep_queue != []:
            self.logger.debug('rep ' + str(bytes(data)))
            self._rep_queue.pop(0)(data)
        else:
            self.logger.debug('unexpected rep ' + str(bytes(data)))

        super().decode(data)

    def isWaiting(self):
        return self._rep_queue != []

    def close(self):
        self.closing = True
        for s in list(self.sockets):
            self.unregister(s)
            s.close(0)

    def __del__(self):
        self.close()

