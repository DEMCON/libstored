# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import asyncio
import atexit
import logging
import queue
import sys
import threading
import typing

try:
    import fcntl, os
    def set_blocking(stdout):
        fileno = stdout.fileno()
        fl = fcntl.fcntl(fileno, fcntl.F_GETFL)
        fcntl.fcntl(fileno, fcntl.F_SETFL, fl & ~os.O_NONBLOCK)
except:
    # Not supported
    def set_blocking(stdout):
        pass



class InfiniteStdoutBuffer:
    '''
    A class that provides a non-blocking infinite buffer wrapper for stdout.
    '''

    def __init__(self, stdout : typing.TextIO | None=sys.__stdout__, cleanup : typing.Callable[[], None] | None= None):
        self.stdout = stdout
        self._queue : queue.Queue[str] | None = queue.Queue()
        self._closed : bool = False
        self._cleanup : typing.Callable[[], None] | None = cleanup
        self._thread : threading.Thread | None = threading.Thread(target=self._worker, daemon=True, name='InfiniteStdoutBufferWorker')
        atexit.register(self.close)
        self._thread.start()

    def write(self, data : str) -> None:
        if self._closed or self._queue is None:
            self._write(data)
        else:
            self._queue.put(data)

    def _write(self, data : str) -> None:
        # This may block.
        if self.stdout is not None:
            self.stdout.write(data)

    def flush(self) -> None:
        if self._queue is None:
            return
        self._queue.join()

    def close(self) -> None:
        if self._closed:
            return
        if self._thread is None:
            return

        self._closed = True

        # Force wakeup
        assert self._queue is not None
        self._queue.put('')
        self._thread.join()
        self._thread = None

        # Drain queue
        try:
            while True:
                self._write(self._queue.get_nowait())
                self._queue.task_done()
        except:
            pass

        self._queue = None
        if self.stdout is not None:
            self.stdout.flush()

        if self._cleanup is not None:
            self._cleanup()

    def __del__(self):
        self.close()

    def _worker(self):
        queue = self._queue
        if queue is None:
            return

        while not self._closed:
            x = queue.get()
            try:
                self._write(x)
            finally:
                queue.task_done()

def reset_stdout(old_stdout : typing.TextIO | None) -> None:
    sys.stdout = old_stdout

def set_infinite_stdout():
    if isinstance(sys.stdout, InfiniteStdoutBuffer):
        return
    sys.stdout.flush()
    old_stdout = sys.stdout
    set_blocking(old_stdout)
    sys.stdout = InfiniteStdoutBuffer(old_stdout, lambda: reset_stdout(old_stdout))


T = typing.TypeVar('T')

