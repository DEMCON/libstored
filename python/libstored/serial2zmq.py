# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import errno
import logging
import serial
import sys
import time
import zmq

from .stream2zmq import Stream2Zmq

class Serial2Zmq(Stream2Zmq):
    """Serial port frame grabber to ZmqServer bridge."""

    def __init__(self, stack='ascii,term', zmqlisten='*', zmqport=Stream2Zmq.default_port, drop_s=1, printStdout=True, **kwargs):
        super().__init__(stack, listen=zmqlisten, port=zmqport, printStdout=printStdout)
        self.logger = logging.getLogger(__name__)
        self.logger.debug('Opening serial port %s', kwargs['port'])
        self.serial = None
        self.serial = self._serialOpen(**kwargs)
        self.serial_socket = self.registerStream(self.serial)
        self.stdin_socket = self.registerStream(sys.stdin)
        self.rep_queue = []

        self._drop = None
        self._bufferStdin = b''
        if not drop_s is None:
            self._drop = time.time() + drop_s

    def _serialOpen(self, timeout_s=60, **kwargs):
        last_e = TimeoutError()
        for i in range(0, timeout_s):
            try:
                return serial.Serial(**kwargs)
            except serial.SerialException as e:
                # For unclear reasons, Windows sometimes reports the port as
                # being in use.  That issue seems to clear automatically after
                # a while.
                if 'PermissionError' not in str(e):
                    raise

                last_e = e
                self.logger.warning(e)
                time.sleep(1)
        raise last_e

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
            self.logger.debug('Closed serial port')

    def drop(self, data):
        # First drop_s seconds of data is dropped to get rid of
        # garbage while reset/boot/connecting the UART.
        self.logger.debug('dropped %s', data)
