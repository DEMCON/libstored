/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Controls as Controls

ApplicationWindow {
    id: wnd
    readonly property real uiScale: parent.uiScale
    readonly property real fontSize: parent.fontSize

    property string stream: '?'

    visible: true
    width: 800
    height: 600

    Controls.TextArea {
        id: data
        readOnly: true
        anchors.fill: parent
    }

    onClosing: {
        if(streams)
            streams.disable(wnd.stream)
    }
}
