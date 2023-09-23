// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

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
            value = obj.valueSafe

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
            value = obj.valueSafe

        comp.refreshed = true
        updatedTimer.restart()
    }

    function set(x) {
        if(obj)
            obj.valueString = x
    }
}
