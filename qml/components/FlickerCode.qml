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

// A chipTAN flicker code (HHD 1.4, optical interface): five bars on black, the
// clock on the left, then bit 0 to bit 3 of the current half-byte; white is 1.
// Each half-byte is shown twice, with the clock white, then black, and the
// code repeats until the dialog closes. The triangles mark where the TAN
// generator's arrows go. Frames come from FlickerCode on the banking thread.
Item {
    id: flickerCode

    // one hex digit per half-byte, in the order shown, sync included
    property string frames
    // frames per second; one frame is half a half-byte's clock cycle
    property int speed: 15
    property bool running: true

    property int position: 0
    property bool clock: true
    readonly property int halfByte: frames.length > 0 ? parseInt(frames.charAt(position), 16) : 0

    // five bars, four gaps of a quarter bar, half a bar of black at either side
    readonly property real barWidth: width / 7
    readonly property real gap: barWidth / 4
    readonly property real markerSize: barWidth * 0.6

    height: markerSize + gap + barWidth * 4 + barWidth

    onFramesChanged: {
        position = 0
        clock = true
    }

    Timer {
        interval: Math.max(1, Math.round(1000 / flickerCode.speed))
        repeat: true
        running: flickerCode.running && flickerCode.frames.length > 0
        onTriggered: {
            if (flickerCode.clock) {
                flickerCode.clock = false
            } else {
                flickerCode.clock = true
                flickerCode.position = (flickerCode.position + 1) % flickerCode.frames.length
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    // where the generator's arrows go: above the outer bars
    Repeater {
        model: [0, 4]

        Text {
            x: flickerCode.barWidth / 2 + modelData * (flickerCode.barWidth + flickerCode.gap)
               + (flickerCode.barWidth - width) / 2
            y: flickerCode.gap / 2
            text: "▼"
            color: "white"
            font.pixelSize: flickerCode.markerSize
        }
    }

    Row {
        x: flickerCode.barWidth / 2
        y: flickerCode.markerSize + flickerCode.gap
        spacing: flickerCode.gap

        Repeater {
            model: 5

            Rectangle {
                width: flickerCode.barWidth
                height: flickerCode.barWidth * 4
                color: (index === 0 ? flickerCode.clock : ((flickerCode.halfByte >> (index - 1)) & 1) === 1)
                       ? "white" : "black"
            }
        }
    }
}
