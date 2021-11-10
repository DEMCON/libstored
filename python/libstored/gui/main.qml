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
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Controls

import libstored.Components as Libstored

Window {
    id: root

    readonly property real uiScale: {
        var s = screen
        if(!s)
            s = Qt.application.screens[0]
        if(!s)
            return 1

        if(s.logicalPixelDensity !== undefined)
            return (s.logicalPixelDensity * 25.4) / 72

        return s.devicePixelRatio
    }
    visible: true
    width: 800
    height: 600

    readonly property color toolTipBase: "#ffffe0"

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

    property real fontSize: 9 * uiScale

    Component {
        id: objectRow
        Rectangle {
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
                        delay: 1000
                        font.pixelSize: root.fontSize
                        background.antialiasing: true
                        palette.toolTipBase: root.toolTipBase
                        contentItem: Label {
                            text: obj.name + (obj.alias ? "\nAlias: " + obj.alias : "")
                        }
                    }

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

                    contentItem: Label {
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

                TextField {
                    id: valueField

                    Layout.fillHeight: true
                    Layout.preferredWidth: root.fontSize * 15
                    Layout.maximumWidth: parent.width / 5
                    font.pixelSize: root.fontSize
                    horizontalAlignment: TextInput.AlignRight
                    text: obj.valueString
                    placeholderText: obj && obj.t ? "" : "value?"
                    placeholderTextColor: "#808080"
                    readOnly: format.currentText === 'bytes'
                    background.antialiasing: true
                    topPadding: 0
                    bottomPadding: 0
                    leftPadding: 0

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
                        function onValueStringChanged() {
                            if(obj && !valueField.editing) {
                                valueField.text = obj.valueString
                                valueField.refreshed = true
                                updatedTimer.restart()
                            }
                        }
                    }

                    ToolTip {
                        visible: parent.hovered && obj && obj.tString
                        delay: 1000
                        font.pixelSize: root.fontSize
                        background.antialiasing: true
                        palette.toolTipBase: root.toolTipBase
                        contentItem: Label {
                            text: obj ? obj.name + "\nLast update: " + obj.tString : ""
                        }
                    }
                }

                CheckBox {
                    id: autoRefresh
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.height

                    indicator: Rectangle {
                        height: autoRefresh.height * 0.618
                        width: autoRefresh.height * 0.618
                        antialiasing: true

                        x: autoRefresh.leftPadding + (autoRefresh.availableWidth - width) / 2
                        y: autoRefresh.topPadding + (autoRefresh.availableHeight - height) / 2

                        color: autoRefresh.down ? autoRefresh.palette.light : autoRefresh.palette.base
                        border.width: autoRefresh.visualFocus ? 2 : 1
                        border.color: autoRefresh.visualFocus ? autoRefresh.palette.highlight : autoRefresh.palette.mid

                        Rectangle {
                            width: parent.width * 0.618
                            height: parent.width * 0.618
                            antialiasing: true
                            x: (parent.width - width) / 2
                            y: (parent.height - height) / 2
                            color: autoRefresh.palette.text
                            visible: autoRefresh.checkState === Qt.Checked
                        }
                    }

                    ToolTip {
                        text: "Enable auto-refresh"
                        visible: parent.hovered
                        delay: 1000
                        font.pixelSize: root.fontSize
                        background.antialiasing: true
                        palette.toolTipBase: root.toolTipBase
                    }

                    checked: obj.polling

                    onCheckedChanged: obj.polling = checked
                }

                Button {
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
                placeholderTextColor: "#808080"
                onTextChanged: regexTimer.restart()
                background.antialiasing: true
                topPadding: 0
                bottomPadding: 0
                font.pixelSize: root.fontSize

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
                placeholderTextColor: "#808080"
                horizontalAlignment: Text.AlignRight
                background.antialiasing: true
                topPadding: 0
                bottomPadding: 0
                leftPadding: 0
                font.pixelSize: root.fontSize

                ToolTip {
                    visible: parent.hovered
                    delay: 1000
                    font.pixelSize: root.fontSize
                    background.antialiasing: true
                    palette.toolTipBase: root.toolTipBase
                    contentItem: Label {
                        text: "poll interval (s).\nThis value is used when auto-refresh is (re)enabled."
                    }
                }

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
                font.pixelSize: root.fontSize
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
            highlightFollowsCurrentItem: false
            ScrollBar.vertical: ScrollBar {}
        }

        Rectangle {
            antialiasing:  true
            color: "#c0c0c0"
            radius: 3
            Layout.fillWidth: true
            Layout.preferredHeight: 3
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: polledObjectList.visible
        }

        ListView {
            clip: true
            Layout.preferredHeight: contentHeight
            Layout.maximumHeight: parent.height / 3
            Layout.fillWidth: true
            id: polledObjectList
            model: polledObjects
            delegate: objectRow
            spacing: 3
            highlightFollowsCurrentItem: false
            currentIndex: -1
            ScrollBar.vertical: ScrollBar {}
            visible: count > 0
        }

        TextField {
            id: req
            Layout.preferredHeight: root.fontSize * 2
            font.pixelSize: root.fontSize
            Layout.fillWidth: true
            placeholderText: "enter command"
            placeholderTextColor: "#808080"
            background.antialiasing: true
            topPadding: 0
            bottomPadding: 0

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
                antialiasing: true
                border.color: "#c0c0c0"
            }

            visible: req.activeFocus || rep.activeFocus
        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true

        onWheel: (wheel) => {
            if(wheel.modifiers & Qt.ControlModifier)
                root.fontSize = root.fontSize * (1 + wheel.angleDelta.y * 0.0002)
            else
                wheel.accepted = false
        }

        onClicked: (mouse) => mouse.accepted = false
        onDoubleClicked: (mouse) => mouse.accepted = false
        onPositionChanged: (mouse) => mouse.accepted = false
        onPressAndHold: (mouse) => mouse.accepted = false
        onPressed: (mouse) => mouse.accepted = false
        onReleased: (mouse) => mouse.accepted = false
    }
}
