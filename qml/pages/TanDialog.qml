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

// Shows the bank's challenge - for photoTAN and QR-TAN the image to scan
// with the bank's app or reader - and takes the TAN.
Dialog {
    id: tanDialog

    allowedOrientations: Orientation.All

    property int interactionId
    property string heading
    property string message
    property var choices
    property string imageUrl
    property bool answered: false

    canAccept: tanField.text.trim().length > 0

    onAccepted: {
        answered = true
        banking.respond(interactionId, true, tanField.text.trim(), 0)
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
            if (interactionId === tanDialog.interactionId && !tanDialog.answered) {
                tanDialog.answered = true
                if (pageStack.currentPage === tanDialog) {
                    tanDialog.reject()
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
                title: qsTr("TAN")
            }

            // A light frame around the code: scanners want contrast, and the
            // image may well be transparent where it is not black.
            Rectangle {
                id: imageFrame

                readonly property int side: Math.min(column.width, tanDialog.height * 0.6) - 2 * Theme.horizontalPageMargin

                anchors.horizontalCenter: parent.horizontalCenter
                visible: tanDialog.imageUrl.length > 0
                width: side
                height: side
                color: "white"
                radius: Theme.paddingSmall

                Image {
                    anchors {
                        fill: parent
                        margins: Theme.paddingMedium
                    }
                    source: tanDialog.imageUrl
                    sourceSize.width: width
                    sourceSize.height: height
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    cache: false
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                textFormat: Text.StyledText
                text: tanDialog.message
            }

            TextField {
                id: tanField
                width: parent.width
                focus: tanDialog.imageUrl.length === 0
                label: qsTr("TAN")
                placeholderText: label
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.enabled: tanDialog.canAccept
                EnterKey.onClicked: tanDialog.accept()
            }
        }

        VerticalScrollDecorator {}
    }
}
