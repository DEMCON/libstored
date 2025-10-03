# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import functools
import logging
import re
import typing
import zmq
import zmq.asyncio

from .event import Event
from ..zmq_server import ZmqServer
from .worker import AsyncioWorker
from .worker import run_sync

class ZmqClient:
    '''
    Asynchronous ZMQ client.

    This client can connect to both the libstored.zmq_server.ZmqServer and stored::DebugZmqLayer.
    '''

    def __init__(self, host : str='localhost', port : int=ZmqServer.default_port,
                multi : bool=False, timeout : float | None=None, context : None | zmq.asyncio.Context=None,
                worker : AsyncioWorker | None=None):

        self.logger = logging.getLogger(__class__.__name__)
        self._context = context or zmq.asyncio.Context.instance()
        self._host = host
        self._port = port
        self._multi = multi
        self._timeout = timeout if timeout is None or timeout > 0 else None
        self._socket = None
        self._lock = asyncio.Lock()
        self._worker = worker if worker is not None else AsyncioWorker(daemon=True)
        self._capabilities = None

        # Events
        self.connected = Event()
        self.disconnected = Event()

    @property
    def host(self) -> str:
        return self._host

    @property
    def port(self) -> int:
        return self._port

    @property
    def multi(self) -> bool:
        return self._multi

    @property
    def socket(self) -> zmq.asyncio.Socket | None:
        return self._socket

    @property
    def context(self) -> zmq.asyncio.Context:
        return self._context

    def isConnected(self) -> bool:
        return self.socket is not None

    @staticmethod
    def locked(f : typing.Callable):
        @functools.wraps(f)
        async def wrapper(self, *args, **kwargs):
            async with self._lock:
                return await f(self, *args, **kwargs)
        return wrapper

    @run_sync
    @locked
    async def connect(self):
        if self.isConnected():
            return

        self._socket = self._context.socket(zmq.REQ)

        try:
            if self._timeout is not None:
                self.logger.debug(f'using a timeout of {self._timeout} s')
                self._socket.setsockopt(zmq.CONNECT_TIMEOUT, int(self._timeout * 1000))
                self._socket.setsockopt(zmq.RCVTIMEO, int(self._timeout * 1000))
                self._socket.setsockopt(zmq.SNDTIMEO, int(self._timeout * 1000))

            self.logger.debug(f'connect to tcp://{self._host}:{self._port}')
            self._socket.connect(f'tcp://{self._host}:{self._port}')
            self.connected.trigger()
        except:
            s = self._socket
            self._socket = None
            if s is not None:
                s.close(0)
            raise

    @run_sync
    @locked
    async def disconnect(self):
        if not self.isConnected():
            return

        self.logger.debug('disconnect')

        try:
            assert self._socket is not None
            s = self._socket
            self._socket = None
            s.close(0)
        finally:
            self._socket = None
            self._capabilities = None
            self.disconnected.trigger()

    def __del__(self):
        if self.isConnected():
            self.disconnect()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.disconnect()

    @run_sync
    @locked
    async def req(self, msg : bytes | str) -> bytes | str:
        if isinstance(msg, str):
            return (await self._req(msg.encode())).decode()
        else:
            return await self._req(msg)

    async def _req(self, msg : bytes) -> bytes:
        if not self.isConnected():
            raise RuntimeError('Not connected')

        assert self._socket is not None
        self.logger.debug('req %s', msg)
        await self._socket.send(msg)
        rep = b''.join(await self._socket.recv_multipart())
        self.logger.debug('rep %s', rep)
        return rep

    @run_sync
    async def capabilities(self) -> str:
        if self._capabilities is None:
            self._capabilities = await self.req('?')
            if self._multi:
                # Remove capabilities that are stateful at the embedded side.
                self._capabilities = re.sub(r'[amstf]', '', self._capabilities)

        return self._capabilities

    @run_sync
    async def echo(self, msg : str) -> str:
        return (await self.req(b'e' + msg.encode())).decode()
