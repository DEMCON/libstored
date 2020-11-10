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

import csv
import time
import threading
import queue

from PySide2.QtCore import QObject, Signal, Slot, Property

class CsvExport(QObject):
    def __init__(self, filename="log.csv", threaded=True, autoFlush=1, parent=None, **fmtparams):
        super().__init__(parent=parent)
        self._fmtparams = fmtparams
        self._filename = filename
        self._objects = set()
        self._csv = None
        self._file = None
        self._autoFlushInterval = autoFlush
        self._autoFlushed = time.time()
        self._thread = None
        self._queue = None
        if threaded:
            self._queue = queue.Queue()
            self._thread = threading.Thread(target=self._worker)
            self._thread.daemon = True
            self._thread.start()
        self.restart()

    def add(self, o):
        if o in self._objects:
            return

        self._objects.add(o)
        self.restart()

    def remove(self, o):
        if not o in self._objects:
            return

        self._objects.remove(o)
        self.restart()

    def restart(self):
        objList = sorted(self._objects, key=lambda x: x.name)
        if self._file != None:
            self._file.close()
        self._file = open(self._filename, 'w', newline='')
        self._csv = csv.writer(self._file, **self._fmtparams)
        self._objValues = [lambda x=x: x._value for x in objList]
        self._csv.writerow(['t'] + [x.name for x in objList])

    def write(self, t=None):
        now = time.time()
        if t == None:
            t = now

        data = [t]
        data.extend(map(lambda x: x(), self._objValues))

        if self._queue == None:
            self._write(data)
        else:
            self._queue.put(data)

    def _write(self, data):
        self._csv.writerow(data)

        if self._autoFlushInterval != None:
            now = time.time()
            if self._autoFlushed + self._autoFlushInterval <= now:
                self._file.flush()
                self._autoFlushed = now

    def _worker(self):
        while True:
            self._write(self._queue.get())

