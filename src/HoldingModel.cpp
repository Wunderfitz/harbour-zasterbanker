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

#include "HoldingModel.h"

#include <QMap>
#include <QSet>

HoldingModel::HoldingModel(QObject *parent)
    : QAbstractListModel(parent)
    , currentAccountId(0)
{
}

int HoldingModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : holdings.value(currentAccountId).size();
}

QVariant HoldingModel::data(const QModelIndex &index, int role) const
{
    const auto current = holdings.constFind(currentAccountId);
    if (!index.isValid() || current == holdings.constEnd() || index.row() >= current->size()) {
        return QVariant();
    }
    const QVariantMap &holding = current->at(index.row());
    switch (role) {
    case NameRole: return holding.value(QStringLiteral("name"));
    case IdKindRole: return holding.value(QStringLiteral("idKind"));
    case IdentifierRole: return holding.value(QStringLiteral("identifier"));
    case HasUnitsRole: return holding.value(QStringLiteral("hasUnits"));
    case UnitsRole: return holding.value(QStringLiteral("units"));
    case HasPriceRole: return holding.value(QStringLiteral("hasPrice"));
    case PriceRole: return holding.value(QStringLiteral("price"));
    case CurrencyRole: return holding.value(QStringLiteral("currency"));
    case PriceDateRole: return holding.value(QStringLiteral("priceDate"));
    case HasValueRole: return holding.value(QStringLiteral("hasValue"));
    case ValueRole: return holding.value(QStringLiteral("value"));
    }
    return QVariant();
}

QHash<int, QByteArray> HoldingModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "name");
    roles.insert(IdKindRole, "idKind");
    roles.insert(IdentifierRole, "identifier");
    roles.insert(HasUnitsRole, "hasUnits");
    roles.insert(UnitsRole, "units");
    roles.insert(HasPriceRole, "hasPrice");
    roles.insert(PriceRole, "price");
    roles.insert(CurrencyRole, "currency");
    roles.insert(PriceDateRole, "priceDate");
    roles.insert(HasValueRole, "hasValue");
    roles.insert(ValueRole, "value");
    return roles;
}

bool HoldingModel::loaded() const
{
    return holdings.contains(currentAccountId);
}

/*
 * Rounded to cents because the sum is one of doubles, like the totals per bank
 * in AccountModel.
 */
QVariantList HoldingModel::totals() const
{
    QMap<QString, double> sums;
    for (const QVariantMap &holding : holdings.value(currentAccountId)) {
        if (holding.value(QStringLiteral("hasValue")).toBool()) {
            sums[holding.value(QStringLiteral("currency")).toString()] += holding.value(QStringLiteral("value")).toDouble();
        }
    }
    QVariantList result;
    for (auto sum = sums.constBegin(); sum != sums.constEnd(); ++sum) {
        QVariantMap entry;
        entry.insert(QStringLiteral("currency"), sum.key());
        entry.insert(QStringLiteral("amount"), qRound64(sum.value() * 100) / 100.0);
        result.append(entry);
    }
    return result;
}

bool HoldingModel::complete() const
{
    for (const QVariantMap &holding : holdings.value(currentAccountId)) {
        if (!holding.value(QStringLiteral("hasValue")).toBool()) {
            return false;
        }
    }
    return true;
}

void HoldingModel::showAccount(int accountId)
{
    beginResetModel();
    currentAccountId = static_cast<uint>(accountId);
    endResetModel();
    emit contentChanged();
}

void HoldingModel::setHoldings(uint accountId, const QVariantList &newHoldings)
{
    QList<QVariantMap> entries;
    for (const QVariant &holding : newHoldings) {
        entries.append(holding.toMap());
    }
    const bool current = accountId == currentAccountId;
    if (current) {
        beginResetModel();
    }
    holdings.insert(accountId, entries);
    if (current) {
        endResetModel();
        emit contentChanged();
    }
}

void HoldingModel::retainAccounts(const QVariantList &accounts)
{
    QSet<uint> remaining;
    for (const QVariant &account : accounts) {
        remaining.insert(account.toMap().value(QStringLiteral("accountId")).toUInt());
    }
    const QList<uint> known = holdings.keys();
    for (const uint accountId : known) {
        if (!remaining.contains(accountId)) {
            const bool current = accountId == currentAccountId;
            if (current) {
                beginResetModel();
            }
            holdings.remove(accountId);
            if (current) {
                endResetModel();
                emit contentChanged();
            }
        }
    }
}
