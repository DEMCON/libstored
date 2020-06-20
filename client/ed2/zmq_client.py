# vim:et

import zmq
from .zmq_server import ZmqServer

class ZmqClient:

    def __init__(self, address, port=ZmqServer.default_port):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect(f'tcp://{address}:{port}')
    
    def req(self, message):
        self.socket.send(message)
        return self.socket.recv()

    def close(self):
        self.socket.close()