class Reader(typing.Generic[T]):
    '''
    Asyncio single-reader from a blocking source.
    '''

    def __init__(self, f : typing.Callable[[], T], thread_name : str | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._f : typing.Callable[[], T] = f
        self._thread : threading.Thread | None = None
        self._queue : list[T] = []
        self._lock : threading.Lock = threading.Lock()
        self._event : asyncio.Event = asyncio.Event()
        self._loop : asyncio.AbstractEventLoop = asyncio.get_event_loop()
        self._running : bool = False
        self._thread_name : str = thread_name if thread_name else self.__class__.__name__

    async def start(self) -> None:
        if self._thread is not None:
            return

        self._running = True
        self._event.clear()
        self._loop = asyncio.get_event_loop()

        self._thread = threading.Thread(target=self._thread_func, daemon=True,
                                        name=self._thread_name)
        self._thread.start()
        await self._event.wait()

    @property
    def running(self) -> bool:
        return self._running

    async def stop(self, join : bool=True) -> None:
        self._running = False

        if self._thread is not None and self._thread is not threading.current_thread():
            if join:
                if self._thread.is_alive():
                    assert self._loop is asyncio.get_event_loop()
                    await self._event.wait()

                self._thread.join()
            self._thread = None

    async def __aenter__(self) -> Reader[T]:
        await self.start()
        return self

    async def __aexit__(self, exc_type, exc, tb) -> None:
        await self.stop()

    def __del__(self):
        self._running = False
        if self._thread is not None:
            self._thread = None

    def _read(self) -> T:
        return self._f()

    def _thread_func(self) -> None:
        try:
            # Indicate that we are alive. start() is waiting for that.
            self._wakeup()

            while self._running:
                x = self._read()
                was_empty = False
                with self._lock:
                    if not self._queue:
                        was_empty = True
                    self._queue.append(x)

                if was_empty:
                    self._wakeup()
        except BaseException as e:
            logging.getLogger(self.__class__.__qualname__).exception(f'Reader thread error: {e}')
            self._running = False
        finally:
            # Signal a blocking read() or stop().
            self._wakeup()

    def _wakeup(self) -> None:
        try:
            if not self._loop.is_closed():
                asyncio.run_coroutine_threadsafe(self._wakeup_coro(), self._loop)
        except Exception as e:
            logging.getLogger(self.__class__.__qualname__).error(f'Error waking up reader: {e}')

    async def _wakeup_coro(self) -> None:
        self._event.set()

    async def read(self) -> T:
        assert self._loop is asyncio.get_event_loop()

        while True:
            if not self._running:
                raise RuntimeError('Reader not running')

            with self._lock:
                if self._queue:
                    x = self._queue.pop(0)
                    return x
                else:
                    self._event.clear()

            await self._event.wait()

    async def __await__(self) -> T:
        return await self.read()



class Writer(typing.Generic[T]):
    '''
    Asyncio single-writer to a blocking sink.
    '''

    def __init__(self, f : typing.Callable[[T], None], thread_name : str | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._f : typing.Callable[[T], None] = f
        self._thread : threading.Thread | None = None
        self._queue : queue.Queue[T | None] = queue.Queue()
        self._event : asyncio.Event = asyncio.Event()
        self._loop : asyncio.AbstractEventLoop = asyncio.get_event_loop()
        self._running : bool = False
        self._thread_name : str = thread_name if thread_name else self.__class__.__name__

    async def start(self) -> None:
        if self._thread is not None:
            return
        self._event.clear()
        self._loop = asyncio.get_event_loop()
        self._thread = threading.Thread(target=self._thread_func, daemon=True,
                                        name=self._thread_name)
        self._running = True
        self._thread.start()
        await self._event.wait()
        self._event.clear()

    @property
    def running(self) -> bool:
        return self._running

    async def stop(self) -> None:
        self._running = False
        if self._thread is not None:
            self._queue.put(None)
            if self._thread is threading.current_thread():
                return

            if self._thread.is_alive():
                assert self._loop is asyncio.get_event_loop()
                await self._event.wait()

            self._thread.join()
            self._thread = None

    async def __aenter__(self) -> Writer[T]:
        await self.start()
        return self

    async def __aexit__(self, exc_type, exc, tb) -> None:
        await self.stop()

    def __del__(self):
        self._running = False
        if self._thread is not None:
            self._queue.put(None)
            self._thread = None

    def _write(self, x : T) -> None:
        self._f(x)

    def _thread_func(self):
        try:
            self._wakeup()

            while self._running or not self._queue.empty():
                x = self._queue.get()
                if x is not None:
                    self._write(x)
                self._queue.task_done()
        except BaseException as e:
            logging.getLogger(self.__class__.__qualname__).error(f'Writer thread error: {e}')
            self._running = False
        finally:
            self._wakeup()

    def _wakeup(self) -> None:
        asyncio.run_coroutine_threadsafe(self._wakeup_coro(), self._loop)

    async def _wakeup_coro(self) -> None:
        self._event.set()

    async def write(self, x : T) -> None:
        if not self._running:
            raise RuntimeError('Writer not running')

        self._queue.put(x)



__all__ = [
    'set_infinite_stdout',
    'Reader',
    'Writer',
]
