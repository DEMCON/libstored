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

import sys
import argparse
import os
import natsort
import logging
import time

from PySide6.QtGui import QGuiApplication, QIcon
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtCore import QUrl, QAbstractListModel, QModelIndex, Qt, Slot, \
    QSortFilterProxyModel, QCoreApplication, qInstallMessageHandler, QtMsgType, \
    QObject, QTimer, Property, Signal
from PySide6.QtQuickControls2 import QQuickStyle

from . import gui_qrc

try:
    import matplotlib.pyplot as plt
    from PySide6.QtWidgets import QApplication
    haveMpl = True
except:
    haveMpl = False

try:
    from lognplot.client import LognplotTcpClient
    haveLognplot = True
except:
    haveLognplot = False

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

class Data:
    WINDOW_s = 30

    def __init__(self):
        self.t = []
        self.values = []

    def append(self, value, t=time.time()):
        self.t.append(t)
        self.values.append(value)

    def cleanup(self):
        if self.t == []:
            return

        drop = 0
        threshold = self.t[-1] - self.WINDOW_s

        for t in self.t:
            if t < threshold:
                drop += 1
            else:
                break

        if drop == 0:
            return

        self.t = self.t[drop:]
        self.values = self.values[drop:]


class Plotter(QObject):
    _instance = None
    _available = haveMpl
    title = None

    def __init__(self, parent=None):
        super().__init__(parent=parent)
        self.__class__._instance = self
        self._data = {}
        self._fig = None
        self._ax = None
        self._first = True
        self._changed = set()
        self._timer = QTimer(parent=self)
        self._timer.setInterval(100)
        self._timer.timeout.connect(self._update_plot)
        self._paused = False

    @classmethod
    def instance(cls):
        if cls._instance is None:
            cls._instance = Plotter()

        return cls._instance

    @classmethod
    def add(cls, o):
        if not cls._available:
            return

        cls.instance()._add(o)

    def _add(self, o):
        if o in self._data:
            return

        if not o.isFixed:
            return

        if self._fig is None:
            self._fig, self._ax = plt.subplots()

        data = Data()
        self._data[o] = data

        value = o.value
        if value is not None:
            self._data[o].append(value, o.t)

        data.connection = o.valueUpdated.connect(lambda: self._update(o, o.value, o.t))

        data.line = self._ax.plot([], [], label=o.name)[0]
        self.update_legend()

        self.show()

        if len(self._data) == 1:
            self.plottingChanged.emit()
            self._timer.start()

    def _update(self, o, value, t=time.time()):
        if o not in self._data:
            return
        if value is None:
            return

        data = self._data[o]
        data.append(value, t)
        self._changed.add(data)

    def _update_plot(self):
        if len(self._changed) == 0 or self._paused:
            return

        for data in self._changed:
            data.cleanup()
            data.line.set_data(data.t, data.values)

        self._changed.clear()

        self._ax.relim()
        self._ax.autoscale()
        self._fig.canvas.draw()

    @classmethod
    def remove(cls, o):
        if not cls._available:
            return

        cls.instance()._remove(o)

    def _remove(self, o):
        if o not in self._data:
            return

        data = self._data[o]
        QObject.disconnect(data.connection)
        self._ax.lines.remove(data.line)
        del self._data[o]

        if len(self._data) > 0:
            self.update_legend()
        else:
            self.plottingChanged.emit()
            self._timer.stop()

    def show(self):
        if self._first:
            if self.title is not None:
                self._ax.set_title(self.title)
                self._fig.canvas.manager.set_window_title(f'Embedded Debugger plots: {self.title}')
            else:
                self._fig.canvas.manager.set_window_title(f'Embedded Debugger plots')

            self._ax.grid(True)
            self._ax.set_xlabel('t (s)')
            self.update_legend()
            plt.show(block=False)
            self._first = False
        else:
            self._fig.show()

    def update_legend(self):
        self._ax.legend().set_draggable(True)

    pausedChanged = Signal()

    @Slot()
    @Slot(bool)
    def pause(self, paused=True):
        if self._paused == paused:
            return

        self._paused = paused
        self.pausedChanged.emit()

    @Slot()
    def resume(self):
        self.pause(False)

    @Slot()
    def togglePause(self):
        self.pause(not self._paused)

    def _paused_get(self):
        return self._paused

    paused = Property(bool, _paused_get, notify=pausedChanged)

    def _available_get(self):
        return self._available

    available = Property(bool, _available_get, constant=True)

    plottingChanged = Signal()

    def _plotting_get(self):
        return len(self._data) != 0

    plotting = Property(bool, _plotting_get, notify=plottingChanged)



