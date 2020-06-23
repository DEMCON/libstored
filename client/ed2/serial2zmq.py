# vim:et

import sys
import serial
import zmq

from .stream2zmq import Stream2Zmq

class Serial2Zmq(Stream2Zmq):
    def __init__(self, zmqport=Stream2Zmq.default_port, **kwargs):
        super().__init__(zmqport)
        self.serial = serial.Serial(**kwargs)
        self.serial_socket = self.registerStream(self.serial)
        self.stdin_socket = self.registerStream(sys.stdin)
        self.rep_queue = []

    def poll(self, timeout_s = None):
        events = super().poll(timeout_s)
        if events.get(self.stdin_socket, 0) & zmq.POLLIN:
            self.sendToApp(self.stdin_socket.recv())
        if events.get(self.serial_socket, 0) & zmq.POLLIN:
            self.recvFromApp(self.serial_socket.recv())

    def sendToApp(self, data):
        if len(data) > 0:
            self.serial.write(data)
    
    def close(self):
        super().close()
        self.serial.close()

