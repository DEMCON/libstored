# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import aiofiles
import asyncio

from . import protocol as lprot

class FileLayer(lprot.ProtocolLayer):
    '''
    A protocol layer that reads/writes a file for I/O.
    '''

    name = 'file'

    def __init__(self, file : str | tuple[str, str], *args, **kwargs):
        super().__init__(*args, **kwargs)

        self._file_in_context = None
        self._file_out_context = None
        self._file_in = None
        self._file_out = None

        if isinstance(file, str):
            file = (file, file)

        self._file_in_context = aiofiles.open(file[0], 'rb')
        self._file_out_context = aiofiles.open(file[1], 'wb')

    async def close(self) -> None:
        self._file_in = None
        self._file_out = None

        if self._file_in_context is not None:
            await self._file_in_context.__aexit__(None, None, None)
            self._file_in_context = None

        if self._file_out_context is not None:
            await self._file_out_context.__aexit__(None, None, None)
            self._file_out_context = None

        await super().close()

lprot.register_layer_type(FileLayer)
