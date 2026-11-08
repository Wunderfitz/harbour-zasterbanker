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
 * The models behind the overview, the cover, the transactions and the depot
 * page: grouping, totals per bank and currency, dates, order, what is kept.
 */

#include "AccountModel.h"
#include "Check.h"
#include "HoldingModel.h"
#include "TransactionModel.h"

#include <QDate>
#include <QDateTime>

#include <cmath>

namespace {

QVariantMap account(uint accountId, uint bankAccessId, const char *name)
{
    QVariantMap map;
    map.insert(QStringLiteral("accountId"), accountId);
    map.insert(QStringLiteral("bankAccessId"), bankAccessId);
    map.insert(QStringLiteral("bankName"), QStringLiteral("Bank %1").arg(bankAccessId));
    map.insert(QStringLiteral("name"), QString::fromUtf8(name));
    return map;
}

QVariantMap balance(uint accountId, double amount, const char *currency, const QDate &date = QDate(),
                    const QDateTime &fetched = QDateTime())
{
    QVariantMap map;
    map.insert(QStringLiteral("accountId"), accountId);
    map.insert(QStringLiteral("balance"), amount);
    map.insert(QStringLiteral("currency"), QString::fromLatin1(currency));
    if (date.isValid()) {
        map.insert(QStringLiteral("date"), date);
    }
    if (fetched.isValid()) {
        map.insert(QStringLiteral("fetched"), fetched);
    }
    return map;
}

QString totalsText(const QVariant &totals)
{
    QStringList parts;
    for (const QVariant &total : totals.toList()) {
        const QVariantMap map = total.toMap();
        parts.append(QString::number(map.value(QStringLiteral("amount")).toDouble(), 'f', 2) + QLatin1Char(' ')
                     + map.value(QStringLiteral("currency")).toString());
    }
    return parts.join(QStringLiteral(", "));
}

template <typename Model>
QVariant role(const Model &model, int row, int role)
{
    return model.data(model.index(row), role);
}

void accountTests()
{
    AccountModel model;
    // A depot without a balance at bank 1, three accounts adding up to zero and
    // one in dollars at bank 2, listed in the banks' order.
    model.setAccounts(QVariantList() << account(20, 2, "Giro") << account(10, 1, "Giro") << account(21, 2, "Tagesgeld")
                                     << account(11, 1, "Depot") << account(22, 2, "Sparen") << account(23, 2, "Dollar"));
    Check::equal("accounts: all listed", model.rowCount(), 6);
    QStringList order;
    for (int row = 0; row < model.rowCount(); row++) {
        order.append(role(model, row, AccountModel::BankAccessIdRole).toString() + QLatin1Char('/')
                     + role(model, row, AccountModel::NameRole).toString());
    }
    Check::equal("accounts: grouped by bank access, each in the bank's order", order.join(QLatin1Char(' ')),
                 QStringLiteral("1/Giro 1/Depot 2/Giro 2/Tagesgeld 2/Sparen 2/Dollar"));
    Check::equal("accounts: no balance before one arrives", role(model, 0, AccountModel::HasBalanceRole), false);
    Check::that("accounts: no bank is on the cover without balances", model.syncedBanks().isEmpty());

    const QDateTime fetched(QDate(2026, 11, 8), QTime(12, 0), Qt::UTC);
    model.setBalances(QVariantList() << balance(10, 1500.25, "EUR", QDate(2026, 11, 7))
                                     << balance(20, 0.1, "EUR") << balance(21, 0.2, "EUR") << balance(22, -0.3, "EUR", QDate(), fetched)
                                     << balance(23, 10, "USD"));
    Check::equal("accounts: the balance arrives", role(model, 0, AccountModel::BalanceRole), 1500.25);
    Check::equal("accounts: with the bank's date", role(model, 0, AccountModel::BalanceDateRole), QDate(2026, 11, 7));
    Check::equal("accounts: or the day it was fetched", role(model, 4, AccountModel::BalanceDateRole), fetched.toLocalTime().date());
    Check::equal("accounts: total of bank 1", totalsText(role(model, 0, AccountModel::BankTotalsRole)), QStringLiteral("1500.25 EUR"));
    Check::equal("accounts: bank 1 is missing a balance", role(model, 0, AccountModel::BankTotalCompleteRole), false);
    Check::equal("accounts: one total per currency, rounded to cents", totalsText(role(model, 2, AccountModel::BankTotalsRole)),
                 QStringLiteral("0.00 EUR, 10.00 USD"));
    const double zero = role(model, 2, AccountModel::BankTotalsRole).toList().value(0).toMap().value(QStringLiteral("amount")).toDouble();
    Check::that("accounts: a total of zero is not a negative zero", !std::signbit(zero));
    Check::equal("accounts: bank 2 is complete", role(model, 2, AccountModel::BankTotalCompleteRole), true);
    Check::equal("accounts: both banks on the cover", model.syncedBanks().size(), 2);

    model.setBalances(QVariantList() << balance(23, 12.5, "USD"));
    Check::equal("accounts: a new balance replaces the old", role(model, 5, AccountModel::BalanceRole), 12.5);
    Check::equal("accounts: the others stay", role(model, 2, AccountModel::BalanceRole), 0.1);
}

QVariantMap transaction(const char *name, const QDate &date, bool pending = false)
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), QString::fromUtf8(name));
    map.insert(QStringLiteral("date"), date);
    map.insert(QStringLiteral("pending"), pending);
    map.insert(QStringLiteral("hasAmount"), true);
    map.insert(QStringLiteral("amount"), -1.0);
    return map;
}

