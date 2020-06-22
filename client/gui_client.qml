// vim:et
import QtQuick 2.0
import QtQuick.Layouts 1.15
import QtQuick.Window 2.2
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

Window {

    id: root
    visible: true
    width: 800
    height: 600

    readonly property int fontSize: 12

    Component {
        id: nameDelegate
        Text {
            text: styleData.value.name
            elide: Text.ElideMiddle
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: root.fontSize
            fontSizeMode: Text.VerticalFit
        }
    }

    Component {
        id: valueDelegate
        TextField {
            text: styleData.value.valueString
            anchors.fill: parent
            font.pixelSize: root.fontSize
            horizontalAlignment: TextInput.AlignRight

            onAccepted: {
                styleData.value.valueString = displayText
                text = styleData.value.valueString
            }

            textColor: displayText == styleData.value.valueString ? "black" : "red"

            onActiveFocusChanged: {
                if(!activeFocus)
                    text = styleData.value.valueString
            }
        }
    }

    Component {
        id: operationsDelegate
        RowLayout {
            id: row
            anchors.fill: parent

            Button {
                id: refreshButton
                Layout.fillHeight: true
                Layout.fillWidth: true
                text: "Refresh"
                style: ButtonStyle {
                    label: Text {
                        renderType: Text.NativeRendering
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: root.fontSize
                        text: refreshButton.text
                        fontSizeMode: Text.Fit
                    }
                }
                onClicked: {
                    styleData.value.read()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        TableView {
            id: objectList

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.margins: 5
            model: objects

            rowDelegate: Rectangle {
                height: root.fontSize * 2
                SystemPalette {
                    id: myPalette
                    colorGroup: SystemPalette.Active
                }
                color: {
                    var baseColor = styleData.alternate ? myPalette.alternateBase : myPalette.base
                    return /* styleData.selected ? myPalette.highlight : */ baseColor
                }
            }

            TableViewColumn {
                id: nameColumn
                title: "Name"
                delegate: nameDelegate
                role: "obj"
            }

            onWidthChanged: nameColumn.width = width - valueColumn.width - operationsColumn.width - 2

            TableViewColumn {
                id: valueColumn
                title: "Value"
                delegate: valueDelegate
                role: "obj"
            }

            TableViewColumn {
                id: operationsColumn
                title: "Operations"
                delegate: operationsDelegate
                role: "obj"
            }
        }
    }
}
