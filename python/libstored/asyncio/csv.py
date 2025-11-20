# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import aiofiles
import concurrent.futures
import csv
import io
import logging
import os
import time
import typing

from typing import overload

from . import worker as laio_worker
from . import zmq as laio_zmq

@overload
def generate_filename(filename : str | None=None, *,
                      add_timestamp : bool=False, ext : str='.csv', now : time.struct_time | float | None=None,
                      unique : bool=False) -> str: ...
@overload
def generate_filename(*, base : str,
                      add_timestamp : bool=False, ext : str='.csv', now : time.struct_time | float | None=None,
                      unique : bool=False) -> str: ...
@overload
def generate_filename(filename : list[str] | str | None=None, *, base : list[str] | str | None=None,
                      add_timestamp : bool=False, ext : list[str] | str='.csv', now : time.struct_time | float | None=None,
                      unique : bool=False) -> str | list[str]: ...

def generate_filename(filename : list[str] | str | None=None, *, base : list[str] | str | None=None,
                      add_timestamp : bool=False, ext : list[str] | str='.csv', now : time.struct_time | float | None=None,
                      unique : bool=False) -> str | list[str]:

    if filename is None and base is None:
        raise ValueError('Specify filename and/or base')

    return_list = False

    if not isinstance(filename, list):
        if filename is None:
            filename = []
        else:
            filename = [filename]
    else:
        return_list = True

    if not isinstance(base, list):
        if base is None:
            base = []
        else:
            base = [base]
    else:
        return_list = True

    if not isinstance(ext, list):
        if ext is None:
            ext = []
        else:
            ext = [ext]
    else:
        return_list = True

    names = []

    # Split given filename(s) into base/ext.
    for f in filename:
        names.append(os.path.splitext(f))

    # Append the combination of bases/exts to names.
    for e in ext:
        for b in base:
            names.append((b, e))

    assert return_list or len(names) > 0

    if now is None:
        now = time.localtime()
    elif not isinstance(now, time.struct_time):
        now = time.localtime(now)

    # Append timestamps to the generated bases.
    if add_timestamp:
        for i in range(0, len(names)):
            names[i] = (names[i][0] + '_%Y%m%dT%H%M%S%z', names[i][1])

    # Time-format collected bases.
    for i in range(0, len(names)):
        names[i] = (time.strftime(names[i][0], now), time.strftime(names[i][1], now))

    # Check existing files.
    if unique:
        for i in range(0, len(names)):
            suffix_nr = 1
            suffix = ''
            n = names[i][0] + names[i][1]
            while True:
                # Check if the file already exists.
                exists = os.path.lexists(n)

                # Check if we would create this file.
                if not exists:
                    for j in range(0, i):
                        if os.path.realpath(n) == os.path.realpath(names[j][0] + names[j][1]):
                            # The name equals another to-be-created file.
                            exists = True
                            break

                if not exists:
                    # Got it
                    names[i] = (names[i][0] + suffix, names[i][1])
                    break

                # Pick another suffix and retry.
                suffix_nr += 1
                suffix = f'_{suffix_nr}'
                n = names[i][0] + suffix + names[i][1]

    # Combine bases/exts.
    for i in range(0, len(names)):
        names[i] = names[i][0] + names[i][1]

    if return_list:
        return names
    elif len(names) == 1:
        return names[0]
    else:
        return names

