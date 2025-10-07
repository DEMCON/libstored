# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import argparse
import asyncio
import logging
import natsort
import re
import sys
import tkinter as tk
import tkinter.ttk as ttk
import typing

from ..version import __version__
from ..zmq_server import ZmqServer
from ..asyncio import ZmqClient, Object
from ..asyncio.tk import AsyncApp, ZmqObjectEntry, AsyncWidget

def darken_color(color, factor=0.9):
    color = color.lstrip('#')
    lv = len(color)
    rgb = tuple(int(color[i:i+lv//3], 16) for i in range(0, lv, lv//3))
    darker = tuple(int(c * factor) for c in rgb)
    return '#{:02x}{:02x}{:02x}'.format(*darker)

class Style:
    grid_padding = 2

class ObjectRow(AsyncWidget, ttk.Frame):
    def __init__(self, app : AsyncApp, parent : ttk.Widget, obj : Object, style : str | None=None, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._obj = obj

        self.columnconfigure(0, weight=1)

        self._label = ttk.Label(self, text=obj.name)
        self._label.grid(row=0, column=0, sticky='w', padx=(0, Style.grid_padding), pady=Style.grid_padding)

        self._value = ZmqObjectEntry(app, self, obj)
        self._value.grid(row=0, column=1, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)

        self._poll_var = tk.BooleanVar(value=obj.polling.value is not None)
        app.connect(obj.polling, self._on_poll_obj_change)
        self._poll = ttk.Checkbutton(self, variable=self._poll_var, command=self._on_poll_check_change)
        self._poll.grid(row=0, column=2, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)

        self._refresh = ttk.Button(self, text='Refresh', command=self._value.refresh)
        self._refresh.grid(row=0, column=3, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)

        if style is not None:
            self.style(style)

    @property
    def obj(self):
        return self._obj

    def style(self, style : str):
        if style != '':
            style += '.'

        self['style'] = f'{style}TFrame'
        self._label['style'] = f'{style}TLabel'
        self._value['style'] = f'{style}TEntry'
        self._poll['style'] = f'{style}TCheckbutton'
        self._refresh['style'] = f'{style}TButton'

    @AsyncApp.tk_func
    def _on_poll_obj_change(self, x):
        self._poll_var.set(x is not None)

    def _on_poll_check_change(self):
        self._obj.polling.value = 1.0 if self._poll_var.get() else None



class ObjectList(ttk.Frame):
    def __init__(self, app : AsyncApp, parent : ttk.Widget, objects : list[Object], filter : typing.Callable[[Object], bool] | None=None, *args, **kwargs):
        super().__init__(parent, *args, **kwargs)
        self._app = app
        self._objects : list[ObjectRow] = []
        self._filter = filter

        self.columnconfigure(0, weight=1)

        for o in natsort.natsorted(objects, key=lambda o: o.name, alg=natsort.ns.IGNORECASE):
            object_row = ObjectRow(app, self, o)
            self._objects.append(object_row)

        self.filter()

    @property
    def objects(self):
        return self._objects

    def filter(self, f : typing.Callable[[Object], bool] | None | bool=True):
        if f is False:
            return
        elif f is True:
            # Use existing filter
            f = self._filter
        else:
            # Save filter
            self._filter = f

        if f is None:
            # No filter
            f = lambda o: True

        row = 0
        for o in self._objects:
            if f(o.obj):
                o.grid(column=0, row=row, columnspan=3, sticky='nsew')
                o.style('Even' if row % 2 == 0 else 'Odd')
                row += 1
            else:
                o.grid_forget()

class ScrollableFrame(ttk.Frame):
    def __init__(self, parent : ttk.Widget, *args, **kwargs):
        super().__init__(parent, *args, **kwargs)

        self._canvas = tk.Canvas(self)
        self._scrollbar = ttk.Scrollbar(self, orient='vertical', command=self._canvas.yview)
        self._content = ttk.Frame(self._canvas)

        self._content.bind("<Configure>", self._update_scrollregion)
        self._canvas.create_window((0, 0), window=self._content, anchor="nw", tags="inner_frame")
        self._canvas.configure(yscrollcommand=self._scrollbar.set)

        s = ttk.Style()
        self._canvas.configure(background=s.lookup('TFrame', 'background'), highlightthickness=0)

        self._canvas.grid(column=0, row=0, sticky='nsew', padx=(0, Style.grid_padding))
        self._scrollbar.grid(column=1, row=0, sticky='ns', padx=(Style.grid_padding, 0))
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        self._canvas.bind("<Configure>", self._fit_content_to_canvas)
        self._canvas.bind_all("<MouseWheel>", self._on_mousewheel)      # Windows and macOS
        self._canvas.bind_all("<Button-4>", self._on_linux_scroll)      # Linux scroll up
        self._canvas.bind_all("<Button-5>", self._on_linux_scroll)      # Linux scroll down

    def _update_scrollregion(self, event):
        self._canvas.configure(scrollregion=self._canvas.bbox("all"))

    def _fit_content_to_canvas(self, event):
        self._canvas.itemconfigure("inner_frame", width=event.width - 1)

    def _on_mousewheel(self, event):
        self._canvas.yview_scroll(int(-1*(event.delta/120)), "units")

    def _on_linux_scroll(self, event):
        if event.num == 4:
            self._canvas.yview_scroll(-1, "units")
        elif event.num == 5:
            self._canvas.yview_scroll(1, "units")

    @property
    def content(self) -> ttk.Frame:
        return self._content

class FilterEntry(ttk.Entry):
    '''
    Regex filter on a given ObjectList.
    '''

    def __init__(self, parent : ttk.Widget, object_list : ObjectList, *args, **kwargs):
        super().__init__(parent, *args, **kwargs)
        self._object_list = object_list
        self._var = tk.StringVar()
        self['textvariable'] = self._var
        self._var.trace_add('write', self._on_change)
        self._empty = True

        self.bind('<FocusIn>', self._focus_in)
        self.bind('<FocusOut>', self._focus_out)

        def select_all(event):
            event.widget.select_range(0, 'end')
            event.widget.icursor('end')
            return 'break'

        self.bind('<Control-a>', select_all)
        self.bind('<Control-A>', select_all)

        self._focus_out()

    def _on_change(self, *args):
        if self._empty:
            return

        if self._var.get() == '':
            self._object_list.filter(None)
        else:
            try:
                regex = re.compile(self._var.get(), re.IGNORECASE)
                self['foreground'] = 'black'
            except re.error:
                self['foreground'] = 'red'
                return

            def f(o : Object) -> bool:
                return regex.search(o.name) is not None

            self._object_list.filter(f)

    def _focus_in(self, *args):
        if self._empty:
            self._var.set('')
            self._empty = False

    def _focus_out(self, *args):
        if self._var.get() == '':
            self._empty = True
            self._var.set('Filter (regex)')
            self['foreground'] = 'grey'



class GUIClient(AsyncApp):
    def __init__(self, client : ZmqClient, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._client = client

        self.root.title(f'libstored GUI')

        self.connect(self.client.disconnected, self._on_disconnected)
        identification, version = self._wait_connected().result()

        self.root.title(f'libstored GUI - {identification} ({version})')
        self.root.geometry("800x600")

        s = ttk.Style()
        s.theme_use('clam')
        s.configure('TEntry', padding=(5, 4))
        odd_bg = darken_color(s.lookup('TFrame', 'background'))
        s.configure('Odd.TFrame', background=odd_bg)
        s.configure('Odd.TLabel', background=odd_bg)
        s.configure('Odd.TCheckbutton', background=odd_bg, focuscolor=odd_bg, activebackground=odd_bg)
        s.map("Odd.TCheckbutton", background=[("active", odd_bg)])
        s.configure('Odd.TButton', width=8, padding=3)

        even_bg = s.lookup('TFrame', 'background')
        s.configure('Even.TFrame')
        s.configure('Even.TLabel', background=even_bg)
        s.configure('Even.TCheckbutton', background=even_bg, focuscolor=even_bg, activebackground=even_bg)
        s.map("Even.TCheckbutton", background=[("active", even_bg)])
        s.configure('Even.TButton', width=8, padding=3)

        scrollable_objects = ScrollableFrame(self)
        objects = ObjectList(self, scrollable_objects.content, self.client.objects)
        objects.pack(fill='both', expand=True)
        scrollable_objects.grid(column=0, row=1, sticky='nsew', columnspan=2)

        filter = FilterEntry(self, objects)
        filter.grid(column=0, row=0, sticky='we', pady=Style.grid_padding)

        refresh_all = ttk.Button(self, text='Refresh all', command=self._refresh_all)
        refresh_all.grid(row=0, column=1, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)

        self._polled_objects = ObjectList(self, self, self.client.objects, lambda o: o.polling.value is not None)
        self._polled_objects.grid(column=0, row=2, sticky='nsew', columnspan=2)
        for o in self.client.objects:
            self.connect(o.polling, self._filter_polled)

        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=2)
        self.rowconfigure(2, weight=1)

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
    async def _wait_connected(self) -> tuple[str, str]:
        if not self.client.is_connected:
            await self.client.connected.wait()

        return (await self.client.identification(), await self.client.version())

    @AsyncApp.worker_func
    async def _refresh_all(self):
        await asyncio.gather(*(o.read(acquire_alias=False) for o in self.client.objects))

    @AsyncApp.tk_func
    def _filter_polled(self, interval_s):
        self._polled_objects.filter()



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
        GUIClient.run(worker=client.worker, client=client)

if __name__ == '__main__':
    main()
