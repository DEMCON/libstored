import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls

import libstored.Components as Libstored

Rectangle {
    required property var obj
    required property int index

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

        Controls.CheckBox {
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
            }

            checked: obj.polling

            onCheckedChanged: obj.polling = checked
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