class CsvExport(laio_worker.Work):
    '''
    asyncio csv exporter via AsyncioWorker.
    '''

    def __init__(self, filename : str = 'out.csv', *,
                 auto_write : float | None=None, write_on_change : bool=True, auto_flush : float | None=1.0,
                 worker : laio_worker.AsyncioWorker | None=None, logger : logging.Logger | None=None,
                 **fmtargs):
        super().__init__(worker=worker, logger=logger)

        self._out = io.StringIO()
        self._writer = csv.writer(self._out, **fmtargs)
        self._filename = filename
        self._file_context = None
        self._file = None
        self._objs : dict[laio_zmq.Object, typing.Any] = {}
        self._t_last : float = 0
        self._t_update : float = 0
        self._coalesced : tuple[float, list[typing.Any]] = (0.0, [])
        self._write_sem = asyncio.BoundedSemaphore(1)

        self._auto_write_task : typing.Optional[asyncio.Task[None]] = None
        self._auto_write : float | None = auto_write

        self._write_on_change_task : typing.Optional[asyncio.Task[None]] = None
        self._write_on_change = write_on_change and auto_write is None

        self._auto_flush_task : typing.Optional[asyncio.Task[None]] = None
        self._auto_flush = auto_flush

    @property
    def opened(self) -> bool:
        return self._file is not None

    @property
    def file(self): # -> some aiofile type
        if not self.opened:
            raise RuntimeError('File not opened')
        assert self._file is not None
        return self._file

    @overload
    async def open(self) -> None: ...
    @overload
    def open(self, *, block : typing.Literal[False]) -> asyncio.Future[bool]: ...
    @overload
    def open(self, *, sync : typing.Literal[True]) -> bool: ...
    @overload
    def open(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[bool]: ...

    @laio_worker.Work.run_sync
    @laio_worker.Work.locked
    async def open(self):
        await self._open()

    async def _open(self):
        assert self.lock.has_lock()

        if self.opened:
            raise RuntimeError('File already opened')

        if self._filename == '-':
            self.logger.info('using stdout for CSV export')
            self._file_context = None
            self._file = aiofiles.stdout
        else:
            self.logger.info('using %s for CSV export', self._filename)
            self._file_context = aiofiles.open(self._filename, 'w', newline='', encoding='utf-8')
            self._file = await self._file_context.__aenter__()

        self._t_last = 0
        self._t_update = 0
        self._need_restart = True
        self._coalesced : tuple[float, list[typing.Any]] = (0.0, [])
        self._queue = []

        self._update_auto_write(self._auto_write)
        self._update_write_on_change(self._write_on_change)
        self._update_auto_flush(self._auto_flush)

    async def __aenter__(self):
        await self.open()
        return self

    @overload
    async def close(self) -> None: ...
    @overload
    def close(self, *, block : typing.Literal[False]) -> asyncio.Future[bool]: ...
    @overload
    def close(self, *, sync : typing.Literal[True]) -> bool: ...
    @overload
    def close(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[bool]: ...

    @laio_worker.Work.run_sync
    @laio_worker.Work.locked
    async def close(self):
        await self._close()

    async def _close(self):
        assert self.lock.has_lock()

        if not self.opened:
            return

        assert self._file is not None

        try:
            self._update_auto_write(None)
            self._update_write_on_change(False)
            self._update_auto_flush(None)
        except Exception as e:
            self.logger.debug('ignore exception: %s', e)

        try:
            data = self._out.getvalue()
            self._out.truncate(0)
            self._out.seek(0)

            if data:
                await self._file.write(data)
        except Exception as e:
            self.logger.debug('ignore exception: %s', e)

        self._file = None

        if self._file_context is not None:
            try:
                await self._file_context.__aexit__(None, None, None)
            except Exception as e:
                self.logger.debug('ignore exception: %s', e)
            finally:
                self._file_context = None
            self.logger.debug('closed %s', self._filename)

    async def __aexit__(self, exc_type, exc_value, traceback):
        await self.close()

    def __del__(self):
        if self.opened:
            self.close(sync=True)

    @laio_worker.Work.locked
    async def _restart(self):
        self.logger.debug('restart')
        self._out.truncate(0)
        self._out.seek(0)

        header = ['t (s)']
        for obj in self._objs.keys():
            header.append(obj.name)
        if len(header) > 1:
            self._writer.writerow(header)

        self._queue = []
        self._need_restart = False

    @overload
    async def write(self, t : float | None=None, *, flush : bool=False) -> None: ...
    @overload
    def write(self, t : float | None=None, *, flush : bool=False, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def write(self, t : float | None=None, *, flush : bool=False, sync : typing.Literal[True]) -> None: ...
    @overload
    def write(self, t : float | None=None, *, flush : bool=False, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @laio_worker.Work.run_sync
    async def write(self, t : float | None=None, *, flush : bool=False) -> None:
        self._collect(t)
        await self._write()

        if flush:
            await self._flush()

    def _collect(self, t : float | None=None) -> None:
        if t is None:
            t = self._t_update

        row = [t]
        for obj, val in self._objs.items():
            row.append(val)

        t_, row_ = self._coalesced
        if t_ < t and row_:
            self._queue.append(self._coalesced)
            try:
                self._write_sem.release()
            except ValueError:
                pass

        self._coalesced = (t, row)

    async def _write(self) -> None:
        assert not self.lock.has_lock()

        if not self.opened:
            raise RuntimeError('File not opened')

        if self._need_restart:
            await self._restart()

        async with self.lock:
            queue = self._queue
            self._queue = []

            for t, row in queue:
                if t > self._t_last:
                    self.logger.debug('write t=%.6f', t)
                    self._t_last = t
                    self._writer.writerow(row)

    @overload
    async def flush(self) -> None: ...
    @overload
    def flush(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def flush(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def flush(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @laio_worker.Work.run_sync
    async def flush(self) -> None:
        await self._flush()

    async def _flush(self) -> None:
        assert not self.lock.has_lock()

        if not self.opened:
            raise RuntimeError('File not opened')

        async with self.lock:
            data = self._out.getvalue()
            self._out.truncate(0)
            self._out.seek(0)

        if data:
            self.logger.debug('flush')
            await self.file.write(data)
            await self.file.flush()
            self.logger.debug('flushed')

    @overload
    async def add(self, obj : laio_zmq.Object) -> None: ...
    @overload
    def add(self, obj : laio_zmq.Object, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def add(self, obj : laio_zmq.Object, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def add(self, obj : laio_zmq.Object, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @laio_worker.Work.run_sync
    @laio_worker.Work.locked
    async def add(self, obj : laio_zmq.Object) -> None:
        if obj not in self._objs:
            self._objs[obj] = await obj.read()
            obj.register(lambda v, o=obj: self._on_object_update(o, v), self)
            self._need_restart = True

    @overload
    async def remove(self, obj : laio_zmq.Object) -> None: ...
    @overload
    def remove(self, obj : laio_zmq.Object, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def remove(self, obj : laio_zmq.Object, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def remove(self, obj : laio_zmq.Object, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @laio_worker.Work.run_sync
    @laio_worker.Work.locked
    async def remove(self, obj : laio_zmq.Object) -> None:
        if obj in self._objs:
            obj.unregister(self)
            del self._objs[obj]
            self._need_restart = True

    @overload
    async def clear(self) -> None: ...
    @overload
    def clear(self, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def clear(self, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def clear(self, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @laio_worker.Work.run_sync
    @laio_worker.Work.locked
    async def clear(self) -> None:
        for o in self._objs.keys():
            o.unregister(self)

        self._objs.clear()
        self._need_restart = True

    def _on_object_update(self, obj : laio_zmq.Object, value : typing.Any) -> None:
        if obj in self._objs:
            self._objs[obj] = value
            obj_t = obj.t.value
            self._t_update = max(self._t_update, obj_t if obj_t is not None else 0.0)
            if self._write_on_change and self.opened:
                self._collect()

    @overload
    def auto_write(self, interval_s : float | None) -> None: ...
    @overload
    def auto_write(self, interval_s : float | None, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def auto_write(self, interval_s : float | None, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def auto_write(self, interval_s : float | None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @laio_worker.Work.thread_safe_async
    def auto_write(self, interval_s : float | None) -> None:
        '''
        Enable/disable automatic writing every interval_s seconds.
        If interval_s is None, automatic writing is disabled.
        '''
        self._auto_write = interval_s

        if self.opened or interval_s is None:
            self._update_auto_write(self._auto_write)

    def _update_auto_write(self, interval_s : float | None) -> None:
        if self._auto_write_task is not None:
            self._auto_write_task.cancel()
            self._auto_write_task = None

        if interval_s is not None:
            if not self.opened:
                raise RuntimeError('File not opened')

            async def auto_write_task():
                try:
                    while True:
                        await asyncio.sleep(interval_s)
                        self._collect()
                        await self._write()
                except asyncio.CancelledError:
                    pass
                except:
                    self.logger.exception(f'Auto write task error')
                    raise

            self._auto_write_task = asyncio.create_task(auto_write_task(), name=f'{self.__class__.__name__} auto write')

    @overload
    def write_on_change(self, enable : bool) -> None: ...
    @overload
    def write_on_change(self, enable : bool, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def write_on_change(self, enable : bool, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def write_on_change(self, enable : bool, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @laio_worker.Work.thread_safe_async
    def write_on_change(self, enable : bool) -> None:
        '''
        Enable/disable writing on object value change.
        If enabled, a write is performed whenever an object's value is updated.
        '''
        self._write_on_change = enable

        if self.opened or not enable:
            self._update_write_on_change(enable)

    def _update_write_on_change(self, enable : bool) -> None:
        if self._write_on_change_task is not None:
            self._write_on_change_task.cancel()
            self._write_on_change_task = None

        if enable:
            if not self.opened:
                raise RuntimeError('File not opened')

            async def write_on_change_task():
                try:
                    while True:
                        await self._write_sem.acquire()
                        await self._write()
                except asyncio.CancelledError:
                    pass
                except:
                    self.logger.exception(f'Write on change task error')
                    raise

            self._write_on_change_task = asyncio.create_task(write_on_change_task(), name=f'{self.__class__.__name__} write on change')

    @overload
    def auto_flush(self, interval_s : float | None) -> None: ...
    @overload
    def auto_flush(self, interval_s : float | None, *, block : typing.Literal[False]) -> asyncio.Future[None]: ...
    @overload
    def auto_flush(self, interval_s : float | None, *, sync : typing.Literal[True]) -> None: ...
    @overload
    def auto_flush(self, interval_s : float | None, *, block : typing.Literal[False], sync : typing.Literal[True]) -> concurrent.futures.Future[None]: ...

    @laio_worker.Work.thread_safe_async
    def auto_flush(self, interval_s : float | None) -> None:
        '''
        Enable/disable automatic flushing every interval_s seconds.
        If interval_s is None, automatic flushing is disabled.
        '''
        self._auto_flush = interval_s

        if self.opened or interval_s is None:
            self._update_auto_flush(self._auto_flush)

    def _update_auto_flush(self, interval_s : float | None) -> None:
        if self._auto_flush_task is not None:
            self._auto_flush_task.cancel()
            self._auto_flush_task = None

        if interval_s is not None:
            if not self.opened:
                raise RuntimeError('File not opened')

            async def auto_flush_task():
                try:
                    while True:
                        await asyncio.sleep(interval_s)
                        await self._flush()
                except asyncio.CancelledError:
                    pass
                except:
                    self.logger.exception(f'Auto flush task error')
                    raise

            self._auto_flush_task = asyncio.create_task(auto_flush_task(), name=f'{self.__class__.__name__} auto flush')
