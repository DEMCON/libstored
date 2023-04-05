/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

import QtQuick
import QtQuick.Controls as Controls

Controls.CheckBox {
    id: control

    indicator: Rectangle {
        height: control.height * 0.618
        width: control.height * 0.618
        antialiasing: true

        x: control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        color: control.down ? control.palette.light : control.palette.base
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? control.palette.highlight : control.palette.mid

        Rectangle {
            width: parent.width * 0.618
            height: parent.width * 0.618
            antialiasing: true
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            color: control.palette.text
            visible: control.checkState === Qt.Checked
        }
    }
}
