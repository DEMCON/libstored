# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

import enum
import re
import tkinter as tk
import tkinter.ttk as ttk
import typing

class Entry(ttk.Entry):
    class State(enum.IntEnum):
        EMPTY = enum.auto()
        FILLED = enum.auto()
        FOCUSED = enum.auto()

    def __init__(self, parent : ttk.Widget, text : str = '',  hint : str = '', hint_color='gray', \
                 validation : str | typing.Callable[[str], bool]='', *args, **kwargs):
        super().__init__(parent, *args, **kwargs)

        self._var = tk.StringVar(self, value=text)
        self['textvariable'] = self._var

        self._hint = hint
        self._hint_color = hint_color
        self._foreground_color = self['foreground']

        self.bind('<FocusIn>', self._focus_in)
        self.bind('<FocusOut>', self._focus_out)

        self.bind('<Control-a>', self._select_all)
        self.bind('<Control-A>', self._select_all)

        if validation != '':
            if isinstance(validation, str):
                self._validation = lambda s: re.compile(validation).match(s) is not None
            else:
                self._validation = validation

            self['validate'] = 'key'
            self['validatecommand'] = (self.register(self._validate), '%P')

        self._state = Entry.State.EMPTY if text == '' else Entry.State.FILLED
        self._update_style()

    @property
    def text(self) -> str:
        if self._state == Entry.State.EMPTY:
            return ''
        else:
            return self._var.get()

    @text.setter
    def text(self, value : str):
        if self._state == Entry.State.EMPTY and value == '':
            pass
        elif self._state == Entry.State.EMPTY and value != '':
            self._state = Entry.State.FILLED
            self._var.set(value)
            self._update_style()
        elif self._state == Entry.State.FILLED and value == '':
            self._state = Entry.State.EMPTY
            self._update_style()
        else:
            self._var.set(value)

    @property
    def hint(self) -> str:
        return self._hint

    @hint.setter
    def hint(self, value : str):
        self._hint = value
        if self._state == Entry.State.EMPTY:
            self._var.set(value)

    def _update_style(self):
        if self._state == Entry.State.EMPTY:
            self['foreground'] = self._hint_color
            self._var.set(self._hint)
        else:
            self['foreground'] = self._foreground_color

    def _focus_in(self, *args):
        if self._state == Entry.State.FOCUSED:
            return

        if self._state == Entry.State.EMPTY:
            self._var.set('')

        self._state = Entry.State.FOCUSED
        self._update_style()

    def _focus_out(self, *args):
        if self._state != Entry.State.FOCUSED:
            return

        if self._var.get() == '':
            self._state = Entry.State.EMPTY
        else:
            self._state = Entry.State.FILLED

        self._update_style()

    def _select_all(self, event):
        event.widget.select_range(0, 'end')
        event.widget.icursor('end')
        return 'break'

    def _validate(self, proposed : str) -> bool:
        if self._validation(proposed):
            return True
        else:
            self.bell()
            return False
