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
import "../components"
import "../js/Format.js" as Format

Page {
    id: overviewPage

    objectName: "overviewPage"
    allowedOrientations: Orientation.All

    property string lastError
    // what the bank said when updating the accounts of an access failed
    property var bankMessages: []

    readonly property bool ready: banking.state === Banking.Ready
    readonly property bool pinsRemembered: Object.keys(banking.pinDeadlines).length > 0
    // for the PINs' countdown
    property real now: Date.now()

    function pinTimeLeft(bankAccessId) {
        var deadline = banking.pinDeadlines[String(bankAccessId)]
        return deadline === undefined ? 0 : deadline - now
    }

    Timer {
        interval: 1000
        repeat: true
        triggeredOnStart: true
        running: overviewPage.pinsRemembered && Qt.application.active && overviewPage.status === PageStatus.Active
        onTriggered: overviewPage.now = Date.now()
    }
    readonly property bool refreshing: banking.busy && (banking.operation === Banking.FetchingBalances
                                                        || banking.operation === Banking.UpdatingBankAccess)

    Connections {
        target: banking
        onOperationFinished: {
            if (operation === Banking.FetchingBalances || operation === Banking.RemovingBankAccess
                    || operation === Banking.UpdatingBankAccess || operation === Banking.ChangingTanMethod) {
                overviewPage.lastError = success ? "" : message
                overviewPage.bankMessages = !success && operation === Banking.UpdatingBankAccess
                        ? banking.bankMessages : []
            }
        }
    }

    SilicaListView {
        id: accountList

        anchors.fill: parent
        model: overviewPage.ready ? banking.accounts : null

        header: PageHeader {
            title: "Zaster Banker"
            description: overviewPage.refreshing ? banking.progressText : overviewPage.lastError
        }

        // By bank access, not by bank name: two accesses at the same bank are two
        // sections. There is no section delegate: a ListView does not lay itself
        // out again when one grows, so a context menu would cover the first account.
        // Each account's delegate carries what surrounds it instead.
        section.property: "bankAccessId"

        PullDownMenu {
            busy: banking.busy || banking.state === Banking.Starting

            MenuItem {
                text: qsTr("About Zaster Banker")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("Delete all data")
                visible: banking.state === Banking.DataUnreadable || banking.state === Banking.Failed
                onClicked: remorse.execute(qsTr("Deleting all data"), function() { banking.resetAllData() })
            }
            MenuItem {
                text: qsTr("Try again")
                visible: banking.state === Banking.Failed
                enabled: !banking.busy
                onClicked: banking.retry()
            }
            MenuItem {
                text: qsTr("Messages from the bank")
                visible: overviewPage.bankMessages.length > 0
                onClicked: pageStack.push(Qt.resolvedUrl("BankMessagesPage.qml"), {
                    messages: overviewPage.bankMessages
                })
            }
            // Whoever picks up the phone next gets no action without the PIN.
            MenuItem {
                text: qsTr("Forget PINs now")
                visible: overviewPage.ready && overviewPage.pinsRemembered
                onClicked: banking.forgetPins()
            }
            MenuItem {
                text: qsTr("Add bank access")
                visible: overviewPage.ready
                enabled: !banking.busy
                onClicked: pageStack.push(Qt.resolvedUrl("InstituteSearchPage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh balances")
                visible: overviewPage.ready && accountList.count > 0
                enabled: !banking.busy
                onClicked: banking.refreshBalances()
            }
        }

        delegate: Column {
            id: accountRow

            readonly property bool firstOfBank: ListView.section !== ListView.previousSection
            readonly property bool lastOfBank: ListView.section !== ListView.nextSection

            width: accountList.width

            // What concerns a bank access as a whole is in the context menu of its
            // name. Accounts are list items of their own, with menus of their own.
            ListItem {
                id: bankHeader

                visible: accountRow.firstOfBank
                // as high as a section header, the text centred in it, also when
                // pressed; the same with or without the countdown, so that the
                // list does not move when it comes or goes
                contentHeight: Math.max(Theme.itemSizeSmall, bankHeaderColumn.height + 2 * Theme.paddingSmall)
                enabled: !banking.busy
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("Refresh balances")
                        visible: !noAccounts
                        enabled: !banking.busy
                        onClicked: banking.refreshBankAccess(bankAccessId)
                    }
                    // The bank's parameters, the accounts and theirs, and the TAN
                    // methods it allows: in AqBanking all one request.
                    MenuItem {
                        text: qsTr("Update accounts and bank data")
                        enabled: !banking.busy
                        onClicked: banking.updateBankAccess(bankAccessId)
                    }
                    MenuItem {
                        text: qsTr("Change TAN method")
                        enabled: !banking.busy
                        onClicked: banking.changeTanMethod(bankAccessId)
                    }
                    MenuItem {
                        text: qsTr("Remove this bank access")
                        enabled: !banking.busy
                        onClicked: {
                            var bankAccess = bankAccessId
                            remorse.execute(qsTr("Removing %1").arg(bankName), function() {
                                banking.removeBankAccess(bankAccess)
                            })
                        }
                    }
                }

                Column {
                    id: bankHeaderColumn

                    width: parent.width
                    anchors.verticalCenter: parent.verticalCenter

                    // looks as a SectionHeader, without its padding above
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: Theme.fontSizeSmall
                        truncationMode: TruncationMode.Fade
                        color: Theme.highlightColor
                        text: bankName
                    }

                    // How long the access's PIN is still remembered, if it is:
                    // below the name, and quieter.
                    Label {
                        readonly property real timeLeft: overviewPage.pinTimeLeft(bankAccessId)

                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        visible: timeLeft > 0
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: bankHeader.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        text: qsTr("PIN remembered %1").arg(Format.countdown(timeLeft))
                    }
                }
            }

            // A bank access the bank listed no accounts for: it stays reachable.
            Item {
                width: parent.width
                height: noAccountsLabel.height + Theme.paddingLarge
                visible: noAccounts

                Label {
                    id: noAccountsLabel

                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    text: qsTr("No accounts. Press and hold the bank's name to update the accounts or to remove the bank access.")
                }
            }

            ListItem {
                id: accountItem

                visible: !noAccounts
                contentHeight: Math.max(accountColumn.height, balanceColumn.height) + 2 * Theme.paddingMedium
                enabled: !banking.busy
                onClicked: {
                    if (isDepot) {
                        pageStack.push(Qt.resolvedUrl("DepotPage.qml"), {
                                           accountId: accountId,
                                           accountName: name ? name : accountNumber,
                                           accountNumber: accountNumber
                                       })
                    } else {
                        pageStack.push(Qt.resolvedUrl("TransactionsPage.qml"), {
                                           accountId: accountId,
                                           accountName: name ? name : (iban ? iban : accountNumber),
                                           accountIban: iban,
                                           accountCurrency: currency ? currency : balanceCurrency
                                       })
                    }
                }

                Column {
                    id: accountColumn

                    anchors {
                        left: parent.left
                        leftMargin: Theme.horizontalPageMargin
                        right: balanceColumn.left
                        rightMargin: Theme.paddingMedium
                        verticalCenter: parent.verticalCenter
                    }

                    Label {
                        width: parent.width
                        text: name ? name : (iban ? iban : accountNumber)
                        color: accountItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        text: iban ? Format.iban(iban) : qsTr("Account %1").arg(accountNumber)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: accountItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        visible: owner.length > 0
                        text: owner
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: accountItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                    }
                }

                Column {
                    id: balanceColumn

                    anchors {
                        right: parent.right
                        rightMargin: Theme.horizontalPageMargin
                        verticalCenter: parent.verticalCenter
                    }

                    Label {
                        anchors.right: parent.right
                        text: hasBalance ? Format.amount(balance, balanceCurrency) : "–"
                        color: hasBalance && balance < 0 ? Theme.errorColor
                                                         : (accountItem.highlighted ? Theme.highlightColor : Theme.primaryColor)
                        font.pixelSize: Theme.fontSizeMedium
                    }
                    Label {
                        anchors.right: parent.right
                        visible: hasBalance && balanceDate !== undefined
                        text: visible ? Format.date(balanceDate) : ""
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: accountItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    }
                }
            }

            BankTotal {
                width: parent.width
                visible: accountRow.lastOfBank && !noAccounts
                totals: accountRow.lastOfBank ? bankTotals : []
                complete: bankTotalComplete
            }
        }

        ViewPlaceholder {
            enabled: overviewPage.ready && accountList.count === 0 && !banking.busy
            text: qsTr("No bank access yet")
            hintText: qsTr("Pull down to add your bank")
        }

        ViewPlaceholder {
            enabled: banking.state === Banking.DataUnreadable || banking.state === Banking.Failed
            text: banking.state === Banking.DataUnreadable ? qsTr("Stored data unreadable") : qsTr("Banking unavailable")
            hintText: banking.stateMessage + "\n" + (banking.state === Banking.DataUnreadable
                                                     ? qsTr("Pull down to delete all data and start afresh.")
                                                     : qsTr("Pull down to try again, or to delete all data and start afresh."))
        }

        VerticalScrollDecorator {}
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        spacing: Theme.paddingLarge
        visible: banking.state === Banking.Starting || (banking.busy && banking.operation === Banking.ResettingData)

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            size: BusyIndicatorSize.Large
            running: parent.visible
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            text: qsTr("Unlocking your data...")
        }
    }

    RemorsePopup {
        id: remorse
    }
}
