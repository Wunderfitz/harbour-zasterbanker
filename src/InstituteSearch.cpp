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

#include "InstituteSearch.h"

#include "InstituteSearchWorker.h"

InstituteSearch::InstituteSearch(const QString &databasePath, QObject *parent)
    : QObject(parent)
    , worker(new InstituteSearchWorker(databasePath, this))
{
    connect(worker, &InstituteSearchWorker::resultsReady, this, &InstituteSearch::handleResults);
    connect(worker, &QThread::finished, this, [this]() {
        if (worker->query() != currentQuery) {
            startWorker();
        } else {
            emit searchingChanged();
        }
    });
}

InstituteSearch::~InstituteSearch()
{
    worker->wait();
}

QString InstituteSearch::query() const
{
    return currentQuery;
}

void InstituteSearch::setQuery(const QString &query)
{
    if (query == currentQuery) {
        return;
    }
    currentQuery = query;
    emit queryChanged();
    if (!worker->isRunning()) {
        startWorker();
    }
}

QVariantList InstituteSearch::results() const
{
    return currentResults;
}

bool InstituteSearch::searching() const
{
    return worker->isRunning();
}

void InstituteSearch::startWorker()
{
    worker->setQuery(currentQuery);
    worker->start();
    emit searchingChanged();
}

void InstituteSearch::handleResults(const QString &query, const QVariantList &results)
{
    // Stale results are still better than an empty list while typing on.
    Q_UNUSED(query)
    currentResults = results;
    emit resultsChanged();
}
