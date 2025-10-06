// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Controls as Controls

import Libstored.Components as Libstored

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
    visible: false
    width: 799
    height: 600

    readonly property color toolTipBase: "#ffffe0"
    readonly property color toolTipText: "#000000"

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
        root.visible = true

        // Somehow, the scaling of the contents of the window is wrong,
        // depending on DPI settings of the monitor.  Redrawing (or
        // moving/resizing the window) seems to work. Fake this by forcing a
        // redraw after first initialization.
        Qt.callLater(function() {
            root.width = root.width + 1
        })
    }

    property real fontSize: 9 * uiScale

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5

        RowLayout {
            Layout.preferredHeight: root.fontSize * 2
            Layout.fillHeight: false

            Controls.TextField {
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

            Controls.TextField {
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
                    text: "poll interval (s).\nThis value is used when auto-refresh is (re)enabled."
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

            Controls.Button {
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
            delegate: ObjectRow {}
            spacing: 3
            highlightFollowsCurrentItem: false
            Controls.ScrollBar.vertical: Controls.ScrollBar {}
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
            delegate: ObjectRow { showPlot: plotter.available }
            spacing: 3
            highlightFollowsCurrentItem: false
            currentIndex: -1
            Controls.ScrollBar.vertical: Controls.ScrollBar {}
            visible: count > 0
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.fontSize * 2
            Layout.fillHeight: false

            Controls.TextField {
                id: req
                Layout.fillHeight: true
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

            Controls.Button {
                id: plotterPause
                font.pixelSize: root.fontSize
                Layout.fillHeight: true
                Layout.preferredWidth: root.fontSize * 5
                background.antialiasing: true

                text: "Plot"
                checked: plotter.paused
                visible: plotter.available
                enabled: plotter.plotting
                onClicked: { plotter.togglePause() }

                ToolTip {
                    text: "Toggles whether the plots are automatically updated"
                }
            }

            Controls.Button {
                id: streamsRefresh
                font.pixelSize: root.fontSize
                Layout.fillHeight: true
                Layout.preferredWidth: root.fontSize * 7
                background.antialiasing: true

                text: "Streams"
                visible: streams.supported
                onClicked: { streams.refresh() }

                ToolTip {
                    text: "Refreshes the available lists of streams"
                }
            }

            Repeater {
                visible: streams.supported && streams.streams != ""
                model: streams.streams.split('')

                RowLayout {
                    Layout.fillWidth: false

                    CheckBox {
                        Layout.fillHeight: true
                        spacing: 3
                        Layout.preferredWidth: root.fontSize * 1.25

                        onCheckedChanged: {
                            if(checked) {
                                var s = streams.enable(modelData, client.defaultPollInterval)
                                var comp = Qt.createComponent("qrc:/Stream.qml")
                                if(comp.status != Component.Ready) {
                                    if(comp.status == Component.Error)
                                        console.error(comp.errorString().trim())
                                    else
                                        console.error("Cannot load Stream window")
                                } else {
                                    var wnd = comp.createObject(root, { main: root, stream: s })
                                    wnd.show()
                                    wnd.closing.connect(function() {
                                        checked = false
                                    })
                                }
                            } else {
                                streams.disable(modelData)
                            }
                        }
                    }

                    Controls.Label {
                        text: modelData
                    }
                }
            }
        }

        Controls.ScrollView {
            Layout.preferredHeight: root.fontSize * 10
            Layout.maximumHeight: parent.height / 4
            Layout.fillWidth: true
            clip: true

            Controls.TextArea {
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

    onClosing: {
        Qt.callLater(Qt.quit)
    }
}
