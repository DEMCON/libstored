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

from .worker import AsyncioWorker

class AsyncTk:
    '''
    A thread running a tkinter mainloop.
    '''

    def __init__(self, cb_init=None):
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
        self._thread = threading.Thread(target=self.run, daemon=False)
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
        self._root.event_generate('<<async_call>>')
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

    def __init__(self, atk : AsyncTk, worker : AsyncioWorker, logger=None):
        '''
        Initialize the application.
        Do not call directly. Use create() instead.
        '''

        super().__init__(atk.root)
        self.logger = logger if logger is not None else logging.getLogger(__class__.__name__)
        self._atk = atk
        self._worker = worker
        self.bind("<Destroy>", self._on_destroy)

    class Context:
        def __init__(self, cls, *args, **kwargs):
            self.cls = cls
            self.args = args
            self.kwargs = kwargs
            self.logger = logging.getLogger(cls.__name__)
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
                if threading.current_thread() == self._worker.thread:
                    # Directly create coro in the same worker context and return future.
                    return f(self, *args, **kwargs)
                else:
                    # Create coro in the worker thread context, and return concurrent future.
                    return self._worker.execute(f(self, *args, **kwargs))
            else:
                if threading.current_thread() == self._worker.thread:
                    # Direct call in the same worker context.
                    return f(self, *args, **kwargs)
                else:
                    # Wait for the worker thread to complete the function.
                    return self._worker.execute(f, self, *args, **kwargs).result()
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
            if threading.current_thread() == self._atk.thread:
                # Direct call in the same tk context.
                return f(self, *args, **kwargs)
            elif threading.current_thread() == self._worker.thread:
                # Schedule the function in the tk thread, and return an asyncio future.
                future = self._atk.execute(f, self, *args, **kwargs)
                return asyncio.wrap_future(future, loop=self._worker.loop)
            else:
                # Schedule the function in the tk thread, and return a concurrent future.
                return self._atk.execute(f, self, *args, **kwargs)
        return wrapper

    def _on_destroy(self, event):
        self.logger.debug("Window destroyed")
        assert threading.current_thread() == self._atk.thread
        # Make sure to clean up all worker tasks, as they keep a reference to
        # Tk, which is going to be destroyed.
        self._worker.cancel()

    def __del__(self):
        self.logger.debug("App deleted")
        assert threading.current_thread() == self._atk.thread

class App(AsyncApp):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.pack()

        label = ttk.Label(self, text="Hello, TTK!")
        label.pack()

        button = ttk.Button(self, text="Click Me", command=self.on_button_click)
        button.pack()

        self._text = ttk.Label(self, text='0')
        self._text.pack()

    @AsyncApp.tk_func
    def on_button_click(self):
        print("Button clicked!")
        self.do_work()

    @AsyncApp.worker_func
    async def do_work(self):
        await asyncio.sleep(1)
        print("Work done!")
        self._update_label()

    @AsyncApp.tk_func
    def _update_label(self):
        print("Updating label")
        self._text.config(text=str(int(self._text.cget("text")) + 1))

logging.basicConfig(level=logging.DEBUG)

with App.create() as app:
    app.atk.wait()
