# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import inspect
import logging
import typing

class Event:
    logger = logging.getLogger(__name__)

    def __init__(self):
        self._callbacks = {}
        self._key = 0
        self._queued = None
        self._paused = False

    def connect(self, callback : typing.Callable[..., None], id : typing.Hashable | None=None):
        if id is None:
            id = self._key
            self._key += 1
        self._callbacks[id] = callback
        return id

    def disconnect(self, id : typing.Hashable):
        if id in self._callbacks:
            del self._callbacks[id]

    def pause(self):
        self._paused = True

    def resume(self):
        self._paused = False
        if self._queued is not None:
            args, kwargs = self._queued
            self._queued = None
            self.trigger(*args, **kwargs)

    @property
    def paused(self):
        return self._paused

    def trigger(self, *args, **kwargs):
        if self._paused:
            if self._callbacks:
                self._queued = (args, kwargs)
            return

        for callback in self._callbacks.values():
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
        key = self.connect(_callback)
        try:
            await fut
        finally:
            self.disconnect(key)

class ValueWrapper(Event):
    def __init__(self, type : typing.Type,
                 get : typing.Callable[[], typing.Any] | None = None,
                 set : typing.Callable[[typing.Any], None] | None = None):
        super().__init__()
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
        self._trigger(value)

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
    def __init__(self, type : typing.Type, initial : typing.Any=None):
        super().__init__(type, self._get, self._set)
        if initial is not None and not isinstance(initial, type):
            raise TypeError(f"expected {type}, got {type(initial)}")
        self._value = initial

    def _get(self) -> typing.Any:
        return self._value

    def _set(self, value : typing.Any):
        self._value = value
