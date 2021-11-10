/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

import QtQuick.Controls
import QtQuick

TextField {
    id: comp

    background.antialiasing: true

    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    horizontalAlignment: TextInput.AlignRight
    readOnly: true

    property string unit: ''

    property alias ref: o.ref
    property alias obj: o.obj
    property alias pollInterval: o.pollInterval
    property alias refreshed: o.refreshed
    property alias value: o.value
    property bool connected: o.obj !== null
    property alias autoReadOnInit: o.autoReadOnInit

    property var o: StoreObject {
        id: o
    }

    // Specify a (lambda) function, which will be used to convert the value
    // to a string. If null, the valueString of the object is used.
    property var formatter: null

    property string valueFormatted: {
        var s;

        if(!connected)
            s = '';
        else if(formatter)
            s = formatter(o.value);
        else
            s = o.valueString;

        return s
    }

    property string _text: {
        var s = '';
        if(!connected)
            s = '?';
        else
            s = valueFormatted;

        if(unit != '')
            s += ' ' + unit

        return s
    }
    text: _text

    color: !connected ? "gray" : refreshed ? "blue" : "black"
}

