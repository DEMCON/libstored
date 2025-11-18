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



class ZmqServer(lprot.ProtocolLayer):
    """A ZMQ Server

    This can be used to create a bridge from an arbitrary interface to ZMQ, which
    in turn can be used to connect a libstored.asyncio.ZmqClient to.
    """

    default_port = lprot.default_port
    name = 'zmq'

    @overload
    def __init__(self, *args, listen : str='*', port : int=default_port, context : zmq.asyncio.Context | None=None, **kwargs): ...
    @overload
    def __init__(self, bind : str, *args, context : zmq.asyncio.Context | None=None, **kwargs): ...

    def __init__(self, bind : str | None=None, *args, listen : str='*', port : int=default_port, context : zmq.asyncio.Context | None=None, **kwargs):
        super().__init__(*args, **kwargs)
        self._sockets : set[typing.Any] = set()
        self._context : zmq.asyncio.Context = context or zmq.asyncio.Context.instance()
        self._socket : zmq.asyncio.Socket = self._context.socket(zmq.REP)
        self._poller : asyncio.Task | None = asyncio.create_task(self._poller_task())
        self._req : bool = False

        listen, port, random_port = parse_bind(bind, listen, port)
        if random_port:
            self.logger.info(f'listening to {listen}:{port}')
        else:
            self.logger.debug(f'listening to {listen}:{port}')

        self._socket.bind(f'tcp://{listen}:{port}')

    @property
    def context(self) -> zmq.Context:
        return self._context

    async def _poller_task(self) -> None:
        try:
            while True:
                req = b''.join(await self._socket.recv_multipart())
                self.logger.debug('req %s', req)
                assert not self._req, 'ZmqServer received request while previous request not yet handled'
                self._req = True
                await self._encode(req)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.exception(f'ZmqServer poller task error: {e}')
            raise

    async def _encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        await super().encode(data)

    async def encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        # Silently ignore. We only get data from the socket.
        pass

    async def decode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if not self._req:
            self.logger.debug('Ignoring unexpected rep %s', data)
            return
        self.logger.debug('rep %s', data)
        self._req = False
        await self._socket.send(data)
        # Don't decode further.

    async def close(self) -> None:
        if self._poller is not None:
            self._poller.cancel()
            try:
                await self._poller
            except asyncio.CancelledError:
                pass
        self._socket.close()
        await super().close()

lprot.register_layer_type(ZmqServer)



class ZmqSocket(lprot.ProtocolLayer):
    """A ZMQ Socket

    This layer forks the data through the stack to a ZMQ socket.
    It can be used to access raw bytes through the stack.
    """

    default_port = 0
    name = 'sock'

    @overload
    def __init__(self, *args, listen : str='*', port : int=default_port, context : zmq.asyncio.Context | None=None, **kwargs): ...
    @overload
    def __init__(self, bind : str, *args, context : zmq.asyncio.Context | None=None, **kwargs): ...

    def __init__(self, bind : str | None=None, *args, listen : str='*', port : int=default_port, type : int=zmq.DEALER, context : zmq.asyncio.Context | None=None, **kwargs):
        super().__init__(*args, **kwargs)
        self._sockets : set[typing.Any] = set()
        self._context : zmq.asyncio.Context = context or zmq.asyncio.Context.instance()
        self._socket : zmq.asyncio.Socket = self._context.socket(type)
        self._poller : asyncio.Task | None = asyncio.create_task(self._poller_task())
        self._req : bool = False

        listen, port, random_port = parse_bind(bind, listen, port)
        if random_port:
            self.logger.info(f'listening to {listen}:{port}')
        else:
            self.logger.debug(f'listening to {listen}:{port}')

        self._socket.bind(f'tcp://{listen}:{port}')

    @property
    def context(self) -> zmq.Context:
        return self._context

    async def _poller_task(self) -> None:
        try:
            while True:
                x = b''.join(await self._socket.recv_multipart())
                self.logger.debug('recv %s', x)
                await self.encode(x)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.exception(f'ZmqSocket poller task error: {e}')
            raise

    async def decode(self, data : lprot.ProtocolLayer.Packet) -> None:
        self.logger.debug('send %s', data)
        await self._socket.send(data)
        await super().decode(data)

    async def close(self) -> None:
        if self._poller is not None:
            self._poller.cancel()
            try:
                await self._poller
            except asyncio.CancelledError:
                pass
        self._socket.close()
        await super().close()

lprot.register_layer_type(ZmqSocket)
