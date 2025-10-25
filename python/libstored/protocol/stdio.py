# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import asyncio
import ctypes
import os
import signal
import subprocess
import sys
import typing

from . import protocol as lprot
from . import util as lprot_util

libc = None

# Helper to clean up the child when python crashes.
def set_pdeathsig_(libc, sig):
    if os.name == 'posix':
        os.setsid()
    libc.prctl(1, sig)

def set_pdeathsig(sig = signal.SIGTERM):
    global libc

    if libc is None:
        try:
            libc = ctypes.CDLL("libc.so.6")
        except:
            # Failed. Not on Linux?
            libc = False

    if libc == False:
        return lambda: None

    return lambda: set_pdeathsig_(libc, sig)



class StdinLayer(lprot.ProtocolLayer):
    '''A terminal layer that reads from stdin.'''

    name = 'stdin'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._stdin_reader = lprot_util.Reader(self._from_stdin, thread_name=self.__class__.__name__)
        self._reader_task : asyncio.Task | None = asyncio.create_task(self._reader_run())

    def _from_stdin(self) -> str:
        return sys.stdin.read(1)

    async def _reader_run(self) -> None:
        try:
            await self._stdin_reader.start()

            while True:
                data = await self._stdin_reader.read()
                await self.encode(data)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.exception(f'ConsoleLayer stdin reader error: {e}')
            raise
        finally:
            await self._stdin_reader.stop(False)

    async def close(self) -> None:
        if self._reader_task is not None:
            self._reader_task.cancel()
            try:
                await self._reader_task
            except asyncio.CancelledError:
                pass
            self._reader_task = None

        await super().close()

lprot.register_layer_type(StdinLayer)



class StdioLayer(lprot.ProtocolLayer):
    '''A protocol layer that runs a subprocess and connects to its stdin/stdout.'''

    name = 'stdio'

    def __init__(self, cmd, *args, **kwargs):
        super().__init__(*args)
        self._process = subprocess.Popen(
            args=cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=sys.stderr, text=False,
            preexec_fn = set_pdeathsig() if os.name == 'posix' else None,
            shell=not os.path.exists(cmd[0] if isinstance(cmd, list) else cmd),
            **kwargs)

        self._reader : lprot_util.Reader[bytes] = lprot_util.Reader(self._from_process, thread_name=f'{self.__class__.__name__}-reader')
        self._writer : lprot_util.Writer[bytes] = lprot_util.Writer(self._to_process, thread_name=f'{self.__class__.__name__}-writer')
        self._reader_task : asyncio.Task | None = asyncio.create_task(self._reader_run())
        self._writer_task : asyncio.Task | None = asyncio.create_task(self._writer.start())
        self._check_task : asyncio.Task | None = None

        self.set_terminate_callback(lambda _: None)

    def _from_process(self) -> bytes:
        if self._process.stdout is None or self._process.stdout.closed:
            raise RuntimeError('Process has no stdout anymore')

        return self._process.stdout.read1(4096) # type: ignore

    def _to_process(self, data : bytes) -> None:
        if self._process.stdin is None or self._process.stdin.closed:
            raise RuntimeError('Process has no stdin anymore')

        try:
            self._process.stdin.write(data) # type: ignore
            self._process.stdin.flush()
        except Exception as e:
            self.logger.info(f'Cannot write to stdin; shutdown: {e}')
            try:
                self._process.stdin.close()
            except BrokenPipeError:
                pass
            raise

    async def _reader_run(self) -> None:
        try:
            await self._reader.start()

            while True:
                data = await self._reader.read()
                await self.decode(data)
        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.logger.error(f'StdioLayer process reader error: {e}')
            await self.close()
        finally:
            await self._reader.stop()

    def set_terminate_callback(self, f : typing.Callable[[int], None | typing.Coroutine[None, None, None]]) -> None:
        '''
        Set a callback function that is called when the process terminates.
        The function is called with the exit code as argument.
        '''
        async def check_task() -> None:
            try:
                while True:
                    await asyncio.sleep(1)
                    ret = self._process.poll()
                    if ret is not None:
                        self.logger.error(f'Process terminated with exit code {ret}')
                        if asyncio.iscoroutinefunction(f):
                            await f(ret)
                        else:
                            f(ret)
                        return
            except asyncio.CancelledError:
                pass
            except Exception as e:
                self.logger.exception(f'StdioLayer process check error: {e}')
                raise

        if self._check_task is not None:
            self._check_task.cancel()

        self._check_task = asyncio.create_task(check_task())

    async def encode(self, data : lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.cast('B')

        await self._writer.write(data)
        await super().encode(data)

    async def close(self) -> None:
        self.logger.debug('Closing; terminate process')
        if os.name == 'posix':
            try:
                os.killpg(os.getpgid(self._process.pid), signal.SIGTERM)
            except ProcessLookupError:
                pass
        self._process.terminate()

        if self._reader_task is not None:
            self._reader_task.cancel()
            try:
                await self._reader_task
            except asyncio.CancelledError:
                pass
            self._reader_task = None

        if self._writer_task is not None:
            await self._writer.stop()
            self._writer_task = None

        if self._check_task is not None:
            self._check_task.cancel()
            try:
                await self._check_task
            except asyncio.CancelledError:
                pass
            self._check_task = None

        await super().close()

lprot.register_layer_type(StdioLayer)



__all__ = [
    'StdinLayer',
    'StdioLayer',
]
