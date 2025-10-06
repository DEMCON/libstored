# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import concurrent.futures
import functools
import gc
import logging
import threading
import time
import tkinter as tk
import tkinter.ttk as ttk
import queue
import typing

from .event import Event
from .worker import AsyncioWorker, Work, default_worker
from .zmq_client import Object

class AsyncTk:
    '''
    A thread running a tkinter mainloop.
    '''

    def __init__(self, cb_init=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.logger = logging.getLogger(__class__.__name__)

        self._thread = None
        self._queue = queue.Queue()
        self._root = None
        self._started = False
        self._cb_init = cb_init

    @property
    def thread(self) -> threading.Thread | None:
        return self._thread

    def start(self):
        '''
        Start the tkinter mainloop in a separate thread.

        Call only once.
        Alternatively, call run() directly from the main thread.
        '''

        if self._started:
            raise RuntimeError("Mainloop already started")

        self.logger.debug("Starting mainloop")
        self._thread = threading.Thread(target=self.run, daemon=False, name='AsyncTk')
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

        if threading.current_thread() != self._thread:
            raise RuntimeError("Accessing tk from wrong thread")

        assert self._root is not None
        return self._root

    def run(self):
        '''
        Run the tkinter mainloop.

        Call from the main thread, or via start().
        '''

        self._started = True

        try:
            self._root = tk.Tk()
            self._root.bind('<<async_call>>', self._on_async_call)

            init = None
            if self._cb_init is not None:
                init = self._cb_init(self)

            self.logger.debug("Running mainloop")
            self._root.mainloop()
            self.logger.debug("Mainloop exited")
            del init
        finally:
            self._root = None

        gc.collect()
        self.logger.debug("thread exit")

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

        gc.collect()
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
        gc.collect()
        self.logger.debug("Mainloop stopped")

    def is_running(self):
        '''
        Check if the mainloop is running.

        Thread-safe.
        '''
        return self._thread is not None and self._thread.is_alive() and self._root is not None

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
            raise RuntimeError("Mainloop is not running")

        self.logger.debug("Queueing async call")
        future = concurrent.futures.Future()
        self._queue.put((f, args, kwargs, future))
        assert self._root is not None
        self._root.event_generate('<<async_call>>', when='tail')
        return future

    def _on_async_call(self, event):
        while not self._queue.empty():
            self.logger.debug("Processing async call")
            func, args, kwargs, future = self._queue.get()
            try:
                future.set_result(func(*args, **kwargs))
            except Exception as e:
                self.logger.debug('Exception in async call', exc_info=True)
                future.set_exception(e)

class AsyncApp(ttk.Frame):
    '''
    A ttk application, running Tk in a separate thread, and an asyncio worker in another thread.
    Calling between contexts is thread-safe, as long functions are decorated with @tk_func or @worker_func.
    '''

    def __init__(self, atk : AsyncTk, worker : AsyncioWorker, logger=None, *args, **kwargs):
        '''
        Initialize the application.
        Do not call directly. Use create() instead.
        '''

        super().__init__(atk.root, *args, **kwargs)
        self.logger = logger if logger is not None else logging.getLogger(__class__.__name__)
        self._atk = atk
        self._worker = worker
        self._connections : dict[typing.Hashable, Event] = {}
        self.bind("<Destroy>", self._on_destroy)

        self.grid(sticky='nsew')
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

    class Context:
        def __init__(self, cls : typing.Type, worker : AsyncioWorker | Work | None=None, *args, **kwargs):
            global default_worker

            self.cls = cls
            self.args = args
            self.kwargs = kwargs
            self.logger = logging.getLogger(cls.__name__)

            if isinstance(worker, AsyncioWorker):
                self.worker = worker
            elif isinstance(worker, Work):
                self.worker = worker.worker
            elif default_worker is not None:
                self.worker = default_worker
            else:
                self.worker = AsyncioWorker()

            self.atk = AsyncTk(cb_init=self._init)

        def _init(self, atk : AsyncTk):
            return self.cls(*self.args, **self.kwargs, atk=atk, worker=self.worker, logger=self.logger)

        def __enter__(self):
            self.atk.start()
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.atk.stop()
            self.worker.stop()

    @classmethod
    def create(cls, *arg, **kwargs) -> Context:
        '''
        Create an instance of the application, running Tk in a separate thread, and an asyncio worker in another thread.

        Usage:

            with App.create() as app:
                # do something in main thread...
                app.atk.wait()
        '''
        return cls.Context(cls, *arg, **kwargs)

    @property
    def atk(self) -> AsyncTk:
        return self._atk

    @property
    def worker(self) -> AsyncioWorker:
        return self._worker

    @property
    def root(self) -> tk.Tk:
        return self.atk.root

    @staticmethod
    def worker_func(f) -> typing.Callable[..., typing.Any | asyncio.Future | concurrent.futures.Future]:
        '''
        Decorator to mark a function to be executed in the worker context.

        The decorated function may be a coroutine function or a regular function.
        In case of a coroutine, a future is returned.
        Otherwise, the call is blocking, and the result is returned.
        '''

        @functools.wraps(f)
        def wrapper(self, *args, **kwargs):
            if asyncio.iscoroutinefunction(f):
                if threading.current_thread() == self.worker.thread:
                    # Directly create coro in the same worker context and return future.
                    return f(self, *args, **kwargs)
                else:
                    # Create coro in the worker thread context, and return concurrent future.
                    return self.worker.execute(f(self, *args, **kwargs))
            else:
                if threading.current_thread() == self.worker.thread:
                    # Direct call in the same worker context.
                    return f(self, *args, **kwargs)
                else:
                    # Wait for the worker thread to complete the function.
                    return self.worker.execute(f, self, *args, **kwargs).result()
        return wrapper

    @staticmethod
    def tk_func(f) -> typing.Callable[..., typing.Any | asyncio.Future | concurrent.futures.Future]:
        '''
        Decorator to mark a function to be executed in the tk context.

        The decorated function must be a regular function.
        A future is returned when called from another thread.
        Otherwise, the call is blocking, and the result is returned.
        '''

        @functools.wraps(f)
        def wrapper(self, *args, **kwargs):
            if threading.current_thread() == self.atk.thread:
                # Direct call in the same tk context.
                return f(self, *args, **kwargs)
            elif threading.current_thread() == self.worker.thread:
                # Schedule the function in the tk thread, and return an asyncio future.
                future = self.atk.execute(f, self, *args, **kwargs)
                return asyncio.wrap_future(future, loop=self.worker.loop)
            else:
                # Schedule the function in the tk thread, and return a concurrent future.
                return self.atk.execute(f, self, *args, **kwargs)
        return wrapper

    def _on_destroy(self, event):
        self.logger.debug("Window destroyed")
        assert threading.current_thread() == self._atk.thread
        # Make sure to clean up all worker tasks, as they keep a reference to
        # Tk, which is going to be destroyed.
        self.cleanup()

    @tk_func
    def cleanup(self):
        for k, v in self._connections.items():
            v.unregister(k)
        self._connections.clear()

        self.logger.debug('Cleanup worker')
        try:
            self.worker.stop(5)
        except TimeoutError:
            self.worker.cancel()

    def __del__(self):
        self.logger.debug("App deleted")
        assert threading.current_thread() == self._atk.thread
        assert not self.worker.is_running()

    @tk_func
    def connect(self, event : Event, callback : typing.Callable) -> typing.Hashable:
        k = event.register(callback, callback)
        assert k not in self._connections
        self._connections[k] = event
        return k

    @tk_func
    def disconnect(self, id : typing.Hashable):
        if id in self._connections:
            event = self._connections[id]
            event.unregister(id)
            del self._connections[id]

class AsyncWidget:
    '''
    Mixin class for all async widgets.
    '''

    def __init__(self, app : AsyncApp, logger : logging.Logger | None= None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.logger = logging.getLogger(self.__class__.__name__)
        self._app = app

    @property
    def app(self) -> AsyncApp:
        return self._app

    @property
    def worker(self) -> AsyncioWorker:
        return self._app.worker

    @property
    def atk(self) -> AsyncTk:
        return self._app.atk

    @property
    def root(self) -> tk.Tk:
        return self._app.root

    @staticmethod
    def worker_func(f) -> typing.Callable[..., typing.Any | asyncio.Future | concurrent.futures.Future]:
        return AsyncApp.worker_func(f)

    @staticmethod
    def tk_func(f) -> typing.Callable[..., typing.Any | asyncio.Future | concurrent.futures.Future]:
        return AsyncApp.tk_func(f)

    def connect(self, event : Event, callback : typing.Callable) -> typing.Hashable:
        return self.app.connect(event, callback)

    def disconnect(self, id : typing.Hashable):
        self.app.disconnect(id)

class ZmqObjectEntry(AsyncWidget, ttk.Entry):
    '''
    An Entry widget, bound to a libstored Object.
    '''

    def __init__(self, app : AsyncApp, parent : tk.Widget, obj : Object, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._obj = obj
        self._focus = False
        self._editing = False
        self._empty = True

        self._var = tk.StringVar()
        self['textvariable'] = self._var
        self.connect(self.obj.value_str, self._refresh)
        self.bind('<Return>', self._write)
        self.bind('<KP_Enter>', self._write)
        self.bind('<FocusIn>', self._focus_in)
        self.bind('<FocusOut>', self._focus_out)
        self.bind('<Key>', self._edit)
        self.bind('<Control-KeyRelease-a>', lambda e: self.select_range(0, 'end'))
        self.bind('<KeyRelease-Escape>', self._revert)

        self['justify'] = 'right'
        self._refresh()

    @property
    def alive(self):
        return self.obj.alive

    @property
    def focused(self):
        return self._focus

    @property
    def editing(self):
        return self._editing

    @property
    def valid(self) -> bool:
        return self.alive and self.obj.value is not None

    @property
    def obj(self) -> Object:
        return self._obj

    @AsyncApp.worker_func
    async def refresh(self):
        if self.alive:
            await self.obj.read()

    @AsyncApp.tk_func
    def _refresh(self, value : str | None = None):
        if value is None:
            value = self.obj.value_str.value

        if value is None or (value == '' and not self.valid):
            value = '?'
            self['foreground'] = 'gray'
            self._empty = True
        elif value is not None and self._empty:
            self._empty = False
            if not self.editing:
                self['foreground'] = 'black'

        if not self.editing:
            self._var.set(value)

    def _write(self, *args):
        x = self._var.get()
        if self._editing:
            self._editing = False
            self['foreground'] = 'black'
        if self.alive:
            self.obj.value_str.value = x
            self.obj.write()

    def _focus_in(self, *args):
        self._focus = True
        if self._var.get() == '?':
            self._var.set('')
            self['foreground'] = 'black'

    def _focus_out(self, *args):
        self._focus = False
        self._revert()

    def _edit(self, *args):
        self._editing = True
        self['foreground'] = 'red'

    def _revert(self, *args):
        if self.editing:
            self._editing = False
            self['foreground'] = 'black'
        self._refresh()
