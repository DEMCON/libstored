# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import sys
import serial
import zmq
import logging
import time

from .stream2zmq import Stream2Zmq

class Serial2Zmq(Stream2Zmq):
    """Serial port frame grabber to ZmqServer bridge."""

    def __init__(self, stack='ascii,term', zmqlisten='*', zmqport=Stream2Zmq.default_port, drop_s=1, printStdout=True, **kwargs):
        super().__init__(stack, listen=zmqlisten, port=zmqport, printStdout=printStdout)
        self.serial = None
        self.serial = serial.Serial(**kwargs)
        self.serial_socket = self.registerStream(self.serial)
        self.stdin_socket = self.registerStream(sys.stdin)
        self.rep_queue = []
        self.logger = logging.getLogger(__name__)

        self._drop = None
        self._bufferStdin = b''
        if not drop_s is None:
            self._drop = time.time() + drop_s

    def poll(self, timeout_s = None):
        dropping = not self._drop is None

        events = super().poll(timeout_s)

        if events.get(self.stdin_socket, 0) & zmq.POLLIN:
            self.recvAll(self.stdin_socket, self.sendToApp)
        if events.get(self.serial_socket, 0) & zmq.POLLIN:
            self.recvAll(self.serial_socket, self.decode if not dropping else self.drop)

        if dropping and time.time() > self._drop:
            self._drop = None
            self._sendToApp(self._bufferStdin)
            self._bufferStdin = None

    def sendToApp(self, data):
        if self._drop is None:
            self._sendToApp(data)
        else:
            self.logger.debug('queue %s', data)
            self._bufferStdin += data

    def _sendToApp(self, data):
        if len(data) > 0:
            cnt = self.serial.write(data)
            assert cnt == len(data)
            self.serial.flush()
            self.logger.debug('sent %s', data)

    def encode(self, data):
        if len(data) > 0:
            self.sendToApp(data)
            super().encode(data)

    def close(self):
        super().close()
        if self.serial is not None:
            self.serial.close()
            self.serial = None

    def drop(self, data):
        # First drop_s seconds of data is dropped to get rid of
        # garbage while reset/boot/connecting the UART.
        self.logger.debug('dropped %s', data)

