#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import locale
import logging
import os
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../python')))
import libstored.asyncio

class ZmqClientTest(unittest.TestCase):

    binary : str | None = None
    logger = logging.getLogger(__name__)

    @classmethod
    def setUpClass(cls):
        assert cls.binary is not None
        cls.logger.info(f'Starting {cls.binary}...')
        cls.process = subprocess.Popen(
            [cls.binary], bufsize=0,
            stdin=subprocess.DEVNULL, stdout=sys.stdout, stderr=sys.stdout)

        cls.logger.info(f'Connecting...')
        cls.c = libstored.asyncio.ZmqClient()
        cls.c.connect()

        cls.logger.info(f'Connected')

    @classmethod
    def tearDownClass(cls):
        cls.c.close()
        cls.logger.info(f'Stopping {cls.binary}...')
        cls.process.terminate()

    def test_dummy(self):
        self.assertTrue(True)

    def test_lib_version(self):
        print(f'Library version {libstored.__version__}')

    def test_reconnect(self):
        self.c.disconnect()
        self.c.connect()

    def test_with(self):
        with libstored.asyncio.ZmqClient(multi=True) as c:
            self.assertEqual(c.identification(), 'zmqserver')

        @libstored.asyncio.run_sync
        async def f(self):
            async with libstored.asyncio.ZmqClient(multi=True) as c:
                self.assertEqual(await c.identification(), 'zmqserver')
        f(self)

    def test_identification(self):
        self.assertEqual(self.c.identification(), 'zmqserver')

        @libstored.asyncio.run_sync
        async def f(self):
            self.assertEqual(await self.c.identification(), 'zmqserver')
        f(self)

    def test_version(self):
        self.assertEqual(self.c.version().split(' ')[0], '2')

        @libstored.asyncio.run_sync
        async def f(self):
            self.assertNotEqual(await self.c.version(), '')
        f(self)

    def test_capabilities(self):
        c = self.c.capabilities()
        self.assertTrue('?' in c)
        self.assertTrue('r' in c)
        self.assertTrue('w' in c)
        self.assertTrue('l' in c)

        @libstored.asyncio.run_sync
        async def f(self):
            self.assertTrue('l' in await self.c.capabilities())
        f(self)

    def test_echo(self):
        self.assertEqual(self.c.echo('Tales as old as time...'), 'Tales as old as time...')

        @libstored.asyncio.run_sync
        async def f(self):
            self.assertEqual(await self.c.echo('True as it can be'), 'True as it can be')
        f(self)

    def test_list(self):
        self.assertTrue(self.c['/an int8'] is not None)
        self.assertTrue(self.c['/comp/an'] is not None)

        self.assertEqual(self.c['/an int8'].name, '/an int8')
        self.assertEqual(self.c['/an int8'].type_name, 'int8')
        self.assertEqual(self.c['/an int8'].value_type, int)

        @libstored.asyncio.run_sync
        async def f(self):
            self.assertNotEqual(self.c['/an int8'], None)
            self.assertNotEqual(self.c['/comp/an'], None)
        f(self)

    def test_read_write(self):
        an_int8 = self.c['/an int8']
        v = an_int8.read()
        an_int8.write(v + 1)
        self.assertEqual(self.c.an_int8.read(), v + 1) # type: ignore
        an_int8.write(v)
        self.assertEqual(an_int8.read(), v)

        @libstored.asyncio.run_sync
        async def f(self):
            an_int8 = self.c['/an int8']
            v = await an_int8.read()
            await an_int8.write(v + 1)
            self.assertEqual(await self.c.an_int8.read(), v + 1)
            await an_int8.write(v)
            self.assertEqual(await an_int8.read(), v)

            o = self.c['/an int16']
            await o.write(32)
            self.assertEqual(await o.read(), 32)
            self.assertEqual(o.get(), 32)
            self.assertTrue('default' in o.formats())
            o.format = 'default'
            self.assertEqual(o.value_str.value, '32')
            o.format = 'hex'
            self.assertEqual(o.value_str.value, '0x20')
            o.format = 'bin'
            self.assertEqual(o.value_str.value, '0b100000')

            l = locale.getlocale(locale.LC_NUMERIC)

            locale.setlocale(locale.LC_NUMERIC, 'C')
            o = self.c['/a float']
            o.value_str.value = '3.14'
            self.assertEqual(o.value_str.value, '3.14')
            self.assertEqual(o.value, 3.14)
            o.value_str.value = '1e2'
            self.assertEqual(o.value_str.value, '100')
            self.assertEqual(o.value, 100.0)
            o.value_str.value = '10000000'
            self.assertEqual(o.value_str.value, '1e+07')
            self.assertEqual(o.value, 10000000.0)
            o.value_str.value = 'asdf'
            self.assertEqual(o.value_str.value, '1e+07')
            self.assertEqual(o.value, 10000000.0)
            o.value_str.value = '0x21'
            self.assertEqual(o.value_str.value, '33')
            self.assertEqual(o.value, 33.0)
            o.value_str.value = '0b100010'
            self.assertEqual(o.value_str.value, '34')
            self.assertEqual(o.value, 34.0)

            try:
                locale.setlocale(locale.LC_NUMERIC, 'nl_NL.utf8')
                o.value_str.value = '3,14'
                self.assertEqual(o.value_str.value, '3,14')
                self.assertEqual(o.value, 3.14)
                o.value_str.value = '1.000,5'
                self.assertEqual(o.value_str.value, '1.000,5')
                self.assertEqual(o.value, 1000.5)
                o.value_str.value = '2.0300,5'
                self.assertEqual(o.value_str.value, '20.300,5')
                self.assertEqual(o.value, 20300.5)
            except locale.Error:
                self.logger.warning('Locale nl_NL.utf8 not available, skipping locale-specific tests')

            locale.setlocale(locale.LC_NUMERIC, l)
        f(self)

    def test_events(self):
        an_int8 = self.c['/an int8']
        v = an_int8.read()

        events = []
        def callback(v):
            events.append(v)
        an_int8.register(callback)

        v = an_int8.read()
        an_int8.write(v + 1)
        an_int8.write(v + 2)
        an_int8.write(v + 3)
        an_int8.write(v)
        an_int8.write(v)
        self.assertEqual(events, [v + 1, v + 2, v + 3, v])

        @libstored.asyncio.run_sync
        async def f(self):
            an_int8 = self.c['/an int8']
            events = []
            def callback(v):
                events.append(v)
            an_int8.register(callback)

            v = await an_int8.read()
            await an_int8.write(v + 1)
            await an_int8.write(v + 2)
            await an_int8.write(v + 3)
            await an_int8.write(v)
            await an_int8.write(v)
            self.assertEqual(events, [v + 1, v + 2, v + 3, v])
        f(self)

def main():
    if len(sys.argv) == 0 or not 'zmqserver' in sys.argv[-1]:
        raise Exception('Provide path to examples/zmqserver binary as last argument')

    ZmqClientTest.binary = sys.argv[-1]
    del sys.argv[-1]

    logging.basicConfig(level=logging.DEBUG)
    unittest.main()

if __name__ == '__main__':
    main()
