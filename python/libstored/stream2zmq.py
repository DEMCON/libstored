# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import atexit
import sys
import zmq
import logging
import time
import queue
import threading

from .zmq_server import ZmqServer
from . import protocol

class Stream2Zmq(protocol.ProtocolLayer):
    """A generic out-of-band frame grabber for ASCII streams."""

    default_port = ZmqServer.default_port

    def __init__(self, stack='ascii,term', listen='*', port=default_port, timeout_s=1, printStdout=True):
        super().__init__()
        self.logger = logging.getLogger(__name__)
        self._stack_def = f'zmq={listen}:{port},' + stack
        self._stack = None
        self._timeout_s = timeout_s
        self._zmq = None
        self._printStdout = printStdout
        if self._printStdout:
            setInfiniteStdout()
        self.reset()

    def reset(self):
        self._stack = protocol.buildStack(self._stack_def)
        for l in self._stack:
            if isinstance(l, protocol.TerminalLayer):
                l.fdout = self.stdout
        self.wrap(self._stack)
        self._zmq = None

    def encode(self, data):
        self.logger.debug('encode %s', bytes(data))
        super().encode(data)

    def decode(self, data):
        self.logger.debug('decode %s', bytes(data))
        super().decode(data)

    def timeout(self):
        self._stack.timeout()

    def stdout(self, data):
        if self._printStdout:
            sys.stdout.write(data.decode(errors="replace"))

    def isWaiting(self):
        return self.zmq.isWaiting()

    def poll(self, timeout_s = None):
        self.logger.debug('poll')

        if self.isWaiting():
            if timeout_s is None:
                timeout_s = self._timeout_s
            remaining = self.zmq.lastActivity() + self._timeout_s - time.time()
            if remaining <= 0:
                self.timeout()
            else:
                timeout_s = min(timeout_s, remaining)

        return self.zmq.poll(timeout_s)

    def recvAll(self, socket, f):
        try:
            while True:
                # Drain socket.
                f(socket.recv(flags=zmq.NOBLOCK))
        except zmq.ZMQError as e:
            if e.errno == zmq.EAGAIN:
                pass
            else:
                raise

    def registerStream(self, stream, f=True):
        return self.zmq.registerStream(stream, f)

    @property
    def zmq(self):
        if self._zmq is None:
            # Cache the zmq layer.
            self._zmq = next(iter(self._stack))
        return self._zmq

    def close(self):
        self.zmq.close()
        if self._stack is not None:
            for s in self._stack:
                s.close()
            self._stack = None

