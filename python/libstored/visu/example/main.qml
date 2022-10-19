/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.12
import QtQuick.Layouts 1.15
import QtQuick.Window 2.2
import QtQuick.Controls 2.5

Window {

    id: root
    visible: true
    width: 400
    height: 300

    readonly property int fontSize: 10

    Component.onCompleted: {
        var text = "Visu"

        var id = client.identification()
        if(id && id !== "?")
        {
            text += ": " + id

            var v = client.version()
            if(v && v !== "?")
                text += " (" + v + ")"
        }

        root.title = text
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5

        TextField {
            id: req
            Layout.preferredHeight: root.fontSize * 2
            font.pixelSize: root.fontSize
            Layout.fillWidth: true
            placeholderText: "enter command"
            background.antialiasing: true
            topPadding: 0
            bottomPadding: 0

            onAccepted: {
                rep.text = client.req(text)
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: rep
                readOnly: true
                font.pixelSize: root.fontSize
            }

            background: Rectangle {
                antialiasing: true
                border.color: "#c0c0c0"
            }
        }
    }
}
