#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import logging
import re

from ...zmq_server import ZmqServer
from ...stdio2zmq import Stdio2Zmq

def main():
    parser = argparse.ArgumentParser(description='stdin/stdout wrapper to ZMQ server',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-l', dest='listen', type=str, default='*', help='listen address')
    parser.add_argument('-p', dest='port', type=int, default=ZmqServer.default_port, help='port')
    parser.add_argument('-S', dest='stack', type=str, default='ascii,pubterm', help='protocol stack')
    parser.add_argument('-v', dest='verbose', default=False, help='Enable verbose output', action='store_true')
    parser.add_argument('command')
    parser.add_argument('args', nargs='*')

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    stack = re.sub(r'\bpubterm\b(,|$)', f'pubterm={args.listen}:{args.port+1}\\1', args.stack)
    bridge = Stdio2Zmq(args=[args.command] + args.args, stack=stack, listen=args.listen, port=args.port)

    try:
        while True:
            bridge.poll()
    except KeyboardInterrupt:
        pass

    bridge.close()

if __name__ == '__main__':
    main()
