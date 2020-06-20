#!/usr/bin/env python3
# vim:et

import ed2
import argparse

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='stdin/stdout wrapper to ZMQ server')
    parser.add_argument('-p', dest='port', type=int, default=ed2.ZmqServer.default_port, help='port')
    parser.add_argument('command')
    parser.add_argument('args', nargs='*')

    args = parser.parse_args()

    bridge = ed2.Stdio2Zmq([args.command] + args.args, args.port)

    try:
        while True:
            bridge.poll()
    except KeyboardInterrupt:
        pass

    bridge.close()

