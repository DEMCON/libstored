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

import unittest
import subprocess
import ed2
import sys
from PySide2.QtCore import QCoreApplication

class ZmqClientTest(unittest.TestCase):

    binary = None

    @classmethod
    def setUpClass(cls):
        cls.process = subprocess.Popen([cls.binary],
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()

    def test_dummy(self):
        self.assertTrue(True)

    def test_ed2_version(self):
        print(ed2.__version__)

    def test_identification(self):
        c = ed2.ZmqClient()
        self.assertEqual(c.identification(), 'zmqserver')

    def test_version(self):
        c = ed2.ZmqClient()
        self.assertEqual(c.version().split(' ')[0], '2')

if __name__ == '__main__':
    if len(sys.argv) == 0 or not 'zmqserver' in sys.argv[-1]:
        raise Exception('Provide path to examples/zmqserver binary as last argument')

    ZmqClientTest.binary = sys.argv[-1]
    del sys.argv[-1]

    app = QCoreApplication()

    unittest.main()
