/*
    Copyright (C) 2026 Sebastian J. Wolf and other contributors

    This file is part of Zaster Banker.

    Zaster Banker is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Zaster Banker is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Zaster Banker. If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.2
import Sailfish.Silica 1.0

// So far only debug logging, for problem reports. The setting is kept, the log
// never is: it lives in memory until the app ends or the switch goes off.
Page {
    id: settingsPage

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Settings")
            }

            TextSwitch {
                text: qsTr("Debug logging")
                description: qsTr("Records every message exchanged with your bank and what the app does, to help find problems with a bank. Names, account numbers, IBANs, balances, transactions, PIN and TAN are masked. The log is kept in memory only and deleted when the app is closed or this is switched off.")
                // Follows the setting: a switch that checked itself would replace
                // this binding on the first tap.
                automaticCheck: false
                checked: appSettings.debugLogging
                onClicked: appSettings.debugLogging = !checked
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Show debug log")
                enabled: appSettings.debugLogging
                onClicked: pageStack.push(Qt.resolvedUrl("DebugLogPage.qml"))
            }
        }

        VerticalScrollDecorator {}
    }
}
