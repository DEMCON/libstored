#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import logging
import re
import sys

from ..version import __version__
from .. import protocol as lprot
from ..asyncio.worker import AsyncioWorker, run_sync

def main():
    parser = argparse.ArgumentParser(description='stdin/stdout wrapper to ZMQ server',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter, prog=__package__)
    parser.add_argument('-V', '--version', action='version', version=__version__)
    parser.add_argument('-l', '--listen', dest='listen', type=str, default='*', help='listen address')
    parser.add_argument('-p', '--port', dest='port', type=int, default=lprot.default_port, help='port')
    parser.add_argument('-S', '--stack', dest='stack', type=str, default='ascii,pubterm,stdin', help='protocol stack')
    parser.add_argument('-v', '--verbose', dest='verbose', default=0, help='Enable verbose output', action='count')
    parser.add_argument('command')
    parser.add_argument('args', nargs='*')

    args = parser.parse_args()

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARN)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    lprot.set_infinite_stdout()

    ret = 0
    with AsyncioWorker() as w:
        try:
            @run_sync
            async def async_main(args : argparse.Namespace):
                stack = lprot.build_stack(
                    ','.join([
                        f'zmq={args.listen}:{args.port}',
                        'repreqcheck',
                        re.sub(r'\bpubterm\b(,|$)', f'pubterm={args.listen}:{args.port+1}\\1', args.stack)])
                    )

                stdio = lprot.StdioLayer(cmd=[args.command] + args.args)
                stdio.wrap(stack)

                async def at_term(code):
                    nonlocal ret
                    ret = code
                    await stack.close()
                    w.cancel()

                stdio.set_terminate_callback(at_term)

            async_main(args)
        except KeyboardInterrupt:
            w.cancel()

    sys.exit(ret)

if __name__ == '__main__':
    main()
