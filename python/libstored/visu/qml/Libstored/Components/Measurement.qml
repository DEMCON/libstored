/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
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

