# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import inspect
import logging
import threading
import typing

from . import worker as laio_worker
from .. import exceptions as lexc

class Event:
    logger = logging.getLogger(__name__)

    def __init__(self, event_name : str | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._callbacks : typing.Dict[typing.Hashable, typing.Callable] = {}
        self._key = 0
        self._queued = None
        self._paused = False
        self._event_name = event_name
        self._lock = lexc.DeadlockChecker(threading.RLock())

    def __repr__(self) -> str:
        return f'{self.__class__.__name__}({self._event_name})' if self._event_name is not None else super().__repr__()

    def __str__(self) -> str:
        return self._event_name if self._event_name is not None else super().__str__()

    def register(self, callback : typing.Callable, id : typing.Hashable | None=None) -> typing.Hashable:
        with self._lock:
            if id is not None and id in self._callbacks:
                raise KeyError(f"Callback with id {id} already registered")

            if id is None:
                while id is None or id in self._callbacks:
                    id = self._key
                    self._key += 1

            self._callbacks[id] = callback
            return id

    def unregister(self, id : typing.Hashable):
        c = []
        with self._lock:
            if id in self._callbacks:
                c.append(self._callbacks[id])
                del self._callbacks[id]
        del c

    def __len__(self) -> int:
        with self._lock:
            return len(self._callbacks)

    def pause(self):
        with self._lock:
            self._paused = True

    def resume(self):
        trigger = None
        with self._lock:
            self._paused = False
            if self._queued is not None:
                trigger = self._queued
                self._queued = None

        if trigger is not None:
            self.trigger(*trigger[0], **trigger[1])

    @property
    def paused(self) -> bool:
        return self._paused

    def trigger(self, *args, **kwargs):
        self.logger.debug('trigger %s', repr(self))

        callbacks = []
        with self._lock:
            if self._paused:
                if self._callbacks:
                    self._queued = (args, kwargs)
                return

            callbacks = list(self._callbacks.values())

        for callback in callbacks:
            try:
                bound = None
                try:
                    bound = inspect.signature(callback).bind(*args, **kwargs)
                except TypeError:
                    callback()
                else:
                    callback(*bound.args, **bound.kwargs)
            except Exception as e:
                self.logger.exception(f'Exception in {repr(self)} callback: {e}')

    def __call__(self, *args, **kwargs):
        self.trigger(*args, **kwargs)

    async def wait(self):
        loop = asyncio.get_event_loop()
        fut = loop.create_future()

        def _callback():
            if not fut.done():
                fut.set_result(None)

        def _safe_callback():
            # Potentially from another thread.
            loop.call_soon_threadsafe(_callback)

        key = self.register(_safe_callback)
        try:
            await fut
        finally:
            self.unregister(key)

class ValueWrapper(Event):
    def __init__(self, type : typing.Type,
                 get : typing.Callable[[], typing.Any] | None = None,
                 set : typing.Callable[[typing.Any], None] | None = None,
                 *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._type = type
        self._get = get
        self._set = set

    @property
    def value(self) -> typing.Any:
        if self._get is None:
            return None
        x = self._get()

        if x is not None and not isinstance(x, self.type):
            raise TypeError(f"expected {self.type}, getter returned {type(x)}")

        return x

    @value.setter
    def value(self, value : typing.Any):
        if self._set is None:
            raise AttributeError("not writable")
        if value is not None and not isinstance(value, self.type):
            raise TypeError(f"expected {self.type}, got {type(value)}")
        self._set(value)

    def get(self) -> typing.Any:
        return self.value

    def set(self, value : typing.Any):
        self.value = value

    def trigger(self, value : typing.Any = None):
        if value is not None and not isinstance(value, self.type):
            raise TypeError(f"expected {self.type}, got {type(value)}")
        super().trigger(value if value is not None else self.value)

    def register(self, *args, trigger=True, **kwargs) -> typing.Hashable:
        id = super().register(*args, *kwargs)
        if trigger:
            self.trigger()
        return id

    @property
    def type(self) -> typing.Type:
        return self._type

    def __str__(self) -> str:
        return str(self.value)

class Value(ValueWrapper):
    def __init__(self, type : typing.Type, initial : typing.Any=None, *args, **kwargs):
        super().__init__(type, self._get, self._set, *args, **kwargs)
        if initial is not None and not isinstance(initial, type):
            raise TypeError(f"expected {type}, got {type(initial)}")
        self._value = initial

    def _get(self) -> typing.Any:
        return self._value

    def _set(self, value : typing.Any):
        if self._value != value:
            self._value = value
            self.trigger()

class AsyncioRateLimit(laio_worker.Work, Event):
    '''
    Event that can be triggered, but not more often than a specified minimum interval.

    If triggered more often, only the last trigger arguments are used, and the event
    will be triggered after the minimum interval has passed.

    The trigger() method is thread-safe.
    The timer callback is executed in the worker's event loop.
    '''

    def __init__(self, Hz : float | None=None, min_interval_s : float | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)

        if Hz is not None and min_interval_s is not None:
            raise ValueError("Only one of Hz and min_interval_s can be specified")
        if Hz is None and min_interval_s is None:
            raise ValueError("One of Hz and min_interval_s must be specified")
        if Hz is not None:
            if Hz <= 0:
                raise ValueError("Hz must be positive")
            min_interval_s = 1.0 / Hz

        assert min_interval_s is not None
        self._min_interval_s = max(0, min_interval_s)
        self._last_trigger = 0.0
        self._timer : asyncio.TimerHandle | None = None
        self._args : tuple[tuple, dict] = ((), {})

    def unregister(self, id : typing.Hashable):
        super().unregister(id)
        if len(self) == 0 and self._timer is not None:
            self._timer.cancel()
            self._timer = None

    @laio_worker.Work.thread_safe_async
    def trigger(self, *args, **kwargs):
        now = self.loop.time()
        if now - self._last_trigger >= self._min_interval_s:
            if self._timer is not None:
                self._timer.cancel()
                self._timer = None

            self._last_trigger = now
            super().trigger(*args, **kwargs)
        else:
            # Only save last arguments for timer callback.
            self._args = (args, kwargs)

            if self._timer is None:
                delay = self._min_interval_s - (now - self._last_trigger)
                self._timer = self.loop.call_later(delay, self._timer_callback)

    def _timer_callback(self):
        self._timer = None
        self.trigger(*self._args[0], **self._args[1])

    @laio_worker.Work.thread_safe_async
    def flush(self):
        if self._timer is None:
            return

        self._timer.cancel()
        self._timer = None

        now = self.loop.time()
        if now - self._last_trigger >= self._min_interval_s:
            self._last_trigger = now
            super().trigger(*self._args[0], **self._args[1])

    def __del__(self):
        if self._timer is not None:
            self._timer.cancel()
            self._timer = None
