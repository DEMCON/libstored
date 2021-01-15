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

import QtQuick.Controls 2.12
import QtQuick 2.12

Measurement {
    readOnly: false
    pollInterval: 0

    property bool editing: activeFocus && displayText != o.valueString

    property bool _edited: false
    onEditingChanged : {
        if(!editing) {
            _edited = true
            Qt.callLater(function() { _edited: false })
        }
    }

    color: editing ? "red" : refreshed && !_edited ? "blue" : "black"
    text: ""

    onAccepted: {
        o.set(displayText)
        Qt.callLater(function() { text = o.valueString })
    }

    onActiveFocusChanged: {
        if(activeFocus)
            text = o.valueString
        else
            text = _text
    }

    on_TextChanged: {
        if(!editing)
            text = _text
    }
}

