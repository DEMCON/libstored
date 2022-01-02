# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
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
import subprocess
import threading
import zmq
import os

from .stream2zmq import Stream2Zmq

import signal
import ctypes

libc = None

# Helper to clean up the child when python crashes.
def set_pdeathsig(sig = signal.SIGTERM):
    global libc

    if libc is None:
        try:
            libc = ctypes.CDLL("libc.so.6")
        except:
            # Failed. Not on Linux?
            libc = False

    if libc == False:
        return lambda: None

    return lambda: libc.prctl(1, sig)

class Stdio2Zmq(Stream2Zmq):
    """A stdin/stdout frame grabber to ZmqServer bridge."""

    def __init__(self, args, stack='ascii,term', listen='*', port=Stream2Zmq.default_port, **kwargs):
        super().__init__(stack=stack, listen=listen, port=port)
        self.process = subprocess.Popen(
            args=args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            preexec_fn = set_pdeathsig() if os.name == 'posix' else None,
            **kwargs)
        self.stdout_socket = self.registerStream(self.process.stdout)
        self.stdin_socket = self.registerStream(sys.stdin)
        self.rep_queue = []

    def poll(self, timeout_s = None):
        # We need to check if the process still runs once in a while.
        # So, still use a timeout, even if none is given.
        events = super().poll(1 if timeout_s == None else timeout_s)
        if events.get(self.stdin_socket, 0) & zmq.POLLIN:
            self.recvAll(self.stdin_socket, self.sendToApp)
        if events.get(self.stdout_socket, 0) & zmq.POLLIN:
            self.recvAll(self.stdout_socket, self.decode)
        if self.process.poll() != None:
            self.logger.debug('Process terminated with exit code %d', self.process.returncode)
            sys.exit(self.process.returncode)

    def sendToApp(self, data):
        if self.process.stdin.closed:
            return

        try:
            self.process.stdin.write(data)
            self.process.stdin.flush()
        except:
            self.logger.info('Cannot write to stdin; shutdown')
            self.process.stdin.close()

    def encode(self, data):
        if len(data) > 0:
            self.sendToApp(data)
            super().encode(data)

    def close(self):
        self.logger.debug('Closing; terminate process')
        self.process.terminate()
        super().close()

