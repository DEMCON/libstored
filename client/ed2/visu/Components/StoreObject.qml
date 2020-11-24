import QtQuick 2.12

Item {
    id: comp

    required property string obj
    property var _o: client && obj ? client.obj(obj) : null
    property real pollInterval: 1

    on_OChanged: {
            if(_o && !_o.polling)
                    _o.poll(pollInterval)
    }

    property string valueString: _o ? _o.valueString : ''
    property var value: null

    property bool refreshed: false

    Timer {
        id: updatedTimer
        interval: 1100
        onTriggered: comp.refreshed = false
    }

    Connections {
        target: _o
        function onValueStringChanged() {
            if(_o)
                value = _o.value
            comp.refreshed = true
            updatedTimer.restart()
        }
    }
}
