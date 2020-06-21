#!/usr/bin/env python3
# vim:et

import sys
import ed2
import argparse
import os
from PySide2.QtGui import QGuiApplication
from PySide2.QtQml import QQmlApplicationEngine
from PySide2.QtCore import QUrl, QAbstractListModel, QModelIndex, Qt

class ObjectListModel(QAbstractListModel):
    ObjectRole = Qt.UserRole + 1000

    def __init__(self, objects, parent=None):
        super().__init__(parent)
        self._objects = objects

    def rowCount(self, parent=QModelIndex()):
        if parent.isValid():
            return 0
        return len(self._objects)

    def data(self, index, role=Qt.DisplayRole):
        if 0 <= index.row() < self.rowCount() and index.isValid():
            o = self._objects[index.row()]
            if role == self.ObjectRole:
                return o

    def roleNames(self):
        return {
            self.ObjectRole: b'obj',
        }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ZMQ command line client')
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ed2.ZmqServer.default_port, help='port')

    args = parser.parse_args()
    app = QGuiApplication(sys.argv)
    engine = QQmlApplicationEngine()

    client = ed2.ZmqClient(args.server, args.port)
    engine.rootContext().setContextProperty("client", client)
    engine.rootContext().setContextProperty("objects", ObjectListModel(client.list(), app))

    engine.load(QUrl.fromLocalFile(os.path.join(os.path.dirname(os.path.realpath(__file__)), "gui_client.qml")))
    if not engine.rootObjects():
        sys.exit(-1)
    sys.exit(app.exec_())

