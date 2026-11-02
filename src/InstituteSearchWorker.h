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

#ifndef INSTITUTESEARCHWORKER_H
#define INSTITUTESEARCHWORKER_H

#include <QString>
#include <QThread>
#include <QVariantList>

/**
 * Runs one query against the institute database, off the UI thread.
 *
 * Each run opens its own read-only connection: QtSql connections belong to
 * the thread that opened them.
 */
class InstituteSearchWorker : public QThread
{
    Q_OBJECT

public:
    InstituteSearchWorker(const QString &databasePath, QObject *parent = nullptr);

    void setQuery(const QString &query);
    QString query() const;

    /** Lower case with umlauts spelled out, as tools/import-banklist.py builds the search key. */
    static QString fold(const QString &text);

signals:
    void resultsReady(const QString &query, const QVariantList &results);

protected:
    void run() override;

private:
    const QString databasePath;
    QString currentQuery;
};

#endif // INSTITUTESEARCHWORKER_H
