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
import "../components"

Page {
    id: aboutPage

    allowedOrientations: Orientation.All

    readonly property string sourcesUrl: "https://github.com/Wunderfitz/harbour-zasterbanker"

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: aboutPage.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("About Zaster Banker") }

            Image {
                id: logo
                anchors.horizontalCenter: parent.horizontalCenter
                source: "../../images/zaster.png"
                width: Math.min(2 * Theme.itemSizeHuge,
                                Math.min(aboutPage.width, aboutPage.height) / 2)
                height: width
                sourceSize.width: logo.width
                sourceSize.height: logo.width
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                // Bump together with rpm/harbour-zasterbanker.spec.
                text: qsTr("Zaster Banker %1").arg("0.5")
                font.pixelSize: Theme.fontSizeExtraLarge
            }

            AboutParagraph {
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Personal finances for Sailfish OS")
            }

            Item { width: 1; height: Theme.paddingMedium }

            AboutParagraph {
                horizontalAlignment: Text.AlignHCenter
                color: Theme.primaryColor
                text: qsTr("By Sebastian J. Wolf and Claude")
            }

            AboutLink {
                url: "mailto:sebastian@ygriega.de"
                label: qsTr("Send e-mail")
            }

            Separator {
                width: parent.width
                color: Theme.primaryColor
                horizontalAlignment: Qt.AlignHCenter
            }

            AboutParagraph {
                horizontalAlignment: Text.AlignHCenter
                color: Theme.primaryColor
                text: qsTr("Licensed under GNU GPLv3 or later")
            }

            AboutLink {
                url: aboutPage.sourcesUrl
                label: qsTr("Sources on GitHub")
            }

            SectionHeader { text: qsTr("Your data") }

            AboutParagraph {
                text: qsTr("Zaster Banker talks to your bank directly, via FinTS, and to nobody else. There is no server in between and no tracking.")
            }

            AboutParagraph {
                text: qsTr("What it keeps - your bank accesses, login names, accounts and their last balances - is stored encrypted with AES-256. The key lives in Sailfish Secrets and is unlocked together with your device lock.")
            }

            AboutParagraph {
                text: qsTr("Your PIN is never stored. Once the bank has accepted it, it is kept in the app's memory for 5 minutes after your last action with that bank access, and forgotten when the app ends. Transactions and holdings are kept in memory only.")
            }

            SectionHeader { text: qsTr("Credits") }

            AboutParagraph {
                text: qsTr("FinTS is spoken by AqBanking and Gwenhywfar by Martin Preuß and contributors. Thanks for making them available under the conditions of the GNU GPL and the GNU LGPL!")
            }

            AboutLink {
                url: "https://www.aquamaniac.de/rdm/projects/aqbanking"
                label: qsTr("Open the AqBanking website")
            }

            AboutParagraph {
                text: qsTr("Secure connections use GnuTLS, Nettle and GMP; encryption uses libgcrypt and libgpg-error. Thanks to their authors for making them available under the conditions of the GNU LGPL!")
            }

            AboutParagraph {
                text: qsTr("The list of banks is the FinTS institute list of the Deutsche Kreditwirtschaft.")
            }

            AboutParagraph {
                text: qsTr("Zaster Banker was written by its author together with Claude, Anthropic's assistant, in Claude Code. The commits it worked on name it as a co-author.")
            }

            AboutLink {
                url: "https://claude.com/claude-code"
                label: qsTr("Open the Claude Code website")
            }
        }

        VerticalScrollDecorator {}
    }
}
