# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import aiofiles
import aiofiles.base
import asyncio
import logging

from . import protocol as lprot

class FileLayer(lprot.ProtocolLayer):
    '''
    A protocol layer that reads/writes a file for I/O.
    '''

    name = 'file'

    def __init__(self, file : str | tuple[str, str], *args, **kwargs):
        super().__init__(*args, **kwargs)

        self._file_in_context : aiofiles.base.AiofilesContextManager | None = None
        self._file_out_context : aiofiles.base.AiofilesContextManager | None = None
        self._file_out = None
        self._reader = asyncio.create_task(self._reader_task(), name=f'{self.__class__.__name__} reader')

        if isinstance(file, str):
            file = (file, file)

        self._file_in_context = aiofiles.open(file[0], 'rb')
        self._file_out_context = aiofiles.open(file[1], 'wb')

    async def _reader_task(self) -> None:
        try:
            fc = self._file_in_context
            assert fc is not None

            async with fc as f:
                while True:
                    x = f.read()
                    self.logger.debug('read %s', x)
                    await self.decode(x)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            await self.async_except(e)
            raise

    async def close(self) -> None:
        if self._reader is not None:
            self._reader.cancel()
            try:
                await self._reader
            except:
                pass
            self._reader = None

        self._file_in_context = None
        self._file_out = None

        if self._file_out_context is not None:
            await self._file_out_context.__aexit__(None, None, None)
            self._file_out_context = None

        await super().close()

    async def encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        if self._file_out is None:
            if self._file_out_context is not None:
                self._file_out = await self._file_out_context.__aenter__()

        if self._file_out is not None:
            if self.logger.getEffectiveLevel() <= logging.DEBUG:
                self.logger.debug(f'write {bytes(data)}')
            await self._file_out.write(data)
            await self._file_out.flush()

        await super().encode(data)

lprot.register_layer_type(FileLayer)
