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

// One of several - so far only ever the TAN method the bank offers.
Dialog {
    id: choiceDialog

    allowedOrientations: Orientation.All

    property int interactionId
    property string heading
    property string message
    property var choices: []
    property string imageUrl
    property int selectedIndex: -1
    property bool answered: false

    canAccept: selectedIndex >= 0

    onAccepted: {
        answered = true
        banking.respond(interactionId, true, "", selectedIndex)
    }
    onRejected: {
        if (!answered) {
            answered = true
            banking.respond(interactionId, false, "", -1)
        }
    }

    Connections {
        target: banking
        onInteractionFinished: {
            if (interactionId === choiceDialog.interactionId && !choiceDialog.answered) {
                choiceDialog.answered = true
                if (pageStack.currentPage === choiceDialog) {
                    choiceDialog.reject()
                }
            }
        }
    }

    SilicaListView {
        id: choiceList

        anchors.fill: parent
        model: choiceDialog.choices

        header: Column {
            width: choiceList.width
            spacing: Theme.paddingMedium

            DialogHeader {
                title: choiceDialog.heading
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: choiceDialog.message
            }

            Item {
                width: 1
                height: Theme.paddingMedium
            }
        }

        delegate: ListItem {
            id: choiceItem

            highlighted: down || index === choiceDialog.selectedIndex
            onClicked: choiceDialog.selectedIndex = index

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                text: modelData
                color: choiceItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                truncationMode: TruncationMode.Fade
            }
        }

        VerticalScrollDecorator {}
    }
}
