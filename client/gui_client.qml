// vim:et
import QtQuick 2.12
import QtQuick.Layouts 1.15
import QtQuick.Window 2.2
import QtQuick.Controls 2.5

Window {

    id: root
    visible: true
    width: 800
    height: 600

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
                }
                
                Text {
                    Layout.fillHeight: true
                    Layout.preferredWidth: root.fontSize * 5
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
                    Layout.preferredWidth: root.fontSize * 10
                    font.pixelSize: root.fontSize
                    horizontalAlignment: TextInput.AlignRight
                    text: obj.valueString

                    onAccepted: {
                        obj.valueString = displayText
                        text = obj.valueString
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
                            if(!valueField.editing) {
                                valueField.text = obj.valueString
                                valueField.refreshed = true
                                updatedTimer.restart()
                            }
                        }
                    }
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
                onTextChanged: objects.setFilterRegularExpression(text)
            }

            Button {
                id: refreshAllButton
                Layout.fillHeight: true
                text: "Refresh all"
                onClicked: {
                    for(var i = 0; i < objects.rowCount(); i++)
                        objects.sourceModel.at(objects.mapToSource(objects.index(i, 0)).row).read()
                }
            }
        }

        ListView {
            Layout.fillHeight: true
            Layout.fillWidth: true
            id: objectList
            model: objects
            delegate: objectRow
            spacing: 3
            ScrollBar.vertical: ScrollBar {}
        }

        ListView {
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
