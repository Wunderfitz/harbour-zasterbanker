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
import de.ygriega.zasterbanker 1.0

// Follows the first contact with the bank: handshake, TAN method, accounts.
// Leaving is only possible once it is over; until then the way out is Cancel.
Page {
    id: setupPage

    allowedOrientations: Orientation.All
    backNavigation: finished

    property string bankName
    property bool finished: false
    property bool succeeded: false
    property string resultText

    function close() {
        pageStack.pop(pageStack.find(function(page) { return page.objectName === "overviewPage" }))
    }

    Connections {
        target: banking
        onOperationFinished: {
            if (operation !== Banking.AddingBankAccess) {
                return
            }
            setupPage.finished = true
            setupPage.succeeded = success
            setupPage.resultText = success ? qsTr("Your bank access is set up.") : message
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: setupPage.finished ? (setupPage.succeeded ? qsTr("Connected") : qsTr("Not connected"))
                                          : qsTr("Connecting")
                description: setupPage.bankName
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Large
                running: !setupPage.finished
                visible: running
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: setupPage.finished ? setupPage.resultText : banking.progressText
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: setupPage.finished ? qsTr("Done") : qsTr("Cancel")
                onClicked: setupPage.finished ? setupPage.close() : banking.cancel()
            }
        }
    }
}
