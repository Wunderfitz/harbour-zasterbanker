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

#ifndef TANIMAGEPROVIDER_H
#define TANIMAGEPROVIDER_H

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

/**
 * Hands the current photoTAN or QR-TAN challenge to QML as image://tan/<n>.
 *
 * The challenge stays in memory; written to a temporary file it would
 * outlive the dialog. requestImage() may run on QML's image loader thread,
 * hence the mutex.
 */
class TanImageProvider : public QQuickImageProvider
{
public:
    TanImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    /** Returns the URL to show the image under; a new one each time, so QML does not cache. */
    QString setImage(const QImage &image);
    void clear();

private:
    QMutex mutex;
    QImage image;
    int generation;
};

#endif // TANIMAGEPROVIDER_H
