# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import logging
import typing

class Event:
    logger = logging.getLogger(__name__)

    def __init__(self):
        self._callbacks = {}
        self._key = 0

    def connect(self, callback : typing.Callable[[], None], id : typing.Hashable | None=None):
        if id is None:
            id = self._key
            self._key += 1
        self._callbacks[id] = callback
        return id

    def disconnect(self, id : typing.Hashable):
        if id in self._callbacks:
            del self._callbacks[id]

    def trigger(self):
        for callback in self._callbacks.values():
            try:
                callback()
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
