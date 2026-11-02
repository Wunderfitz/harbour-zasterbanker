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
import "pages"

ApplicationWindow {
    id: app

    initialPage: Component { OverviewPage {} }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    // Whatever the bank wants to know in the middle of an operation - PIN,
    // TAN, a choice, a confirmation - can come at any time and on top of any
    // page, so it is handled here once. The dialogs answer through
    // banking.respond() themselves.
    Connections {
        target: banking
        onInteractionRequested: {
            var properties = {
                interactionId: interactionId,
                heading: title,
                message: text,
                choices: choices,
                imageUrl: imageUrl
            }
            var dialog
            switch (type) {
            case Banking.PinInteraction:
                dialog = "pages/PinDialog.qml"
                break
            case Banking.TextInteraction:
                dialog = "pages/PinDialog.qml"
                properties.secret = false
                break
            case Banking.TanInteraction:
                dialog = "pages/TanDialog.qml"
                break
            case Banking.ChoiceInteraction:
                dialog = "pages/TanMethodDialog.qml"
                break
            default:
                dialog = "pages/MessageDialog.qml"
                break
            }
            pageStack.completeAnimation()
            pageStack.push(Qt.resolvedUrl(dialog), properties)
            // A refresh started from the cover needs the app in front to be answered.
            app.activate()
        }
    }
}
