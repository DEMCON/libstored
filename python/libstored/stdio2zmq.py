# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

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
def set_pdeathsig_(libc, sig):
    if os.name == 'posix':
        os.setsid()
    libc.prctl(1, sig)

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

    return lambda: set_pdeathsig_(libc, sig)

class Stdio2Zmq(Stream2Zmq):
    """A stdin/stdout frame grabber to ZmqServer bridge."""

    def __init__(self, args, stack='ascii,term', listen='*', port=Stream2Zmq.default_port, **kwargs):
        super().__init__(stack=stack, listen=listen, port=port)
        self.process = subprocess.Popen(
            args=args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            preexec_fn = set_pdeathsig() if os.name == 'posix' else None,
            shell=not os.path.exists(args[0] if isinstance(args, list) else args),
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
        if os.name == 'posix':
            os.killpg(os.getpgid(self.process.pid), signal.SIGTERM)
        self.process.terminate()
        super().close()

