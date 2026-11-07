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

#ifndef ACCOUNTMODEL_H
#define ACCOUNTMODEL_H

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

/**
 * The accounts of all bank accesses, with the last balance fetched for each.
 *
 * The banking thread keeps the last balances in the encrypted store and
 * publishes them at start, so the last known ones show until the user fetches
 * new ones. Each shows its date, so an old one does not pass for current.
 */
class AccountModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QVariantList syncedBanks READ syncedBanks NOTIFY balancesChanged)

public:
    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        BankAccessIdRole,
        BankNameRole,
        BankCodeRole,
        AccountNumberRole,
        IbanRole,
        NameRole,
        OwnerRole,
        CurrencyRole,
        HasBalanceRole,
        BalanceRole,
        BalanceCurrencyRole,
        BalanceDateRole,
        BankTotalsRole,
        BankTotalCompleteRole,
        IsDepotRole
    };

    explicit AccountModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariantList syncedBanks() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setAccounts(const QVariantList &accounts);
    void setBalances(const QVariantList &balances);

signals:
    void countChanged();
    void balancesChanged();

private:
    QVariantList bankTotals(uint bankAccessId) const;
    bool bankTotalComplete(uint bankAccessId) const;
    QVariant balanceDate(uint accountId) const;

    QList<QVariantMap> accounts;
    QHash<uint, QVariantMap> balances;
};

#endif // ACCOUNTMODEL_H
