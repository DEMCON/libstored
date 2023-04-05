/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

import QtQuick.Controls
import QtQuick

Measurement {
    readOnly: false
    pollInterval: 0

    property bool editing: activeFocus && displayText != valueFormatted

    property bool _edited: false
    onEditingChanged : {
        if(!editing) {
            _edited = true
            Qt.callLater(function() { _edited: false })
        }
    }

    property bool valid: true
    property color validBackgroundColor: "white"
    property color invalidBackgroundColor: "#ffe0e0"
    palette.base: valid ? validBackgroundColor : invalidBackgroundColor

    color: editing ? "red" : !connected ? "gray" : refreshed && !_edited ? "blue" : "black"
    text: ""

    onAccepted: {
        o.set(displayText)
        Qt.callLater(function() { text = valueFormatted })
    }

    onActiveFocusChanged: {
        if(activeFocus)
            text = valueFormatted
        else
            text = _text
    }

    on_TextChanged: {
        if(!editing)
            text = _text
    }

    Keys.forwardTo: decimalPointConversion
    Item {
        id: decimalPointConversion
        Keys.onPressed: (event) => {
            if(obj !== null && event.key == Qt.Key_Period && (event.modifiers & Qt.KeypadModifier)) {
                event.accepted = true
                obj.injectDecimalPoint(parent)
            }
        }
    }
}

