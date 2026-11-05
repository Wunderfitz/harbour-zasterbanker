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

#ifndef TRANSACTIONMODEL_H
#define TRANSACTIONMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

/**
 * The transactions of the account currently shown, newest first, with the ones
 * still pending on top.
 *
 * What the bank sent is kept per account for as long as the app runs, so that
 * opening an account again does not ask the bank (and possibly for a TAN) again.
 * Like balances, transactions live in memory only.
 */
class TransactionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)

public:
    enum Roles {
        DateRole = Qt::UserRole + 1,
        HasAmountRole,
        AmountRole,
        CurrencyRole,
        NameRole,
        IbanRole,
        PurposeRole,
        KindRole,
        PendingRole
    };

    explicit TransactionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loaded() const;

    Q_INVOKABLE void showAccount(int accountId);

public slots:
    void setTransactions(uint accountId, const QVariantList &transactions);
    /** Forgets the transactions of accounts that are gone. */
    void retainAccounts(const QVariantList &accounts);

signals:
    void countChanged();
    void loadedChanged();

private:
    QHash<uint, QList<QVariantMap>> transactions;
    uint currentAccountId;
};

#endif // TRANSACTIONMODEL_H
