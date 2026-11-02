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

Page {
    id: searchPage

    allowedOrientations: Orientation.All

    // While true, the search field gets its focus back whenever something
    // takes it - search as you type means the keyboard stays until the user
    // closes it (Enter) or picks a bank.
    property bool typing: false

    function details(institute) {
        var parts = []
        if (institute.city) {
            parts.push(institute.city)
        }
        parts.push(qsTr("Bank code %1").arg(institute.bankCode))
        if (institute.bic) {
            parts.push(institute.bic)
        }
        return parts.join(" · ")
    }

    Component.onCompleted: {
        instituteSearch.query = ""
        searchField.forceActiveFocus()
    }

    // Deliberately not the list's header: a header lives inside the list's
    // focus scope, and every new result list moved the focus to the list's
    // current item, closing the keyboard after each keystroke.
    Column {
        id: searchHeader

        width: parent.width

        PageHeader {
            title: qsTr("Choose your bank")
        }

        SearchField {
            id: searchField
            width: parent.width
            placeholderText: qsTr("Name, city, bank code or BIC")
            inputMethodHints: Qt.ImhNoPredictiveText
            onTextChanged: instituteSearch.query = text
            onActiveFocusChanged: {
                if (activeFocus) {
                    searchPage.typing = true
                } else if (searchPage.typing) {
                    refocusTimer.restart()
                }
            }
            EnterKey.iconSource: "image://theme/icon-m-enter-close"
            EnterKey.onClicked: {
                searchPage.typing = false
                focus = false
            }
        }
    }

    // Takes the focus back once whatever took it is done, rather than in
    // the middle of its focus change.
    Timer {
        id: refocusTimer
        interval: 0
        onTriggered: {
            if (searchPage.typing && searchPage.status === PageStatus.Active && !searchField.activeFocus) {
                searchField.forceActiveFocus()
            }
        }
    }

    SilicaListView {
        id: resultList

        anchors {
            top: searchHeader.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        clip: true
        model: instituteSearch.results

        delegate: ListItem {
            id: instituteItem

            contentHeight: instituteColumn.height + 2 * Theme.paddingMedium

            onClicked: {
                searchPage.typing = false
                pageStack.push(Qt.resolvedUrl("CredentialsDialog.qml"), { institute: modelData })
            }

            Column {
                id: instituteColumn

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    text: modelData.name
                    color: instituteItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: searchPage.details(modelData)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: instituteItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                }
            }
        }

        ViewPlaceholder {
            enabled: resultList.count === 0 && instituteSearch.query.trim().length > 0 && !instituteSearch.searching
            text: qsTr("No bank found")
            hintText: qsTr("Only banks offering FinTS with PIN/TAN are listed.")
        }

        VerticalScrollDecorator {}
    }
}
