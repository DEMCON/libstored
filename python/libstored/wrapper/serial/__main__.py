#!/usr/bin/env python3

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
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

import argparse
import logging
import re
import serial

from ...zmq_server import ZmqServer
from ...serial2zmq import Serial2Zmq

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='serial wrapper to ZMQ server',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-l', dest='zmqlisten', type=str, default='*', help='ZMQ listen address')
    parser.add_argument('-p', dest='zmqport', type=int, default=ZmqServer.default_port, help='ZMQ port')
    parser.add_argument('port', help='serial port')
    parser.add_argument('baud', nargs='?', type=int, default=115200, help='baud rate')
    parser.add_argument('-r', dest='rtscts', default=False, help='RTS/CTS flow control', action='store_true')
    parser.add_argument('-x', dest='xonxoff', default=False, help='XON/XOFF flow control', action='store_true')
    parser.add_argument('-v', dest='verbose', default=False, help='Enable verbose output', action='store_true')
    parser.add_argument('-S', dest='stack', type=str, default='ascii,pubterm', help='protocol stack')

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    stack = re.sub(r'\bpubterm\b(,|$)', f'pubterm={args.zmqlisten}:{args.zmqport+1}\\1', args.stack)
    bridge = Serial2Zmq(stack=stack, zmqlisten=args.zmqlisten, zmqport=args.zmqport, port=args.port, baudrate=args.baud, rtscts=args.rtscts, xonxoff=args.xonxoff)

    try:
        while True:
            bridge.poll()
    except KeyboardInterrupt:
        pass

    bridge.close()


