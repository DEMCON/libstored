// vim:et
import QtQuick 2.0
import QtQuick.Layouts 1.15
import QtQuick.Window 2.2
import QtQuick.Controls 1.4

Window {

    id: root
    visible: true
    width: 800
    height: 600

    Component {
        id: objectDelegate
        Item {
            width: row.width
            height: row.implicitHeight
            
            MouseArea {
                anchors.fill: parent
                onClicked: objectList.currentIndex = index
            }

            RowLayout {
                id: row
                width: objectList.width
                Text {
                    Layout.fillWidth: true
                    text: obj.name
                    elide: Text.ElideMiddle
                    clip: true
                }

                Text {
                    Layout.preferredWidth: 50
                    text: obj.typeName
                }

                TextField {
                    Layout.preferredWidth: 100
                    text: obj.value

                    onAccepted: {
                        obj.value = displayText
                        text = obj.value
                    }

                    onFocusChanged: {
                        if(!focus)
                            text = obj.value
                        else {
                            objectList.currentIndex = index
                            focus = true
                        }
                    }
                }

                Component.onCompleted: {
                    // Make sure to read at least once.
                    obj.read()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        ListView {
            id: objectList
            Layout.fillHeight: true
            Layout.fillWidth: true
            model: objects
            delegate: objectDelegate
            highlight: Rectangle { color: "lightsteelblue" }
            focus: true
            spacing: 3
            Layout.margins: 5
        }
    }
}
