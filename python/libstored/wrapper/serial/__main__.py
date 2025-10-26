#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import asyncio
import logging
import re
import serial

from ...version import __version__
from ... import protocol as lprot
from ...asyncio.worker import AsyncioWorker, run_sync

def main():
    parser = argparse.ArgumentParser(description='serial wrapper to ZMQ server',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter, prog=__package__)
    parser.add_argument('-V', action='version', version=__version__)
    parser.add_argument('-l', dest='zmqlisten', type=str, default='*', help='ZMQ listen address')
    parser.add_argument('-p', dest='zmqport', type=int, default=lprot.default_port, help='ZMQ port')
    parser.add_argument('port', help='serial port')
    parser.add_argument('baud', nargs='?', type=int, default=115200, help='baud rate')
    parser.add_argument('-r', dest='rtscts', default=False, help='RTS/CTS flow control', action='store_true')
    parser.add_argument('-x', dest='xonxoff', default=False, help='XON/XOFF flow control', action='store_true')
    parser.add_argument('-v', dest='verbose', default=0, help='Enable verbose output', action='count')
    parser.add_argument('-S', dest='stack', type=str, default='ascii,pubterm,stdin', help='protocol stack')

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
            @run_sync
            async def async_main(args : argparse.Namespace):
                stack = lprot.build_stack(
                    ','.join([
                        f'zmq={args.zmqlisten}:{args.zmqport}',
                        'repreqcheck',
                        re.sub(r'\bpubterm\b(,|$)', f'pubterm={args.zmqlisten}:{args.zmqport+1}\\1', args.stack)])
                    )

                serial = lprot.SerialLayer(port=args.port, baudrate=args.baud, rtscts=args.rtscts, xonxoff=args.xonxoff)
                serial.wrap(stack)
                try:
                    while True:
                        await asyncio.sleep(3600)
                finally:
                    await stack.close()

            async_main(args)
        except KeyboardInterrupt:
            w.cancel()

if __name__ == '__main__':
    main()
