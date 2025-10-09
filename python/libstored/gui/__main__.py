# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import argparse
import asyncio
import locale
import logging
import natsort
import re
import sys
import tkinter as tk
import tkinter.ttk as ttk
import typing

from ..version import __version__
from ..zmq_server import ZmqServer
from ..asyncio import ZmqClient, Object, Event
from ..asyncio.tk import AsyncApp, ZmqObjectEntry, AsyncWidget
from ..tk import Entry

def darken_color(color, factor=0.9):
    color = color.lstrip('#')
    lv = len(color)
    rgb = tuple(int(color[i:i+lv//3], 16) for i in range(0, lv, lv//3))
    darker = tuple(int(c * factor) for c in rgb)
    return '#{:02x}{:02x}{:02x}'.format(*darker)

class Style:
    root_width = 800
    root_height = 600
    grid_padding = 2
    separator_padding = grid_padding * 10

class ClientConnection(AsyncWidget, ttk.Frame):
    def __init__(self, app : AsyncApp, parent : ttk.Widget, client : ZmqClient, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._client = client

        self.columnconfigure(1, weight=1)

        self._host_label = ttk.Label(self, text='Host:')
        self._host_label.grid(row=0, column=0, sticky='e', padx=(0, Style.grid_padding), pady=Style.grid_padding)

        self._host = Entry(self, text=self.host)
        self._host.grid(row=0, column=1, sticky='we', padx=Style.grid_padding, pady=Style.grid_padding)

        self._port_label = ttk.Label(self, text='Port:')
        self._port_label.grid(row=0, column=2, sticky='e', padx=Style.grid_padding, pady=Style.grid_padding)

        self._port = Entry(self, text=str(self.port), hint=f'default: {ZmqServer.default_port}', validation=r'^[0-9]{0,5}$')
        self._port.grid(row=0, column=3, sticky='we', padx=Style.grid_padding, pady=Style.grid_padding)

        self._multi_var = tk.BooleanVar(value=client.multi)
        self._multi = ttk.Checkbutton(self, text='Multi', variable=self._multi_var)
        self._multi.grid(row=0, column=4, sticky='we', padx=Style.grid_padding, pady=Style.grid_padding)

        self._connect = ttk.Button(self, text='Connect')
        self._connect.grid(row=0, column=5, sticky='we', padx=(Style.grid_padding, 0), pady=Style.grid_padding)
        self._connect['command'] = self._on_connect_button

        self.connect(self.client.connecting, self._on_connected)
        self.connect(self.client.disconnected, self._on_disconnected)

        if self.client.is_connected():
            self._on_connected()
        else:
            self._on_disconnected()
            self._on_connect_button()

    @property
    def client(self) -> ZmqClient:
        return self._client

    @property
    def host(self) -> str:
        return self.client.host

    @property
    def port(self) -> int:
        return int(self.client.port)

    @AsyncApp.tk_func
    def _on_connected(self, *args, **kwargs):
        self._connect['text'] = 'Disconnect'
        self._host['state'] = 'disabled'
        self._port['state'] = 'disabled'
        self._multi['state'] = 'disabled'

    @AsyncApp.tk_func
    def _on_disconnected(self, *args, **kwargs):
        self._connect['text'] = 'Connect'
        self._host['state'] = 'normal'
        self._port['state'] = 'normal'
        self._multi['state'] = 'normal'
        self.app.root.title(f'libstored GUI')

    def _on_connect_button(self):
        host = self._host.get()
        if host == '':
            return

        try:
            port = int(self._port.get())
        except ValueError:
            return

        if port == 0:
            return

        self._do_connect_button(host, port, self._multi_var.get())

    @AsyncApp.worker_func
    async def _do_connect_button(self, host : str, port : int, multi : bool):
        if self.client.is_connected():
            await self.client.disconnect()
        else:
            try:
                await self.client.connect(host, port, multi)
                self._after_connection(await self.client.identification(), await self.client.version())
            except asyncio.CancelledError:
                raise
            except Exception as e:
                self.logger.warning('Connect failed: %s', e)

    @AsyncApp.tk_func
    def _after_connection(self, identification : str, version : str):
        self.logger.info(f'Connected to {identification} ({version})')
        self.app.root.title(f'libstored GUI - {identification} ({version})')



class ObjectRow(AsyncWidget, ttk.Frame):
    def __init__(self, app : GUIClient, parent : ttk.Widget, obj : Object, style : str | None=None, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._obj = obj

        self.columnconfigure(0, weight=1)

        self._label = ttk.Label(self, text=obj.name)
        self._label.grid(row=0, column=0, sticky='w', padx=(0, Style.grid_padding), pady=Style.grid_padding)

        self._format = ttk.Combobox(self, values=obj.formats(), width=10, state='readonly')
        self._format.set(obj.format)
        self._format.bind('<<ComboboxSelected>>', lambda e: obj.format.set(self._format.get()))
        self._format.grid(row=0, column=1, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)
        app.connect(obj.format, self._format.set)

        self._type = ttk.Label(self, text=obj.type_name, width=10, anchor='e')
        self._type.grid(row=0, column=2, sticky='e', padx=Style.grid_padding, pady=Style.grid_padding)

        self._value = ZmqObjectEntry(app, self, obj)
        self._value.grid(row=0, column=3, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)

        self._poll_var = tk.BooleanVar(value=obj.polling.value is not None)
        app.connect(obj.polling, self._on_poll_obj_change)
        self._poll = ttk.Checkbutton(self, variable=self._poll_var, command=self._on_poll_check_change)
        self._poll.grid(row=0, column=4, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)

        self._refresh = ttk.Button(self, text='Refresh', command=self._value.refresh)
        self._refresh.grid(row=0, column=5, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)

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
        self._format['style'] = f'{style}TCombobox'
        self._type['style'] = f'{style}TLabel'
        self._value['style'] = f'{style}TEntry'
        self._poll['style'] = f'{style}TCheckbutton'
        self._refresh['style'] = f'{style}TButton'

    @AsyncApp.tk_func
    def _on_poll_obj_change(self, x):
        self._poll_var.set(x is not None)

    def _on_poll_check_change(self):
        self._obj.polling.value = typing.cast(GUIClient, self.app).default_poll() if self._poll_var.get() else None



class ObjectList(ttk.Frame):
    def __init__(self, app : GUIClient, parent : ttk.Widget, objects : list[Object]=[], filter : typing.Callable[[Object], bool] | None=None, *args, **kwargs):
        super().__init__(parent, *args, **kwargs)
        self._app = app
        self._objects : list[ObjectRow] = []
        self._filter = filter
        self.columnconfigure(0, weight=1)
        self.filtered = Event('filtered')
        self.changed = Event('changed')
        self.set_objects(objects)

    @property
    def objects(self) -> list[ObjectRow]:
        return self._objects

    def set_objects(self, objects : list[Object]):
        for o in self._objects:
            o.destroy()
        self._objects = []

        for o in natsort.natsorted(objects, key=lambda o: o.name, alg=natsort.ns.IGNORECASE):
            object_row = ObjectRow(self._app, self, o)
            self._objects.append(object_row)

        self.changed.trigger()
        self.filter()

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
                o.grid(column=0, row=row, sticky='nsew')
                o.style('Even' if row % 2 == 0 else 'Odd')
                row += 1
            else:
                o.grid_forget()

        if row == 0:
            self.configure(height=1)

        self.filtered.trigger()

class ScrollableFrame(ttk.Frame):
    def __init__(self, parent : ttk.Widget, *args, **kwargs):
        super().__init__(parent, *args, **kwargs)

        self._canvas = tk.Canvas(self)
        self._scrollbar = ttk.Scrollbar(self, orient='vertical', command=self._canvas.yview)
        self._scrollbar.grid(column=1, row=0, sticky='ns', padx=(Style.grid_padding, 0))
        self._content = ttk.Frame(self._canvas)

        self._content.bind("<Configure>", self._update_scrollregion)
        self._canvas.create_window((0, 0), window=self._content, anchor="nw", tags="inner_frame")
        self._canvas.configure(yscrollcommand=self._scrollbar.set)

        s = ttk.Style()
        self._canvas.configure(background=s.lookup('TFrame', 'background'), highlightthickness=0)

        self._canvas.grid(column=0, row=0, sticky='nsew', padx=(0, Style.grid_padding))
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        self._canvas.bind("<Configure>", self._fit_content_to_canvas)
        self.after_idle(self.bind_scroll)

    def _update_scrollregion(self, event):
        self.update_idletasks()
        self._canvas.configure(scrollregion=self._canvas.bbox("all"))

    def _fit_content_to_canvas(self, event):
        self._canvas.itemconfigure("inner_frame", width=event.width - 1)

    def _on_mousewheel(self, event):
        self._canvas.yview_scroll(int(-1*(event.delta/120)), "units")
        return "break"

    def _on_linux_scroll(self, event):
        if event.num == 4:
            self._canvas.yview_scroll(-1, "units")
        elif event.num == 5:
            self._canvas.yview_scroll(1, "units")
        return "break"

    @staticmethod
    def _children(widget : tk.Widget) -> set[tk.Widget]:
        return set(widget.winfo_children()).union(*(ScrollableFrame._children(w) for w in widget.winfo_children()))

    def bind_scroll(self):
        # find all children recursively and bind to mousewheel
        for child in self._children(self._canvas):
            child.bind("<MouseWheel>", self._on_mousewheel)     # Windows and macOS
            child.bind("<Button-4>", self._on_linux_scroll)     # Linux scroll up
            child.bind("<Button-5>", self._on_linux_scroll)     # Linux scroll down

    def updated_content(self):
        self._update_scrollregion(None)

    @property
    def content(self) -> ttk.Frame:
        return self._content

    @property
    def canvas(self) -> tk.Canvas:
        return self._canvas



class FilterEntry(Entry):
    '''
    Regex filter on a given ObjectList.
    '''

    def __init__(self, parent : ttk.Widget, object_list : ObjectList, *args, **kwargs):
        super().__init__(parent, hint='enter regex filter', *args, **kwargs)
        self._object_list = object_list
        self._var.trace_add('write', self._on_change)

    def _on_change(self, *args):
        text = self.text

        if text == '':
            self._object_list.filter(None)
        else:
            try:
                regex = re.compile(text, re.IGNORECASE)
                self['foreground'] = 'black'
            except re.error:
                self['foreground'] = 'red'
                return

            def f(o : Object) -> bool:
                return regex.search(o.name) is not None

            self._object_list.filter(f)


class ManualCommand(AsyncWidget, ttk.Frame):
    def __init__(self, app : GUIClient, parent : ttk.Widget, client : ZmqClient, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._client = client
        self._empty = True

        self._req = Entry(self, hint='enter command')
        self._req.grid(column=0, row=0, sticky='we')
        self.columnconfigure(0, weight=1)

        self._req.bind('<Return>', self._on_enter)

        def select_all(event):
            if isinstance(event.widget, ttk.Entry):
                event.widget.select_range(0, 'end')
                event.widget.icursor('end')
                return 'break'
            elif isinstance(event.widget, tk.Text):
                event.widget.tag_add('sel', '1.0', 'end')
                event.widget.mark_set('insert', 'end')
                return 'break'

        self._req.bind('<Control-a>', select_all)
        self._req.bind('<Control-A>', select_all)

        self._rep = tk.Text(self, height=5)
        self._rep.configure(state='disabled')
        self._rep.bind('<Control-a>', select_all)
        self._rep.bind('<Control-A>', select_all)

        self.bind('<FocusIn>', self._focus_in)
        self.bind('<FocusOut>', self._focus_out)

        self._focus_out()

    def _on_enter(self, event):
        self._do_command(self._req.text)
        return 'break'

    @AsyncApp.worker_func
    async def _do_command(self, command : str):
        if not self._client.is_connected():
            return
        if command == '':
            return

        try:
            self._response(await self._client.req(command))
        except Exception as e:
            self.logger.exception(f'Manual command: {e}')

    @AsyncApp.tk_func
    def _response(self, response : str):
        self._rep.configure(state='normal')
        self._rep.delete('1.0', 'end')
        self._rep.insert('end', response)
        self._rep.see('1.0')
        self._rep.configure(state='disabled')

    def _focus_in(self, *args):
        self._rep.grid(column=0, row=1, sticky='nsew', pady=(Style.grid_padding, 0))
        self.rowconfigure(1, weight=1)

    def _focus_out(self, *args):
        self._rep.grid_forget()
        self.rowconfigure(1, weight=0)



class GUIClient(AsyncApp):
    def __init__(self, client : ZmqClient, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._client = client
        self._client_connections = set()

        self.root.title(f'libstored GUI')

        w = Style.root_width
        h = Style.root_height

        ws = self.root.winfo_screenwidth() # width of the screen
        hs = self.root.winfo_screenheight() # height of the screen

        x = (ws/2) - (w/2)
        y = (hs/2) - (h/2)

        self.root.geometry('%dx%d+%d+%d' % (w, h, x, y))

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

        connect = ClientConnection(self, self, self.client)
        connect.grid(column=0, row=0, sticky='we', pady=Style.grid_padding, columnspan=3)

        scrollable_objects = ScrollableFrame(self)
        self._objects = ObjectList(self, scrollable_objects.content)
        self._objects.pack(fill='both', expand=True)
        scrollable_objects.grid(column=0, row=2, sticky='nsew', columnspan=3)
        self.connect(self._objects.changed, scrollable_objects.bind_scroll)
        self.connect(self._objects.filtered, scrollable_objects.updated_content)

        filter = FilterEntry(self, self._objects)
        filter.grid(column=0, row=1, sticky='nswe', padx=(0, Style.grid_padding), pady=Style.grid_padding)

        self._default_poll = Entry(self, text='1', hint='poll (s)', width=7, justify='right')
        self._default_poll.grid(row=1, column=1, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)

        refresh_all = ttk.Button(self, text='Refresh all', command=self._refresh_all)
        refresh_all.grid(row=1, column=2, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)

        self._scrollable_polled = ScrollableFrame(self)
        self._polled_objects = ObjectList(self, self._scrollable_polled.content, filter=lambda o: o.polling.value is not None)
        self._polled_objects.pack(fill='both', expand=True)
        self.connect(self._polled_objects.changed, self._scrollable_polled.bind_scroll)
        self.connect(self._polled_objects.filtered, self._scrollable_polled.updated_content)

        self._manual = ManualCommand(self, self, self.client)
        self._manual.grid(column=0, row=4, sticky='nsew', columnspan=3, pady=(Style.grid_padding, 0))

        self.columnconfigure(0, weight=1)
        self.rowconfigure(2, weight=2)
        self.rowconfigure(4, weight=0)

        self.bind("<Configure>", self._resize_polled_objects)
        self.connect(self._polled_objects.filtered, self._resize_polled_objects)
        self._resize_polled_objects()

        self.connect(self.client.connected, self._on_connected)
        self.connect(self.client.disconnected, self._on_disconnected)
        if self.client.is_connected():
            self._on_connected()
        else:
            self._on_disconnected()

    @property
    def client(self):
        return self._client

    @AsyncApp.tk_func
    def _on_connected(self, *args, **kwargs):
        self._polled_objects.set_objects(self.client.objects)
        self._objects.set_objects(self.client.objects)
        for o in self.client.objects:
            self._client_connections.add(self.connect(o.polling, self._filter_polled))
        self._resize_polled_objects()

    @AsyncApp.tk_func
    def _on_disconnected(self, *args, **kwargs):
        for c in self._client_connections:
            self.disconnect(c)
        self._client_connections.clear()
        self._polled_objects.set_objects([])
        self._objects.set_objects([])

    @AsyncApp.tk_func
    def cleanup(self):
        self.disconnect_all()
        self.logger.debug('Close client')
        self._close_async()
        super().cleanup()

    @AsyncApp.worker_func
    async def _close_async(self):
        self.logger.debug('Closing client')
        await self.client.close()

    @AsyncApp.worker_func
    async def _refresh_all(self):
        await asyncio.gather(*(o.read(acquire_alias=False) for o in self.client.objects))

    @AsyncApp.tk_func
    def _filter_polled(self, interval_s):
        self._polled_objects.filter()

    @AsyncApp.tk_func
    def _resize_polled_objects(self, *args):
        self._scrollable_polled.update_idletasks()
        height = 0
        for o in self._polled_objects.objects:
            if o.obj.polling.value is not None:
                height += o.winfo_reqheight()

        if height > 0:
            self._scrollable_polled.grid(column=0, row=3, sticky='nsew', columnspan=3, pady=(Style.separator_padding, 0))
            self._scrollable_polled.update_idletasks()
            total_height = self.winfo_height()
            if total_height > 0:
                max_height = total_height // 3
                if height > max_height:
                    height = max_height
            print(f'Setting polled height to {height}')
            self._scrollable_polled.canvas.config(height=height)
        else:
            self._scrollable_polled.grid_forget()

    @AsyncApp.tk_func
    def default_poll(self) -> float:
        try:
            v = locale.atof(self._default_poll.get())
            if v <= 0:
                v = 1.0
        except ValueError:
            v = 1.0

        self._default_poll.delete(0, 'end')
        self._default_poll.insert(0, locale.format_string('%g', v, grouping=True))
        return v



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

    client = ZmqClient(host=args.server, port=args.port, multi=args.multi)
    GUIClient.run(worker=client.worker, client=client)

if __name__ == '__main__':
    main()
