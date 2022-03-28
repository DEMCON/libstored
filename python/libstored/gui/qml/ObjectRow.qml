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
import QtQuick.Controls as Controls

import Libstored.Components as Libstored

Rectangle {
    required property var obj
    required property var model
    required property int index
    property bool showPlot: false

    id: row
    width: objectList.width
    height: root.fontSize * 2
    color: {
        if(ListView.view.currentIndex == index)
            return "#d0d0d0"
        if(index % 2 == 0)
            return "#f0f0f0"
        return "white"
    }

    RowLayout {
        anchors.fill: parent

        Text {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: root.fontSize * 5
            text: obj.name
            elide: Text.ElideMiddle
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.fontSize
            fontSizeMode: Text.VerticalFit

            ToolTip {
                visible: mouseArea.containsMouse
                text: obj.name + (obj.alias ? "\nAlias: " + obj.alias : "")
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
            }
        }

        CheckBox {
            id: plotbox
            Layout.fillHeight: true
            Layout.preferredWidth: parent.height

            ToolTip {
                text: "Enable plot"
            }

            checked: model.plot
            onCheckedChanged: model.plot = checked
            visible: showPlot
        }

        Controls.ComboBox {
            id: format

            function strToIndex(s) {
                var f = obj.formats;
                for(var i = 0; i < f.length; i++)
                    if(f[i] == s)
                        return i;
                return -1;
            }

            model: obj.formats
            font.pixelSize: root.fontSize
            Layout.fillHeight: true
            Layout.preferredWidth: root.fontSize * 8
            Layout.maximumWidth: parent.width / 5
            currentIndex: strToIndex(obj.format)
            topPadding: 0
            bottomPadding: 0
            indicator.height: format.height * 0.618
            indicator.width: indicator.height * 1.5
            popup.font.pixelSize: root.fontSize
            popup.contentItem.height: format.height

            onCurrentTextChanged: {
                obj.format = currentText
            }

            contentItem: Controls.Label {
                text: format.displayText
                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: root.fontSize
                leftPadding: root.fontSize / 2
                elide: Text.ElideRight
            }
        }

        Text {
            Layout.fillHeight: true
            Layout.preferredWidth: root.fontSize * 5.5
            Layout.maximumWidth: parent.width / 5
            text: obj.typeName
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.fontSize
            fontSizeMode: Text.VerticalFit
        }

        Libstored.Input {
            id: valueField
            ref: row.obj
            autoReadOnInit: false

            Layout.fillHeight: true
            Layout.preferredWidth: root.fontSize * 15
            Layout.maximumWidth: parent.width / 5
            font.pixelSize: root.fontSize
            horizontalAlignment: TextInput.AlignRight
            placeholderText: obj && obj.t ? "" : "value?"
            placeholderTextColor: "#808080"
            readOnly: format.currentText === 'bytes'
            background.antialiasing: true
            topPadding: 0
            bottomPadding: 0
            leftPadding: 0

            ToolTip {
                visible: parent.hovered && obj && obj.tString
                text: obj ? obj.name + "\nLast update: " + obj.tString : ""
            }
        }

        CheckBox {
            id: autoRefresh
            Layout.fillHeight: true
            Layout.preferredWidth: parent.height

            ToolTip {
                text: "Enable auto-refresh"
            }

            checked: obj.polling
            onCheckedChanged: {
                obj.polling = checked
                if(!checked && model.plot)
                    model.plot = false
            }
        }

        Controls.Button {
            id: refreshButton
            Layout.fillHeight: true
            Layout.preferredWidth: implicitWidth
            Layout.maximumWidth: parent.width / 5
            text: "Refresh"
            font.pixelSize: root.fontSize
            onClicked: {
                obj.asyncRead()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: (mouse) => {
            objectList.currentIndex = -1
            polledObjectList.currentIndex = -1
            row.ListView.view.currentIndex = index
            row.ListView.view.focus = true
            mouse.accepted = false
        }
    }

    TextEdit {
        id: objNameCopy
        visible: false
        text: obj.name
    }

    Keys.onPressed: (event) => {
        if(event.matches(StandardKey.Copy)) {
            objNameCopy.selectAll()
            objNameCopy.copy()
        }
    }
}
