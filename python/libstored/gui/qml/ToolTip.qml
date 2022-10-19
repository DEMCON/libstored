/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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

