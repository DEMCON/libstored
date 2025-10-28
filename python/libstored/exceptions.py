# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import asyncio
import concurrent
import concurrent.futures
import enum
import filelock
import typing
import logging

class Disconnected(RuntimeError):
    pass

class OperationFailed(RuntimeError):
    pass

class InvalidState(RuntimeError):
    pass

class NotSupported(RuntimeError):
    pass

class InvalidResponse(ValueError):
    pass

class Deadlock(RuntimeError):
    pass

class DeadlockChecker:
    '''
    Context manager to check for deadlocks when acquiring a lock.

    Usage:

        with DeadlockChecker(lock, timeout_s=5):
            # Critical section
            ...
    '''

    default_timeout_s : float | None = None

    class Type(enum.Enum):
        THREADING_LOCK = enum.auto()
        ASYNCIO_LOCK = enum.auto()
        ASYNCIO_FILELOCK = enum.auto()
        COROUTINE = enum.auto()
        ASYNCIO_FUTURE = enum.auto()
        CONCURRENT_FUTURE = enum.auto()

    def __init__(self, lock : typing.Any, timeout_s : float | None=None):
        self._lock = lock
        self._timeout_s = timeout_s if timeout_s is not None else self.default_timeout_s
        self._acquired = False
        self.logger = logging.getLogger(__class__.__name__)

        if isinstance(lock, asyncio.Lock):
            # asyncio.Lock
            self._type = self.Type.ASYNCIO_LOCK
        elif isinstance(lock, filelock.AsyncFileLock):
            # filelock.AsyncFileLock
            self._type = self.Type.ASYNCIO_FILELOCK
        elif asyncio.futures.isfuture(lock):
            # asyncio.Future
            self._type = self.Type.ASYNCIO_FUTURE
        elif asyncio.iscoroutine(lock):
            # coroutine
            self._type = self.Type.COROUTINE
        elif isinstance(lock, concurrent.futures.Future):
            # concurrent.futures.Future
            self._type = self.Type.CONCURRENT_FUTURE
        elif hasattr(lock, 'acquire') and callable(getattr(lock, 'acquire')):
            # Looks like threading.Lock or threading.RLock
            self._type = self.Type.THREADING_LOCK
        else:
            raise TypeError("Unsupported lock type %s" % type(lock))

    def _deadlock(self, type):
        self.logger.critical(f"Deadlock detected: could not {type} {self._lock} within {self._timeout_s} seconds")
        raise Deadlock("Deadlock detected") from None

    def __enter__(self):
        if self._type != self.Type.THREADING_LOCK:
            raise RuntimeError('Wrong access method')

        self._acquired = self._lock.acquire(timeout=self._timeout_s if self._timeout_s is not None else -1)
        if not self._acquired:
            self._deadlock('acquire lock')
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self._acquired:
            self._lock.release()
            self._acquired = False

    async def __aenter__(self):
        try:
            if self._type == self.Type.ASYNCIO_LOCK:
                await asyncio.wait_for(self._lock.acquire(), timeout=self._timeout_s)
            elif self._type == self.Type.ASYNCIO_FILELOCK:
                await self._lock.acquire(timeout=self._timeout_s)
            else:
                raise RuntimeError('Wrong access method')
            self._acquired = True
        except asyncio.TimeoutError:
            self._deadlock('acquire lock')
        return self

    async def __aexit__(self, exc_type, exc_value, traceback):
        if self._acquired:
            if self._type == self.Type.ASYNCIO_LOCK:
                self._lock.release()
            elif self._type == self.Type.ASYNCIO_FILELOCK:
                await self._lock.release()

            self._acquired = False

    def result(self):
        try:
            if self._type == self.Type.ASYNCIO_FUTURE:
                return asyncio.wait_for(self._lock, timeout=self._timeout_s)
            elif self._type == self.Type.CONCURRENT_FUTURE:
                try:
                    asyncio.get_running_loop()
                except RuntimeError:
                    # No running loop, safe to block.
                    return self._lock.result(timeout=self._timeout_s)

                # We are in an event loop, wrap in asyncio future.
                return asyncio.wait_for(asyncio.wrap_future(self._lock), timeout=self._timeout_s)
            else:
                raise RuntimeError('Wrong access method')
        except asyncio.TimeoutError:
            self._deadlock('acquire future')

    def __await__(self):
        if self._type != self.Type.COROUTINE:
            raise RuntimeError('Wrong access method')

        try:
            return asyncio.wait_for(self._lock, timeout=self._timeout_s).__await__()
        except asyncio.TimeoutError:
            self._deadlock('complete coroutine')

    def has_lock(self):
        return self._acquired
