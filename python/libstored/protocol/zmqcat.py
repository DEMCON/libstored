#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import asyncio
import logging
import zmq

from .. import protocol as lprot
from ..asyncio.worker import AsyncioWorker, run_sync

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

    stack = lprot.stack([
        lprot.PrintLayer(),
        lprot.StdinLayer(),
        lprot.ZmqSocketClient(server=args.host, port=int(args.port), type=type)
    ])

    try:
        while True:
            await asyncio.sleep(3600)
    except asyncio.CancelledError:
        pass
    finally:
        await stack.close()



def main():
    parser = argparse.ArgumentParser(prog=__package__,
                                     description='ZMQ cat utility that fits nicely with libstored.protocol.ZmqSocketServer',
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    from ..version import __version__
    parser.add_argument('-s', dest='host', help='Server hostname', default='localhost')
    parser.add_argument('-p', dest='port', help='Specify TCP port')
    parser.add_argument('-t', dest='type', choices=['dealer', 'pair', 'req'], help='Socket type', default='dealer')
    parser.add_argument('-v', dest='verbose', default=0, help='Enable verbose output', action='count')

    args = parser.parse_args()

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARN)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    lprot.set_infinite_stdout()

    with AsyncioWorker() as w:
        try:
            async_main(args, block=True, sync=True)
        except KeyboardInterrupt:
            w.cancel()

if __name__ == '__main__':
    main()
