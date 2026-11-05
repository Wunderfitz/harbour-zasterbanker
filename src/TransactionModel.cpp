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

#include "TransactionModel.h"

#include <QDate>
#include <QSet>

#include <algorithm>

TransactionModel::TransactionModel(QObject *parent)
    : QAbstractListModel(parent)
    , currentAccountId(0)
{
}

int TransactionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : transactions.value(currentAccountId).size();
}

QVariant TransactionModel::data(const QModelIndex &index, int role) const
{
    const auto current = transactions.constFind(currentAccountId);
    if (!index.isValid() || current == transactions.constEnd() || index.row() >= current->size()) {
        return QVariant();
    }
    const QVariantMap &transaction = current->at(index.row());
    switch (role) {
    case DateRole: return transaction.value(QStringLiteral("date"));
    case HasAmountRole: return transaction.value(QStringLiteral("hasAmount"));
    case AmountRole: return transaction.value(QStringLiteral("amount"));
    case CurrencyRole: return transaction.value(QStringLiteral("currency"));
    case NameRole: return transaction.value(QStringLiteral("name"));
    case IbanRole: return transaction.value(QStringLiteral("iban"));
    case PurposeRole: return transaction.value(QStringLiteral("purpose"));
    case KindRole: return transaction.value(QStringLiteral("kind"));
    case PendingRole: return transaction.value(QStringLiteral("pending"));
    }
    return QVariant();
}

QHash<int, QByteArray> TransactionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(DateRole, "date");
    roles.insert(HasAmountRole, "hasAmount");
    roles.insert(AmountRole, "amount");
    roles.insert(CurrencyRole, "currency");
    roles.insert(NameRole, "name");
    roles.insert(IbanRole, "iban");
    roles.insert(PurposeRole, "purpose");
    roles.insert(KindRole, "kind");
    roles.insert(PendingRole, "pending");
    return roles;
}

bool TransactionModel::loaded() const
{
    return transactions.contains(currentAccountId);
}

void TransactionModel::showAccount(int accountId)
{
    beginResetModel();
    currentAccountId = static_cast<uint>(accountId);
    endResetModel();
    emit countChanged();
    emit loadedChanged();
}

void TransactionModel::setTransactions(uint accountId, const QVariantList &newTransactions)
{
    QList<QVariantMap> sorted;
    for (const QVariant &transaction : newTransactions) {
        sorted.append(transaction.toMap());
    }
    // Banks list the oldest first. Within a day their order is all there is to go
    // by, so it is reversed first and the sort keeps it.
    std::reverse(sorted.begin(), sorted.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](const QVariantMap &a, const QVariantMap &b) {
        const bool aPending = a.value(QStringLiteral("pending")).toBool();
        const bool bPending = b.value(QStringLiteral("pending")).toBool();
        if (aPending != bPending) {
            return aPending;
        }
        return a.value(QStringLiteral("date")).toDate() > b.value(QStringLiteral("date")).toDate();
    });

    const bool current = accountId == currentAccountId;
    if (current) {
        beginResetModel();
    }
    transactions.insert(accountId, sorted);
    if (current) {
        endResetModel();
        emit countChanged();
        emit loadedChanged();
    }
}

void TransactionModel::retainAccounts(const QVariantList &accounts)
{
    QSet<uint> remaining;
    for (const QVariant &account : accounts) {
        remaining.insert(account.toMap().value(QStringLiteral("accountId")).toUInt());
    }
    const QList<uint> known = transactions.keys();
    for (const uint accountId : known) {
        if (!remaining.contains(accountId)) {
            const bool current = accountId == currentAccountId;
            if (current) {
                beginResetModel();
            }
            transactions.remove(accountId);
            if (current) {
                endResetModel();
                emit countChanged();
                emit loadedChanged();
            }
        }
    }
}
