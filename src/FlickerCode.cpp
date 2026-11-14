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

#include "FlickerCode.h"

namespace {
const QString SYNC = QStringLiteral("0FFF");
}

QString FlickerCode::frames(const QString &challenge)
{
    const QString code = SYNC + challenge.trimmed().toUpper();
    if (challenge.trimmed().isEmpty() || code.size() % 2 != 0) {
        return QString();
    }
    QString result;
    result.reserve(code.size());
    for (int i = 0; i < code.size(); i += 2) {
        const QChar high = code.at(i);
        const QChar low = code.at(i + 1);
        for (const QChar digit : { high, low }) {
            if (!((digit >= QLatin1Char('0') && digit <= QLatin1Char('9')) || (digit >= QLatin1Char('A') && digit <= QLatin1Char('F')))) {
                return QString();
            }
        }
        // the low half-byte first
        result.append(low);
        result.append(high);
    }
    return result;
}
