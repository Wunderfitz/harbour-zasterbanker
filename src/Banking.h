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

#ifndef BANKING_H
#define BANKING_H

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>

#include "AccountModel.h"
#include "BankingThread.h"
#include "KeyStore.h"
#include "TransactionModel.h"
#include "HoldingModel.h"

class TanImageProvider;

/**
 * QML-facing facade over the banking thread, exposed as the "banking" context
 * property.
 *
 * At start it fetches the data key from Sailfish Secrets (KeyStore), then has
 * the banking thread open the encrypted store and AqBanking with it. QML calls
 * the invokables; whatever the bank wants to know in between (PIN, TAN, a
 * choice) comes back as interactionRequested() and is answered with respond().
 */
class Banking : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString stateMessage READ stateMessage NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(Operation operation READ operation NOTIFY busyChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressTextChanged)
    Q_PROPERTY(AccountModel *accounts READ accounts CONSTANT)
    Q_PROPERTY(TransactionModel *transactions READ transactions CONSTANT)
    Q_PROPERTY(HoldingModel *holdings READ holdings CONSTANT)

public:
    enum State {
        Starting,
        Ready,
        DataUnreadable,
        Failed
    };
    Q_ENUM(State)

    enum Operation {
        NoOperation = -1,
        AddingBankAccess = BankingThread::AddBankAccess,
        FetchingBalances = BankingThread::FetchBalances,
        RemovingBankAccess = BankingThread::RemoveBankAccess,
        ResettingData = BankingThread::ResetData,
        FetchingTransactions = BankingThread::FetchTransactions,
        FetchingHoldings = BankingThread::FetchHoldings
    };
    Q_ENUM(Operation)

    enum Interaction {
        PinInteraction = BankingThread::PinInteraction,
        TanInteraction = BankingThread::TanInteraction,
        TextInteraction = BankingThread::TextInteraction,
        ChoiceInteraction = BankingThread::ChoiceInteraction,
        MessageInteraction = BankingThread::MessageInteraction
    };
    Q_ENUM(Interaction)

    Banking(const QString &dataDirectory, TanImageProvider *tanImageProvider, QObject *parent = nullptr);
    ~Banking();

    State state() const;
    QString stateMessage() const;
    bool busy() const;
    Operation operation() const;
    QString progressText() const;
    AccountModel *accounts();
    TransactionModel *transactions();
    HoldingModel *holdings();

    Q_INVOKABLE void addBankAccess(const QString &bankCode, const QString &bankName, const QString &serverUrl,
                                   const QString &userId, const QString &customerId);
    Q_INVOKABLE void refreshBalances();
    Q_INVOKABLE void fetchTransactions(int accountId);
    Q_INVOKABLE void fetchHoldings(int accountId);
    Q_INVOKABLE void removeBankAccess(int bankAccessId);
    Q_INVOKABLE void respond(int interactionId, bool accepted, const QString &text, int choice);
    /** Aborts whatever the bank is waiting for: the running operation fails as cancelled. */
    Q_INVOKABLE void cancel();
    /** Deletes all stored data and the key. Also the way out when the data cannot be decrypted. */
    Q_INVOKABLE void resetAllData();

signals:
    void stateChanged();
    void busyChanged();
    void progressTextChanged();
    void interactionRequested(int interactionId, int type, const QString &title, const QString &text,
                              const QStringList &choices, const QString &imageUrl);
    void interactionFinished(int interactionId);
    void operationFinished(int operation, bool success, const QString &message);

private slots:
    void handleKeyAvailable(const QByteArray &key);
    void handleKeyFailed(const QString &message, bool keyMissing);
    void handleKeyDeleted(bool success);
    void handleInitialized(int result, const QString &message);
    void handleCommandStarted(int command);
    void handleCommandFinished(int command, bool success, const QString &message);
    void handleProgress(const QString &text);
    void handleInteractionRequested(int interactionId, int type, const QString &title, const QString &text,
                                    const QStringList &choices, const QImage &image);
    void handleInteractionFinished(int interactionId);

private:
    void setState(State state, const QString &message = QString());
    void startUp();

    BankingThread thread;
    KeyStore keyStore;
    AccountModel accountModel;
    TransactionModel transactionModel;
    HoldingModel holdingModel;
    TanImageProvider *tanImageProvider;
    State currentState;
    QString currentStateMessage;
    Operation currentOperation;
    QString currentProgressText;
};

#endif // BANKING_H
