#!/usr/bin/env python3

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import sys
import argparse
import logging

from ..zmq_client import ZmqClient
from ..zmq_server import ZmqServer

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ZMQ command line client')
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ZmqServer.default_port, help='port')
    parser.add_argument('-v', dest='verbose', default=False, help='Enable verbose output', action='store_true')

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    client = ZmqClient(args.server, args.port, multi=True)

    try:
        prefix = '>  '
        sys.stdout.write(prefix)
        sys.stdout.flush()
        for line in sys.stdin:
            line = line.strip()
            if len(line) > 0:
                print('<  ' + client.req(line.encode()).decode())
            sys.stdout.write(prefix)
            sys.stdout.flush()

    except KeyboardInterrupt:
        pass

    client.close()

