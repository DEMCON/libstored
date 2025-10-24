# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import typing
from typing import overload
import zmq
import zmq.asyncio

from .. import protocol as lprot

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

        self._socket.bind(f'tcp://{listen}:{port}')

    @property
    def context(self) -> zmq.Context:
        return self._context

    async def _poller_task(self) -> None:
        try:
            while True:
                req = b''.join(await self._socket.recv_multipart())
                self.logger.debug('req %s', req)
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
        self.logger.debug('rep %s', data)
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
