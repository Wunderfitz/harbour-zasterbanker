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

#include "AppSettings.h"

#include <QStandardPaths>

namespace {
const QString KEY_FLICKER_WIDTH = QStringLiteral("flickerWidth");
const QString KEY_FLICKER_SPEED = QStringLiteral("flickerSpeed");
// TAN generators read at least 10 frames per second; most manage far more.
const int DEFAULT_FLICKER_SPEED = 15;
const int MINIMUM_FLICKER_SPEED = 5;
const int MAXIMUM_FLICKER_SPEED = 40;
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , settings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/settings.conf"),
               QSettings::NativeFormat)
{
}

int AppSettings::flickerWidth() const
{
    return qMax(0, settings.value(KEY_FLICKER_WIDTH, 0).toInt());
}

void AppSettings::setFlickerWidth(int width)
{
    width = qMax(0, width);
    if (width != flickerWidth()) {
        settings.setValue(KEY_FLICKER_WIDTH, width);
        emit flickerWidthChanged();
    }
}

int AppSettings::flickerSpeed() const
{
    return qBound(MINIMUM_FLICKER_SPEED, settings.value(KEY_FLICKER_SPEED, DEFAULT_FLICKER_SPEED).toInt(),
                  MAXIMUM_FLICKER_SPEED);
}

void AppSettings::setFlickerSpeed(int speed)
{
    speed = qBound(MINIMUM_FLICKER_SPEED, speed, MAXIMUM_FLICKER_SPEED);
    if (speed != flickerSpeed()) {
        settings.setValue(KEY_FLICKER_SPEED, speed);
        emit flickerSpeedChanged();
    }
}

int AppSettings::minimumFlickerSpeed() const
{
    return MINIMUM_FLICKER_SPEED;
}

int AppSettings::maximumFlickerSpeed() const
{
    return MAXIMUM_FLICKER_SPEED;
}
