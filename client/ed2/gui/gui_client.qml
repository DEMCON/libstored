// vim:et

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
import QtQuick.Layouts 1.15
import QtQuick.Window 2.2
import QtQuick.Controls 2.5

Window {

    id: root
    visible: true
    width: 800
    height: 600

    Component.onCompleted: {
        var text = "Embedded Debugger"

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

    readonly property int fontSize: 12

    Component {
        id: objectRow
        Rectangle {
            id: row
            width: objectList.width
            height: root.fontSize * 2
            color: index % 2 == 0 ? "#f0f0f0" : "white"

            RowLayout {
                anchors.fill: parent

                Text {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    text: obj.name
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: root.fontSize
                    fontSizeMode: Text.VerticalFit

                    ToolTip.text: obj.name + (obj.alias ? "\nAlias: " + obj.alias : "")
                    ToolTip.visible: mouseArea.containsMouse
                    ToolTip.delay: 1000

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                ComboBox {
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
                    currentIndex: strToIndex(obj.format)

                    onCurrentTextChanged: {
                        obj.format = currentText
                    }
                }

                Text {
                    Layout.fillHeight: true
                    Layout.preferredWidth: root.fontSize * 5.5
                    text: obj.typeName
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: root.fontSize
                    fontSizeMode: Text.VerticalFit
                }

                TextField {
                    id: valueField

                    Layout.fillHeight: true
                    Layout.preferredWidth: root.fontSize * 15
                    font.pixelSize: root.fontSize
                    horizontalAlignment: TextInput.AlignRight
                    text: obj.valueString
                    placeholderText: obj && obj.t ? "" : "value?"
                    readOnly: format.currentText === 'bytes'

                    onAccepted: {
                        obj.valueString = displayText
                        Qt.callLater(function() { text = obj.valueString })
                    }

                    property bool editing: activeFocus && text != obj.valueString
                    property bool refreshed: false
                    color: editing ? "red" : refreshed ? "blue" : "black"

                    onActiveFocusChanged: {
                        if(!activeFocus)
                            text = obj.valueString
                    }

                    Timer {
                        id: updatedTimer
                        interval: 1100
                        onTriggered: valueField.refreshed = false
                    }

                    Connections {
                        target: obj
                        function onValueChanged() {
                            if(obj && !valueField.editing) {
                                valueField.text = obj.valueString
                                valueField.refreshed = true
                                updatedTimer.restart()
                            }
                        }
                    }

                    ToolTip.text: obj ? obj.name + "\nLast update: " + obj.tString : ""
                    ToolTip.visible: hovered && obj && obj.tString
                    ToolTip.delay: 1000
                }

                CheckBox {
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.height
                    indicator.height: height * 0.618
                    indicator.width: height * 0.618

                    ToolTip.text: "Enable auto-refresh"
                    ToolTip.visible: hovered
                    ToolTip.delay: 1000

                    checked: obj.polling

                    onCheckedChanged: obj.polling = checked
                }

                Button {
                    id: refreshButton
                    Layout.fillHeight: true
                    Layout.preferredWidth: implicitWidth
                    text: "Refresh"
                    onClicked: {
                        obj.read()
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5

        RowLayout {
            Layout.preferredHeight: root.fontSize * 2
            Layout.fillHeight: false

            TextField {
                id: filter
                Layout.fillHeight: true
                Layout.fillWidth: true
                placeholderText: "enter regex filter"
                onTextChanged: regexTimer.restart()

                Timer {
                    id: regexTimer
                    interval: 300
                    repeat: false
                    onTriggered: objects.setFilterRegularExpression("(?i)" + filter.text)
                }
            }

            TextField {
                id: defaultPollField
                Layout.fillHeight: true
                Layout.preferredWidth: root.fontSize * 5
                placeholderText: "poll interval (s)"
                horizontalAlignment: Text.AlignRight

                ToolTip.text: "poll interval (s)"
                ToolTip.visible: hovered
                ToolTip.delay: 1000

                property string valueString: client ? client.defaultPollInterval : ""
                text: valueString

                onAccepted: {
                    client.defaultPollInterval = text
                    Qt.callLater(function() { text = valueString })
                }

                property bool editing: activeFocus && text != valueString
                color: editing ? "red" : "black"

                onActiveFocusChanged: {
                    if(!activeFocus)
                        text = valueString
                }
            }

            Button {
                id: refreshAllButton
                Layout.fillHeight: true
                text: "Refresh all"
                onClicked: {
                    for(var i = 0; i < objects.rowCount(); i++)
                        objects.sourceModel.at(objects.mapToSource(objects.index(i, 0)).row).asyncRead()
                }
            }
        }

        ListView {
            clip: true
            Layout.fillHeight: true
            Layout.fillWidth: true
            id: objectList
            model: objects
            delegate: objectRow
            spacing: 3
            ScrollBar.vertical: ScrollBar {}
        }

        ListView {
            clip: true
            Layout.preferredHeight: contentHeight
            Layout.maximumHeight: parent.height / 3
            Layout.topMargin: 5
            Layout.fillWidth: true
            id: polledObjectList
            model: polledObjects
            delegate: objectRow
            spacing: 3
            ScrollBar.vertical: ScrollBar {}
        }

        TextField {
            id: req
            Layout.preferredHeight: root.fontSize * 2
            font.pixelSize: root.fontSize
            Layout.fillWidth: true
            placeholderText: "enter command"

            onAccepted: {
                rep.text = client.req(text)
            }
        }

        ScrollView {
            Layout.preferredHeight: root.fontSize * 10
            Layout.maximumHeight: parent.height / 4
            Layout.fillWidth: true
            clip: true

            TextArea {
                id: rep
                readOnly: true
                font.pixelSize: root.fontSize
            }

            background: Rectangle {
                border.color: "#c0c0c0"
            }
        }
    }
}
