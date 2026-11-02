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

/*
 * The coin silhouette from Zaster Banker's first life, faint, as a backdrop.
 * It is drawn in white only, so on a light ambience it dims further to stay
 * out of the way of the text.
 */
Image {
    property int sourceDimension: Math.min(parent.width, parent.height) - Theme.paddingMedium

    asynchronous: true
    fillMode: Image.PreserveAspectFit
    width: sourceDimension
    height: sourceDimension
    opacity: Theme.colorScheme ? 0.1 : 0.2
    source: "../../images/background.png"

    sourceSize {
        width: sourceDimension
        height: sourceDimension
    }

    anchors {
        centerIn: parent
    }
}
