/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
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
