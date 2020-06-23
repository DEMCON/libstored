#!/usr/bin/env python3
# vim:et

import ed2
import argparse
import serial

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='stdin/stdout wrapper to ZMQ server',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-p', dest='zmqport', type=int, default=ed2.ZmqServer.default_port, help='ZMQ port')
    parser.add_argument('port', help='serial port')
    parser.add_argument('baud', nargs='?', type=int, default=115200, help='baud rate')
    parser.add_argument('-r', dest='rtscts', default=False, help='RTS/CTS flow control', action='store_true')

    args = parser.parse_args()

    bridge = ed2.Serial2Zmq(args.port, port=args.port, baudrate=args.baud, rtscts=args.rtscts)

    try:
        while True:
            bridge.poll()
    except KeyboardInterrupt:
        pass

    bridge.close()


