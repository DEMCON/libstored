#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import logging
import signal
import sys

from PySide6.QtCore import QCoreApplication, QTimer, qInstallMessageHandler, QtMsgType

from ..zmq_client import ZmqClient
from ..zmq_server import ZmqServer
from ..csv import generateFilename
from ..version import __version__

def msgHandler(msgType, context, msg):
    global logger
    if msgType == QtMsgType.QtDebugMsg:
        logger.debug(msg)
    elif msgType == QtMsgType.QtInfoMsg:
        logger.info(msg)
    elif msgType == QtMsgType.QtWarningMsg:
        logger.warning(msg)
    elif msgType == QtMsgType.QtCriticalMsg:
        logger.error(msg)
    else:
        logger.critical(msg)

def signal_handler(sig, stk, app):
    app.exit(1)
    signal.signal(sig, signal.SIG_DFL)

def main():
    global logger

    logger = logging.getLogger('log')
    qInstallMessageHandler(msgHandler)

    QCoreApplication.setApplicationName("libstored.log")
    QCoreApplication.setApplicationVersion(__version__)
    app = QCoreApplication(sys.argv)

    parser = argparse.ArgumentParser(prog=sys.modules[__name__].__package__,
            description='ZMQ command line logging client', formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ZmqServer.default_port, help='port')
    parser.add_argument('-v', dest='verbose', default=0, help='Enable verbose output', action='count')
    parser.add_argument('-f', dest='csv', default='log.csv',
        help='File to log to. The file name may include strftime() format codes.')
    parser.add_argument('-t', dest='timestamp', default=False, help='Append time stamp in csv file name', action='store_true')
    parser.add_argument('-u', dest='unique', default=False,
        help='Make sure that the log filename is unique by appending a suffix', action='store_true')
    parser.add_argument('-m', dest='multi', default=False,
        help='Enable multi-mode; allow multiple simultaneous connections to the same target, ' +
            'but it is less efficient.', action='store_true')
    parser.add_argument('-i', dest='interval', type=float, default=1, help='Poll interval (s)')
    parser.add_argument('-d', dest='duration', type=float, default=None, help='Poll duration (s)')
    parser.add_argument('objects', metavar='obj', type=str, nargs='*', help='Object to poll')
    parser.add_argument('-o', dest='objectfile', type=str, action='append', help='File with list of objects to poll')

    args = parser.parse_args(app.arguments()[1:])

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARN)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    csv = generateFilename(args.csv, addTimestamp=args.timestamp, unique=args.unique)
    logger.info('Log to %s', csv)

    client = ZmqClient(args.server, args.port, multi=args.multi, csv=csv)

    objs = 0

    for o in args.objects:
        obj = client[o]
        logger.info('Poll %s', obj.name)
        obj.poll(args.interval)
        objs += 1

    if args.objectfile is not None:
        for of in args.objectfile:
            with open(of) as f:
                for o in f:
                    obj = client[o.strip()]
                    logger.info('Poll %s', obj.name)
                    obj.poll(args.interval)
                    objs += 1

    if objs == 0:
        logger.error('No objects specified')
        sys.exit(1)

    signal.signal(signal.SIGINT, lambda sig, stk: signal_handler(sig, stk, app))

    if args.duration is not None:
        QTimer.singleShot(int(args.duration * 1000), app.quit)
        logger.info('Start logging for %g s', args.duration)
    else:
        logger.info('Start logging')

    res = app.exec()
    logger.info('Stop logging')
    client.close()

    sys.exit(res)

if __name__ == '__main__':
    main()
