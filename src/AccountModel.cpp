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

#include <algorithm>

AccountModel::AccountModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AccountModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : accounts.size();
}

bool AccountModel::hasBalances() const
{
    for (const QVariantMap &account : accounts) {
        if (balances.contains(account.value(QStringLiteral("accountId")).toUInt())) {
            return true;
        }
    }
    return false;
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
    return roles;
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
                         QVector<int>() << HasBalanceRole << BalanceRole << BalanceCurrencyRole << BalanceDateRole);
    }
    emit balancesChanged();
}
