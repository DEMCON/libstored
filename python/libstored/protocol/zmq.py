# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
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



def parse_bind(bind : str | None, default_listen : str='*', default_port : int=0) -> tuple[str, int, bool]:
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


class ZmqSocketBase(lprot.ProtocolLayer):
    default_timeout_s : float | None = 10

    def __init__(self, *args, type : int, context : zmq.asyncio.Context | None=None, **kwargs):
        super().__init__(*args, **kwargs)
        self._context : zmq.asyncio.Context = context or zmq.asyncio.Context.instance()
        self._socket : zmq.asyncio.Socket = self._context.socket(type)
        self._poller : asyncio.Task | None = asyncio.create_task(self._poller_task())
        self._timeout_s : float | None = self.default_timeout_s
        self._open : bool = False
        self._sent : list[tuple[asyncio.Future, float]] = []

    @property
    def context(self) -> zmq.asyncio.Context:
        return self._context

    @property
    def socket(self) -> zmq.asyncio.Socket:
        return self._socket

    def mark_open(self) -> None:
        self._open = True

    @property
    def open(self) -> bool:
        return self._open

    async def _poller_task(self) -> None:
        try:
            while True:
                x = b''.join(await self._socket.recv_multipart())
                self.mark_open()
                await self._handle_recv(x)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.exception(f'poller task error: {e}')
            raise

    async def _handle_recv(self, data : bytes) -> None:
        raise NotImplementedError()

    async def close(self) -> None:
        if self._poller is not None:
            self._poller.cancel()
            try:
                await self._poller
            except asyncio.CancelledError:
                pass
            finally:
                self._poller = None
        self._socket.close()

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
            f = self._socket.send_multipart([data])
            assert isinstance(f, asyncio.Future)
            self._sent.append((f, asyncio.get_running_loop().time()))

        await super().decode(data)

    @property
    def timeout_s(self) -> float | None:
        return self._timeout_s

    @timeout_s.setter
    def timeout_s(self, value : float | None) -> None:
        self._timeout_s = value



class ZmqSocketClient(ZmqSocketBase):
    default_port = lprot.default_port

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    async def _handle_recv(self, data : bytes) -> None:
        await self.decode(data)

    async def encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        await super()._send(data)
        await super().encode(data)



class ZmqSocketServer(ZmqSocketBase):
    default_port = lprot.default_port

    @overload
    def __init__(self, *args, type : int, listen : str='*', port : int=default_port, context : zmq.asyncio.Context | None=None, **kwargs): ...
    @overload
    def __init__(self, bind : str, *args, type : int, context : zmq.asyncio.Context | None=None, **kwargs): ...

    def __init__(self, bind : str | None=None, *args, type : int, listen : str='*', port : int=default_port, **kwargs):
        super().__init__(*args, **kwargs)

        listen, port, random_port = parse_bind(bind, listen, port)
        if random_port:
            self.logger.info(f'listening to {listen}:{port}')
        else:
            self.logger.debug(f'listening to {listen}:{port}')

        self._socket.bind(f'tcp://{listen}:{port}')

    async def _handle_recv(self, data : bytes) -> None:
        await self.encode(data)

    async def decode(self, data : lprot.ProtocolLayer.Packet) -> None:
        await super()._send(data)
        await super().decode(data)



class ZmqServer(lprot.ZmqSocketServer):
    """A ZMQ Server

    This can be used to create a bridge from an arbitrary interface to ZMQ, which
    in turn can be used to connect a libstored.asyncio.ZmqClient to.
    """

    name = 'zmq'

    @overload
    def __init__(self, *args, listen : str='*', port : int=lprot.ZmqSocketServer.default_port, context : zmq.asyncio.Context | None=None, **kwargs): ...
    @overload
    def __init__(self, bind : str, *args, context : zmq.asyncio.Context | None=None, **kwargs): ...

    def __init__(self, bind : str | None=None, *args, **kwargs):
        super().__init__(bind, *args, type=zmq.REP, **kwargs)
        self._req : bool = False

    async def _handle_recv(self, data : bytes) -> None:
        self.logger.debug('req %s', data)
        assert not self._req, 'ZmqServer received request while previous request not yet handled'
        self._req = True
        await super()._handle_recv(data)

    async def decode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if not self._req:
            self.logger.debug('Ignoring unexpected rep %s', data)
            return
        self.logger.debug('rep %s', data)
        self._req = False
        await super().decode(data)

    def disconnected(self) -> None:
        super().disconnected()
        self._req = False

lprot.register_layer_type(ZmqServer)



class ZmqSocket(ZmqSocketServer):
    """A ZMQ Socket

    This layer forks the data through the stack to a ZMQ socket.
    It can be used to access raw bytes through the stack.
    """

    default_port : int = 0
    name = 'sock'

    @overload
    def __init__(self, *args, listen : str='*', port : int=default_port, type : int=zmq.DEALER, context : zmq.asyncio.Context | None=None, **kwargs): ...
    @overload
    def __init__(self, bind : str, *args, type : int=zmq.DEALER, context : zmq.asyncio.Context | None=None, **kwargs): ...

    def __init__(self, *args, port : int=default_port, type : int=zmq.DEALER, **kwargs):
        super().__init__(*args, port=port, type=type, **kwargs)

lprot.register_layer_type(ZmqSocket)