class ObjectListModel(QAbstractListModel):
    NameRole = Qt.UserRole + 1000
    ObjectRole = Qt.UserRole + 1001
    PollingRole = Qt.UserRole + 1002
    PlotRole = Qt.UserRole + 1003

    def __init__(self, objects, parent=None):
        super().__init__(parent)
        self._objects = objects
        self._plot = set()
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
            elif role == self.PlotRole:
                return o in self._plot

    def setData(self, index, value, role=Qt.EditRole):
        if 0 <= index.row() < self.rowCount() and index.isValid():
            o = self._objects[index.row()]
            assert o != None

            if role != self.PlotRole:
                return False

            if value and o not in self._plot:
                self._plot.add(o)
                self.addPlot(o)
            elif not value and o in self._plot:
                self.removePlot(o)
                self._plot.remove(o)
            else:
                return False

            self.dataChanged.emit(index,index, [role])
            return True

    def addPlot(self, o):
        Plotter.add(o)

    def removePlot(self, o):
        Plotter.remove(o)

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
            self.PlotRole: b'plot',
        }

def lognplot_send(lognplot, o):
    if not haveLognplot:
        return
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
    logger = logging.getLogger('gui')
    qInstallMessageHandler(msgHandler)

    QCoreApplication.setApplicationName("Embedded Debugger")
    QCoreApplication.setApplicationVersion(__version__)
    try:
        QGuiApplication.setHighDpiScaleFactorRoundingPolicy(Qt.HighDpiScaleFactorRoundingPolicy.PassThrough)
    except:
        pass

    if haveMpl:
        # Need to have widgets for matplotlib
        app = QApplication(sys.argv)
    else:
        app = QGuiApplication(sys.argv)

    QQuickStyle.setStyle("Basic")

    parser = argparse.ArgumentParser(prog=sys.modules[__name__].__package__, description='ZMQ GUI client', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-V', action='version', version=__version__)
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ZmqServer.default_port, help='port')
    if haveLognplot:
        parser.add_argument('-l', dest='lognplot', type=str, nargs='?', default=None, help='Connect to lognplot server', const='localhost')
        parser.add_argument('-P', dest='lognplotport', type=int, default=12345, help='Lognplot port to connect to')
    parser.add_argument('-v', dest='verbose', default=False, help='Enable verbose output', action='store_true')
    parser.add_argument('-f', dest='csv', default=None, nargs='?',
        help='Log auto-refreshed data to csv file. ' +
            'The file is truncated upon startup and when the set of auto-refreshed objects change. ' +
            'The file name may include strftime() format codes.', const='log.csv')
    parser.add_argument('-t', dest='timestamp', default=False, help='Append time stamp in csv file name', action='store_true')
    parser.add_argument('-m', dest='multi', default=False,
        help='Enable multi-mode; allow multiple simultaneous connections to the same target, ' +
            'but it is less efficient.', action='store_true')
    parser.add_argument('-c', dest='clearState', default=False, help='Clear previously saved state', action='store_true')

    args = parser.parse_args(app.arguments()[1:])

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)

    app.setWindowIcon(QIcon(":/twotone_bug_report_black_48dp.png"))
    engine = QQmlApplicationEngine(parent=app)

    csv = None
    if not args.csv is None:
        csv = generateFilename(args.csv, addTimestamp=args.timestamp)

    client = ZmqClient(args.server, args.port, csv=csv, multi=args.multi)
    Plotter.title = client.identification()

    engine.rootContext().setContextProperty("client", client)

    if haveLognplot and args.lognplot != None:
        Plotter._available = False
    plotter = Plotter(parent=app)
    engine.rootContext().setContextProperty("plotter", plotter)

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

    if haveLognplot and args.lognplot != None:
        print(f'Connecting to lognplot at {args.lognplot}:{args.lognplotport}...')
        lognplot = LognplotTcpClient(args.lognplot, args.lognplotport)
        lognplot.connect()
        for o in client.objects:
            if o.isFixed():
                o.valueUpdated.connect(lambda o=o: lognplot_send(lognplot, o))

    engine.addImportPath("qrc:/");
    engine.load('qrc:/main.qml')
    if not engine.rootObjects():
        sys.exit(-1)

    if not args.clearState:
        # We are not really clearing the state; we won't load it, but we still
        # do overwrite it afterwards.
        client.restoreState()

    res = app.exec()

    client.saveState()
    client.close()
    sys.exit(res)

