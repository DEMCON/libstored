#!/usr/bin/env python3
# vim:et

import sys
import ed2
import argparse
import os
from PySide2.QtGui import QGuiApplication
from PySide2.QtQml import QQmlApplicationEngine
from PySide2.QtCore import QUrl, QAbstractListModel, QModelIndex, Qt, Slot, QSortFilterProxyModel 

class ObjectListModel(QAbstractListModel):
    NameRole = Qt.UserRole + 1000
    ObjectRole = Qt.UserRole + 1001
    PollingRole = Qt.UserRole + 1002

    def __init__(self, objects, parent=None):
        super().__init__(parent)
        self._objects = objects
        for i in range(0, len(objects)):
            objects[i].pollingChanged.connect(lambda i=i: self._pollingChanged(i))
    
    def _pollingChanged(self, i):
        index = self.createIndex(i, 0)
        self.dataChanged.emit(index, index, [self.PollingRole])

    def rowCount(self, parent=QModelIndex()):
        if parent.isValid():
            return 0
        return len(self._objects)

    def data(self, index, role=Qt.DisplayRole):
        if 0 <= index.row() < self.rowCount() and index.isValid():
            o = self._objects[index.row()]
            if role == self.NameRole:
                return o.name
            elif role == self.ObjectRole:
                return o
            elif role == self.PollingRole:
                return o.polling

    @Slot(int, result='QVariant')
    def at(self, index):
        return self._objects[index];

    def roleNames(self):
        return {
            self.NameRole: b'name',
            self.ObjectRole: b'obj',
            self.PollingRole: b'polling',
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

    model = ObjectListModel(client.list(), parent=app)
    filteredObjects = QSortFilterProxyModel(parent=app)
    filteredObjects.setSourceModel(model)
    filteredObjects.setSortRole(model.NameRole)
    filteredObjects.setFilterRole(model.NameRole)
    polledObjects = QSortFilterProxyModel(parent=app)
    polledObjects.setSourceModel(model)
    polledObjects.setSortRole(model.NameRole)
    polledObjects.setFilterRole(model.PollingRole)
    polledObjects.setFilterRegularExpression('true')

    engine.rootContext().setContextProperty("objects", filteredObjects)
    engine.rootContext().setContextProperty("polledObjects", polledObjects)

    engine.load(QUrl.fromLocalFile(os.path.join(os.path.dirname(os.path.realpath(__file__)), "gui_client.qml")))
    if not engine.rootObjects():
        sys.exit(-1)
    sys.exit(app.exec_())
