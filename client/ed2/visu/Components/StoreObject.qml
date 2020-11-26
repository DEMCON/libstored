/*
 * libstored, a Store for Embedded Debugger.
 * Copyright (C) 2020  Jochem Rutgers
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

import QtQuick 2.12

Item {
    id: comp

    required property string name
    property var obj: client && name ? client.obj(name) : null
    property real pollInterval: 1

    onObjChanged: {
        if(obj) {
            value = obj.value

            if(!obj.polling)
                obj.poll(pollInterval)
        }
    }

    property string valueString: obj ? obj.valueString : ''
    property var value: null

    property bool refreshed: false

    Timer {
        id: updatedTimer
        interval: 1100
        onTriggered: comp.refreshed = false
    }

    onValueStringChanged: {
        if(obj)
            value = obj.value

        comp.refreshed = true
        updatedTimer.restart()
    }
}
