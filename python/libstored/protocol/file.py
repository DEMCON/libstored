# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import logging
import os

if os.name == 'posix':
    import posix
    import select

from . import protocol as lprot
from . import util as lutil

class FileLayer(lprot.ProtocolLayer):
    '''
    A protocol layer that reads/writes a file for I/O.
    '''

    name = 'file'

    def __init__(self, file : str | tuple[str, str], *args, **kwargs):
        super().__init__(*args, **kwargs)

        read = self._posix_read if os.name == 'posix' else self._read
        self._reader = lutil.Reader(read, thread_name=f'{self.__class__.__name__} reader')
        self._writer = lutil.Writer(self._write, thread_name=f'{self.__class__.__name__} writer')
        self._task : asyncio.Task | None = asyncio.create_task(self._reader_task(), name=f'{self.__class__.__name__} reader')

        if isinstance(file, str):
            file = (file, file)

        file_in, file_out = file

        if os.name == 'posix':
            if not os.path.exists(file_in):
                os.mkfifo(file_in)
            if not os.path.exists(file_out):
                os.mkfifo(file_out)

            self._file_in = os.fdopen(posix.open(file_in, posix.O_RDWR), 'rb')
            self._file_out = os.fdopen(posix.open(file_out, posix.O_RDWR), 'wb')
        else:
            self._file_in = open(file_in, 'rb')
            self._file_out = open(file_out, 'wb')

    def _posix_read(self) -> bytes:
        f = self._file_in
        if f is None:
            return b''

        while self._reader.running:
            res = select.select([f.fileno()], [], [], 1)

            if res[0]:
                # Readable
                return f.read1(4096)

        return b''

    def _read(self) -> bytes:
        f = self._file_in
        if f is None:
            return b''
        return f.read1(4096)

    def _write(self, data : bytes) -> None:
        f = self._file_out
        if f is None:
            return

        self.logger.debug('write %s', data)

        f.write(data)
        f.flush()

    async def _reader_task(self) -> None:
        try:
            await self._reader.start()

            while self._reader.running:
                x = await self._reader.read()
                self.logger.debug('read %s', x)
                await self.decode(x)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            if self._reader.running:
                await self.async_except(e)
                raise

    async def close(self) -> None:
        await self._reader.stop()
        await self._writer.stop()

        if self._task is not None:
            self._task.cancel()
            try:
                await self._task
            except:
                pass
            self._task = None

        if self._file_in is not None:
            self._file_in.close()
            self._file_in = None

        if self._file_out is not None:
            self._file_out.close()
            self._file_out = None

        await super().close()

    async def encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if not self._writer.running:
            await self._writer.start()

        if self._writer.running:
            await self._writer.write(data)

        await super().encode(data)

lprot.register_layer_type(FileLayer)
