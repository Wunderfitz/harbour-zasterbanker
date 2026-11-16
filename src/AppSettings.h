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

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QSettings>

/**
 * The app's own settings, exposed as the "appSettings" context property: how
 * the TAN dialog shows a chipTAN flicker code, as the user adjusted it to their
 * TAN generator, and whether debug logging is on. Nothing about banking; a
 * plain file in the app's configuration folder,
 * ~/.config/de.ygriega/zasterbanker/settings.conf.
 */
class AppSettings : public QObject
{
    Q_OBJECT
    /** Width of the flicker code in pixels; 0 until the user changes it. */
    Q_PROPERTY(int flickerWidth READ flickerWidth WRITE setFlickerWidth NOTIFY flickerWidthChanged)
    /** Frames per second, each half of a half-byte's clock cycle being one. */
    Q_PROPERTY(int flickerSpeed READ flickerSpeed WRITE setFlickerSpeed NOTIFY flickerSpeedChanged)
    Q_PROPERTY(int minimumFlickerSpeed READ minimumFlickerSpeed CONSTANT)
    Q_PROPERTY(int maximumFlickerSpeed READ maximumFlickerSpeed CONSTANT)
    /** Whether the debug log records; only the setting, the log itself is never stored. */
    Q_PROPERTY(bool debugLogging READ debugLogging WRITE setDebugLogging NOTIFY debugLoggingChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    int flickerWidth() const;
    void setFlickerWidth(int width);
    int flickerSpeed() const;
    void setFlickerSpeed(int speed);
    int minimumFlickerSpeed() const;
    int maximumFlickerSpeed() const;
    bool debugLogging() const;
    void setDebugLogging(bool enabled);

signals:
    void flickerWidthChanged();
    void flickerSpeedChanged();
    void debugLoggingChanged();

private:
    QSettings settings;
};

#endif // APPSETTINGS_H
