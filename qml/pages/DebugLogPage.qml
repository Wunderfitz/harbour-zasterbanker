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

// The debug log, newest at the bottom, and below it the button that copies all
// of it, to paste into a problem report. A long entry - a bank statement, say -
// shows its beginning; tapping shows more. Copying always takes everything.
//
// What the bank sent is in here, so everything is Text.PlainText.
Page {
    id: debugLogPage

    allowedOrientations: Orientation.All

    SilicaListView {
        id: logList

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: copyArea.top
        }
        clip: true
        model: debugLog

        header: PageHeader {
            title: qsTr("Debug log")
            description: qsTr("Entries: %1").arg(debugLog.count)
        }

        delegate: ListItem {
            id: entryItem

            property bool expanded: false

            contentHeight: entryColumn.height + 2 * Theme.paddingSmall
            onClicked: {
                if (model.shortened) {
                    expanded = !expanded
                }
            }

            Column {
                id: entryColumn

                x: Theme.horizontalPageMargin
                y: Theme.paddingSmall
                width: parent.width - 2 * Theme.horizontalPageMargin

                Label {
                    width: parent.width
                    textFormat: Text.PlainText
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: entryItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    text: model.time + "  " + model.source
                }
                Label {
                    width: parent.width
                    textFormat: Text.PlainText
                    // FinTS messages have no spaces to break at
                    wrapMode: Text.WrapAnywhere
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSizeTiny
                    color: entryItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: entryItem.expanded ? model.text : model.preview
                }
                Label {
                    width: parent.width
                    visible: model.shortened && (!entryItem.expanded || !model.complete)
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeTiny
                    color: entryItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    // only for entries of hundreds of characters: always plural
                    text: entryItem.expanded ? qsTr("%1 characters in all. Copy the log to see everything.").arg(model.length)
                                             : qsTr("%1 characters. Tap to show more.").arg(model.length)
                }
            }
        }

        ViewPlaceholder {
            enabled: logList.count === 0
            text: qsTr("Nothing logged yet")
            hintText: qsTr("Use the app with your bank, then come back here.")
        }

        VerticalScrollDecorator {}

        Component.onCompleted: positionViewAtEnd()
    }

    Item {
        id: copyArea

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: copyButton.height + 2 * Theme.paddingLarge

        Button {
            id: copyButton

            property bool copied: false

            anchors.centerIn: parent
            enabled: debugLog.count > 0
            text: copied ? qsTr("Copied") : qsTr("Copy debug log")
            onClicked: {
                debugLog.copyToClipboard()
                copied = true
                copiedTimer.restart()
            }

            Timer {
                id: copiedTimer
                interval: 2000
                onTriggered: copyButton.copied = false
            }
        }
    }
}
