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

#include "AccountModel.h"

#include <QMap>
#include <QSet>

#include <algorithm>

AccountModel::AccountModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AccountModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : accounts.size();
}

// The bank accesses that have balances, with their totals, for the cover.
QVariantList AccountModel::syncedBanks() const
{
    QVariantList result;
    QSet<uint> seen;
    for (const QVariantMap &account : accounts) {
        const uint bankAccessId = account.value(QStringLiteral("bankAccessId")).toUInt();
        if (seen.contains(bankAccessId)) {
            continue;
        }
        seen.insert(bankAccessId);
        const QVariantList totals = bankTotals(bankAccessId);
        if (!totals.isEmpty()) {
            QVariantMap bank;
            bank.insert(QStringLiteral("bankName"), account.value(QStringLiteral("bankName")));
            bank.insert(QStringLiteral("totals"), totals);
            bank.insert(QStringLiteral("complete"), bankTotalComplete(bankAccessId));
            result.append(bank);
        }
    }
    return result;
}

QVariant AccountModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= accounts.size()) {
        return QVariant();
    }
    const QVariantMap &account = accounts.at(index.row());
    const uint accountId = account.value(QStringLiteral("accountId")).toUInt();
    switch (role) {
    case AccountIdRole: return accountId;
    case BankAccessIdRole: return account.value(QStringLiteral("bankAccessId"));
    case BankNameRole: return account.value(QStringLiteral("bankName"));
    case BankCodeRole: return account.value(QStringLiteral("bankCode"));
    case AccountNumberRole: return account.value(QStringLiteral("accountNumber"));
    case IbanRole: return account.value(QStringLiteral("iban"));
    case NameRole: return account.value(QStringLiteral("name"));
    case OwnerRole: return account.value(QStringLiteral("owner"));
    case CurrencyRole: return account.value(QStringLiteral("currency"));
    case HasBalanceRole: return balances.contains(accountId);
    case BalanceRole: return balances.value(accountId).value(QStringLiteral("balance"), 0.0);
    case BalanceCurrencyRole: return balances.value(accountId).value(QStringLiteral("currency"));
    case BalanceDateRole: return balances.value(accountId).value(QStringLiteral("date"));
    case BankTotalsRole: return bankTotals(account.value(QStringLiteral("bankAccessId")).toUInt());
    case BankTotalCompleteRole: return bankTotalComplete(account.value(QStringLiteral("bankAccessId")).toUInt());
    }
    return QVariant();
}

QHash<int, QByteArray> AccountModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(AccountIdRole, "accountId");
    roles.insert(BankAccessIdRole, "bankAccessId");
    roles.insert(BankNameRole, "bankName");
    roles.insert(BankCodeRole, "bankCode");
    roles.insert(AccountNumberRole, "accountNumber");
    roles.insert(IbanRole, "iban");
    roles.insert(NameRole, "name");
    roles.insert(OwnerRole, "owner");
    roles.insert(CurrencyRole, "currency");
    roles.insert(HasBalanceRole, "hasBalance");
    roles.insert(BalanceRole, "balance");
    roles.insert(BalanceCurrencyRole, "balanceCurrency");
    roles.insert(BalanceDateRole, "balanceDate");
    roles.insert(BankTotalsRole, "bankTotals");
    roles.insert(BankTotalCompleteRole, "bankTotalComplete");
    return roles;
}

/*
 * One entry per currency, as amounts in different currencies cannot be added
 * up. Rounded to cents because the sum is one of doubles: what should be zero
 * may come out as a tiny negative number, which would show as a red "-0,00".
 */
QVariantList AccountModel::bankTotals(uint bankAccessId) const
{
    QMap<QString, double> totals;
    for (const QVariantMap &account : accounts) {
        if (account.value(QStringLiteral("bankAccessId")).toUInt() == bankAccessId) {
            const auto balance = balances.constFind(account.value(QStringLiteral("accountId")).toUInt());
            if (balance != balances.constEnd()) {
                totals[balance->value(QStringLiteral("currency")).toString()] += balance->value(QStringLiteral("balance")).toDouble();
            }
        }
    }
    QVariantList result;
    for (auto total = totals.constBegin(); total != totals.constEnd(); ++total) {
        QVariantMap entry;
        entry.insert(QStringLiteral("amount"), qRound64(total.value() * 100) / 100.0);
        entry.insert(QStringLiteral("currency"), total.key());
        result.append(entry);
    }
    return result;
}

// Banks need not send a balance for every account, a securities account for one.
bool AccountModel::bankTotalComplete(uint bankAccessId) const
{
    for (const QVariantMap &account : accounts) {
        if (account.value(QStringLiteral("bankAccessId")).toUInt() == bankAccessId
                && !balances.contains(account.value(QStringLiteral("accountId")).toUInt())) {
            return false;
        }
    }
    return true;
}

void AccountModel::setAccounts(const QVariantList &newAccounts)
{
    beginResetModel();
    accounts.clear();
    for (const QVariant &account : newAccounts) {
        accounts.append(account.toMap());
    }
    // Grouped by bank for the list's sections, in the order the bank listed them.
    std::stable_sort(accounts.begin(), accounts.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("bankAccessId")).toUInt() < b.value(QStringLiteral("bankAccessId")).toUInt();
    });
    endResetModel();
    emit countChanged();
    emit balancesChanged();
}

void AccountModel::setBalances(const QVariantList &newBalances)
{
    for (const QVariant &balance : newBalances) {
        const QVariantMap entry = balance.toMap();
        balances.insert(entry.value(QStringLiteral("accountId")).toUInt(), entry);
    }
    if (!accounts.isEmpty()) {
        emit dataChanged(index(0), index(accounts.size() - 1),
                         QVector<int>() << HasBalanceRole << BalanceRole << BalanceCurrencyRole << BalanceDateRole
                                        << BankTotalsRole << BankTotalCompleteRole);
    }
    emit balancesChanged();
}
