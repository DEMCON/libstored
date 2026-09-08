#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Demcon
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import asyncio
import concurrent.futures
import logging

from ..version import __version__
from .. import protocol as lprot
from ..asyncio.worker import AsyncioWorker, run_sync


def build_stack(
    host: str,
    port: int,
    *,
    listen: str = "*",
    zmq_port: int = lprot.default_port,
    stack: str = "cobs",
) -> lprot.ProtocolStack:
    """Build a ZMQ bridge stack for a remote TCP debugger."""
    result = lprot.build_stack(
        ",".join(
            [
                f"zmq={listen}:{zmq_port}",
                "reqrepcheck",
                stack,
                f"tcp={host}:{port}",
            ]
        )
    )
    assert isinstance(result, lprot.ProtocolStack)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(
        description="TCP wrapper to ZMQ server",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        prog=__package__,
    )
    parser.add_argument("-V", "--version", action="version", version=__version__)
    parser.add_argument(
        "-l", "--listen", dest="zmqlisten", type=str, default="*", help="ZMQ listen address"
    )
    parser.add_argument(
        "-p", "--port", dest="zmqport", type=int, default=lprot.default_port, help="ZMQ port"
    )
    parser.add_argument(
        "-S",
        "--stack",
        dest="stack",
        type=str,
        default="cobs",
        help="protocol stack between ZMQ and TCP",
    )
    parser.add_argument(
        "-v", "--verbose", dest="verbose", default=0, help="Enable verbose output", action="count"
    )
    parser.add_argument("host", help="TCP host")
    parser.add_argument("tcpport", type=int, help="TCP port")

    args = parser.parse_args()

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARN)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    lprot.set_infinite_stdout()

    with AsyncioWorker() as worker:
        try:

            @run_sync
            async def async_main(args: argparse.Namespace) -> None:
                stack = build_stack(
                    args.host,
                    args.tcpport,
                    listen=args.zmqlisten,
                    zmq_port=args.zmqport,
                    stack=args.stack,
                )
                tcp_layer = next(layer for layer in stack if layer.name == "tcp")

                async def handle_tcp_exception(error: BaseException) -> None:
                    if isinstance(error, (ConnectionError, OSError)):
                        worker.cancel()
                        return
                    await tcp_layer.default_async_except_hook(error)

                tcp_layer.async_except_hook = handle_tcp_exception
                try:
                    while True:
                        await asyncio.sleep(3600)
                finally:
                    await stack.close()

            async_main(args)
        except KeyboardInterrupt:
            worker.cancel()
        except concurrent.futures.CancelledError:
            pass


if __name__ == "__main__":
    main()
