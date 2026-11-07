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

    readonly property bool ready: banking.state === Banking.Ready
    readonly property bool refreshing: banking.busy && banking.operation === Banking.FetchingBalances

    Connections {
        target: banking
        onOperationFinished: {
            if (operation === Banking.FetchingBalances || operation === Banking.RemovingBankAccess) {
                overviewPage.lastError = success ? "" : message
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
                visible: banking.state === Banking.DataUnreadable
                onClicked: remorse.execute(qsTr("Deleting all data"), function() { banking.resetAllData() })
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
                contentHeight: bankHeaderLabel.height
                enabled: !banking.busy
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("Refresh balances")
                        enabled: !banking.busy
                        onClicked: banking.refreshBankAccess(bankAccessId)
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

                SectionHeader {
                    id: bankHeaderLabel
                    text: bankName
                }
            }

            ListItem {
                id: accountItem

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
                visible: accountRow.lastOfBank
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
            hintText: banking.stateMessage + (banking.state === Banking.DataUnreadable
                                              ? "\n" + qsTr("Pull down to delete all data and start afresh.") : "")
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
