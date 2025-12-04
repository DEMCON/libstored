# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import argparse
import asyncio
import locale
import logging
import os
import natsort
import re
import time
import tkinter as tk
import tkinter.ttk as ttk
import typing

try:
    import matplotlib.pyplot as plt
except ImportError:
    plt = None

from .. import __version__
from ..asyncio import zmq as laio_zmq
from ..asyncio import event as laio_event
from ..asyncio import tk as laio_tk
from ..asyncio import csv as laio_csv
from .. import tk as ltk
from .. import exceptions as lexc
from .. import protocol as lprot



#####################################################################
# Style
#

def darken_color(color, factor=0.9):
    color = color.lstrip('#')
    lv = len(color)
    rgb = tuple(int(color[i:i+lv//3], 16) for i in range(0, lv, lv//3))
    darker = tuple(int(c * factor) for c in rgb)
    return '#{:02x}{:02x}{:02x}'.format(*darker)

class Style:
    root_width = 800
    root_height = 600
    window_padding = 2
    grid_padding = 2
    separator_padding = grid_padding * 10



#####################################################################
# Plotter
#

class PlotData:
    WINDOW_s = 30

    def __init__(self):
        self.t : list[float] = []
        self.values : list[float] = []
        self.connection : typing.Hashable | None = None
        self.line : plt.Line2D | None = None # type: ignore

    def append(self, value : float, t=time.time()):
        self.t.append(t)
        self.values.append(value)

    def cleanup(self):
        if self.t == []:
            return

        drop = 0
        threshold = self.t[-1] - self.WINDOW_s

        for t in self.t:
            if t < threshold:
                drop += 1
            else:
                break

        if drop == 0:
            return

        self.t = self.t[drop:]
        self.values = self.values[drop:]


plotter : Plotter | None = None

class Plotter(laio_tk.Work):
    available : bool = plt is not None
    title : str | None = None

    @classmethod
    def instance(cls) -> Plotter:
        '''
        Get the singleton instance of the Plotter.
        '''

        if not cls.available:
            raise RuntimeError("Matplotlib is not available")

        global plotter

        if plotter is None:
            raise RuntimeError("Construct Plotter first")

        return plotter

    def __init__(self, *args, **kwargs):
        assert self.available, "Matplotlib is not available"

        super().__init__(*args, **kwargs)
        self._data : dict[laio_zmq.Object, PlotData] = {}
        self._fig : plt.Figure | None = None # type: ignore
        self._ax : plt.Axes | None = None # type: ignore
        self._ready = False
        self._changed : set[PlotData] = set()
        self._paused = False

        self._timer : typing.Any = None

        self.plotting = laio_event.ValueWrapper(bool, self._plotting_get, event_name='plotting')
        self.paused = laio_event.ValueWrapper(bool, self._paused_get, self._paused_set, event_name='paused')
        self.closed = laio_event.Event('closed')

    @laio_tk.Work.tk_func
    def start(self):
        '''
        Start the plotter.
        '''

        self._timer_start()

    def _timer_start(self):
        if self._timer is not None:
            return

        self._update_plot()

    def _timer_stop(self):
        if self._timer is None:
            return

        self.atk.root.after_cancel(self._timer)
        self._timer = None

    @laio_tk.Work.tk_func
    def stop(self):
        '''
        Stop the plotter.
        '''

        self._timer_stop()

        for o in list(self._data.keys()):
            self.remove(o)

        if not self._ready:
            return

        self.logger.debug('Closing plotter')

        self._ready = False
        assert plt is not None
        plt.close()

        self._fig = None
        self._ax = None

        self.closed.trigger()
        self.plotting.trigger()

    def __del__(self):
        self.stop()

        global plotter
        if plotter is self:
            plotter = None

    @laio_tk.Work.tk_func
    def add(self, o : laio_zmq.Object):
        '''
        Add a libstored.asyncio.Object to the plotter.
        '''

        if o in self._data:
            return

        if not o.is_fixed():
            return

        self.logger.debug(f'Plot {o.name}')

        if self._fig is None:
            assert plt
            self._fig, self._ax = plt.subplots()

        assert self._ax is not None

        data = PlotData()
        self._data[o] = data

        value = o.value
        if value is not None:
            self._data[o].append(value, o.t.value)

        data.line = self._ax.plot([], [], label=o.name)[0]
        data.connection = self.connect(o.t, lambda: self._update(o, o.value, o.t.value))

        self._update_legend()

        self.show()

        if not self._paused:
            self._timer_start()
        if len(self._data) == 1:
            self.plotting.trigger()

    @laio_tk.Work.tk_func
    def _update(self, o : laio_zmq.Object, value : typing.Any, t : float=time.time()):
        if o not in self._data:
            return
        if value is None:
            return

        data = self._data[o]
        data.append(value, t)
        self._changed.add(data)
        self._timer_start()

    def _update_plot(self):
        self.logger.debug('Update plot')

        if len(self._changed) == 0 or self._paused:
            self._timer = None
            return

        for data in self._changed:
            data.cleanup()
            assert data.line is not None
            data.line.set_data(data.t, data.values)

        self._changed.clear()

        assert self._ax is not None
        self._ax.relim()
        self._ax.autoscale()

        assert self._fig is not None
        self._fig.canvas.draw()

        self._timer = self.atk.root.after(100, self._update_plot)
        assert self._timer is not None

    @laio_tk.Work.tk_func
    def remove(self, o : laio_zmq.Object):
        '''
        Remove a libstored.asyncio.Object from the plotter.
        '''

        if o not in self._data:
            return

        self.logger.debug(f'Remove plot {o.name}')

        data = self._data[o]
        del self._data[o]
        self.disconnect(data.connection)

        assert self._ax is not None
        assert data.line is not None

        try:
            self._ax.lines.remove(data.line)
        except AttributeError:
            # This is a newer MPL, apparently.
            data.line.remove()

        del data

        self._update_legend()

        if len(self._data) == 0:
            self.stop()

    @laio_tk.Work.tk_func
    def show(self):
        '''
        Show the plotter window.
        '''

        if self._fig is None or self._ax is None:
            return

        if not self._ready:
            if self.title is not None:
                self._ax.set_title(self.title)
                self._fig.canvas.manager.set_window_title(f'libstored GUI plots: {self.title}')
            else:
                self._fig.canvas.manager.set_window_title(f'libstored GUI plots')

            self._ax.grid(True)
            self._ax.set_xlabel('t (s)')
            self._update_legend()

            self._fig.canvas.mpl_connect('close_event', lambda _: self.stop())

            assert plt is not None
            plt.show(block=False)
            self._ready = True
        else:
            self._fig.show()

    def _update_legend(self):
        if len(self._data) == 0:
            return
        assert self._ax is not None
        self._ax.legend().set_draggable(True)

    @laio_tk.Work.tk_func
    def pause(self, paused : bool=True):
        '''
        Pause or resume the plotter.
        '''

        if self._paused == paused:
            return

        self._paused = paused
        if paused:
            self._timer_stop()
        else:
            self._timer_start()

        self.paused.trigger()

    @laio_tk.Work.tk_func
    def resume(self):
        self.pause(False)

    @laio_tk.Work.tk_func
    def toggle_pause(self):
        self.pause(not self._paused)

    def _paused_get(self) -> bool:
        return self._paused

    def _paused_set(self, x : bool):
        self.pause(x)

    def _plotting_get(self):
        return len(self._data) > 0



#####################################################################
# GUI elements
#

class ClientConnection(laio_tk.AsyncWidget, ttk.Frame):
    def __init__(self, app : laio_tk.AsyncApp, parent : ttk.Widget, client : laio_zmq.ZmqClient, clear_state : bool=False, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._client = client
        self._clear_state = clear_state

        self.columnconfigure(1, weight=1)

        self._host_label = ttk.Label(self, text='Host:')
        self._host_label.grid(row=0, column=0, sticky='e', padx=(0, Style.grid_padding), pady=Style.grid_padding)

        self._host = ltk.Entry(self, text=self.host)
        self._host.grid(row=0, column=1, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)

        self._port_label = ttk.Label(self, text='Port:')
        self._port_label.grid(row=0, column=2, sticky='e', padx=Style.grid_padding, pady=Style.grid_padding)

        self._port = ltk.Entry(self, text=str(self.port), hint=f'default: {lprot.default_port}', validation=r'^[0-9]{0,5}$')
        self._port.grid(row=0, column=3, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)

        self._multi_var = tk.BooleanVar(value=client.multi)
        self._multi = ttk.Checkbutton(self, text='Multi', variable=self._multi_var)
        self._multi.grid(row=0, column=4, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)

        self._connect = ttk.Button(self, text='Connect')
        self._connect.grid(row=0, column=5, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)
        self._connect['command'] = self._on_connect_button

        self.connect(self.client.connecting, self._on_connected)
        self.connect(self.client.disconnected, self._on_disconnected)

        if self.client.is_connected():
            self._on_connected()
        else:
            self._on_disconnected()
            self._on_connect_button()

    @property
    def client(self) -> laio_zmq.ZmqClient:
        return self._client

    @property
    def host(self) -> str:
        return self.client.host

    @property
    def port(self) -> int:
        return int(self.client.port)

    @laio_tk.AsyncApp.tk_func
    def _on_connected(self, *args, **kwargs):
        self._connect['text'] = 'Disconnect'
        self._host['state'] = 'disabled'
        self._port['state'] = 'disabled'
        self._multi['state'] = 'disabled'

    @laio_tk.AsyncApp.tk_func
    def _on_disconnected(self, *args, **kwargs):
        self._connect['text'] = 'Connect'
        self._host['state'] = 'normal'
        self._port['state'] = 'normal'
        self._multi['state'] = 'normal'
        self.app.root.title(f'libstored GUI')
        if plotter is not None:
            plotter.stop()

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

    @laio_tk.AsyncApp.worker_func
    async def _do_connect_button(self, host : str, port : int, multi : bool):
        if self.client.is_connected():
            await self.client.disconnect()
        else:
            try:
                await self.client.connect(host, port, multi, default_state=self._clear_state)
                self._after_connection(await self.client.identification(), await self.client.version())
            except asyncio.CancelledError:
                raise
            except BaseException as e:
                self.logger.warning('Connect failed: %s', e)

    @laio_tk.AsyncApp.tk_func
    def _after_connection(self, identification : str, version : str):
        self.logger.info(f'Connected to {identification} ({version})')
        self.app.root.title(f'libstored GUI - {identification} ({version})')



class ObjectRow(laio_tk.AsyncWidget, ttk.Frame):
    def __init__(self, app : GUIClient, parent : ttk.Widget, obj : laio_zmq.Object, style : str | None=None, show_plot : bool=False, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._obj = obj

        self.columnconfigure(0, weight=1)

        self._label = ttk.Label(self, text=obj.name)
        self._label.grid(row=0, column=0, sticky='w', padx=(0, Style.grid_padding), pady=Style.grid_padding)

        self._show_plot = show_plot and Plotter.available and plotter and obj.is_fixed()
        self._plot_var = tk.BooleanVar(value=False)
        self._plot = ttk.Checkbutton(self, variable=self._plot_var, command=self._on_plot_check_change)

        self._format = ttk.Combobox(self, values=obj.formats(), width=10, state='readonly')
        self._format.set(obj.format)
        self._format.bind('<<ComboboxSelected>>', lambda e: obj.format.set(self._format.get()))
        self._format.grid(row=0, column=2, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)
        app.connect(obj.format, self._format.set)

        self._type = ttk.Label(self, text=obj.type_name, width=10, anchor='e')
        self._type.grid(row=0, column=3, sticky='e', padx=Style.grid_padding, pady=Style.grid_padding)

        self._value = laio_tk.ZmqObjectEntry(app, self, obj)
        self._value.grid(row=0, column=4, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)

        self._poll_var = tk.BooleanVar(value=obj.polling.value is not None)
        app.connect(obj.polling, self._on_poll_obj_change)
        app.connect(obj.polling, self._on_poll_obj_change_csv)
        self._poll = ttk.Checkbutton(self, variable=self._poll_var, command=self._on_poll_check_change)
        self._poll.grid(row=0, column=5, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)
        if obj.polling.value is not None:
            self._on_poll_obj_change_csv(obj.polling.value)

        if plotter:
            app.connect(plotter.closed, lambda: self._plot_var.set(False))

        self._refresh = ttk.Button(self, text='Refresh', command=self._value.refresh)
        self._refresh.grid(row=0, column=6, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)

        if style is not None:
            self.style(style)

        self.connect(self.obj.client.disconnected, self._on_disconnect)

    @property
    def obj(self) -> laio_zmq.Object:
        return self._obj

    def style(self, style : str):
        if style != '':
            style += '.'

        self['style'] = f'{style}TFrame'
        self._label['style'] = f'{style}TLabel'
        self._plot['style'] = f'{style}TCheckbutton'
        self._format['style'] = f'{style}TCombobox'
        self._type['style'] = f'{style}TLabel'
        self._value['style'] = f'{style}TEntry'
        self._poll['style'] = f'{style}TCheckbutton'
        self._refresh['style'] = f'{style}TButton'

    @laio_tk.AsyncApp.tk_func
    def _on_poll_obj_change(self, x):
        if x is not None:
            self._poll_var.set(True)
            if self._show_plot:
                self._plot.grid(row=0, column=1, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)
        else:
            self._poll_var.set(False)
            self._plot.grid_forget()
            self._plot_var.set(False)
            if plotter is not None:
                plotter.remove(self._obj)

    @laio_tk.AsyncApp.worker_func
    async def _on_poll_obj_change_csv(self, x):
        app = typing.cast(GUIClient, self.app)
        csv = app.csv
        if csv is None:
            return

        if x is not None:
            await csv.add(self._obj)
        else:
            await csv.remove(self._obj)

    def _on_poll_check_change(self):
        if self._poll_var.get():
            if self._obj.polling.value is None:
                self._obj.polling.value = typing.cast(GUIClient, self.app).default_poll()
        else:
            self._obj.polling.value = None

    def _on_plot_check_change(self):
        if plotter is None:
            return

        if self._plot_var.get():
            plotter.add(self._obj)
        else:
            plotter.remove(self._obj)

    @laio_tk.AsyncApp.tk_func
    def _on_disconnect(self):
        if not self.winfo_exists():
            return

        self._plot_var.set(False)
        self._plot['state'] = 'disabled'
        self._format['state'] = 'disabled'
        self._poll_var.set(False)
        self._poll['state'] = 'disabled'
        self._refresh['state'] = 'disabled'



class ObjectList(ttk.Frame):
    def __init__(self, app : GUIClient, parent : ttk.Widget, objects : list[laio_zmq.Object]=[], \
                 filter : typing.Callable[[laio_zmq.Object], bool] | None=None, show_plot : bool=False, *args, **kwargs):
        super().__init__(parent, *args, **kwargs)
        self._app = app
        self._objects : list[ObjectRow] = []
        self._filtered_objects : list[ObjectRow] = []
        self._filter = filter
        self._show_plot = show_plot
        self.columnconfigure(0, weight=1)
        self.filtered = laio_event.Event('filtered')
        self.changed = laio_event.Event('changed')
        self.set_objects(objects)

    @property
    def objects(self) -> list[ObjectRow]:
        return self._filtered_objects

    def set_objects(self, objects : list[laio_zmq.Object]):
        for o in self._objects:
            o.destroy()
        self._objects = []

        for o in natsort.natsorted(objects, key=lambda o: o.name, alg=natsort.ns.IGNORECASE):
            object_row = ObjectRow(self._app, self, o, show_plot=self._show_plot)
            self._objects.append(object_row)

        self.changed.trigger()
        self.filter()

    def filter(self, f : typing.Callable[[laio_zmq.Object], bool] | None | bool=True):
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
        self._filtered_objects = []
        for o in self._objects:
            if f(o.obj):
                o.grid(column=0, row=row, sticky='nsew')
                o.style('Even' if row % 2 == 0 else 'Odd')
                row += 1
                self._filtered_objects.append(o)
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
    def _children(widget : tk.BaseWidget) -> set[tk.BaseWidget]:
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



class FilterEntry(ltk.Entry):
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

            def f(o : laio_zmq.Object) -> bool:
                return regex.search(o.name) is not None

            self._object_list.filter(f)



class Tools(laio_tk.Work, ttk.Frame):
    def __init__(self, app : GUIClient, parent : ttk.Widget, \
                 filter_objects : ObjectList, refresh_command : typing.Callable[[], typing.Any], *args, **kwargs):
        super().__init__(atk=app.atk, master=parent, *args, **kwargs)
        self._app = app

        filter = FilterEntry(self, filter_objects)
        filter.grid(column=0, row=0, sticky='nswe', padx=(0, Style.grid_padding), pady=Style.grid_padding)

        self._plot = None
        if plotter is not None:
            self._plot = ttk.Button(self, text='Show plots', command=self._on_plot, width=12)
            self._plot.grid(column=1, row=0, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)
            self.connect(plotter.plotting, self._on_plotter_update)
            self.connect(plotter.paused, self._on_plotter_update)
            self._on_plotter_update()

        self._default_poll = ltk.Entry(self, text='1', hint='poll (s)', width=7, justify='right')
        self._default_poll.grid(row=0, column=2, sticky='nswe', padx=Style.grid_padding, pady=Style.grid_padding)

        refresh_all = ttk.Button(self, text='Refresh all', command=refresh_command)
        refresh_all.grid(row=0, column=3, sticky='nswe', padx=(Style.grid_padding, 0), pady=Style.grid_padding)

        self.columnconfigure(0, weight=1)

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

    def _on_plotter_update(self):
        assert self._plot is not None

        if not plotter or not plotter.plotting.value:
            self._plot['state'] = 'disabled'
            self._plot['text'] = 'No plots'
        else:
            self._plot['state'] = 'normal'
            if plotter.paused.value:
                self._plot['text'] = 'Resume plot'
            else:
                self._plot['text'] = 'Pause plot'

    def _on_plot(self):
        assert plotter is not None

        if not plotter.plotting.value:
            return

        plotter.toggle_pause()



class Stream(laio_tk.AsyncWidget, tk.Toplevel):
    def __init__(self, app : GUIClient, parent : ttk.Widget, name : str, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._name = name
        self._stream = app.client.stream(name)
        self._task : asyncio.Task | None = None
        self.title(f'libstored GUI - stream {name}')

        self._out = tk.Text(self)
        self._out.configure(state='disabled')
        self._out.grid(column=0, row=0, sticky='nsew')
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        self._out.bind('<Control-a>', self._select_all)
        self._out.bind('<Control-A>', self._select_all)

        self._start()

    @property
    def client(self) -> laio_zmq.ZmqClient:
        return typing.cast(GUIClient, self.app).client

    def _select_all(self, event):
        event.widget.tag_add('sel', '1.0', 'end')
        event.widget.mark_set('insert', 'end')
        return 'break'

    @laio_tk.AsyncApp.worker_func
    async def _start(self):
        if self._task is None:
            self._task = self.client.periodic(1.0, self._poll, name=f'stream {self._name}')

    @laio_tk.AsyncApp.worker_func
    async def _stop(self):
        if self._task:
            self._task.cancel()
            self._task = None

    @laio_tk.AsyncApp.worker_func
    async def _poll(self):
        await self._stream.flush()
        x = await self._stream.poll()
        if len(x) > 0:
            self._append(x)

    @laio_tk.AsyncApp.tk_func
    def _append(self, x : str):
        self._out.configure(state='normal')
        self._out.insert(tk.END, x)
        if self.focus_get() != self._out:
            self._out.see(tk.END)
        self._out.configure(state='disabled')

    @laio_tk.AsyncApp.tk_func
    def cleanup(self):
        if self.client.is_connected():
            self._stop()
        super().cleanup()



class Streams(laio_tk.AsyncWidget, ttk.Frame):
    def __init__(self, app : GUIClient, parent : ttk.Widget, client : laio_zmq.ZmqClient, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._client = client
        self._streams : dict[str, dict] = {}
        self._refresh = ttk.Button(self, text='Refresh streams', command=self._on_refresh)

        self.connect(self._client.connected, self._on_connect)
        self.connect(self._client.disconnected, self._on_disconnect)

    @laio_tk.AsyncApp.worker_func
    async def _on_connect(self):
        if self._client.is_connected() and 's' in await self._client.capabilities():
            self._on_connected()
        else:
            self._on_disconnect()

    @laio_tk.AsyncApp.tk_func
    def _on_connected(self):
        if self._client.is_connected():
            self._refresh.grid(column=256, row=0, sticky='nswe', padx=(Style.grid_padding * 2, 0), pady=0)
            self._on_refresh()
        else:
            self._on_disconnect()

    @laio_tk.AsyncApp.tk_func
    def _on_disconnect(self):
        self._refresh.grid_forget()
        self._on_streams([])
        self.configure(width=1)
        self.update_idletasks()

    @laio_tk.AsyncApp.worker_func
    async def _on_refresh(self):
        if not self._client.is_connected():
            self._on_streams([])
        else:
            self._on_streams(await self._client.other_streams())

    @laio_tk.AsyncApp.tk_func
    def _on_streams(self, streams : list[str]):
        for s, sconf in list(self._streams.items()):
            if s not in streams:
                if 'window' in sconf:
                    sconf['window'].destroy()
                sconf['check'].destroy()
                del self._streams[s]

        for s in streams:
            if s not in self._streams:
                var = tk.BooleanVar(value=False)
                check = ttk.Checkbutton(self, text=s, command=lambda s=s: self._show_stream(s), variable=var)
                check.grid(row=0, column=len(self._streams), sticky='nswe', padx=(Style.grid_padding * 2, 0), pady=0)
                self._streams[s] = {'check': check, 'var': var}

    @laio_tk.AsyncApp.tk_func
    def _show_stream(self, stream : str):
        if stream not in self._streams:
            return

        if not self._client.is_connected():
            self._hide_stream(stream)
            return

        sconf = self._streams[stream]
        if 'check' not in sconf:
            self._hide_stream(stream)
            return

        if not sconf['check'].instate(['selected']):
            self._hide_stream(stream)
            return

        if 'window' not in sconf:
            w = Stream(typing.cast(GUIClient, self.app), self, stream)
            sconf['window'] = w
            w.protocol("WM_DELETE_WINDOW", lambda: self._hide_stream(stream))

    @laio_tk.AsyncApp.tk_func
    def _hide_stream(self, stream : str):
        if stream not in self._streams:
            return

        sconf = self._streams[stream]
        if 'window' in sconf:
            w = sconf['window']
            del sconf['window']
            w.destroy()

        if 'check' in sconf and sconf['check'].instate(['selected']):
            sconf['check'].state(['!selected'])



class ManualCommand(laio_tk.AsyncWidget, ttk.Frame):
    def __init__(self, app : GUIClient, parent : ttk.Widget, client : laio_zmq.ZmqClient, *args, **kwargs):
        super().__init__(app=app, master=parent, *args, **kwargs)
        self._client = client
        self._empty = True

        self._req = ltk.Entry(self, hint='enter command')
        self._req.grid(column=0, row=0, sticky='nswe', padx=0, pady=Style.grid_padding)
        self.columnconfigure(0, weight=1)

        self._req.bind('<Return>', self._on_enter)
        self._req.bind('<KP_Enter>', self._on_enter)

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

        self._req.bind('<FocusIn>', self._focus_in, add=True)
        self.bind('<FocusOut>', self._focus_out)

        streams = Streams(app, self, client)
        streams.grid(column=1, row=0, sticky='nswe', padx=0, pady=Style.grid_padding)
        self.columnconfigure(1, weight=0)

        self._focus_out()

    def _on_enter(self, event):
        self._do_command(self._req.text)
        return 'break'

    @laio_tk.AsyncApp.worker_func
    async def _do_command(self, command : str):
        if not self._client.is_connected():
            return
        if command == '':
            return

        try:
            self._response(await self._client.req(command))
        except BaseException as e:
            self.logger.exception(f'Manual command: {e}')

    @laio_tk.AsyncApp.tk_func
    def _response(self, response : str):
        self._rep.configure(state='normal')
        self._rep.delete('1.0', 'end')
        self._rep.insert('end', response)
        self._rep.see('1.0')
        self._rep.configure(state='disabled')

    def _focus_in(self, *args):
        self._rep.grid(column=0, row=1, sticky='nsew', pady=(Style.grid_padding, 0), columnspan=2)
        self.rowconfigure(1, weight=1)

    def _focus_out(self, *args):
        self._rep.grid_forget()
        self.rowconfigure(1, weight=0)



#####################################################################
# GUI
#

class GUIClient(laio_tk.AsyncApp):
    def __init__(self, client : laio_zmq.ZmqClient, clear_state : bool=False, csv : laio_csv.CsvExport | None=None, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._client = client
        self._client_connections = set()
        self._csv = csv

        self.root.title(f'libstored GUI')
        icon_path = os.path.join(os.path.dirname(__file__), 'twotone_bug_report_black_48dp.png')
        icon = tk.PhotoImage(file=icon_path)
        self.root.iconphoto(False, icon)

        global plotter
        if Plotter.available and plotter is None:
            plotter = Plotter(atk=self.atk)

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

        connect = ClientConnection(self, self, self.client, clear_state)
        connect.grid(column=0, row=0, sticky='we', padx=Style.window_padding, pady=(Style.window_padding, Style.grid_padding))

        scrollable_objects = ScrollableFrame(self)
        self._objects = ObjectList(self, scrollable_objects.content)
        self._objects.pack(fill='both', expand=True)
        scrollable_objects.grid(column=0, row=2, sticky='nsew', padx=Style.window_padding)
        self.connect(self._objects.changed, scrollable_objects.bind_scroll)
        self.connect(self._objects.filtered, scrollable_objects.updated_content)

        self._tools = Tools(app=self, parent=self, filter_objects=self._objects, refresh_command=self._refresh_all)
        self._tools.grid(column=0, row=1, sticky='nswe', padx=Style.window_padding, pady=Style.grid_padding)

        self._scrollable_polled = ScrollableFrame(self)
        self._polled_objects = ObjectList(self, self._scrollable_polled.content, filter=lambda o: o.polling.value is not None, show_plot=True)
        self._polled_objects.pack(fill='both', expand=True)
        self.connect(self._polled_objects.changed, self._scrollable_polled.bind_scroll)
        self.connect(self._polled_objects.filtered, self._scrollable_polled.updated_content)

        self._manual = ManualCommand(self, self, self.client)
        self._manual.grid(column=0, row=4, sticky='nsew', padx=Style.window_padding, pady=(Style.grid_padding, Style.window_padding))

        self.columnconfigure(0, weight=1)
        self.rowconfigure(2, weight=2)
        self.rowconfigure(4, weight=0)

        self.bind("<Configure>", self._resize_polled_objects)
        self.connect(self._polled_objects.filtered, self._resize_polled_objects)
        self._resize_polled_objects()

        self.connect(self.client.connecting, self._on_connecting)
        self.connect(self.client.connected, self._on_connected)
        self.connect(self.client.disconnecting, self._on_disconnected_csv)
        self.connect(self.client.disconnected, self._on_disconnected)
        if self.client.is_connected():
            self._on_connecting()
            self._on_connected()
        else:
            self._on_disconnected()

    @property
    def client(self) -> laio_zmq.ZmqClient:
        return self._client

    @property
    def csv(self) -> laio_csv.CsvExport | None:
        return self._csv

    @laio_tk.AsyncApp.tk_func
    def _on_connecting(self, *args, **kwargs):
        self._objects.set_objects([])

        if self._csv is not None:
            self._csv.open(sync=True, block=False)

    @laio_tk.AsyncApp.tk_func
    def _on_connected(self, *args, **kwargs):
        self._polled_objects.set_objects(self.client.objects)
        self._objects.set_objects(self.client.objects)
        for o in self.client.objects:
            self._client_connections.add(self.connect(o.polling, self._filter_polled))
        self._resize_polled_objects()

    @laio_tk.AsyncApp.tk_func
    def _on_disconnected(self, *args, **kwargs):
        for c in self._client_connections:
            self.disconnect(c)
        self._client_connections.clear()
        self._polled_objects.set_objects([])

    @laio_tk.AsyncApp.worker_func
    async def _on_disconnected_csv(self):
        if self._csv is not None:
            await self._csv.close()
            await self._csv.clear()

    @laio_tk.AsyncApp.tk_func
    def cleanup(self):
        global plotter
        if plotter is not None:
            plotter.stop()
            plotter = None

        self.disconnect_all()
        self.logger.debug('Close client')
        self._close_async()
        super().cleanup()

        self.root.quit()

    @laio_tk.AsyncApp.worker_func
    async def _close_async(self):
        self.logger.debug('Closing client')
        await self.client.close()
        if self._csv is not None:
            await self._csv.close()

    @laio_tk.AsyncApp.tk_func
    def _refresh_all(self):
        self._refresh_objects([o.obj for o in self._objects.objects])

    @laio_tk.AsyncApp.worker_func
    async def _refresh_objects(self, objs : list[laio_zmq.Object]):
        await asyncio.gather(*(o.read(acquire_alias=False) for o in objs))

    @laio_tk.AsyncApp.tk_func
    def _filter_polled(self, interval_s):
        self._polled_objects.filter()

    @laio_tk.AsyncApp.tk_func
    def _resize_polled_objects(self, *args):
        self._scrollable_polled.update_idletasks()
        height = 0
        for o in self._polled_objects.objects:
            height += o.winfo_reqheight()

        if height > 0:
            self._scrollable_polled.grid(column=0, row=3, sticky='nsew', padx=Style.window_padding, pady=(Style.separator_padding, 0))
            self._scrollable_polled.update_idletasks()
            total_height = self.winfo_height()
            if total_height > 0:
                max_height = total_height // 3
                if height > max_height:
                    height = max_height
            self._scrollable_polled.canvas.config(height=height)
        else:
            self._scrollable_polled.grid_forget()

    @laio_tk.AsyncApp.tk_func
    def default_poll(self) -> float:
        return self._tools.default_poll()



#####################################################################
# CLI
#

def main():
    parser = argparse.ArgumentParser(prog=__package__, description='ZMQ GUI client',
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-V', '--version', action='version', version=__version__)
    parser.add_argument('-s', '--server', dest='server', type=str, default='localhost', help='ZMQ server to connect to')
    parser.add_argument('-p', '--port', dest='port', type=int, default=lprot.default_port, help='port')
    parser.add_argument('-v', '--verbose', dest='verbose', default=0, help='Enable verbose output', action='count')
    parser.add_argument('-m', '--multi', dest='multi', default=False,
        help='Enable multi-mode; allow multiple simultaneous connections to the same target, ' +
            'but it is less efficient.', action='store_true')
    parser.add_argument('-c', '--clearstate', dest='clear_state', default=False,
        help='Clear previously saved state', action='store_true')
    parser.add_argument('-D', '--deadlock', dest='deadlock', default=0, nargs='?',
        help='Enable deadlock checks after x seconds', type=float, const=10.0)
    parser.add_argument('-f', '--csv', dest='csv', default=None, nargs='?',
        help='Log auto-refreshed data to csv file. ' +
            'The file is truncated upon startup and when the set of auto-refreshed objects change. ' +
            'The file name may include strftime() format codes.', const='log.csv')
    parser.add_argument('-e', '--encrypt', dest='encrypted', type=str, default=None,
        help='Enable AES-256 CTR encryption with the given pre-shared key file', metavar='file')

    args = parser.parse_args()

    logging_config : dict[str, typing.Any] = {
        'format': '[%(asctime)s.%(msecs)03d] %(levelname)s %(name)s (%(threadName)s): %(message)s',
        'datefmt': '%H:%M:%S',
    }

    logger = logging.getLogger(__package__)

    if args.verbose == 0:
        logging_config['level'] = logging.WARNING
    elif args.verbose == 1:
        logging_config['level'] = logging.INFO
    else:
        logging_config['level'] = logging.DEBUG

    logging.basicConfig(**logging_config)
    if args.deadlock > 0:
        logger.info(f'Enable deadlock checks after {args.deadlock} seconds')
        lexc.DeadlockChecker.default_timeout_s = args.deadlock

    csv = None
    if args.csv:
        assert isinstance(args.csv, str)
        csv = laio_csv.CsvExport(laio_csv.generate_filename(args.csv))

    stack = None
    if args.encrypted:
        stack = lprot.Aes256Layer(args.encrypted, reqrep=True)
        logger.info(f'Enable AES-256 encryption with key file {args.encrypted}')

    client = laio_zmq.ZmqClient(host=args.server, port=args.port, multi=args.multi, use_state='gui', stack=stack)
    GUIClient.run(worker=client.worker, client=client, clear_state=args.clear_state, csv=csv)

if __name__ == '__main__':
    main()
