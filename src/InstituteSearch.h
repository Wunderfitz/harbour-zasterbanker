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

#ifndef INSTITUTESEARCH_H
#define INSTITUTESEARCH_H

#include <QObject>
#include <QString>
#include <QVariantList>

class InstituteSearchWorker;

/**
 * Search as you type over the institutes that offer FinTS with PIN/TAN,
 * exposed as the "instituteSearch" context property.
 *
 * Each keystroke sets the query; one worker runs at a time, and when it comes
 * back with a query that has meanwhile changed, it runs once more with the
 * latest one. Keystrokes in between never queue up.
 */
class InstituteSearch : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)

public:
    InstituteSearch(const QString &databasePath, QObject *parent = nullptr);
    ~InstituteSearch();

    QString query() const;
    void setQuery(const QString &query);
    QVariantList results() const;
    bool searching() const;

signals:
    void queryChanged();
    void resultsChanged();
    void searchingChanged();

private slots:
    void handleResults(const QString &query, const QVariantList &results);

private:
    void startWorker();

    InstituteSearchWorker *worker;
    QString currentQuery;
    QVariantList currentResults;
};

#endif // INSTITUTESEARCH_H
