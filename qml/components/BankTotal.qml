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
import "../js/Format.js" as Format

// The sum of a bank's accounts, closing its part of the overview: a line per
// currency, and a note if the bank left out the balance of an account.
Item {
    id: bankTotal

    property var totals: []
    property bool complete: true
    property string incompleteText: qsTr("Not all accounts included")

    height: totalColumn.height + 2 * Theme.paddingMedium

    Separator {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        color: Theme.primaryColor
    }

    Label {
        id: totalLabel

        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            right: totalColumn.left
            rightMargin: Theme.paddingMedium
            top: totalColumn.top
        }
        color: Theme.highlightColor
        truncationMode: TruncationMode.Fade
        text: qsTr("Total balance")
    }

    Column {
        id: totalColumn

        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }

        Repeater {
            model: bankTotal.totals

            Label {
                anchors.right: parent.right
                color: modelData.amount < 0 ? Theme.errorColor : Theme.primaryColor
                text: Format.amount(modelData.amount, modelData.currency)
            }
        }
        Label {
            anchors.right: parent.right
            visible: bankTotal.totals.length === 0
            color: Theme.primaryColor
            text: "–"
        }
        Label {
            anchors.right: parent.right
            visible: bankTotal.totals.length > 0 && !bankTotal.complete
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            text: bankTotal.incompleteText
        }
    }
}
