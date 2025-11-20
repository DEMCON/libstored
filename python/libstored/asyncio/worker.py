# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import asyncio
import concurrent.futures
import functools
import time
import threading
import time
import logging
import sys
import typing

if sys.platform == 'win32' and sys.version_info < (3, 16):
    asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy()) # type: ignore

from .. import exceptions as lexc

default_worker : AsyncioWorker | None = None

workers : set[AsyncioWorker] = set()

# Do a graceful shutdown when the main thread exits.
def monitor_workers():
    threading.main_thread().join()
    for w in list(workers):
        try:
            w.cancel(0)
        except TimeoutError:
            pass

monitor = threading.Thread(target=monitor_workers, daemon=False, name='AsyncioWorkerMonitor')
monitor.start()



def current_worker() -> AsyncioWorker | None:
    t = threading.current_thread()
    for w in workers:
        if w.thread == t:
            return w
    return None



class AsyncioWorker:
    '''
    A worker thread running an asyncio event loop.
    '''

    def __init__(self, daemon : None | bool=False, name='AsyncioWorker', *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.logger = logging.getLogger(__class__.__name__)
        self._loop : asyncio.AbstractEventLoop | None = None
        self._started : bool = False
        self._thread : threading.Thread | None = threading.Thread(target=self._run, daemon=daemon, name=name)
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
        self._loop.create_task(self._flag_started(), name=f'{self.__class__.__name__} flag')

        global default_worker
        if default_worker is None:
            self.logger.debug("Making self the default worker")
            default_worker = self

        workers.add(self)

        try:
            self._loop.run_forever()
            while True:
                tasks = asyncio.all_tasks(self._loop)
                if not tasks:
                    break

                for t in [t for t in tasks if not (t.done() or t.cancelled())]:
                    # give canceled tasks the last chance to run
                    try:
                        self._loop.run_until_complete(t)
                    except asyncio.CancelledError:
                        pass
                    except lexc.Disconnected:
                        pass
                    except lexc.InvalidState:
                        pass
                    except:
                        self.logger.debug('Exception in %s during shutdown', t, exc_info=True)
        except Exception as e:
            self.logger.exception("Exception in event loop: %s", e, exc_info=True)
            raise
        finally:
            self._loop.close()
            self._loop = None

            if default_worker is self:
                default_worker = None

            workers.remove(self)

            self.logger.debug("Event loop stopped")

    async def _flag_started(self):
        self._started = True

    def make_default(self):
        '''
        Make this worker the default worker.

        Thread-safe.
        '''

        if not self.is_running():
            raise lexc.InvalidState("Event loop is not running")

        global default_worker
        default_worker = self

    def cancel(self, timeout_s : float | None=None):
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
        self.stop(timeout_s)

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

        self.wait(timeout_s)

    def wait(self, timeout_s : float | None=None):
        '''
        Wait for the event loop to complete all tasks.

        Thread-safe.
        '''

        global default_worker

        if self._thread is None:
            return
        if self._thread is threading.current_thread():
            return

        try:
            self._thread.join(timeout_s)
        except:
            if default_worker is self:
                default_worker = None
            raise

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
        try:
            self.wait()
        except KeyboardInterrupt:
            self.cancel()

    def execute(self, f : typing.Callable | typing.Coroutine, *args, **kwargs) -> concurrent.futures.Future | asyncio.Future:
        '''
        Schedule a coroutine or a callable to be executed in the event loop.

        Thread-safe and safe to call from within the event loop.
        '''

        if not self.is_running():
            raise lexc.InvalidState("Event loop is not running")

        name = f.__qualname__

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

        # self.logger.debug("Scheduling coroutine")

        assert self._loop is not None

        if threading.current_thread() is self._thread:
            # Directly create coro in the same worker context and return future.
            return asyncio.ensure_future(self._log(coro, name), loop=self._loop)
        else:
            # Create coro in the worker thread context, and return async future.
            return asyncio.run_coroutine_threadsafe(self._log(coro, name), self._loop)

    async def _log(self, coro : typing.Coroutine, name : str | None=None) -> typing.Any:
        try:
            if name:
                t = asyncio.current_task(self._loop)
                assert t is not None
                t.set_name(name)

            return await coro
        except asyncio.CancelledError:
            raise
        except KeyboardInterrupt:
            raise
        except lexc.Disconnected:
            raise
        except Exception as e:
            self.logger.debug('Exception in %s', asyncio.current_task(self._loop), exc_info=True)
            raise

    async def _execute(self, f, *args, **kwargs) -> typing.Any:
        return f(*args, **kwargs)

def silence_future(future : concurrent.futures.Future | asyncio.Future, logger : logging.Logger | None=None) -> concurrent.futures.Future | asyncio.Future:
    '''
    Silences exceptions in a future by adding a done callback that
    retrieves the result.

    This prevents "unhandled exception in future" warnings.
    '''
    def _callback(fut: concurrent.futures.Future | asyncio.Future):
        try:
            fut.result()
        except Exception as e:
            if logger is not None:
                logger.debug('Silenced exception in %s: %s', fut, e)

    future.add_done_callback(_callback)
    return future

def run_sync(f : typing.Callable) -> typing.Callable:
    '''
    Decorator to run an async function synchronously.

    If called from within an event loop, the coroutine is directly started,
    returning an awaitable.  If called from outside an event loop, the coroutine
    is executed in the default worker (creating one if needed).

    When block=False is passed, a future is returned instead.
    When sync is passed, it checks consistency with the detected (a)sync context.
    '''

    @functools.wraps(f)
    def run_sync(*args, block : bool=True, sync : bool | None=None, **kwargs) -> typing.Any:
        assert asyncio.iscoroutinefunction(f)

        self : typing.Any = args[0] if len(args) > 0 else None

        loop = None
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            pass

        assert sync is None or (loop is None) == sync or not block, 'sync argument contradicts current context'

        if loop is not None and not sync:
            # We are in an event loop, just start the coro.
            coro = f(*args, **kwargs)
            if block:
                return coro
            else:
                return asyncio.ensure_future(coro)
        else:
            # We are not in an event loop, run the coro in the (default) worker.
            w = None
            logger = None
            if isinstance(self, Work):
                logger = self.logger
                logger.debug("Running %s in worker %s", f.__qualname__, str(self.worker))
                w = self.worker
            else:
                if hasattr(self, 'logger'):
                    logger = self.logger
                    logger.debug("Running %s in default worker", f.__qualname__)
                global default_worker
                w = default_worker

            if w is None or not w.is_running():
                if hasattr(self, 'logger'):
                    logger = self.logger
                    logger.debug('No worker running, creating new one')
                w = AsyncioWorker()

            future = w.execute(f(*args, **kwargs))
            if block:
                return lexc.DeadlockChecker(future).result()
            elif sync:
                # Non-blocking sync calls are probably not really interested in the result.
                return silence_future(future, logger)
            else:
                return future

    return run_sync



class Work:
    '''
    Mixin class for objects that have work to run by an AsyncioWorker.
    '''

    def __init__(self, worker : AsyncioWorker | None=None, logger : logging.Logger | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.logger = logger or logging.getLogger(self.__class__.__name__)

        if worker is None:
            global default_worker
            worker = default_worker
            if worker is None or not worker.is_running():
                worker = AsyncioWorker()

        self._worker = worker

    @property
    def worker(self) -> AsyncioWorker:
        return self._worker

    @property
    def loop(self) -> asyncio.AbstractEventLoop:
        l = self.worker.loop
        if l is None:
            raise lexc.InvalidState("Event loop is not running")

        return l

    @staticmethod
    @functools.wraps(run_sync)
    def run_sync(f : typing.Callable) -> typing.Callable:
        # This is just an alias of the global function.
        return run_sync(f)

    @staticmethod
    def thread_safe_async(f : typing.Callable) -> typing.Callable:
        '''
        Decorator to make a method thread-safe by executing it in the worker
        thread, without waiting for completion.

        In case the method is called from within the worker thread, it is
        directly executed.  The actual result is returned.

        Otherwise, a concurrent.futures.Future is returned.
        '''

        assert not asyncio.iscoroutinefunction(f)

        @functools.wraps(f)
        def thread_safe_async(self, *args, **kwargs) -> typing.Any | concurrent.futures.Future:
            if threading.current_thread() is self.worker.thread:
                return f(self, *args, **kwargs)
            else:
                return self.worker.execute(f, self, *args, **kwargs)

        return thread_safe_async

    @staticmethod
    def thread_safe(f : typing.Callable) -> typing.Callable:
        '''
        Decorator to make a method thread-safe by executing it in the worker
        thread.

        By default, the call blocks until the method has completed and the
        actual result is returned.  If block=False is passed, a
        concurrent.futures.Future *may* be returned.
        '''

        assert not asyncio.iscoroutinefunction(f)

        @functools.wraps(f)
        def thread_safe(self, *args, block=True, **kwargs) -> typing.Any | concurrent.futures.Future:
            x = Work.thread_safe_async(f)(self, *args, **kwargs)
            if isinstance(x, concurrent.futures.Future):
                if block:
                    return lexc.DeadlockChecker(x).result()
                else:
                    return x
            else:
                return x

        return thread_safe

    @staticmethod
    def locked(f : typing.Callable) -> typing.Callable:
        '''Decorator to lock a method with the instance's lock.'''

        @functools.wraps(f)
        async def locked(self, *args, **kwargs):
            async with self.lock:
                return await f(self, *args, **kwargs)
        return locked

    @property
    def lock(self) -> lexc.DeadlockChecker:
        if not hasattr(self, '_lock'):
            self._lock = lexc.DeadlockChecker(asyncio.Lock())
        return self._lock
