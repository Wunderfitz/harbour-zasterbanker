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

// What the bank answered to a transaction fetch, a setup or a retrieval of
// accounts that failed, one result code with its text per line, e.g. "3010 -
// Keine Umsätze vorhanden. (S)". It explains a list that stays empty, and can be
// copied to send along with a problem report.
//
// The texts are the bank's, so they are Text.PlainText: Silica's default would
// render markup.
Page {
    id: bankMessagesPage

    allowedOrientations: Orientation.All

    property string accountName
    property var messages: []

    SilicaListView {
        id: messageList

        anchors.fill: parent
        model: bankMessagesPage.messages

        header: Column {
            width: messageList.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Messages from the bank")
                description: bankMessagesPage.accountName
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Each line is a result code with the bank's text, from 0 (done) over 3 (a note or warning) to 9 (an error). S: about part of the request, M: about the whole of it.")
            }

            Item {
                width: parent.width
                height: Theme.paddingMedium
            }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Copy to clipboard")
                onClicked: Clipboard.text = bankMessagesPage.messages.join("\n")
            }
        }

        delegate: Item {
            width: messageList.width
            height: messageLabel.height + Theme.paddingMedium

            Label {
                id: messageLabel
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                color: Theme.highlightColor
                text: modelData
            }
        }

        VerticalScrollDecorator {}
    }
}
