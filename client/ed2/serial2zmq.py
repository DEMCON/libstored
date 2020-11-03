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
import serial
import zmq
import logging

from .stream2zmq import Stream2Zmq

##
# \brief Serial port frame grabber to ZmqServer bridge.
# \ingroup libstored_client
class Serial2Zmq(Stream2Zmq):
    def __init__(self, stack='ascii,term', zmqport=Stream2Zmq.default_port, **kwargs):
        super().__init__(stack, zmqport)
        self.serial = serial.Serial(**kwargs)
        self.serial_socket = self.registerStream(self.serial)
        self.stdin_socket = self.registerStream(sys.stdin)
        self.rep_queue = []
        self.logger = logging.getLogger(__name__)

    def poll(self, timeout_s = None):
        events = super().poll(timeout_s)
        if events.get(self.stdin_socket, 0) & zmq.POLLIN:
            self.sendToApp(self.stdin_socket.recv())
        if events.get(self.serial_socket, 0) & zmq.POLLIN:
            self.decode(self.serial_socket.recv())

    def sendToApp(self, data):
        if len(data) > 0:
            self.serial.write(data)
            self.serial.flush()
            self.logger.info('sent ' + str(data))

    def encode(self, data):
        if len(data) > 0:
            self.sendToApp(data)
            super().encode(data)

    def close(self):
        super().close()
        self.serial.close()

