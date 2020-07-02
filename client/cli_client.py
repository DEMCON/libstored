#!/usr/bin/env python3
# vim:et

# libstored, a Store for Embedded Debugger.
# Copyright (C) 2020  Jochem Rutgers
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

##
# \file
# \brief A Command line interface client to enter Embedded Debugger commands.
# \ingroup libstored_client

import sys
import ed2
import argparse
import logging

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ZMQ command line client')
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ed2.ZmqServer.default_port, help='port')
    parser.add_argument('-v', dest='verbose', default=False, help='Enable verbose output', action='store_true')

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    client = ed2.ZmqClient(args.server, args.port)

    try:
        prefix = '>  '
        sys.stdout.write(prefix)
        sys.stdout.flush()
        for line in sys.stdin:
            print('<  ' + client.req(line.strip().encode()).decode())
            sys.stdout.write(prefix)
            sys.stdout.flush()

    except KeyboardInterrupt:
        pass

    client.close()

