
import QtQuick.Controls as Controls

Controls.ToolTip {
    id: tip
    visible: parent.hovered
    delay: 1000
    font.pixelSize: root.fontSize
    background.antialiasing: true
    palette.toolTipBase: root.toolTipBase
    contentItem: Controls.Label {
        palette.text: root.toolTipText
        text: tip.text
    }
}

