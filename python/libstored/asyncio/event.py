# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import inspect
import logging
import threading
import typing

class Event:
    logger = logging.getLogger(__name__)

    def __init__(self, event_name : str | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._callbacks : typing.Dict[typing.Hashable, typing.Callable] = {}
        self._key = 0
        self._queued = None
        self._paused = False
        self._event_name = event_name
        self._lock = threading.RLock()

    def __repr__(self) -> str:
        return f'Event({self._event_name})' if self._event_name is not None else super().__repr__()

    def __str__(self) -> str:
        return self._event_name if self._event_name is not None else super().__str__()

    def register(self, callback : typing.Callable, id : typing.Hashable | None=None):
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
        with self._lock:
            if id in self._callbacks:
                del self._callbacks[id]

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
    def paused(self):
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
                self.logger.exception(f'Exception in Event callback: {e}')

    async def wait(self):
        loop = asyncio.get_event_loop()
        fut = loop.create_future()
        def _callback():
            if not fut.done():
                fut.set_result(None)
        key = self.register(_callback)
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
        return None if self._get is None else self._get()

    @value.setter
    def value(self, value : typing.Any):
        if self._set is None:
            raise AttributeError("not writable")
        if value is not None and not isinstance(value, self.type):
            raise TypeError(f"expected {self.type}, got {type(value)}")
        self._set(value)

    def trigger(self):
        self._trigger(self.value)

    def _trigger(self, value : typing.Any = None):
        if value is not None and not isinstance(value, self.type):
            raise TypeError(f"expected {self.type}, got {type(value)}")
        super().trigger(value)

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
