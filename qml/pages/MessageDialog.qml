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

// A message with up to three answers, as AqBanking asks them - among others
// "confirm in your banking app" for decoupled TAN methods. Accepting gives the
// first answer, the buttons below the others; backing out cancels.
Dialog {
    id: messageDialog

    allowedOrientations: Orientation.All

    property int interactionId
    property string heading
    property string message
    property var choices: []
    property string imageUrl
    property bool answered: false

    function answer(choice) {
        answered = true
        banking.respond(interactionId, true, "", choice)
    }

    onAccepted: {
        if (!answered) {
            answer(1)
        }
    }
    onRejected: {
        if (!answered) {
            answered = true
            banking.respond(interactionId, false, "", 0)
        }
    }

    Connections {
        target: banking
        onInteractionFinished: {
            if (interactionId === messageDialog.interactionId && !messageDialog.answered) {
                messageDialog.answered = true
                if (pageStack.currentPage === messageDialog) {
                    messageDialog.reject()
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
            spacing: Theme.paddingLarge

            DialogHeader {
                acceptText: messageDialog.choices.length > 0 ? messageDialog.choices[0] : qsTr("OK")
                title: messageDialog.heading
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                textFormat: Text.StyledText
                text: messageDialog.message
            }

            Repeater {
                model: messageDialog.choices.length > 1 ? messageDialog.choices.slice(1) : []

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData
                    onClicked: {
                        messageDialog.answer(index + 2)
                        pageStack.pop()
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
