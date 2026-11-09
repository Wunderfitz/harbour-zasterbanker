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
import "../js/Format.js" as Format

// The transactions of one account. Asks the bank the first time an account is
// opened; after that, what is in memory is shown until the user refreshes.
//
// Names and purposes are written by whoever sent the money, so everything of
// theirs is Text.PlainText: Silica's default would render markup, and an <img>
// in a purpose would make the app load from any address.
Page {
    id: transactionsPage

    allowedOrientations: Orientation.All

    property int accountId
    property string accountName
    property string accountIban
    // Pending transactions (MT942) tend to arrive without a currency of their own.
    property string accountCurrency

    property bool requested: false
    property string lastError
    // What the bank answered to this page's last fetch, for when the list stays empty.
    property var bankMessages: []
    readonly property string bankMessagesHint: bankMessages.length > 0 ? qsTr("Pull down for the bank's messages.") : ""

    readonly property bool fetching: banking.busy && banking.operation === Banking.FetchingTransactions

    Component.onCompleted: banking.transactions.showAccount(accountId)

    // A transaction as plain text, one line for each thing it says, for pasting
    // into a note or a message.
    function clipboardText(transaction) {
        var lines = [
            transaction.pending ? qsTr("Pending") : Format.date(transaction.date),
            transaction.name,
            transaction.hasAmount ? Format.amount(transaction.amount, transaction.currency ? transaction.currency
                                                                                          : accountCurrency) : "",
            transaction.purpose,
            transaction.iban ? Format.iban(transaction.iban) : "",
            transaction.kind
        ]
        return lines.filter(function(line) { return line && line.length > 0 }).join("\n")
    }

    onStatusChanged: {
        if (status === PageStatus.Active && !requested) {
            requested = true
            if (!banking.transactions.loaded) {
                banking.fetchTransactions(accountId)
            }
        }
    }

    Connections {
        target: banking
        onOperationFinished: {
            if (operation === Banking.FetchingTransactions) {
                transactionsPage.lastError = success ? "" : message
                transactionsPage.bankMessages = banking.bankMessages
            }
        }
    }

    SilicaListView {
        id: transactionList

        anchors.fill: parent
        model: banking.transactions

        header: PageHeader {
            title: transactionsPage.accountName
            description: transactionsPage.fetching ? banking.progressText
                                                   : (banking.transactions.count > 0 && transactionsPage.lastError
                                                      ? transactionsPage.lastError : Format.iban(transactionsPage.accountIban))
        }

        PullDownMenu {
            busy: banking.busy

            MenuItem {
                text: qsTr("Messages from the bank")
                visible: transactionsPage.bankMessages.length > 0
                onClicked: pageStack.push(Qt.resolvedUrl("BankMessagesPage.qml"), {
                    accountName: transactionsPage.accountName,
                    messages: transactionsPage.bankMessages
                })
            }
            MenuItem {
                text: qsTr("Refresh transactions")
                enabled: !banking.busy
                onClicked: banking.fetchTransactions(transactionsPage.accountId)
            }
        }

        delegate: ListItem {
            id: transactionItem

            // Tapping shows everything a transaction says.
            property bool expanded: false
            // Outlasts expanded while the row closes: the text has to stay until the
            // row has shrunk, and the row's height then comes from collapsedHeight.
            property bool showDetails: false
            property real collapsedHeight
            // Only on a tap, not when the list creates rows or the screen rotates.
            property bool animateHeight: false

            contentHeight: showDetails && !expanded ? collapsedHeight
                                                    : Math.max(textColumn.height, amountColumn.height) + 2 * Theme.paddingMedium
            clip: heightAnimation.running

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Copy to clipboard")
                    onClicked: Clipboard.text = transactionsPage.clipboardText({
                        pending: pending, date: date, name: name, hasAmount: hasAmount, amount: amount,
                        currency: currency, purpose: purpose, iban: iban, kind: kind
                    })
                }
            }

            onClicked: {
                animateHeight = true
                if (expanded) {
                    expanded = false
                    // Nothing to animate if the details took up no room.
                    if (!heightAnimation.running && contentHeight === collapsedHeight) {
                        showDetails = false
                    }
                } else {
                    if (!showDetails) {
                        collapsedHeight = contentHeight
                        showDetails = true
                    }
                    expanded = true
                }
            }

            Behavior on contentHeight {
                enabled: transactionItem.animateHeight

                NumberAnimation {
                    id: heightAnimation

                    duration: 200
                    easing.type: Easing.InOutQuad
                    onRunningChanged: {
                        if (!running) {
                            transactionItem.animateHeight = false
                            transactionItem.showDetails = transactionItem.expanded
                        }
                    }
                }
            }

            Column {
                id: textColumn

                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    right: amountColumn.left
                    rightMargin: Theme.paddingMedium
                    // Centred if it is the smaller column. It must not centre itself
                    // in a row still resizing, as it would stick out above the row.
                    top: parent.top
                    topMargin: Math.max(Theme.paddingMedium, (parent.height - height) / 2)
                }

                Label {
                    width: parent.width
                    textFormat: Text.PlainText
                    text: name ? name : (kind ? kind : "–")
                    color: transactionItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    visible: text.length > 0
                    textFormat: Text.PlainText
                    text: purpose
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: transactionItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    wrapMode: Text.Wrap
                    maximumLineCount: transactionItem.showDetails ? 100 : 2
                    truncationMode: TruncationMode.Elide
                }
                Label {
                    width: parent.width
                    visible: transactionItem.showDetails && text.length > 0
                    textFormat: Text.PlainText
                    text: iban ? Format.iban(iban) : ""
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: transactionItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    visible: transactionItem.showDetails && name.length > 0 && text.length > 0
                    textFormat: Text.PlainText
                    text: kind
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: transactionItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    wrapMode: Text.Wrap
                }
            }

            Column {
                id: amountColumn

                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    anchors.right: parent.right
                    text: hasAmount ? Format.amount(amount, currency ? currency : transactionsPage.accountCurrency) : "–"
                    color: amount < 0 ? Theme.errorColor
                                      : (transactionItem.highlighted ? Theme.highlightColor : Theme.primaryColor)
                }
                Label {
                    anchors.right: parent.right
                    text: pending ? qsTr("Pending") : Format.date(date)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: transactionItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                }
            }
        }

        ViewPlaceholder {
            enabled: banking.transactions.loaded && banking.transactions.count === 0 && !transactionsPage.fetching
            text: qsTr("No transactions")
            hintText: qsTr("The bank reports none for the last 90 days.")
                      + (transactionsPage.bankMessagesHint ? "\n" + transactionsPage.bankMessagesHint : "")
        }

        ViewPlaceholder {
            enabled: !banking.transactions.loaded && !transactionsPage.fetching && transactionsPage.lastError.length > 0
            text: qsTr("Transactions unavailable")
            hintText: transactionsPage.lastError
                      + (transactionsPage.bankMessagesHint ? "\n" + transactionsPage.bankMessagesHint : "")
        }

        VerticalScrollDecorator {}
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: transactionsPage.fetching && banking.transactions.count === 0
    }
}
