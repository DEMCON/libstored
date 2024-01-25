#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import libstored
import logging
import os
import subprocess
import sys
import unittest

class HeatshrinkDecoderTest(unittest.TestCase):
    encoder = None
    logger = logging.getLogger(__name__)
    _decoder = None

    def encode(self, x):
        p = subprocess.run(self.encoder, input=x, capture_output=True, text=False, check=True)
#        self.logger.debug('\n' + p.stderr.decode())
        return p.stdout

    def do_endec(self, x):
        if self._decoder is None:
            self._decoder = libstored.heatshrink.HeatshrinkDecoder()

        c = self.encode(x)
        d = self._decoder.finish(c)
#        self.logger.info(f'Compressed {x} into {c}, and decompressed to {d}')
        self.assertEqual(d, x)

    def test_empty(self):
        self.do_endec(b'')

    def test_simple(self):
        self.do_endec(b'a')
        self.do_endec(b'aa')
        self.do_endec(b'aaa')
        self.do_endec(b'aaaa')
        self.do_endec(b'aaaaa')

    def test_random(self):
        # Test all lengths
        for l in range(1, 1024):
            # Random pattern with this length
            self.do_endec(bytearray(os.urandom(l)))

if __name__ == '__main__':
    HeatshrinkDecoderTest.encoder = sys.argv[-1]
    del sys.argv[-1]

    logging.basicConfig(level=logging.INFO)
    suite = unittest.TestLoader().loadTestsFromTestCase(HeatshrinkDecoderTest)
    unittest.TextTestRunner(verbosity=2).run(suite)
