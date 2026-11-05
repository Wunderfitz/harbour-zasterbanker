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

CoverBackground {
    id: cover

    // The coin silhouette, exactly as the original Zaster Banker drew it.
    // Anchoring both verticalCenter and bottom makes the image as high as the
    // cover less the margins; PreserveAspectFit makes it as wide, so it runs
    // over the cover's left edge. The silhouette is white; on a light ambience
    // a black copy of it takes its place.
    Image {
        source: Theme.colorScheme === Theme.DarkOnLight ? "../../images/background-black.png" : "../../images/background.png"
        anchors {
            verticalCenter: parent.verticalCenter

            bottom: parent.bottom
            bottomMargin: Theme.paddingMedium

            right: parent.right
            rightMargin: Theme.paddingMedium
        }

        fillMode: Image.PreserveAspectFit
        opacity: 0.2
    }

    // The banks that have balances, with their totals. The list stops above the
    // cover action, and fades out there if there are more banks than fit.
    Item {
        id: bankArea

        visible: banking.state === Banking.Ready
        clip: true

        anchors {
            top: parent.top
            topMargin: Theme.paddingLarge
            left: parent.left
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingMedium
            bottom: parent.bottom
            bottomMargin: Theme.itemSizeSmall
        }

        Column {
            id: bankColumn

            width: parent.width
            spacing: Theme.paddingMedium

            Repeater {
                model: banking.accounts.syncedBanks

                Column {
                    id: bankItem

                    readonly property var bank: modelData

                    width: bankColumn.width

                    Label {
                        width: parent.width
                        font.pixelSize: Theme.fontSizeMedium
                        color: Theme.primaryColor
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        truncationMode: TruncationMode.Elide
                        text: bankItem.bank.bankName
                    }
                    Repeater {
                        model: bankItem.bank.totals

                        // Shrinks rather than cuts off: an amount must be readable in full.
                        Label {
                            width: bankItem.width
                            font.pixelSize: Theme.fontSizeLarge
                            fontSizeMode: Text.HorizontalFit
                            minimumPixelSize: Theme.fontSizeTiny
                            color: modelData.amount < 0 ? Theme.errorColor : Theme.highlightColor
                            text: Format.amount(modelData.amount, modelData.currency)
                        }
                    }
                    Label {
                        width: parent.width
                        visible: !bankItem.bank.complete
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                        text: qsTr("Not all accounts")
                    }
                }
            }
        }
    }

    OpacityRampEffect {
        sourceItem: bankArea
        enabled: bankColumn.height > bankArea.height
        direction: OpacityRamp.TopToBottom
        offset: 0.7
        slope: 3.3
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Medium
        running: banking.busy
    }

    CoverActionList {
        enabled: banking.state === Banking.Ready && banking.accounts.count > 0 && !banking.busy

        CoverAction {
            iconSource: "image://theme/icon-cover-refresh"
            onTriggered: banking.refreshBalances()
        }
    }
}
