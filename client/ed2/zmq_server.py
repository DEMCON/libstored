# vim:et

import zmq
import threading
import io

class ZmqServer:
    default_port = 19026

    def __init__(self, port=default_port):
        self.sockets = set()
        self.context = zmq.Context()
        self.poller = zmq.Poller()
        self.streams = 0
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind(f'tcp://*:{port}')
        self.register(self.socket, zmq.POLLIN)
        self.closing = False
    
    def register(self, socket, flags):
        self.poller.register(socket, flags)
        if flags & zmq.POLLIN:
            self.sockets.add(socket)
    
    def unregister(self, socket):
        try:
            self.poller.unregister(socket)
            self.sockets.remove(socket)
        except:
            pass

    def poll(self, timeout_s = None):
        events = dict(self.poller.poll(None if timeout_s == None else timeout_s * 1000))
        if events.get(self.socket, 0) & zmq.POLLIN:
            self.req(self.socket.recv(), self.socket.send)
        return events

    def _forwardStream(self, stream, socket):
        try:
            if isinstance(stream, io.TextIOBase):
                r = lambda: stream.readline().encode()
            else:
                r = lambda: stream.read(1)

            data = r()
            while len(data) > 0:
                socket.send(data)
                data = r()

            # Send EOF
            socket.send(bytearray())
        except:
            if not self.closing:
                raise
        finally:
            socket.close()
            self.sockets.remove(socket)
    
    def registerStream(self, stream, f=True):
        reader = self.context.socket(zmq.PAIR)
        reader.bind(f'inproc://stream-{self.streams}')
        writer = self.context.socket(zmq.PAIR)
        writer.connect(f'inproc://stream-{self.streams}')
        self.streams += 1
        self.register(reader, flags=zmq.POLLIN)

        if f == True:
            self.sockets.add(writer)
            thread = threading.Thread(target=self._forwardStream, args=(stream, writer))
            thread.daemon = True
            thread.start()
            return reader
        else:
            return (reader, writer)

    def req(self, message, rep):
        # Default implementation. As there is no backend to handle requests,
        # answer with an error response.
        rep(b'?')

    def close(self):
        self.closing = True
        for s in list(self.sockets):
            self.unregister(s)
            s.close()

    def __del__(self):
        self.close()
