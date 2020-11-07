import csv
import time

from PySide2.QtCore import QObject, Signal, Slot, Property

class CsvExport(QObject):
    def __init__(self, filename="log.csv", autoFlush=1, parent=None, **fmtparams):
        super().__init__(parent=parent)
        self._fmtparams = fmtparams
        self._filename = filename
        self._objects = set()
        self._csv = None
        self._file = None
        self._autoFlushInterval = autoFlush
        self._autoFlushed = time.time()
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

        self._csv.writerow([t] + list(map(lambda x: x(), self._objValues)))

        if self._autoFlushInterval != None:
            if self._autoFlushed + self._autoFlushInterval <= now:
                self._file.flush()
                self._autoFlushed = now



