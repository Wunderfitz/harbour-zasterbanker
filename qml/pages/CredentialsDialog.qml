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

Dialog {
    id: credentialsDialog

    allowedOrientations: Orientation.All

    property var institute

    canAccept: userIdField.text.trim().length > 0
    acceptDestination: Qt.resolvedUrl("SetupPage.qml")
    acceptDestinationProperties: { "bankName": institute.name }

    onAccepted: banking.addBankAccess(institute.bankCode, institute.name, institute.serverUrl,
                                      userIdField.text, customerIdField.text)

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingMedium

            DialogHeader {
                acceptText: qsTr("Connect")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: institute.name
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                wrapMode: Text.Wrap
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: institute.city ? qsTr("%1 · Bank code %2").arg(institute.city).arg(institute.bankCode)
                                     : qsTr("Bank code %1").arg(institute.bankCode)
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }

            TextField {
                id: userIdField
                width: parent.width
                label: qsTr("Login name or user ID")
                placeholderText: label
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: customerIdField.focus = true
            }

            TextField {
                id: customerIdField
                width: parent.width
                label: qsTr("Customer ID, if your bank gave you one")
                placeholderText: qsTr("Customer ID (optional)")
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.enabled: credentialsDialog.canAccept
                EnterKey.onClicked: credentialsDialog.accept()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Use the same login as for your bank's online banking. Your PIN is asked for when connecting and is never stored.")
            }
        }

        VerticalScrollDecorator {}
    }
}
