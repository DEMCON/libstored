#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import aiofiles
import argparse
import logging
import os

from ..asyncio.zmq_client import ZmqClient
from ..asyncio.worker import run_sync, AsyncioWorker
from ..version import __version__
from .. import protocol as lprot

@run_sync
async def async_main(args : argparse.Namespace):
    async with ZmqClient(args.server, args.port, multi=True) as client:
        prefix = '>  '
        await aiofiles.stdout.write(prefix)
        await aiofiles.stdout.flush()
        async for line in aiofiles.stdin:
            line = line.strip()
            if len(line) > 0:
                await aiofiles.stdout.write('<  ' + await client.req(line) + '\n')
            await aiofiles.stdout.write(prefix)
            await aiofiles.stdout.flush()

def main():
    parser = argparse.ArgumentParser(description='ZMQ command line client', prog=__package__)
    parser.add_argument('-V', action='version', version=__version__)
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=lprot.default_port, help='port')
    parser.add_argument('-v', dest='verbose', default=0, help='Enable verbose output', action='count')

    args = parser.parse_args()

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARN)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    with AsyncioWorker() as w:
        try:
            async_main(args)
        except KeyboardInterrupt:
            pass
        finally:
            w.cancel()

    os._exit(0)

if __name__ == '__main__':
    main()
