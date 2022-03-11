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

import csv
import time
import threading
import queue
import logging
import os

from PySide6.QtCore import QObject, Signal, Slot, Property

def generateFilename(filename=None, base=None, addTimestamp=False, ext='.csv', now=None):
    if filename is None and base is None:
        raise ValueError('Specify filename and/or base')

    returnList = False

    if not isinstance(filename, list):
        if filename is None:
            filename = []
        else:
            filename = [filename]
    else:
        returnList = True

    if not isinstance(base, list):
        if base is None:
            base = []
        else:
            base = [base]
    else:
        returnList = True

    if not isinstance(ext, list):
        if ext is None:
            ext = []
        else:
            ext = [ext]
    else:
        returnList = True

    names = []

    for f in filename:
        names.append(os.path.splitext(f))

    for e in ext:
        for b in base:
            names.append((b, e))

    if now is None:
        now = time.localtime()

    if addTimestamp:
        for i in range(0, len(names)):
            names[i] = (names[i][0] + '_%Y%m%d-%H%M%S%z', names[i][1])

    for i in range(0, len(names)):
        names[i] = time.strftime(names[i][0] + names[i][1], now)

    if returnList:
        return names
    elif len(names) == 0:
        return None
    elif len(names) == 1:
        return names[0]
    else:
        return names

class CsvExport(QObject):
    def __init__(self, filename="log.csv", threaded=True, autoFlush=1, parent=None, **fmtparams):
        super().__init__(parent=parent)
        self.logger = logging.getLogger(__name__)
        self._fmtparams = fmtparams
        if not isinstance(filename, str):
            filename = None
        self._filename = filename
        self._objects = set()
        self._csv = None
        self._file = None
        self._autoFlushInterval = autoFlush
        self._autoFlushed = time.time()
        self._thread = None
        self._queue = None
        self._dropNext = False
        self._lock = threading.RLock()
        self._paused = False
        self._postponedAutoRestart = False

        if filename is None:
            self.pause()
        else:
            self.logger.info('Writing samples to %s...', self._filename)
            self.restart()

        if threaded:
            self._queue = queue.Queue()
            self._thread = threading.Thread(target=self._worker)
            self._thread.daemon = True
            self._thread.start()

    def __del__(self):
        self.close()

    def _clear(self):
        if self._queue is None:
            return

        try:
            self._queue.get(False)
        except queue.Empty:
            pass

        self._dropNext = True

    def add(self, o):
        if o in self._objects:
            return

        self._objects.add(o)
        self._postponedAutoRestart = True

    def remove(self, o):
        if not o in self._objects:
            return

        self._objects.remove(o)
        self._postponedAutoRestart = True

    def restart(self, filename=None):
        self._postponedAutoRestart = False
        self._lock.acquire()

        try:
            if not self._file is None:
                self._file.close()
                self._file = None

            if not filename is None:
                self._filename = filename
                self.logger.info('Writing samples to %s...', self._filename)
                self._paused = False
            elif self._filename is None:
                self.pause()
            elif self._paused:
                self.unpause()

            objList = sorted(self._objects, key=lambda x: x.name)
            self._objValues = [lambda x=x: x._value for x in objList]
            self._clear()

            if not self._filename is None:
                f = open(self._filename, 'w', newline='')
                self._csv = csv.writer(f, **self._fmtparams)
                self._csv.writerow(['t'] + [x.name for x in objList])
                self._file = f

        finally:
            self._lock.release()

    def pause(self, closeFile=False):
        if self._paused:
            return

        self.logger.info('Paused')
        self._paused = True
        if closeFile:
            self._postponedAutoRestart = True
            if not self._file is None:
                self._file.close()
                self._file = None

    def unpause(self):
        if not self._paused or self._file is None:
            return

        self.logger.info('Continue')
        self._paused = False

    @property
    def paused(self):
        return self._paused

    @paused.setter
    def paused(self, value):
        if value:
            self.pause()
        else:
            self.unpause()

    def write(self, t=None):
        if self._paused:
            return

        now = time.time()
        if t is None:
            t = now

        data = [t]
        data.extend(map(lambda x: x(), self._objValues))

        if self._postponedAutoRestart:
            self.restart()

        if self._queue is None:
            self._write(data)
        else:
            self._queue.put(data)

    def close(self):
        if not self._queue is None:
            self._queue.put(None)
            self._thread.join()
            if not self._file is None:
                self._file.flush()

            self._queue = None
            self._thread = None

        if not self._file is None:
            self.logger.info('Close')
            self._file.close()
            self._file = None

    def _write(self, data):
        if self._file is None:
            return

        self._csv.writerow(data)

        if not self._autoFlushInterval is None:
            now = time.time()
            if self._autoFlushed + self._autoFlushInterval <= now:
                self._file.flush()
                self._autoFlushed = now

    def _worker(self):
        while True:
            d = self._queue.get()
            if d is None:
                return
            self._lock.acquire()
            try:
                if self._dropNext:
                    self._dropNext = False
                else:
                    self._write(d)
            finally:
                self._lock.release()