void transactionTests()
{
    TransactionModel model;
    model.showAccount(7);
    Check::equal("transactions: not loaded before they arrive", model.loaded(), false);
    // As banks send them: oldest first, the pending ones apart.
    model.setTransactions(7, QVariantList() << transaction("A", QDate(2026, 11, 1)) << transaction("B", QDate(2026, 11, 3))
                                            << transaction("C", QDate(2026, 11, 3)) << transaction("P", QDate(2026, 11, 7), true)
                                            << transaction("D", QDate(2026, 11, 5)));
    Check::equal("transactions: loaded", model.loaded(), true);
    QString order;
    for (int row = 0; row < model.rowCount(); row++) {
        order += role(model, row, TransactionModel::NameRole).toString();
    }
    Check::equal("transactions: pending first, then newest first, a day's in reverse", order, QStringLiteral("PDCBA"));

    model.setTransactions(8, QVariantList() << transaction("X", QDate(2026, 11, 2)));
    Check::equal("transactions: another account's do not show", model.rowCount(), 5);
    model.showAccount(8);
    Check::equal("transactions: until it is shown", role(model, 0, TransactionModel::NameRole), QStringLiteral("X"));
    model.retainAccounts(QVariantList() << account(7, 1, "Giro"));
    Check::equal("transactions: those of a removed account are forgotten", model.loaded(), false);
    model.showAccount(7);
    Check::equal("transactions: the others kept", model.rowCount(), 5);
}

QVariantMap holding(const char *name, const char *currency, bool hasValue, double value)
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), QString::fromUtf8(name));
    map.insert(QStringLiteral("currency"), QString::fromLatin1(currency));
    map.insert(QStringLiteral("hasValue"), hasValue);
    map.insert(QStringLiteral("value"), value);
    return map;
}

void holdingTests()
{
    HoldingModel model;
    model.showAccount(11);
    Check::equal("holdings: not loaded before they arrive", model.loaded(), false);
    model.setHoldings(11, QVariantList() << holding("SAP", "EUR", true, 1234.5) << holding("Allianz", "EUR", true, 0.1)
                                         << holding("Apple", "USD", true, 200.0) << holding("Anleihe", "", false, 0.0));
    Check::equal("holdings: in the bank's order", role(model, 0, HoldingModel::NameRole), QStringLiteral("SAP"));
    Check::equal("holdings: one total per currency", totalsText(model.totals()), QStringLiteral("1234.60 EUR, 200.00 USD"));
    Check::equal("holdings: incomplete with a position without value", model.complete(), false);
    model.setHoldings(11, QVariantList());
    Check::that("holdings: an empty depot is loaded and complete", model.loaded() && model.complete() && model.totals().isEmpty());
}

}

void runModelTests()
{
    Check::section("Models");
    accountTests();
    transactionTests();
    holdingTests();
}
