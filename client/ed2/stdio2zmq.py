# vim:et

import sys
import subprocess
import threading
import zmq

from .stream2zmq import Stream2Zmq

class Stdio2Zmq(Stream2Zmq):
    def __init__(self, args, port=Stream2Zmq.default_port, **kwargs):
        super().__init__(port)
        self.process = subprocess.Popen(args=args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, **kwargs)

        self.stdout_socket = self.registerStream(self.process.stdout)
        self.stdout_buffer = bytearray()
        self.stdout_msg = bytearray()

        self.stdin_socket = self.registerStream(sys.stdin)

        self.rep_queue = []

    def poll(self, timeout_s = None):
        # We need to check it the process still runs once in a while.
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
    
    def close(self):
        self.process.terminate()
        super().close()

