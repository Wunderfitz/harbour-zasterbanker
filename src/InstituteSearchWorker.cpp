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

#include "InstituteSearchWorker.h"

#include <QDebug>
#include <QRegExp>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariantMap>

namespace {
const int MAX_RESULTS = 100;
}

InstituteSearchWorker::InstituteSearchWorker(const QString &databasePath, QObject *parent)
    : QThread(parent)
    , databasePath(databasePath)
{
}

void InstituteSearchWorker::setQuery(const QString &query)
{
    currentQuery = query;
}

QString InstituteSearchWorker::query() const
{
    return currentQuery;
}

QString InstituteSearchWorker::fold(const QString &text)
{
    QString folded = text.toLower();
    folded.replace(QChar(0x00e4), QStringLiteral("ae"));
    folded.replace(QChar(0x00f6), QStringLiteral("oe"));
    folded.replace(QChar(0x00fc), QStringLiteral("ue"));
    folded.replace(QChar(0x00df), QStringLiteral("ss"));
    return folded;
}

void InstituteSearchWorker::run()
{
    QVariantList results;
    const QStringList terms = fold(currentQuery).split(QRegExp(QStringLiteral("\\s+")), QString::SkipEmptyParts);
    if (terms.isEmpty()) {
        emit resultsReady(currentQuery, results);
        return;
    }

    const QString connectionName = QStringLiteral("institutes-%1").arg(reinterpret_cast<quintptr>(this));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!database.open()) {
            qWarning() << "Cannot open institute database" << databasePath << database.lastError().text();
        } else {
            // Every term has to appear somewhere in bank code, BIC, name or city.
            QStringList conditions;
            for (int i = 0; i < terms.size(); i++) {
                conditions.append(QStringLiteral("search_key LIKE ? ESCAPE '\\'"));
            }
            QSqlQuery query(database);
            query.prepare(QStringLiteral("SELECT blz, bic, name, city, url FROM institutes WHERE %1 "
                                         "ORDER BY name, city LIMIT %2").arg(conditions.join(QStringLiteral(" AND "))).arg(MAX_RESULTS));
            for (QString term : terms) {
                term.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
                term.replace(QLatin1Char('%'), QStringLiteral("\\%"));
                term.replace(QLatin1Char('_'), QStringLiteral("\\_"));
                query.addBindValue(QLatin1Char('%') + term + QLatin1Char('%'));
            }
            if (query.exec()) {
                while (query.next()) {
                    QVariantMap institute;
                    institute.insert(QStringLiteral("bankCode"), query.value(0));
                    institute.insert(QStringLiteral("bic"), query.value(1));
                    institute.insert(QStringLiteral("name"), query.value(2));
                    institute.insert(QStringLiteral("city"), query.value(3));
                    institute.insert(QStringLiteral("serverUrl"), query.value(4));
                    results.append(institute);
                }
            } else {
                qWarning() << "Institute query failed" << query.lastError().text();
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    emit resultsReady(currentQuery, results);
}
