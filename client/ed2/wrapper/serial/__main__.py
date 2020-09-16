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

import argparse
import serial
import logging

from ...zmq_server import ZmqServer
from ...serial2zmq import Serial2Zmq

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='stdin/stdout wrapper to ZMQ server',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-p', dest='zmqport', type=int, default=ZmqServer.default_port, help='ZMQ port')
    parser.add_argument('port', help='serial port')
    parser.add_argument('baud', nargs='?', type=int, default=115200, help='baud rate')
    parser.add_argument('-r', dest='rtscts', default=False, help='RTS/CTS flow control', action='store_true')
    parser.add_argument('-v', dest='verbose', default=False, help='Enable verbose output', action='store_true')
    parser.add_argument('-S', dest='stack', type=str, default='ascii,term', help='protocol stack')

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    bridge = Serial2Zmq(stack=args.stack, zmqport=args.zmqport, port=args.port, baudrate=args.baud, rtscts=args.rtscts)

    try:
        while True:
            bridge.poll()
    except KeyboardInterrupt:
        pass

    bridge.close()


