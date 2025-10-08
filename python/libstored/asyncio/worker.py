# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import concurrent.futures
import functools
import time
import threading
import time
import logging
import typing

default_worker = None

class AsyncioWorker:
    '''
    A worker thread running an asyncio event loop.
    '''

    def __init__(self, daemon : bool=False, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.logger = logging.getLogger(__class__.__name__)
        self._loop = None
        self._started = False
        self._thread = threading.Thread(target=self._run, daemon=daemon, name='AsyncioWorker')
        self._thread.start()
        self.logger.debug("Waiting for event loop to start")
        while not self._started:
            time.sleep(0.1)
        self.logger.debug("Event loop started")

    @property
    def thread(self) -> threading.Thread | None:
        return self._thread

    @property
    def loop(self) -> asyncio.AbstractEventLoop | None:
        return self._loop

    def _run(self):
        self.logger.debug("Starting event loop")
        assert self._loop is None
        assert self._thread == threading.current_thread()
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop.create_task(self._flag_started())

        global default_worker
        if default_worker is None:
            default_worker = self

        try:
            self._loop.run_forever()
            tasks = asyncio.all_tasks(self._loop)
            for t in [t for t in tasks if not (t.done() or t.cancelled())]:
                # give canceled tasks the last chance to run
                self._loop.run_until_complete(t)
        finally:
            self._loop.close()
            self._loop = None

            if default_worker == self:
                default_worker = None

        self.logger.debug("Event loop stopped")

    async def _flag_started(self):
        self._started = True

    def make_default(self):
        '''
        Make this worker the default worker.

        Thread-safe.
        '''

        if not self.is_running():
            raise RuntimeError("Event loop is not running")

        global default_worker
        default_worker = self

    def cancel(self):
        '''
        Cancel all tasks and stop the event loop and wait for the thread to exit.

        Thread-safe.
        '''

        loop = self._loop
        if loop is None:
            return

        self.logger.debug("Cancelling event loop")
        loop.call_soon_threadsafe(self._cancel)
        del loop
        self.stop()

    def _cancel(self):
        assert self._loop is not None
        for task in asyncio.all_tasks(self._loop):
            task.cancel()

    def stop(self, timeout_s : float | None=None):
        '''
        Request to stop the event loop and wait for the thread to exit.

        Thread-safe.
        '''

        loop = self._loop
        if loop is None:
            return

        self.logger.debug("Stopping event loop")
        loop.call_soon_threadsafe(loop.stop)
        del loop

        assert self._thread is not None
        self._thread.join(timeout_s)
        if self._thread.is_alive():
            raise TimeoutError()

        self._thread = None

        assert default_worker is not self

    def __del__(self):
        self.stop()

    def is_running(self) -> bool:
        '''
        Return True if the event loop is running.

        Thread-safe.
        '''

        return self._loop is not None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.logger.debug("exit")
        self.stop()

    def execute(self, f : typing.Callable | typing.Coroutine, *args, **kwargs) -> concurrent.futures.Future | asyncio.Future:
        '''
        Schedule a coroutine or a callable to be executed in the event loop.

        Thread-safe and safe to call from within the event loop.
        '''

        if not self.is_running():
            raise RuntimeError("Event loop is not running")

        if asyncio.coroutines.iscoroutine(f):
            if len(args) > 0 or len(kwargs) > 0:
                raise TypeError("When passing a coroutine function, no additional arguments are supported")
            coro = f
        elif asyncio.iscoroutinefunction(f):
            coro = f(*args, **kwargs)
        elif callable(f):
            coro = self._execute(f, *args, **kwargs)
        else:
            raise TypeError("First argument must be a coroutine function or a callable returning a coroutine")

        self.logger.debug("Scheduling coroutine")

        assert self._loop is not None

        if threading.current_thread() == self._thread:
            # Directly create coro in the same worker context and return future.
            return asyncio.ensure_future(self._log(coro), loop=self._loop)
        else:
            # Create coro in the worker thread context, and return async future.
            return asyncio.run_coroutine_threadsafe(self._log(coro), self._loop)

    async def _log(self, coro : typing.Coroutine) -> typing.Any:
        try:
            return await coro
        except asyncio.CancelledError:
            raise
        except KeyboardInterrupt:
            raise
        except Exception as e:
            self.logger.debug('Exception in %s', asyncio.current_task(self._loop), exc_info=True)
            raise

    async def _execute(self, f, *args, **kwargs) -> typing.Any:
        return f(*args, **kwargs)

class Work:
    def __init__(self, worker : AsyncioWorker | None=None, logger : logging.Logger | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.logger = logger or logging.getLogger(self.__class__.__name__)

        if worker is None:
            global default_worker
            worker = default_worker
            if worker is None or not worker.is_running():
                worker = AsyncioWorker(daemon=True)

        self._worker = worker

    @property
    def worker(self) -> AsyncioWorker:
        return self._worker

def run_async(f : typing.Callable) -> typing.Callable:
    '''
    Decorator to start a function asynchronously in the default worker, and
    return the future.
    '''

    @functools.wraps(f)
    def wrapper(self, *args, **kwargs):
        assert asyncio.iscoroutinefunction(f)

        loop = None
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            pass

        if loop is not None:
            # We are in an event loop, just start the coro.
            return f(self, *args, **kwargs)
        else:
            # We are not in an event loop, run the coro in the (default) worker.
            w = None
            if isinstance(self, Work):
                self.logger.debug("Running %s in worker %s", f.__name__, str(self.worker))
                w = self.worker
            else:
                if hasattr(self, 'logger'):
                    self.logger.debug("Running %s in default worker", f.__name__)
                global default_worker
                w = default_worker

            if w is None or not w.is_running():
                self.logger.debug('No worker running, creating new one')
                w = AsyncioWorker(daemon=True)

            return w.execute(f(self, *args, **kwargs))

    return wrapper

def run_sync(f : typing.Callable) -> typing.Callable:
    '''
    Decorator to run an async function synchronously.

    If called from within an event loop, the coroutine is directly started,
    returning an awaitable.  If called from outside an event loop, the coroutine
    is executed in the default worker (creating one if needed).
    '''

    run_async_f = run_async(f)

    @functools.wraps(f)
    def wrapper(*args, **kwargs):
        x = run_async_f(*args, **kwargs)
        if asyncio.coroutines.iscoroutine(x):
            return x
        else:
            return x.result()

    return wrapper
