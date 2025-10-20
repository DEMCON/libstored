# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import aiofiles
import asyncio
import argparse
import logging
import sys

from ..asyncio.zmq_client import ZmqClient
from ..asyncio.worker import AsyncioWorker, run_sync
from ..zmq_server import ZmqServer
from ..asyncio.csv import generate_filename, CsvExport
from ..version import __version__

@run_sync
async def async_main(args : argparse.Namespace):
    global logger

    filename : str = args.csv
    if filename != '-':
        filename = generate_filename(filename, add_timestamp=args.timestamp, unique=args.unique)

    async with CsvExport(filename) as csv:
        async with ZmqClient(args.server, args.port, multi=args.multi) as client:
            objs = 0

            for o in args.objects:
                obj = client[o]
                logger.info('Poll %s', obj.name)
                await obj.poll(args.interval)
                await csv.add(obj)
                objs += 1

            if args.objectfile is not None:
                for of in args.objectfile:
                    async with aiofiles.open(of) as f:
                        async for o in f:
                            obj = client[o.strip()]
                            logger.info('Poll %s', obj.name)
                            await obj.poll(float(args.interval))
                            await csv.add(obj)
                            objs += 1

            if objs == 0:
                logger.error('No objects specified')
                return

            if args.duration is not None:
                logger.info('Start logging for %g s', args.duration)
                await asyncio.sleep(args.duration)
            else:
                logger.info('Start logging')
                await asyncio.Event().wait()

def main():
    global logger

    logger = logging.getLogger('log')

    parser = argparse.ArgumentParser(prog=sys.modules[__name__].__package__,
            description='ZMQ command line logging client', formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ZmqServer.default_port, help='port')
    parser.add_argument('-v', dest='verbose', default=0, help='Enable verbose output', action='count')
    parser.add_argument('-f', dest='csv', default='-',
        help='File to log to. The file name may include strftime() format codes.')
    parser.add_argument('-t', dest='timestamp', default=False, help='Append time stamp in csv file name', action='store_true')
    parser.add_argument('-u', dest='unique', default=False,
        help='Make sure that the log filename is unique by appending a suffix', action='store_true')
    parser.add_argument('-m', dest='multi', default=False,
        help='Enable multi-mode; allow multiple simultaneous connections to the same target, ' +
            'but it is less efficient.', action='store_true')
    parser.add_argument('-i', dest='interval', type=float, default=1, help='Poll interval (s)')
    parser.add_argument('-d', dest='duration', type=float, default=None, help='Poll duration (s)')
    parser.add_argument('objects', metavar='obj', type=str, nargs='*', help='Object to poll')
    parser.add_argument('-o', dest='objectfile', type=str, action='append', help='File with list of objects to poll')

    args = parser.parse_args()

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARN)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    try:
        async_main(args)
    except KeyboardInterrupt:
        logger.info('Interrupted, exiting')

if __name__ == '__main__':
    main()
