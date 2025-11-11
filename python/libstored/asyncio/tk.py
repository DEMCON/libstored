# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import asyncio
import concurrent.futures
import enum
import functools
import gc
import logging
import threading
import time
import tkinter as tk
import tkinter.ttk as ttk
import queue
import typing

from . import event as laio_event
from . import worker as laio_worker
from . import zmq as laio_zmq
from .. import exceptions as lexc

class AsyncTk:
    '''
    A thread running a tkinter mainloop.
    '''

    def __init__(self, cb_init=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.logger = logging.getLogger(__class__.__name__)

        self._thread = None
        self._run_from_main = False
        self._queue = queue.Queue()
        self._root = None
        self._started = False
        self._cb_init = cb_init
        self._do_async = False

    @property
    def thread(self) -> threading.Thread | None:
        if self._run_from_main:
            return threading.main_thread()

        return self._thread

    def start(self):
        '''
        Start the tkinter mainloop in a separate thread.

        Call only once.
        Alternatively, call run() directly from the main thread.

        Note that Tk is not fully thread-safe. This call is not recommended.
        Just call run() from the main thread instead.
        '''

        if self._started:
            raise lexc.InvalidState("Mainloop already started")

        self.logger.debug("Starting mainloop")
        self._thread = threading.Thread(target=self._run, daemon=False, name='AsyncTk')
        self._thread.start()
        while not self._started:
            time.sleep(0.1)
        while self._thread.is_alive() and self._root is None:
            time.sleep(0.1)

    @property
    def root(self) -> tk.Tk:
        '''
        Return the root Tk instance.

        Only to be called from within the Tk context (run()).
        '''

        if threading.current_thread() != self.thread:
            raise lexc.InvalidState("Accessing tk from wrong thread")

        assert self._root is not None
        return self._root

    def run(self):
        '''
        Run the tkinter mainloop.

        Call from the main thread.
        '''

        if threading.current_thread() != threading.main_thread():
            raise lexc.InvalidState("run() must be called from the main thread")
        if self.is_running():
            raise lexc.InvalidState("Mainloop already started")

        self._run_from_main = True
        try:
            self._run()
        finally:
            self._run_from_main = False

    def _run(self):
        '''
        Run the tkinter mainloop.

        Call from the main thread, or via start().
        '''

        self._do_async = True
        self._started = True

        if not self._run_from_main:
            self.logger.warning('Running Tk mainloop in a separate thread. This is not recommended, as Tk is not fully thread-safe.')

        try:
            while True:
                self._queue.get_nowait()
        except queue.Empty:
            pass

        try:
            self._root = tk.Tk()
            self._root.report_callback_exception = lambda *args: self.logger.exception('Unhandled exception in Tk', exc_info=args)
            self._root.protocol("WM_DELETE_WINDOW", self._on_stop)
            self._root.bind('<<async_call>>', self._on_async_call)

            init = None
            if self._cb_init is not None:
                init = self._cb_init(self)
                if isinstance(init, tk.Widget):
                    init.bind("<Destroy>", self._on_stopping, add=True)

            self.logger.debug("Running mainloop")
            self._root.mainloop()
            self.logger.debug("Mainloop exited")
            self._on_stopping(None)

            gc.collect()
            if not self._run_from_main:
                self._dump_referrers(init, 1)

            del init
            self.logger.debug("deleted init")
        finally:
            self._root = None
            self._do_async = False

        gc.collect()
        self.logger.debug("thread exit")

    def _on_stop(self, event = None):
        self._on_stopping()
        if self._root is not None:
            self._root.destroy()

    def _on_stopping(self, event = None):
        if self._do_async:
            self.logger.debug("Prevent further async calls")
            self._do_async = False

    def _dump_referrers(self, obj, depth : int=3, indent : str=''):
        if depth < 0 or obj is None:
            return

        ref = gc.get_referrers(obj)
        if ref == []:
            return

        self.logger.debug(f'{indent}{repr(obj)}: {len(ref)} referrers')

        for r in ref:
            self._dump_referrers(r, depth-1, indent + '  ')

    def _stop(self):
        if self._root is None:
            return

        self._root.quit()

    def stop(self):
        '''
        Stop the tkinter mainloop and wait for the thread to exit.

        Thread-safe.
        '''

        if self._thread is None:
            return

        if self.is_running():
            self.logger.debug("Stopping mainloop")
            self.execute(lambda: self._stop())

        assert self._thread is not None
        self._thread.join()
        self._thread = None

        self.logger.debug("Mainloop stopped")

    def wait(self, timeout=None):
        '''
        Wait for the tkinter mainloop thread to exit.

        Thread-safe.
        '''

        if self._thread is None:
            return

        self.logger.debug("Waiting for mainloop")
        self._thread.join(timeout)
        if self._thread.is_alive():
            raise TimeoutError("Mainloop did not exit in time")

        self._thread = None
        self.logger.debug("Mainloop stopped")

    def is_running(self):
        '''
        Check if the mainloop is running.

        Thread-safe.
        '''
        return self._run_from_main or \
            (self._thread is not None and self._thread.is_alive() and self._root is not None)

    def __del__(self):
        self.stop()

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()

    def execute(self, f, *args, **kwargs) -> concurrent.futures.Future:
        '''
        Queue a function for the tkinter mainloop thread.

        Thread-safe.
        '''

        if not self.is_running():
            raise lexc.InvalidState("Mainloop is not running")

        self.logger.debug("Queueing async call to %s", f.__qualname__)
        future = concurrent.futures.Future()
        try:
            self._queue.put((f, args, kwargs, future), block=True, timeout=lexc.DeadlockChecker.default_timeout_s)
        except queue.Full:
            raise lexc.Deadlock("AsyncTk queue full") from None

        if not self._do_async:
            self.logger.debug("Dropped async call")
            future.set_exception(lexc.InvalidState("AsyncTk not ready anymore for async calls"))
            return future

        assert self._root is not None
        self._root.event_generate('<<async_call>>', when='tail')
        return future

    def _on_async_call(self, event):
        try:
            while True:
                func, args, kwargs, future = self._queue.get_nowait()
                self.logger.debug("Processing async call to %s", func.__qualname__)
                try:
                    future.set_result(func(*args, **kwargs))
                except BaseException as e:
                    self.logger.debug('Exception in async call to %s', func.__qualname__, exc_info=True)
                    future.set_exception(e)
        except queue.Empty:
            pass



class Work:
    '''
    Mixin class for all async Tk modules.
    '''
    def __init__(self, atk : AsyncTk, logger : logging.Logger | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.logger = logger if logger is not None else logging.getLogger(self.__class__.__name__)

        self._atk : AsyncTk = atk
        self._connections : dict[typing.Hashable, laio_event.Event] = {}
        self._connections_key = 0

        if hasattr(self, 'bind') and callable(getattr(self, 'bind')):
            # Assume this is a tk widget.
            typing.cast(tk.Widget, self).bind("<Destroy>", self._on_destroy)
        else:
            self.atk.root.bind("<Destroy>", self._on_destroy, add=True)

    @property
    def atk(self) -> AsyncTk:
        return self._atk

    @staticmethod
    def tk_func(f) -> typing.Callable[..., typing.Any | asyncio.Future | concurrent.futures.Future]:
        '''
        Decorator to mark a function to be executed in the tk context.

        The decorated function must be a regular function.
        A future is returned when called from another thread.
        Otherwise, the call is blocking, and the result is returned.

        When block=True is passed, the call is always blocking, and the result is returned.
        '''

        @functools.wraps(f)
        def tk_func(self : Work, *args, block : bool=False, **kwargs) -> typing.Any:
            # self.logger.debug(f'Scheduling {f} in tk')

            try:
                if threading.current_thread() == self.atk.thread:
                    # Direct call in the same tk context.
                    return f(self, *args, **kwargs)
                else:
                    # Schedule the function in the tk thread, and return a concurrent future.
                    future = self.atk.execute(f, self, *args, **kwargs)

                    try:
                        asyncio.get_running_loop()
                    except RuntimeError:
                        # No running loop, safe to block.
                        if block:
                            return lexc.DeadlockChecker(future).result()
                        else:
                            return future

                    # Wrap in an asyncio future.
                    future = asyncio.wrap_future(future)
                    if block:
                        return lexc.DeadlockChecker(future).result()
                    else:
                        return future
            except BaseException as e:
                self.logger.debug(f'Exception {e} in scheduling tk function {f}')
                raise
        return tk_func

    @tk_func
    def connect(self, event : laio_event.Event, callback : typing.Callable, *args, **kwargs) -> typing.Hashable:
        k = (self, self._connections_key)
        self._connections_key += 1
        k = event.register(callback, k, *args, **kwargs)
        assert k not in self._connections
        self._connections[k] = event
        return k

    @tk_func
    def disconnect(self, id : typing.Hashable):
        if id in self._connections:
            event = self._connections[id]
            del self._connections[id]
            event.unregister(id)

    @tk_func
    def disconnect_all(self):
        c = self._connections
        self._connections = {}
        for k, v in c.items():
            v.unregister(k)

    @tk_func
    def cleanup(self):
        self.disconnect_all()

    def _on_destroy(self, event):
        assert threading.current_thread() == self._atk.thread
        if event.widget is self:
            self.cleanup()

    def __del__(self):
        # Tk is not thread-safe. Ignore this check.
        #assert threading.current_thread() == self._atk.thread

        self.cleanup()



class AsyncApp(Work, ttk.Frame):
    '''
    A ttk application, running Tk in a separate thread, and an asyncio worker in another thread.
    Calling between contexts is thread-safe, as long functions are decorated with @tk_func or @worker_func.
    '''

    def __init__(self, atk : AsyncTk, worker : laio_worker.AsyncioWorker, *args, **kwargs):
        '''
        Initialize the application.
        Do not call directly. Use create() instead.
        '''

        super().__init__(atk=atk, master=atk.root, *args, **kwargs)
        self._worker = worker

        self.grid(sticky='nsew')
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

    class Context:
        def __init__(self, cls : typing.Type, worker : laio_worker.AsyncioWorker | laio_worker.Work | None=None, *args, **kwargs):
            global default_worker

            self.cls = cls
            self.args = args
            self.kwargs = kwargs
            self.logger = logging.getLogger(cls.__name__)

            if isinstance(worker, laio_worker.AsyncioWorker):
                self.worker = worker
            elif isinstance(worker, laio_worker.Work):
                self.worker = worker.worker
            elif laio_worker.default_worker is not None:
                self.worker = laio_worker.default_worker
            else:
                self.worker = laio_worker.AsyncioWorker()

            self.atk = AsyncTk(cb_init=self._init)

        def _init(self, atk : AsyncTk):
            return self.cls(*self.args, **self.kwargs, atk=atk, worker=self.worker, logger=self.logger)

        def __enter__(self):
            # Disabled, as Tk is not fully thread-safe.
            # Just call run() from the main thread instead.
            #self.atk.start()
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.atk.stop()
            self.worker.stop()

    @classmethod
    def create(cls, *arg, **kwargs) -> Context:
        '''
        Create an instance of the application, running Tk in a separate thread, and an asyncio worker in another thread.

        Usage:

            async def stuff():
                ...

            with App.create() as app:
                app.worker.execute(stuff())
                app.atk.run()
        '''
        return cls.Context(cls, *arg, **kwargs)

    @classmethod
    def run(cls, *args, coro: typing.Coroutine | None=None, **kwargs):
        '''
        Create and run an instance of the application, running Tk in the main thread, and an asyncio worker in another thread.
        When coro is provided, coro is started in the worker context.

        Usage:

            async def stuff():
                ...
                return result

            stuff_result = App.run(stuff())
        '''
        with cls.create(*args, **kwargs) as context:
            res = None if coro is None else context.worker.execute(coro)
            context.atk.run()
            return res.result() if res is not None else None

    @property
    def worker(self) -> laio_worker.AsyncioWorker:
        return self._worker

    @property
    def root(self) -> tk.Tk:
        return self.atk.root

    @staticmethod
    def worker_func(f) -> typing.Callable[..., typing.Any | asyncio.Future | concurrent.futures.Future]:
        '''
        Decorator to mark a function to be executed in the worker context.

        The decorated function may be a coroutine function or a regular function.
        By default, a future is returned, unless block=True is passed.
        '''

        @functools.wraps(f)
        def worker_func(self : AsyncApp, *args, block : bool=False, **kwargs) -> typing.Any:
            # self.logger.debug(f'Scheduling {f} in worker')

            try:
                if asyncio.iscoroutinefunction(f):
                    loop = None
                    try:
                        loop = asyncio.get_running_loop()
                    except RuntimeError:
                        pass

                    if loop is not None and loop is self.worker.loop:
                        # Directly create coro in the same worker context and return future.
                        coro = f(self, *args, **kwargs)
                        if block:
                            return coro
                        else:
                            return asyncio.ensure_future(coro)
                    else:
                        # Create coro in the worker thread context, and return concurrent future.
                        future = self.worker.execute(f(self, *args, **kwargs))
                        if block:
                            return lexc.DeadlockChecker(future).result()
                        else:
                            return future
                else:
                    if threading.current_thread() is self.worker.thread:
                        # Direct call in the same worker context.
                        return f(self, *args, **kwargs)
                    else:
                        # Wait for the worker thread to complete the function.
                        future = self.worker.execute(f, self, *args, **kwargs)
                        if block:
                            return lexc.DeadlockChecker(future).result()
                        else:
                            return future
            except asyncio.CancelledError:
                raise
            except BaseException as e:
                self.logger.debug(f'Exception {e} in scheduling worker function {f}')
                raise

        return worker_func

    @Work.tk_func
    def cleanup(self):
        super().cleanup()

        self.logger.debug('Cleanup worker')
        try:
            self.worker.stop(lexc.DeadlockChecker.default_timeout_s + 1 \
                             if lexc.DeadlockChecker.default_timeout_s is not None else None)
        except TimeoutError:
            self.logger.debug('Cleanup worker - forcing')
            self.worker.cancel()

    def __del__(self):
        assert not self.worker.is_running()



class AsyncWidget(Work):
    '''
    Mixin class for all async widgets.
    '''

    def __init__(self, app : AsyncApp, *args, **kwargs):
        super().__init__(atk=app.atk, *args, **kwargs)
        self._app = app

    @property
    def app(self) -> AsyncApp:
        return self._app

    @property
    def worker(self) -> laio_worker.AsyncioWorker:
        return self._app.worker

    @property
    def root(self) -> tk.Tk:
        return self._app.root

    @staticmethod
    def worker_func(f) -> typing.Callable[..., typing.Any | asyncio.Future | concurrent.futures.Future]:
        return AsyncApp.worker_func(f)



class ZmqObjectEntry(AsyncWidget, ttk.Entry):
    '''
    An Entry widget, bound to a libstored Object.
    '''

    class State(enum.IntEnum):
        INIT = enum.auto()
        DEFAULT = enum.auto()
        DISCONNECTED = enum.auto()
        INVALID = enum.auto()
        VALID = enum.auto()
        UPDATED = enum.auto()
        FOCUSED = enum.auto()
        EDITING = enum.auto()

    def __init__(self, app : AsyncApp, parent : tk.Widget, obj : laio_zmq.Object,
                 rate_limit_Hz=3, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._obj = obj
        self._updated : float = 0
        self._state = ZmqObjectEntry.State.INIT

        self._var = tk.StringVar()
        self['textvariable'] = self._var
        self._rate_limit = laio_event.AsyncioRateLimit(worker=self.worker, Hz=rate_limit_Hz, event_name=obj.name)
        self.connect(self._rate_limit, self._refresh)
        self.connect(self.obj.value_str, self._rate_limit)
        self.connect(self.obj.client.disconnected, self._refresh)
        self.bind('<Return>', self._write)
        self.bind('<KP_Enter>', self._write)
        self.bind('<FocusIn>', self._focus_in)
        self.bind('<FocusOut>', self._focus_out)
        self.bind('<Key>', self._edit)

        def select_all(event):
            event.widget.select_range(0, 'end')
            event.widget.icursor('end')
            return 'break'

        self.bind('<Control-a>', select_all)
        self.bind('<Control-A>', select_all)

        self.bind('<KeyRelease-Escape>', self._revert)

        self['justify'] = 'right'
        self._set_state(ZmqObjectEntry.State.DEFAULT)
        self._refresh()

    def __repr__(self):
        return f'ZmqObjectEntry({self.obj.name})@0x{id(self):x}'

    @property
    def alive(self):
        try:
            return self.obj.alive and self.obj.client.is_connected()
        except lexc.Disconnected:
            return False

    @property
    def focused(self) -> bool:
        return self._state >= ZmqObjectEntry.State.FOCUSED

    @property
    def editing(self):
        return self._state >= ZmqObjectEntry.State.EDITING

    @property
    def valid(self) -> bool:
        return self.alive and self._state >= ZmqObjectEntry.State.VALID

    @property
    def obj(self) -> laio_zmq.Object:
        return self._obj

    @property
    def updated(self) -> bool:
        return self._state == ZmqObjectEntry.State.UPDATED

    def _set_state(self, state : State):
        if not self.alive:
            state = ZmqObjectEntry.State.DISCONNECTED

        if state <= ZmqObjectEntry.State.DEFAULT:
            if self.obj.value_str.value is None or self.obj.value is None:
                state = ZmqObjectEntry.State.INVALID
            else:
                state = ZmqObjectEntry.State.VALID

        if state == self._state:
            return

        if self._state == ZmqObjectEntry.State.INVALID and state != ZmqObjectEntry.State.DISCONNECTED:
            self._var.set('')

        self._state = state

        if not self.winfo_exists():
            return

        if self._state == ZmqObjectEntry.State.DISCONNECTED:
            # Freeze field.
            self['state'] = 'disabled'
        elif self['state'] == 'disabled':
            self['state'] = 'normal'

        if self._state == ZmqObjectEntry.State.DISCONNECTED:
            self['foreground'] = 'gray'
        elif self._state == ZmqObjectEntry.State.INVALID:
            self['foreground'] = 'gray'
            self._var.set('?')
        elif self._state == ZmqObjectEntry.State.VALID:
            self['foreground'] = 'black'
        elif self._state == ZmqObjectEntry.State.UPDATED:
            self['foreground'] = 'blue'
        elif self._state == ZmqObjectEntry.State.FOCUSED:
            self['foreground'] = 'black'
        elif self._state == ZmqObjectEntry.State.EDITING:
            self['foreground'] = 'red'

    @AsyncApp.worker_func
    async def refresh(self, acquire_alias : bool=False):
        if self.alive:
            await self.obj.read(acquire_alias=acquire_alias)
            self._rate_limit.flush()

    @AsyncApp.tk_func
    def _refresh(self, value : str | None = None):
        if value is None:
            value = self.obj.value_str.value

        if not self.alive:
            self._set_state(ZmqObjectEntry.State.DISCONNECTED)
            return
        elif self.focused:
            return
        elif value is None or self.obj.value is None:
            self._set_state(ZmqObjectEntry.State.INVALID)
            return

        if self._var.get() == value:
            return

        if not self.focused:
            self._set_state(ZmqObjectEntry.State.UPDATED)
            self._updated = time.time() + 1.05
            self.after(1100, self._updated_end)

        self._var.set(value)

    def _updated_end(self):
        if not self.updated:
            return
        if time.time() >= self._updated:
            self._set_state(ZmqObjectEntry.State.DEFAULT)

    def _write(self, *args):
        if not self.alive:
            self._set_state(ZmqObjectEntry.State.DEFAULT)
            return

        if self.editing:
            self._set_state(ZmqObjectEntry.State.FOCUSED)

        x = self._var.get()
        self.logger.debug(f'Write {x} to {self.obj.name}')
        self.obj.value_str.value = x
        self.obj.write(block=False)

    def _focus_in(self, *args):
        if not self.focused:
            self._set_state(ZmqObjectEntry.State.FOCUSED)

    def _focus_out(self, *args):
        self._revert()

        if self.focused:
            self._set_state(ZmqObjectEntry.State.DEFAULT)

    def _edit(self, e):
        try:
            if e.keysym == 'Escape':
                return
        except:
            pass

        self._set_state(ZmqObjectEntry.State.EDITING)

    def _revert(self, *args):
        if self.editing:
            self._set_state(ZmqObjectEntry.State.FOCUSED)

        if self.focused:
            value = self.obj.value_str.value
            self._var.set(value if value is not None else '')
        else:
            self._refresh()
