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

CoverBackground {
    id: cover

    BackgroundImage {
        sourceDimension: Math.min(cover.width, cover.height) - 2 * Theme.paddingMedium
        anchors {
            centerIn: undefined
            right: parent.right
            rightMargin: Theme.paddingMedium
            bottom: parent.bottom
            bottomMargin: Theme.paddingMedium
        }
    }

    Label {
        anchors.centerIn: parent
        visible: !accountColumn.visible
        width: parent.width - 2 * Theme.paddingLarge
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.secondaryColor
        text: "Zaster Banker"
    }

    // The accounts that have a balance, as many as fit, the first ones brightest.
    Column {
        id: accountColumn

        visible: banking.state === Banking.Ready && banking.accounts.hasBalances
        spacing: Theme.paddingMedium

        anchors {
            top: parent.top
            topMargin: Theme.paddingLarge
            left: parent.left
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingMedium
        }

        Repeater {
            id: repeater

            model: banking.state === Banking.Ready ? banking.accounts : null

            Column {
                width: accountColumn.width
                visible: hasBalance && index < 4
                opacity: 1.0 - index * 0.2

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                    text: name ? name : bankName
                }
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeSmall
                    color: balance < 0 ? Theme.errorColor : Theme.highlightColor
                    truncationMode: TruncationMode.Fade
                    text: hasBalance ? Format.amount(balance, balanceCurrency) : ""
                }
            }
        }
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
