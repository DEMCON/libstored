# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import logging
import socketserver
import typing
from typing import overload
import zmq
import zmq.asyncio

from .. import protocol as lprot

@overload
def free_ports() -> int: ...
@overload
def free_ports(num : typing.Literal[None]) -> int: ...
@overload
def free_ports(num : int) -> list[int]: ...

def free_ports(num : int | None=None) -> list[int] | int:
    ss : list[socketserver.TCPServer] = []
    ports = []

    for i in range(0, max(1, num) if num is not None else 1):
        s = socketserver.TCPServer(("localhost", 0), socketserver.BaseRequestHandler)
        ss.append(s)
        ports.append(s.server_address[1])

    for s in ss:
        s.server_close()

    return ports if num is not None else ports[0]



class ZmqSocketBase(lprot.ProtocolLayer):
    '''
    Generic ZMQ socket layer.
    '''

    default_timeout_s : float | None = 10

    def __init__(self, *args, type : int=zmq.DEALER, context : zmq.asyncio.Context | None=None, **kwargs):
        super().__init__(*args, **kwargs)
        self._context : zmq.asyncio.Context = context or zmq.asyncio.Context.instance()
        self._socket : zmq.asyncio.Socket | None = self._context.socket(type)
        self._recv : asyncio.Task | None = asyncio.create_task(self._recv_task(), name=f'{self.__class__.__name__} recv')
        self._timeout_s : float | None = self.default_timeout_s
        self._open : bool = False
        self._sent : list[tuple[asyncio.Future, float]] = []

    @property
    def context(self) -> zmq.asyncio.Context:
        return self._context

    @property
    def socket(self) -> zmq.asyncio.Socket:
        if self._socket is None:
            raise RuntimeError('ZMQ socket is closed')
        return self._socket

    def mark_open(self) -> None:
        self._open = True

    @property
    def open(self) -> bool:
        return self._open

    async def _recv_task(self) -> None:
        try:
            socket = self.socket

            await self._recv_init()

            while True:
                x = b''.join(await socket.recv_multipart())
                if self.logger.getEffectiveLevel() <= logging.DEBUG:
                    self.logger.debug(f'recv {x}')
                self.mark_open()
                await self._handle_recv(x)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            await self.async_except(e)
            raise

    async def _recv_init(self) -> None:
        pass

    async def _handle_recv(self, data : bytes) -> None:
        raise NotImplementedError()

    async def close(self) -> None:
        if self._recv is not None:
            self._recv.cancel()
            try:
                await self._recv
            except:
                pass
            self._recv = None

        if self._socket is not None:
            self._socket.close()
            self._socket = None

        self.disconnected()
        await super().close()

    def _check_sent(self) -> None:
        if self._timeout_s is None:
            t = None
        else:
            t = asyncio.get_running_loop().time() - self._timeout_s

        while self._sent:
            if self._sent[0][0].done():
                f, _ = self._sent.pop(0)
                try:
                    f.result()
                except Exception as e:
                    self.logger.warning(f'send error: {e}')
                continue

            if t is None or self._sent[0][1] > t:
                # Still waiting
                break

            if self.open:
                self.logger.info('connection timed out')
                self.disconnected()
            return

    def disconnected(self) -> None:
        self._open = False
        for f, _ in self._sent:
            f.cancel()
        self._sent = []

    async def _send(self, data : lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        self._check_sent()

        if self.open:
            if self.logger.getEffectiveLevel() <= logging.DEBUG:
                self.logger.debug(f'send {bytes(data)}')
            f = self.socket.send_multipart([data])
            assert isinstance(f, asyncio.Future)
            self._sent.append((f, asyncio.get_running_loop().time()))

    @property
    def timeout_s(self) -> float | None:
        return self._timeout_s

    @timeout_s.setter
    def timeout_s(self, value : float | None) -> None:
        self._timeout_s = value



class ZmqSocketClient(ZmqSocketBase):
    '''
    Generic ZMQ client socket layer.

    This layer is expected to be at the bottom of the protocol stack.
    Received data is passed up the stack.
    '''

    default_port = lprot.default_port
    name = 'connect'

    @overload
    def __init__(self, *args, server : str='localhost', port : int=default_port, type : int=zmq.DEALER, context : zmq.asyncio.Context | None=None, **kwargs): ...
    @overload
    def __init__(self, connect : str, *args, context : zmq.asyncio.Context | None=None, type : int=zmq.DEALER, **kwargs): ...

    def __init__(self, connect : str | None=None, *args, server : str='localhost', port : int=default_port, **kwargs):
        super().__init__(*args, **kwargs)
        server, port = self.parse_connect(connect, server, port)
        self.logger.debug(f'connecting to {server}:{port}')
        self.socket.connect(f'tcp://{server}:{port}')

    @staticmethod
    def parse_connect(connect : str | None=None, default_server : str='*', default_port : int=default_port) -> tuple[str, int]:
        server = default_server
        port = default_port

        if connect is not None:
            s = connect.split(':', 1)
            if len(s) == 2:
                if s[0] != '':
                    server = s[0]
                if s[1] != '':
                    port = int(s[1])
            else:
                try:
                    port = int(s[0])
                except:
                    server = s[0]

        return (server, port)

    async def _handle_recv(self, data : bytes) -> None:
        await self.decode(data)

    async def _recv_init(self) -> None:
        # Indicate that we are connected.
        self.mark_open()
        await self._send(b'')

    async def encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        await super()._send(data)
        await super().encode(data)

lprot.register_layer_type(ZmqSocketClient)



class ZmqSocketServer(ZmqSocketBase):
    '''
    Generic ZMQ server (listening) socket layer.

    This layer is expected to be at the top of the protocol stack.
    Received data is passed down the stack.
    '''

    default_port = 0
    name = 'sock'

    @overload
    def __init__(self, *args, type : int=zmq.DEALER, listen : str='*', port : int=default_port, context : zmq.asyncio.Context | None=None, **kwargs): ...
    @overload
    def __init__(self, bind : str, *args, type : int=zmq.DEALER, context : zmq.asyncio.Context | None=None, **kwargs): ...

    def __init__(self, bind : str | None=None, *args, listen : str='*', port : int=default_port, **kwargs):
        super().__init__(*args, **kwargs)

        listen, port, random_port = self.parse_bind(bind, listen, port)
        if random_port:
            self.logger.info(f'listening to {listen}:{port}')
        else:
            self.logger.debug(f'listening to {listen}:{port}')

        self.socket.bind(f'tcp://{listen}:{port}')

    @staticmethod
    def parse_bind(bind : str | None=None, default_listen : str='*', default_port : int=default_port) -> tuple[str, int, bool]:
        listen = default_listen
        port = default_port

        if bind is not None:
            s = bind.split(':', 1)
            if len(s) == 2:
                if s[0] != '':
                    listen = s[0]
                if s[1] != '':
                    port = int(s[1])
            else:
                try:
                    port = int(s[0])
                except:
                    listen = s[0]

        random_port = port == 0
        if port == 0:
            port = free_ports()

        return (listen, port, random_port)

    async def _handle_recv(self, data : bytes) -> None:
        await self.encode(data)

    async def decode(self, data : lprot.ProtocolLayer.Packet) -> None:
        await super()._send(data)
        await super().decode(data)

lprot.register_layer_type(ZmqSocketServer)



class ZmqServer(ZmqSocketServer):
    '''
    A ZMQ Server, for REQ/REP debug messages.

    This can be used to create a bridge from an arbitrary interface to ZMQ, which
    in turn can be used to connect a libstored.asyncio.ZmqClient to.
    '''

    default_port = lprot.default_port
    name = 'zmq'

    @overload
    def __init__(self, *args, listen : str='*', port : int=default_port, context : zmq.asyncio.Context | None=None, **kwargs): ...
    @overload
    def __init__(self, bind : str, *args, context : zmq.asyncio.Context | None=None, **kwargs): ...

    def __init__(self, bind : str | None=None, *args, **kwargs):
        super().__init__(bind, *args, type=zmq.REP, **kwargs)
        self._req : bool = False

    async def _handle_recv(self, data : bytes) -> None:
        assert not self._req, 'ZmqServer received request while previous request not yet handled'
        self._req = True
        await super()._handle_recv(data)

    async def decode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if not self._req:
            self.logger.debug('Ignoring unexpected rep %s', data)
            return
        self._req = False
        await super().decode(data)

    def disconnected(self) -> None:
        super().disconnected()
        self._req = False

lprot.register_layer_type(ZmqServer)
