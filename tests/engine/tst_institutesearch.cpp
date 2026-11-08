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

/*
 * The bank search on a made-up list (tests/data/institutes.csv, turned into a
 * database by tools/import-banklist.py, as the real one is).
 */

#include "Check.h"
#include "InstituteSearchWorker.h"

namespace {

QVariantList search(const QString &database, const QString &query)
{
    InstituteSearchWorker worker(database);
    worker.setQuery(query);
    QVariantList results;
    QObject::connect(&worker, &InstituteSearchWorker::resultsReady, &worker,
                     [&results](const QString &, const QVariantList &found) { results = found; }, Qt::DirectConnection);
    worker.start();
    worker.wait();
    return results;
}

QString codes(const QVariantList &results)
{
    QStringList list;
    for (const QVariant &result : results) {
        list.append(result.toMap().value(QStringLiteral("bankCode")).toString());
    }
    return list.join(QLatin1Char(' '));
}

}

void runInstituteSearchTests(const QString &database)
{
    Check::section("Bank search");
    Check::equal("fold: lower case, umlauts spelled out", InstituteSearchWorker::fold(QString::fromUtf8("Äpfel Öl Übel Straße")),
                 QStringLiteral("aepfel oel uebel strasse"));

    Check::equal("by name", codes(search(database, QStringLiteral("volksbank"))), QStringLiteral("10010010"));
    Check::equal("by city, with umlauts", codes(search(database, QString::fromUtf8("München"))), QStringLiteral("10010010"));
    Check::equal("by city, spelled out", codes(search(database, QStringLiteral("muenchen"))), QStringLiteral("10010010"));
    Check::equal("with ß", codes(search(database, QString::fromUtf8("Weißenburg"))), QStringLiteral("20020020"));
    Check::equal("by bank code", codes(search(database, QStringLiteral("2002"))), QStringLiteral("20020020"));
    Check::equal("by BIC", codes(search(database, QStringLiteral("testdeff400"))), QStringLiteral("40040040"));
    // Ordered by name as SQLite compares them: "X" before "_".
    Check::equal("every word has to match", codes(search(database, QStringLiteral("bank teststadt"))), QStringLiteral("50050050 40040040"));
    Check::equal("in any order", codes(search(database, QStringLiteral("eg  fuer"))), QStringLiteral("20020020"));
    Check::equal("% is no wildcard", codes(search(database, QStringLiteral("100%"))), QStringLiteral("40040040"));
    Check::equal("nor is _", codes(search(database, QStringLiteral("k_1"))), QStringLiteral("40040040"));
    Check::equal("nothing found", search(database, QStringLiteral("zzz")).size(), 0);
    Check::equal("an empty query finds nothing", search(database, QStringLiteral("  ")).size(), 0);

    const QVariantMap bank = search(database, QStringLiteral("volksbank")).value(0).toMap();
    Check::equal("with its name", bank.value(QStringLiteral("name")), QStringLiteral("Volksbank Musterstadt eG"));
    Check::equal("its BIC", bank.value(QStringLiteral("bic")), QStringLiteral("TESTDEFF100"));
    Check::equal("its city", bank.value(QStringLiteral("city")), QString::fromUtf8("München"));
    Check::equal("and its server", bank.value(QStringLiteral("serverUrl")), QStringLiteral("https://fints.example.test/volksbank"));
}
