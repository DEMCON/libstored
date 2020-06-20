#!/usr/bin/env python3
# vim:et

import sys
import ed2
import argparse

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ZMQ command line client')
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ed2.ZmqServer.default_port, help='port')

    args = parser.parse_args()

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

