# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import logging
import sys
import tkinter as tk
import tkinter.ttk as ttk

from ..version import __version__
from ..zmq_server import ZmqServer
from ..asyncio import ZmqClient, Object
from ..asyncio.tk import AsyncApp, ZmqObjectEntry

def darken_color(color, factor=0.9):
    color = color.lstrip('#')
    lv = len(color)
    rgb = tuple(int(color[i:i+lv//3], 16) for i in range(0, lv, lv//3))
    darker = tuple(int(c * factor) for c in rgb)
    return '#{:02x}{:02x}{:02x}'.format(*darker)

class GUIClient(AsyncApp):
    def __init__(self, client : ZmqClient, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._client = client

        self.connect(self.client.disconnected, self._on_disconnected)
        self._wait_connected()

        odd = ttk.Style()
        odd.configure('Odd.TFrame', background=darken_color(odd.lookup('TFrame', 'background')))
        odd.configure('Odd.TLabel', background=odd.lookup('Odd.TFrame', 'background'))

        even = ttk.Style()
        even.configure('Even.TFrame')
        even.configure('Even.TLabel', background=even.lookup('Even.TFrame', 'background'))

        row = 0
        for o in self.client.objects:
            self.logger.info(f"Found object: {o}")

            object_row = ttk.Frame(self)
            label = ttk.Label(self, text=o.name)

            if row % 2 == 0:
                object_row['style'] = 'Even.TFrame'
                label['style'] = 'Even.TLabel'
            else:
                object_row['style'] = 'Odd.TFrame'
                label['style'] = 'Odd.TLabel'

            object_row.grid(column=0, row=row, columnspan=3, sticky='nsew')
            label.grid(row=row, column=0, sticky='w')

            value = ZmqObjectEntry(self, self, o)
            value.grid(row=row, column=1, sticky='we')

            refresh = ttk.Button(self, text='Refresh', command=value.refresh)
            refresh.grid(row=row, column=2)
            row += 1

        self.columnconfigure(0, weight=1)

    @property
    def client(self):
        return self._client

    @AsyncApp.tk_func
    def _on_disconnected(self, *args, **kwargs):
        self.quit()

    @AsyncApp.tk_func
    def cleanup(self):
        self.logger.debug('Close client')
        self._close_async()
        super().cleanup()

    @AsyncApp.worker_func
    async def _close_async(self):
        self.logger.debug('Closing client')
        await self.client.close()

    @AsyncApp.worker_func
    async def _wait_connected(self):
        if not self.client.is_connected:
            await self.client.connected.wait()



def main():
    parser = argparse.ArgumentParser(prog=sys.modules[__name__].__package__, description='ZMQ GUI client', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-V', action='version', version=__version__)
    parser.add_argument('-s', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', dest='port', type=int, default=ZmqServer.default_port, help='port')
    parser.add_argument('-v', dest='verbose', default=0, help='Enable verbose output', action='count')
    parser.add_argument('-m', dest='multi', default=False,
        help='Enable multi-mode; allow multiple simultaneous connections to the same target, ' +
            'but it is less efficient.', action='store_true')

    args = parser.parse_args()

    if args.verbose == 0:
        logging.basicConfig(level=logging.WARNING)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.DEBUG)

    with ZmqClient(host=args.server, port=args.port, multi=args.multi) as client:
        with GUIClient.create(worker=client.worker, client=client) as app:
            app.atk.wait()

if __name__ == '__main__':
    main()
