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

// Asks for the PIN, or - with secret set to false - for any other text the
// bank wants to know.
Dialog {
    id: pinDialog

    allowedOrientations: Orientation.All

    property int interactionId
    property string heading
    property string message
    property var choices
    property string imageUrl
    property bool secret: true
    property bool answered: false

    canAccept: (secret ? pinField.text : textField.text).length > 0

    onAccepted: {
        answered = true
        banking.respond(interactionId, true, secret ? pinField.text : textField.text, 0)
        pinField.text = ""
    }
    onRejected: {
        if (!answered) {
            answered = true
            banking.respond(interactionId, false, "", 0)
        }
    }

    // The bank's side gave up meanwhile (cancelled, timed out): nothing to answer anymore.
    Connections {
        target: banking
        onInteractionFinished: {
            if (interactionId === pinDialog.interactionId && !pinDialog.answered) {
                pinDialog.answered = true
                if (pageStack.currentPage === pinDialog) {
                    pinDialog.reject()
                }
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingMedium

            DialogHeader {
                title: pinDialog.secret ? qsTr("PIN") : pinDialog.heading
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: pinDialog.message
            }

            PasswordField {
                id: pinField
                width: parent.width
                visible: pinDialog.secret
                focus: pinDialog.secret
                label: qsTr("PIN")
                placeholderText: label
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.enabled: pinDialog.canAccept
                EnterKey.onClicked: pinDialog.accept()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: pinDialog.secret
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Once the bank accepts it, the PIN is remembered for 5 minutes after your last action with this bank access, as long as the app runs.")
            }

            TextField {
                id: textField
                width: parent.width
                visible: !pinDialog.secret
                focus: !pinDialog.secret
                label: pinDialog.heading
                inputMethodHints: Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.enabled: pinDialog.canAccept
                EnterKey.onClicked: pinDialog.accept()
            }
        }

        VerticalScrollDecorator {}
    }
}
