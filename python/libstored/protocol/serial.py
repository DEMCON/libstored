# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import serial

from . import protocol as lprot
from . import util as lprot_util

class SerialLayer(lprot.ProtocolLayer):

    name = 'serial'

    def __init__(self, *, drop_s : float | None=1, **kwargs):
        super().__init__()
        self.logger.debug('Opening serial port %s', kwargs['port'])
        self._serial : serial.Serial | None = None
        self._writer : lprot_util.Writer | None = None

        self._encode_buffer = bytearray()
        self._open : bool = True

        self._serial_task : asyncio.Task | None = asyncio.create_task(self._serial_run(drop_s, kwargs), name=self.__class__.__name__)

    @property
    def open(self) -> bool:
        return self._open

    def _read(self):
        if not self._open:
            raise RuntimeError('Serial port closed')

        assert self._serial is not None
        data = self._serial.read(max(1, self._serial.in_waiting))
        self.logger.debug('received %s', data)
        return data

    def _write(self, data : bytes) -> None:
        if not self._open:
            raise RuntimeError('Serial port closed')

        assert self._serial is not None
        self.logger.debug('send %s', data)
        cnt = self._serial.write(data)
        assert cnt == len(data)
        self._serial.flush()

    async def _serial_run(self, drop_s : float | None, serial_args : dict) -> None:
        try:
            # Only access _serial within the current asyncio loop.
            self._serial = await self._serial_open(**serial_args)
            self.logger.debug('Serial port %s opened', serial_args['port'])

            if drop_s is not None and drop_s > 0:
                await asyncio.sleep(drop_s)

            if self._serial.in_waiting > 0:
                data = self._serial.read(self._serial.in_waiting)
                self.logger.debug('Flushing initial data: %s', data)

            # Only access self._serial read/write in reader/writer threads.
            async with lprot_util.Writer(self._write, thread_name=f'{self.__class__.__name__}-writer') as writer:
                async with lprot_util.Reader(self._read, thread_name=f'{self.__class__.__name__}-reader') as reader:
                    try:
                        if self._encode_buffer:
                            self.logger.debug('sending buffered %s', self._encode_buffer)
                            await self._encode(self._encode_buffer)
                            self._encode_buffer = bytearray()

                        self._writer = writer

                        while self._open:
                            data = await reader.read()
                            await self.decode(data)
                    except:
                        # Make sure reader/writer threads are not blocking before closing.
                        self._open = False
                        self._serial.cancel_read()
                        self._serial.cancel_write()
                        raise
        except asyncio.CancelledError:
            pass
        except Exception as e:
            await self.async_except(e)
            raise
        finally:
            self._open = False

            # Reader/writer are closed by their context managers.
            self._writer = None

            # It is now safe to access self._serial from within this coroutine.
            if self._serial is not None:
                self._serial.close()
                self._serial = None
                self.logger.debug('Closed serial port')

    async def _serial_open(self, timeout_s : int=60, **kwargs) -> serial.Serial:
        last_e = TimeoutError()
        for i in range(0, timeout_s):
            try:
                s = serial.Serial(**kwargs)
                try:
                    s.set_low_latency_mode(True)
                except:
                    pass
                return s
            except serial.SerialException as e:
                # For unclear reasons, Windows sometimes reports the port as
                # being in use.  That issue seems to clear automatically after
                # a while.
                if 'PermissionError' not in str(e):
                    raise

                last_e = e
                self.logger.info(e)
                await asyncio.sleep(1)
        raise last_e

    async def encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if not self.open:
            self.logger.debug('Serial port closed; dropping data %s', data)
        elif self._writer is None:
            self.logger.debug('buffering %s', data)
            self._encode_buffer += data
        else:
            await self._encode(data)

        await super().encode(data)

    async def _encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if len(data) == 0:
            return

        assert self._writer is not None
        await self._writer.write(data)
        await super().encode(data)

    async def close(self):
        self._open = False

        if self._serial_task is not None:
            if self._serial is not None:
                # Try to unblock reader/writer threads.
                self._serial.cancel_read()
                self._serial.cancel_write()

            self._serial_task.cancel()
            try:
                await self._serial_task
            except:
                pass
            self._serial_task = None

        await super().close()

lprot.register_layer_type(SerialLayer)
