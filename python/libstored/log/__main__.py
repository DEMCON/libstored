# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import aiofiles
import asyncio
import argparse
import logging
import sys

from ..asyncio.zmq import ZmqClient
from ..asyncio.worker import run_sync
from ..asyncio.csv import generate_filename, CsvExport
from ..version import __version__
from .. import protocol as lprot

@run_sync
async def async_main(args : argparse.Namespace) -> int:
    global logger

    filename : str = args.csv
    if filename != '-':
        filename = generate_filename(filename, add_timestamp=args.timestamp, unique=args.unique)

    stack = None
    if args.encrypted:
        stack = lprot.Aes256Layer(args.encrypted, reqrep=True)

    async with ZmqClient(args.server, args.port, multi=args.multi, stack=stack) as client:
        objs = []

        for o in args.objects:
            try:
                obj = client[o]
            except ValueError:
                logger.fatal('Unknown object: %s', o)
                return 1

            if obj not in objs:
                objs.append(obj)

        if args.objectfile is not None:
            for of in args.objectfile:
                async with aiofiles.open(of) as f:
                    async for o in f:
                        o = o.strip()
                        try:
                            obj = client[o]
                        except ValueError:
                            logger.fatal('Unknown object: %s', o)
                            return 1

                        if obj not in objs:
                            objs.append(obj)

        if not objs:
            logger.error('No objects specified')
            return 1

        for o in objs:
            logger.info('Poll %s', o.name)
            await o.poll(args.interval)

        async with CsvExport(filename) as csv:
            for obj in objs:
                await csv.add(obj)

            if args.duration is not None:
                logger.info('Start logging for %g s', args.duration)
                await asyncio.sleep(args.duration)
            else:
                logger.info('Start logging')
                await asyncio.Event().wait()

    return 0

def main():
    global logger

    logger = logging.getLogger('log')

    parser = argparse.ArgumentParser(prog=__package__,
            description='ZMQ command line logging client', formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('-V', '--version', action='version', version=__version__)
    parser.add_argument('-s', '--server', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', '--port', dest='port', type=int, default=lprot.default_port, help='port')
    parser.add_argument('-v', '--verbose', dest='verbose', default=0, help='Enable verbose output', action='count')
    parser.add_argument('-f', '--csv', dest='csv', default='-',
        help='File to log to. The file name may include strftime() format codes.')
    parser.add_argument('-t', '--timestamp', dest='timestamp', default=False, help='Append time stamp in csv file name', action='store_true')
    parser.add_argument('-u', '--unique', dest='unique', default=False,
        help='Make sure that the log filename is unique by appending a suffix', action='store_true')
    parser.add_argument('-m', '--multi', dest='multi', default=False,
        help='Enable multi-mode; allow multiple simultaneous connections to the same target, ' +
            'but it is less efficient.', action='store_true')
    parser.add_argument('-i', '--interval', dest='interval', type=float, default=1, help='Poll interval (s)')
    parser.add_argument('-d', '--duration', dest='duration', type=float, default=None, help='Poll duration (s)')
    parser.add_argument('objects', metavar='obj', type=str, nargs='*', help='Object to poll')
    parser.add_argument('-o', '--objectfile', dest='objectfile', type=str, action='append', help='File with list of objects to poll')
    parser.add_argument('-e', '--encrypt', dest='encrypted', type=str, default=None,
        help='Enable AES-256 CTR encryption with the given pre-shared key file', metavar='file')

    args = parser.parse_args()

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARN)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    res = 1
    try:
        res = async_main(args)
    except KeyboardInterrupt:
        logger.info('Interrupted, exiting')

    sys.exit(res)

if __name__ == '__main__':
    main()
