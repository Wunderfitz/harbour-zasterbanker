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

#ifndef CHECK_H
#define CHECK_H

#include <QString>
#include <QVariant>

#include <cstdio>

/*
 * The tests' bookkeeping, as in the author's other apps: every check prints
 * PASS or FAIL with what it saw, and main() returns the number of failures.
 */
namespace Check
{
extern int failures;

inline bool that(const char *name, bool ok, const QString &detail = QString())
{
    std::printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name, detail.isEmpty() ? "" : ": ",
                detail.toUtf8().constData());
    if (!ok) {
        ++failures;
    }
    return ok;
}

inline QString show(const QVariant &value)
{
    if (!value.isValid()) {
        return QStringLiteral("(none)");
    }
    if (value.type() == QVariant::String) {
        return QLatin1Char('"') + value.toString() + QLatin1Char('"');
    }
    if (value.type() == QVariant::Double) {
        return QString::number(value.toDouble(), 'f', 4);
    }
    return value.toString();
}

inline bool equal(const char *name, const QVariant &actual, const QVariant &expected)
{
    const bool ok = actual.type() == QVariant::Double || expected.type() == QVariant::Double
            ? qAbs(actual.toDouble() - expected.toDouble()) < 0.00001 && actual.isValid()
            : actual == expected;
    return that(name, ok, ok ? show(actual) : show(actual) + QStringLiteral(", expected ") + show(expected));
}

inline void section(const char *name)
{
    std::printf("\n== %s\n", name);
}
}

#endif // CHECK_H
