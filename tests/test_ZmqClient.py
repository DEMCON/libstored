#!/usr/bin/env python3

# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2021  Jochem Rutgers
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
import logging
from PySide2.QtCore import QCoreApplication

class ZmqClientTest(unittest.TestCase):

    binary = None
    logger = logging.getLogger(__name__)

    @classmethod
    def setUpClass(cls):
        cls.logger.info(f'Starting {cls.binary}...')
        cls.process = subprocess.Popen(
            [cls.binary], bufsize=0,
            stdin=subprocess.DEVNULL, stdout=sys.stdout, stderr=sys.stdout)

        cls.logger.info(f'Connecting...')
        cls.c = ed2.ZmqClient()

        cls.logger.info(f'Connected')

    @classmethod
    def tearDownClass(cls):
        cls.c.close()
        cls.logger.info(f'Stopping {cls.binary}...')
        cls.process.terminate()

    def test_dummy(self):
        self.assertTrue(True)

    def test_ed2_version(self):
        print(f'Library version {ed2.__version__}')

    def test_identification(self):
        self.assertEqual(self.c.identification(), 'zmqserver')

    def test_version(self):
        self.assertEqual(self.c.version().split(' ')[0], '2')

    def test_capabilities(self):
        c = self.c.capabilities()
        self.assertTrue('?' in c)
        self.assertTrue('r' in c)
        self.assertTrue('w' in c)
        self.assertTrue('l' in c)

    def test_echo(self):
        self.assertEqual(self.c.echo('Tales as old as time...'), 'Tales as old as time...')

    def test_list(self):
        self.assertTrue(self.c['/an int8'] != None)
        self.assertTrue(self.c['/comp/an'] != None)

if __name__ == '__main__':
    if len(sys.argv) == 0 or not 'zmqserver' in sys.argv[-1]:
        raise Exception('Provide path to examples/zmqserver binary as last argument')

    ZmqClientTest.binary = sys.argv[-1]
    del sys.argv[-1]

    app = QCoreApplication()

    logging.basicConfig(level=logging.INFO)

    unittest.main()

