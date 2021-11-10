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

import QtQuick

Item {
    id: comp

    required property var ref
    property var obj: null
    property string name: obj ? obj.name : ""
    property real pollInterval: 2
    property bool autoReadOnInit: true

    onRefChanged: {
        if(typeof(ref) != "string") {
            obj = ref
        } else if(typeof(client) == "undefined") {
            obj = null
        } else {
            obj = client.obj(ref)
        }
    }

    onObjChanged: {
        if(obj) {
            value = obj.value

            if(!obj.polling) {
                if(pollInterval > 0)
                    obj.poll(pollInterval)
                else if(autoReadOnInit)
                    obj.asyncRead()
            } else if(pollInterval > 0 && obj.pollInterval > pollInterval) {
                // Prefer the faster setting, if there are multiple.
                obj.poll(pollInterval)
            }
        } else {
            value = null
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

    function set(x) {
        if(obj)
            obj.valueString = x
    }
}
