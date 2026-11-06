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

#ifndef HOLDINGMODEL_H
#define HOLDINGMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

/**
 * What the depot currently shown holds, in the bank's order.
 *
 * What the bank sent is kept per account for as long as the app runs, so that
 * opening a depot again does not ask the bank (and possibly for a TAN) again.
 * Like balances and transactions, holdings live in memory only.
 */
class HoldingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY contentChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY contentChanged)
    Q_PROPERTY(QVariantList totals READ totals NOTIFY contentChanged)
    Q_PROPERTY(bool complete READ complete NOTIFY contentChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IdKindRole,
        IdentifierRole,
        HasUnitsRole,
        UnitsRole,
        HasPriceRole,
        PriceRole,
        CurrencyRole,
        PriceDateRole,
        HasValueRole,
        ValueRole
    };

    explicit HoldingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loaded() const;
    /** One entry per currency, as amounts in different currencies cannot be added up. */
    QVariantList totals() const;
    /** Whether every position has a value, and so is part of the totals. */
    bool complete() const;

    Q_INVOKABLE void showAccount(int accountId);

public slots:
    void setHoldings(uint accountId, const QVariantList &holdings);
    /** Forgets the holdings of accounts that are gone. */
    void retainAccounts(const QVariantList &accounts);

signals:
    void contentChanged();

private:
    QHash<uint, QList<QVariantMap>> holdings;
    uint currentAccountId;
};

#endif // HOLDINGMODEL_H
