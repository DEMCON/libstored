import QtQuick 2.10
import QtQuick.Controls 2.10
import QtQuick.Layouts 1.10

ApplicationWindow {
    id: root

    property real fontSize: 9
    width: fontSize * 40
    height: fontSize * 15
    visible: true

    GridLayout {
        anchors.fill: parent
        anchors.margins: 10
        columns: 2

        Label {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: "Connect via ZeroMQ to debug this application"
            antialiasing: true
            elide: Text.ElideRight
            font.pixelSize: root.fontSize
        }

        Label {
            text: "/hello"
            antialiasing: true
            elide: Text.ElideRight
            Layout.fillWidth: true
            font.pixelSize: root.fontSize
        }

        TextField {
            Layout.fillWidth: true
            antialiasing: true
            Layout.preferredHeight: root.fontSize * 2.5
            font.pixelSize: root.fontSize
            topPadding: 0
            bottomPadding: 0
            validator: IntValidator { bottom: -32768; top: 32767 }
            text: store.hello
            onEditingFinished: { store.hello = text }
        }

        Label {
            text: "/world"
            antialiasing: true
            elide: Text.ElideRight
            Layout.fillWidth: true
            font.pixelSize: root.fontSize
        }

        TextField {
            Layout.fillWidth: true
            antialiasing: true
            Layout.preferredHeight: root.fontSize * 2.5
            font.pixelSize: root.fontSize
            topPadding: 0
            bottomPadding: 0
            validator: DoubleValidator {}
            text: store.world
            onEditingFinished: { store.world = text }
        }

        Label {
            text: "/to everyone"
            antialiasing: true
            elide: Text.ElideRight
            Layout.fillWidth: true
            font.pixelSize: root.fontSize
        }

        TextField {
            Layout.fillWidth: true
            antialiasing: true
            Layout.preferredHeight: root.fontSize * 2.5
            font.pixelSize: root.fontSize
            topPadding: 0
            bottomPadding: 0
            maximumLength: 5
            text: store.to_everyone
            onEditingFinished: { store.to_everyone = text }
        }
    }
}
