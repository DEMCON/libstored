import QtQuick.Controls 2.12
import QtQuick 2.12

TextField {
    id: comp

    background.antialiasing: true
    topPadding: 0
    bottomPadding: 0
    horizontalAlignment: TextInput.AlignRight
    readOnly: true

    property string unit: ''

    property alias obj: o.obj
    property alias pollInterval: o.pollInterval

    StoreObject {
        id: o
    }

    text: unit === '' ? o.valueString : o.valueString + ' ' + unit

    color: o.refreshed ? "blue" : "black"
}

