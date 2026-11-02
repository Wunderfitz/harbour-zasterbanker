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

#include "TanImageProvider.h"

#include <QMutexLocker>

TanImageProvider::TanImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
    , generation(0)
{
}

QImage TanImageProvider::requestImage(const QString &, QSize *size, const QSize &requestedSize)
{
    QMutexLocker locker(&mutex);
    if (size) {
        *size = image.size();
    }
    if (image.isNull() || !requestedSize.isValid()) {
        return image;
    }
    // A matrix code has to stay crisp for the bank's app to read it: never
    // smooth it, and only ever scale it by whole pixels.
    const int factor = qMax(1, qMin(requestedSize.width() / qMax(1, image.width()),
                                    requestedSize.height() / qMax(1, image.height())));
    return image.scaled(image.size() * factor, Qt::KeepAspectRatio, Qt::FastTransformation);
}

QString TanImageProvider::setImage(const QImage &newImage)
{
    QMutexLocker locker(&mutex);
    image = newImage;
    return QStringLiteral("image://tan/%1").arg(++generation);
}

void TanImageProvider::clear()
{
    QMutexLocker locker(&mutex);
    image = QImage();
}
