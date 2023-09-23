#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import sys
import argparse
import os
import logging
import importlib.util

from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtCore import Qt, QCoreApplication, qInstallMessageHandler, QtMsgType
from PySide6.QtQuickControls2 import QQuickStyle

from ..zmq_client import ZmqClient
from ..zmq_server import ZmqServer
from ..csv import generateFilename

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

def main():
    global logger

    logger = logging.getLogger('visu')
    qInstallMessageHandler(msgHandler)

    QCoreApplication.setApplicationName("Embedded Debugger visu")

    parser = argparse.ArgumentParser(description='ZMQ visu', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ZmqServer.default_port, help='port')
    parser.add_argument('-v', dest='verbose', default=False, help='Enable verbose output', action='store_true')
    parser.add_argument('-f', dest='csv', default=None, nargs='?',
        help='Log auto-refreshed data to csv file. ' +
            'The file is truncated upon startup and when the set of auto-refreshed objects change. ' +
            'The file name may include strftime() format codes.', const='')
    parser.add_argument('-t', dest='timestamp', default=False, help='Append time stamp in csv file name', action='store_true')
    parser.add_argument('-m', dest='multi', default=False,
        help='Enable multi-mode; allow multiple simultaneous connections to the same target, ' +
            'but it is less efficient.', action='store_true')
    parser.add_argument('rcc', type=str, nargs=1, help='rcc file with ":/main.qml" visu')

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    app = QGuiApplication(sys.argv)
    QQuickStyle.setStyle("Basic")

    csv = args.csv
    if csv != None:
        if csv == '':
            try:
                csv = os.path.splitext(os.path.basename(args.rcc[0]))[0] + '.csv'
            except:
                pass
        if csv == '':
            csv = 'visu.csv'

        csv = generateFilename(csv, addTimestamp=args.timestamp)

    client = ZmqClient(args.server, args.port, csv=csv, multi=args.multi)

    engine = QQmlApplicationEngine(parent=app)
    engine.rootContext().setContextProperty("client", client)

    if os.path.exists(args.rcc[0]):
        # Try loading from file.
        spec = importlib.util.spec_from_file_location("visu_rcc", args.rcc[0])
        visu_rcc = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(visu_rcc)
    else:
        # Try loading as module.
        importlib.import_module(args.rcc[0])

    engine.addImportPath("qrc:/");
    engine.load('qrc:/main.qml')
    if not engine.rootObjects():
        sys.exit(-1)

    res = app.exec()
    client.close()
    sys.exit(res)

if __name__ == '__main__':
    main()
