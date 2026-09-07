# SPDX-FileCopyrightText: 2026 Demcon
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import asyncio

from . import protocol as lprot


class TcpLayer(lprot.ProtocolLayer):
    """A protocol layer that connects to a remote TCP stream."""

    name = "tcp"

    def __init__(
        self,
        connect: str | None = None,
        *args,
        host: str | None = None,
        server: str = "localhost",
        port: int = 0,
        connect_timeout_s: float | None = None,
        connect_retry_s: float = 0.1,
        **kwargs,
    ):
        super().__init__(*args, **kwargs)

        if host is not None:
            server = host
        if connect_timeout_s is not None and connect_timeout_s < 0:
            raise ValueError("connect_timeout_s must be non-negative or None")
        if connect_retry_s <= 0:
            raise ValueError("connect_retry_s must be positive")
        self._server, self._port = self.parse_connect(connect, server, port)
        self._connect_timeout_s = connect_timeout_s
        self._connect_retry_s = connect_retry_s
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None
        self._encode_buffer = bytearray()
        self._open = False
        self._connected = False
        self._tcp_task: asyncio.Task | None = asyncio.create_task(
            self._tcp_run(), name=self.__class__.__name__
        )

    @staticmethod
    def parse_connect(
        connect: str | None = None,
        default_server: str = "localhost",
        default_port: int = 0,
    ) -> tuple[str, int]:
        """Parse a ``host:port`` string using defaults for omitted fields."""
        server = default_server
        port = default_port

        if connect is not None:
            parts = connect.split(":", 1)
            if len(parts) == 2:
                if parts[0] != "":
                    server = parts[0]
                if parts[1] != "":
                    port = int(parts[1])
            else:
                try:
                    port = int(parts[0])
                except ValueError:
                    server = parts[0]

        return server, port

    @property
    def host(self) -> str:
        """Return the configured remote host."""
        return self._server

    @property
    def port(self) -> int:
        """Return the configured remote port."""
        return self._port

    @property
    def open(self) -> bool:
        """Return whether the TCP connection is open."""
        return self._open

    async def _write(self, data: bytes) -> None:
        writer = self._writer
        if not self._open or writer is None:
            raise RuntimeError("TCP connection closed")

        self.logger.debug("send %s", data)
        writer.write(data)
        await writer.drain()

    async def _open_connection(self) -> tuple[asyncio.StreamReader, asyncio.StreamWriter]:
        """Connect to the TCP server, retrying while it is unavailable."""
        loop = asyncio.get_running_loop()
        deadline = None
        if self._connect_timeout_s is not None:
            deadline = loop.time() + self._connect_timeout_s

        while True:
            try:
                return await asyncio.open_connection(self._server, self._port)
            except ConnectionRefusedError:
                if deadline is not None:
                    remaining = deadline - loop.time()
                    if remaining <= 0:
                        raise
                    retry_s = min(self._connect_retry_s, remaining)
                else:
                    retry_s = self._connect_retry_s

                self.logger.debug("TCP connection refused, retrying in %.3f seconds", retry_s)
                await asyncio.sleep(retry_s)

    async def _tcp_run(self) -> None:
        try:
            self._reader, self._writer = await self._open_connection()
            self._open = True
            await super().connected()

            if self._encode_buffer:
                data = bytes(self._encode_buffer)
                self._encode_buffer.clear()
                await self._write(data)

            reader = self._reader
            assert reader is not None
            while self._open:
                data = await reader.read(4096)
                if not data:
                    raise ConnectionError("TCP connection closed by remote host")
                self.logger.debug("received %s", data)
                await self.decode(data)
        except asyncio.CancelledError:
            pass
        except (ConnectionError, OSError) as e:
            if self._open:
                self.logger.info("TCP connection closed: %s", e)
            await self.async_except(e)
            raise
        except Exception as e:
            await self.async_except(e)
            raise
        finally:
            self._open = False
            if self.is_connected():
                await super().disconnected()

            writer = self._writer
            self._reader = None
            self._writer = None
            if writer is not None:
                writer.close()
                try:
                    await writer.wait_closed()
                except (ConnectionError, OSError):
                    pass

    async def encode(self, data: lprot.ProtocolLayer.Packet) -> None:
        if isinstance(data, str):
            data = data.encode()
        elif isinstance(data, memoryview):
            data = data.tobytes()

        if self._open and self._writer is not None:
            await self._write(bytes(data))
        elif not self._closed:
            self.logger.debug("buffering %s", data)
            self._encode_buffer.extend(data)

        await super().encode(data)

    async def close(self) -> None:
        self._open = False

        if self._writer is not None:
            self._writer.close()

        if self._tcp_task is not None:
            self._tcp_task.cancel()
            try:
                await self._tcp_task
            except BaseException:
                pass
            self._tcp_task = None

        await super().close()


lprot.register_layer_type(TcpLayer)
