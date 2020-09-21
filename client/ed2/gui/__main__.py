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
# \brief A Pyside2/QtQuick GUI client.
# \ingroup libstored_client

import sys
import argparse
import os
import natsort
import logging

from PySide2.QtGui import QGuiApplication, QIcon
from PySide2.QtQml import QQmlApplicationEngine
from PySide2.QtCore import QUrl, QAbstractListModel, QModelIndex, Qt, Slot, QSortFilterProxyModel, QCoreApplication

from lognplot.client import LognplotTcpClient

from ..zmq_client import ZmqClient
from ..zmq_server import ZmqServer

class NatSort(QSortFilterProxyModel):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def lessThan(self, left, right):
        left_data = self.sourceModel().data(left, role=self.sortRole())
        right_data = self.sourceModel().data(right, role=self.sortRole())

        alg = natsort.ns.REAL | natsort.ns.LOCALE
        if self.sortCaseSensitivity() == Qt.CaseInsensitive:
            alg = alg | natsort.ns.IGNORECASE

        sorted_data = natsort.natsorted([left_data, right_data], alg=alg)
        return left_data == sorted_data[0]

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
            assert o != None
            if role == self.NameRole or role == Qt.DisplayRole:
                return o.name
            elif role == self.ObjectRole:
                return o
            elif role == self.PollingRole:
                return o.polling

    @Slot(int, result='QVariant')
    def at(self, index):
        o = self._objects[index]
        assert o != None
        return o

    def roleNames(self):
        return {
            self.NameRole: b'name',
            self.ObjectRole: b'obj',
            self.PollingRole: b'polling',
        }

def lognplot_send(lognplot, o):
    if not o.polling:
        return
    if not o.isFixed():
        # Not supported
        return

    try:
        lognplot.send_sample(o.name, o.t, float(o.value))
    except ConnectionResetError:
        print(f'Reconnecting to lognplot...')
        try:
            lognplot.connect()
        except:
            pass



if __name__ == '__main__':
    QCoreApplication.setApplicationName("Embedded Debugger")

    parser = argparse.ArgumentParser(description='ZMQ command line client')
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ZmqServer.default_port, help='port')
    parser.add_argument('-l', dest='lognplot', type=str, nargs='?', default=None, help='Connect to lognplot server', const='localhost')
    parser.add_argument('-P', dest='lognplotport', type=int, default=12345, help='Lognplot port to connect to')
    parser.add_argument('-v', dest='verbose', default=False, help='Enable verbose output', action='store_true')

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    app = QGuiApplication(sys.argv)
    app.setWindowIcon(QIcon(os.path.join(os.path.dirname(os.path.realpath(__file__)), "twotone_bug_report_black_48dp.png")))
    engine = QQmlApplicationEngine(parent=app)

    client = ZmqClient(args.server, args.port)
    engine.rootContext().setContextProperty("client", client)

    model = ObjectListModel(client.list(), parent=app)
    filteredObjects = NatSort(parent=app)
    filteredObjects.setSourceModel(model)
    filteredObjects.setSortRole(model.NameRole)
    filteredObjects.setFilterRole(model.NameRole)
    filteredObjects.setSortCaseSensitivity(Qt.CaseInsensitive)
    filteredObjects.sort(0)
    polledObjects = NatSort(parent=app)
    polledObjects.setSourceModel(model)
    polledObjects.setSortRole(model.NameRole)
    polledObjects.setFilterRole(model.PollingRole)
    polledObjects.setFilterRegularExpression('true')
    polledObjects.setSortCaseSensitivity(Qt.CaseInsensitive)
    polledObjects.sort(0)

    engine.rootContext().setContextProperty("objects", filteredObjects)
    engine.rootContext().setContextProperty("polledObjects", polledObjects)

    if args.lognplot != None:
        print(f'Connecting to lognplot at {args.lognplot}:{args.lognplotport}...')
        lognplot = LognplotTcpClient(args.lognplot, args.lognplotport)
        lognplot.connect()
        for o in client.objects:
            if o.isFixed():
                o.valueUpdated.connect(lambda o=o: lognplot_send(lognplot, o))

    engine.load(QUrl.fromLocalFile(os.path.join(os.path.dirname(os.path.realpath(__file__)), "gui_client.qml")))
    if not engine.rootObjects():
        sys.exit(-1)

    client.restoreState()

    res = app.exec_()

    client.saveState()
    sys.exit(res)

