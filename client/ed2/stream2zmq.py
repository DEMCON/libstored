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
class Stream2Zmq(protocol.ProtocolLayer):
    default_port = ZmqServer.default_port

    def __init__(self, stack='ascii,term', port=default_port, timeout_s=1, **kwargs):
        super().__init__()
        self.logger = logging.getLogger(__name__)
        self._stack_def = f'zmq={port},' + stack
        self._timeout_s = timeout_s
        self.reset()

    def reset(self):
        self._stack = protocol.buildStack(self._stack_def)
        for l in self._stack:
            if isinstance(l, protocol.TerminalLayer):
                l.fdout = self.stdout
        self.wrap(self._stack)

    def encode(self, data):
        self.logger.debug('encode ' + str(bytes(data)))
        super().encode(data)

    def decode(self, data):
        self.logger.debug('decode ' + str(bytes(data)))
        super().decode(data)

    def timeout(self):
        self._stack.timeout()

    def stdout(self, data):
        sys.stdout.write(data.decode(errors="replace"))

    def isWaiting(self):
        return self.zmq.isWaiting()

    def poll(self, timeout_s = None):
        self.logger.debug('poll')

        if self.isWaiting():
            if timeout_s == None:
                timeout_s = self._timeout_s
            remaining = self.zmq.lastActivity() + self._timeout_s - time.time()
            if remaining <= 0:
                self.timeout()
            else:
                timeout_s = min(timeout_s, remaining)

        return self.zmq.poll(timeout_s)

    def registerStream(self, stream, f=True):
        return self.zmq.registerStream(stream, f)

    @property
    def zmq(self):
        return next(iter(self._stack))

    def close(self):
        self.zmq.close()
