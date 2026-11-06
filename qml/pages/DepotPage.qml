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

// What a depot holds. Asks the bank the first time a depot is opened; after that,
// what is in memory is shown until the user refreshes.
//
// Names of securities are the bank's, so they are Text.PlainText like everything
// else on TransactionsPage: Silica's default would render markup.
Page {
    id: depotPage

    allowedOrientations: Orientation.All

    property int accountId
    property string accountName
    property string accountNumber

    property bool requested: false
    property string lastError

    readonly property bool fetching: banking.busy && banking.operation === Banking.FetchingHoldings

    Component.onCompleted: banking.holdings.showAccount(accountId)

    onStatusChanged: {
        if (status === PageStatus.Active && !requested) {
            requested = true
            if (!banking.holdings.loaded) {
                banking.fetchHoldings(accountId)
            }
        }
    }

    Connections {
        target: banking
        onOperationFinished: {
            if (operation === Banking.FetchingHoldings) {
                depotPage.lastError = success ? "" : message
            }
        }
    }

    SilicaListView {
        id: holdingList

        anchors.fill: parent
        model: banking.holdings

        header: PageHeader {
            title: depotPage.accountName
            description: depotPage.fetching ? banking.progressText
                                            : (banking.holdings.count > 0 && depotPage.lastError
                                               ? depotPage.lastError : depotPage.accountNumber)
        }

        footer: BankTotal {
            width: holdingList.width
            visible: banking.holdings.count > 0
            totals: banking.holdings.totals
            complete: banking.holdings.complete
            incompleteText: qsTr("Not all holdings included")
        }

        PullDownMenu {
            busy: banking.busy

            MenuItem {
                text: qsTr("Refresh holdings")
                enabled: !banking.busy
                onClicked: banking.fetchHoldings(depotPage.accountId)
            }
        }

        delegate: Item {
            width: holdingList.width
            height: Math.max(holdingColumn.height, valueColumn.height) + 2 * Theme.paddingMedium

            Column {
                id: holdingColumn

                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    right: valueColumn.left
                    rightMargin: Theme.paddingMedium
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    width: parent.width
                    textFormat: Text.PlainText
                    text: name ? name : (identifier ? identifier : "–")
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    truncationMode: TruncationMode.Elide
                }
                Label {
                    width: parent.width
                    visible: hasUnits
                    textFormat: Text.PlainText
                    text: !hasUnits ? "" : (hasPrice ? qsTr("%1 × %2").arg(Format.units(units)).arg(Format.price(price, currency))
                                                     : Format.units(units))
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    visible: identifier.length > 0
                    textFormat: Text.PlainText
                    text: idKind ? idKind + " " + identifier : identifier
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                }
            }

            Column {
                id: valueColumn

                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    anchors.right: parent.right
                    text: hasValue ? Format.amount(value, currency) : "–"
                }
                Label {
                    anchors.right: parent.right
                    visible: priceDate !== undefined
                    text: visible ? Format.date(priceDate) : ""
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }
            }
        }

        ViewPlaceholder {
            enabled: banking.holdings.loaded && banking.holdings.count === 0 && !depotPage.fetching
            text: qsTr("No holdings")
            hintText: qsTr("The bank reports no securities in this depot.")
        }

        ViewPlaceholder {
            enabled: !banking.holdings.loaded && !depotPage.fetching && depotPage.lastError.length > 0
            text: qsTr("Holdings unavailable")
            hintText: depotPage.lastError
        }

        VerticalScrollDecorator {}
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: depotPage.fetching && banking.holdings.count === 0
    }
}
