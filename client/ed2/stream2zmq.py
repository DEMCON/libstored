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
import logging
import time

from .zmq_server import ZmqServer
from . import protocol

##
# \brief A generic out-of-band frame grabber for ASCII streams.
# \ingroup libstored_client
class Stream2Zmq(ZmqServer):
    def __init__(self, stack='ascii,term', port=ZmqServer.default_port, timeout_s=5, **kwargs):
        super().__init__(port)
        self.rep_queue = []
        self.logger = logging.getLogger(__name__)
        self._stack_def = stack
        self._timeout_s = timeout_s
        self.reset()

    def reset(self):
        self._stack = protocol.buildStack(self._stack_def)
        for l in self._stack:
            if isinstance(l, protocol.TerminalLayer):
                l.fdout = self.stdout
        self._stack.up = self.recvFromApp
        self._stack.down = self.sendToApp

    def timeout(self):
        self._stack.timeout()
        self._t_req = time.time()

    def req(self, message, rep):
        self.rep_queue.append(rep)
        self._stack.encode(message)
        self.sendToApp(encoded)
        self._t_req = time.time()

    def sendToApp(self, data):
        self.logger.debug("send " + str(data))

    def stdout(self, data):
        sys.stdout.write(data.decode(errors="replace"))

    def isWaiting(self):
        return self.rep_queue != []

    def poll(self, timeout_s = None):
        if isWaiting:
            if timeout_s = None:
                timeout_s = self._timeout_s
            remaining = self._t_req + self._timeout_s - time.time()
            if remaining <= 0:
                self.timeout()
            else:
                timeout_s = min(timeout_s, remaining)

        return super().poll(timeout_s)

    # Extract Zmq response from the application's stdout, and forward the rest
    # to self.stdout.
    def recvFromApp(self, data):
        self.logger.debug("recv " + str(data))

        if self.rep_queue == []:
            # Can't contain a response, as we don't expect one.
            return

        self.rep_queue.pop(0)(data)

