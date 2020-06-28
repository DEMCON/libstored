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
import subprocess
import threading
import zmq

from .stream2zmq import Stream2Zmq

##
# \brief A stdin/stdout frame grabber to ZmqServer bridge.
# \ingroup libstored_client
class Stdio2Zmq(Stream2Zmq):
    def __init__(self, args, port=Stream2Zmq.default_port, **kwargs):
        super().__init__(port)
        self.process = subprocess.Popen(args=args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, **kwargs)
        self.stdout_socket = self.registerStream(self.process.stdout)
        self.stdin_socket = self.registerStream(sys.stdin)
        self.rep_queue = []

    def poll(self, timeout_s = None):
        # We need to check if the process still runs once in a while.
        # So, still use a timeout, even if none is given.
        events = super().poll(1 if timeout_s == None else timeout_s)
        if events.get(self.stdin_socket, 0) & zmq.POLLIN:
            self.sendToApp(self.stdin_socket.recv())
        if events.get(self.stdout_socket, 0) & zmq.POLLIN:
            self.recvFromApp(self.stdout_socket.recv())
        if self.process.poll() != None:
            sys.exit(self.process.returncode)

    def sendToApp(self, data):
        if len(data) == 0:
            # Our stdin has closed, close the process's too.
            self.process.stdin.close()
        else:
            self.process.stdin.write(data)
            self.process.stdin.flush()
    
    def decode(self, message):
        # Remove \r as they can be inserted automatically by Windows.
        # If \r is meant to be sent, escape it.
        return super().decode(message.replace(b'\r', b''))
    
    def close(self):
        self.process.terminate()
        super().close()

