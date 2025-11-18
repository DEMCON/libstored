#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import asyncio
import os
import sys
import zmq.asyncio

from .. import protocol as lprot
from ..asyncio.worker import AsyncioWorker, run_sync



class PrintLayer(lprot.ProtocolLayer):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)



class ZmqSocketClient(lprot.ProtocolLayer):
    def __init__(self, host : str, port : int, type : int=zmq.DEALER, context : zmq.asyncio.Context | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self._poller : asyncio.Task | None = asyncio.create_task(self._poller_task())
        self._context : zmq.asyncio.Context = context or zmq.asyncio.Context.instance()
        self._socket : zmq.asyncio.Socket = self.context.socket(type)
        self._socket.connect(f'tcp://{host}:{port}')

    @property
    def context(self) -> zmq.asyncio.Context:
        return self._context

    async def _poller_task(self) -> None:
        try:
            while True:
                x = b''.join(await self._socket.recv_multipart())
                self.logger.debug('recv %s', x)
                await self.decode(x)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.exception(f'ZmqSocket poller task error: {e}')
            raise

    async def encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        await self._socket.send_multipart([data])
        await super().encode(data)

    async def decode(self, data : lprot.ProtocolLayer.Packet) -> None:

@run_sync
async def async_main(args : argparse.Namespace) -> None:

    if args.type == 'dealer':
        type = zmq.DEALER
    elif args.type == 'pair':
        type = zmq.PAIR
    elif args.type == 'req':
        type = zmq.REQ
    else:
        raise ValueError(f'Unknown socket type: {args.type}')

    layers = []
    try:
        print_layer = PrintLayer()
        layers.append(print_layer)
        stdin_layer = lprot.StdinLayer()
        layers.append(stdin_layer)
        stdin_layer.wrap(print_layer)
        zmq_socket = ZmqSocketClient(args.host, int(args.port), type=type)
        layers.append(zmq_socket)
        zmq_socket.wrap(stdin_layer)

        while True:
            await asyncio.sleep(3600)

    except asyncio.CancelledError:
        pass
    except KeyboardInterrupt:
        pass
    finally:
        for layer in reversed(layers):
            await layer.close()



def main():
    parser = argparse.ArgumentParser(prog=__package__,
                                     description='ZMQ cat utility that fits nicely with libstored.protocol.ZmqSocket',
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    from ..version import __version__
    parser.add_argument('-s', dest='host', help='Server hostname', default='localhost')
    parser.add_argument('-p', dest='port', help='Specify TCP port')
    parser.add_argument('-t', dest='type', choices=['dealer', 'pair', 'req'], help='Socket type', default='dealer')
    parser.add_argument('-n', dest='newline', action='store_true', help='Remove newlines from input')

    args = parser.parse_args()
    async_main(args)

    socket.connect(f'tcp://{args.host}:{args.port}')

    socket.send(b'')

    poller = zmq.Poller()
    if os.name == 'posix':
        poller.register(sys.stdin, zmq.POLLIN) # This does not work on Windows.
    poller.register(socket, zmq.POLLIN)

    while True:
        events = dict(poller.poll(1000))

        e = events.get(sys.stdin.fileno(), 0)
        if e != 0:
            if e & zmq.POLLIN:
                data = sys.stdin.buffer.read1(4096)
                if args.newline:
                    data = data.replace(b'\r', b'').replace(b'\n', b'')
                socket.send(data)
            else:
                # Probably closed stdin. That's fine, but remove it from the poller.
                poller.unregister(sys.stdin)

        e = events.get(socket, 0)
        if e != 0:
            if e & zmq.POLLIN:
                data = socket.recv()
                sys.stdout.buffer.write(data)
                sys.stdout.flush()
            else:
                # Other error, abort.
                break

if __name__ == '__main__':
    main()
