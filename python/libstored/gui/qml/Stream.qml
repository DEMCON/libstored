/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Controls as Controls

Window {
    id: root

    required property var main
    readonly property real uiScale: main.uiScale
    readonly property real fontSize: main.fontSize

    required property var stream

    visible: true
    width: fontSize * 40
    height: fontSize * 25
    title: 'Stream ' + stream.name

    Controls.ScrollView {
        anchors.fill: parent
        anchors.margins: 5
        clip: true

        Controls.TextArea {
            id: data
            readOnly: true
            font.pixelSize: root.fontSize
            font.family: "Courier"
        }

        background: Rectangle {
            antialiasing: true
            border.color: "#c0c0c0"
        }
    }

    function append(x) {
        x = x.replace('\x00', '')
        data.text += x
        data.cursorPosition = data.length - 1
    }

    Component.onCompleted: {
        append(stream.data)
    }

    Connections {
        target: stream
        function onDataChanged(x) {
            append(x)
        }
    }
}
