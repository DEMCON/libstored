#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2020-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import logging
import os
import sys
import threading
import unittest

sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../python"))
)

import libstored.asyncio.worker

logger = logging.getLogger(__name__)


def f() -> tuple[threading.Thread, asyncio.AbstractEventLoop | None]:
    t = threading.current_thread()
    loop = None
    try:
        loop = asyncio.get_running_loop()
    except RuntimeError:
        pass

    logger.info(
        "running in thread %s, loop %s",
        t.name,
        hex(id(loop)) if loop is not None else None,
    )

    return (t, loop)


async def coro_f() -> tuple[threading.Thread, asyncio.AbstractEventLoop | None]:
    return f()


@libstored.asyncio.worker.run_sync
async def async_f() -> tuple[threading.Thread, asyncio.AbstractEventLoop | None]:
    return await coro_f()


class WorkF(libstored.asyncio.worker.Work):
    @libstored.asyncio.worker.Work.thread_safe
    def f(self) -> tuple[threading.Thread, asyncio.AbstractEventLoop | None]:
        return f()

    @libstored.asyncio.worker.Work.thread_safe
    def f2(self) -> tuple[threading.Thread, asyncio.AbstractEventLoop | None]:
        return self.f()


class AsyncTest(unittest.TestCase):
    def test_normal(self):
        # Just runs here.
        context = f()
        self.assertEqual(context, (threading.current_thread(), None))

    def test_coro(self):
        # Runs in the created loop in the current thread.
        context = asyncio.run(coro_f())
        self.assertEqual(context[0], threading.current_thread())
        self.assertIsNotNone(context[1])

    def test_sync(self):
        # Runs in the default worker thread and loop.
        context = async_f()
        self.assertIsNotNone(libstored.asyncio.worker.default_worker)
        assert libstored.asyncio.worker.default_worker
        self.assertEqual(
            context,
            (
                libstored.asyncio.worker.default_worker.thread,
                libstored.asyncio.worker.default_worker.loop,
            ),
        )

    def test_sync_nonblock(self):
        # Runs in the default worker thread and loop.
        context = async_f(block=False).result()
        self.assertIsNotNone(libstored.asyncio.worker.default_worker)
        assert libstored.asyncio.worker.default_worker
        self.assertEqual(
            context,
            (
                libstored.asyncio.worker.default_worker.thread,
                libstored.asyncio.worker.default_worker.loop,
            ),
        )

    def test_sync_in_loop(self):
        # Still runs in the default worker thread and loop and not in the same loop.
        async def test():
            context = await async_f()
            self.assertIsNotNone(libstored.asyncio.worker.default_worker)
            assert libstored.asyncio.worker.default_worker
            self.assertEqual(
                context,
                (
                    libstored.asyncio.worker.default_worker.thread,
                    libstored.asyncio.worker.default_worker.loop,
                ),
            )

        asyncio.run(test())

    def test_thread_safe(self):
        # Runs in the default worker thread and loop.
        context = WorkF().f()
        self.assertIsNotNone(libstored.asyncio.worker.default_worker)
        assert libstored.asyncio.worker.default_worker
        self.assertEqual(
            context,
            (
                libstored.asyncio.worker.default_worker.thread,
                libstored.asyncio.worker.default_worker.loop,
            ),
        )

    def test_thread_safe_nonblock(self):
        # Runs in the default worker thread and loop.
        context = WorkF().f(block=False).result()
        self.assertIsNotNone(libstored.asyncio.worker.default_worker)
        assert libstored.asyncio.worker.default_worker
        self.assertEqual(
            context,
            (
                libstored.asyncio.worker.default_worker.thread,
                libstored.asyncio.worker.default_worker.loop,
            ),
        )

    def test_thread_safe2(self):
        # Runs in the default worker thread and loop.
        context = WorkF().f2()
        self.assertIsNotNone(libstored.asyncio.worker.default_worker)
        assert libstored.asyncio.worker.default_worker
        self.assertEqual(
            context,
            (
                libstored.asyncio.worker.default_worker.thread,
                libstored.asyncio.worker.default_worker.loop,
            ),
        )


def main():
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()


if __name__ == "__main__":
    main()
