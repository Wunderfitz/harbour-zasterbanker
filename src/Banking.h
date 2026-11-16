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
#include <QVariantMap>

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
    /** Bank access ID (as text) to the time its remembered PIN is wiped, in milliseconds since the epoch. */
    Q_PROPERTY(QVariantMap pinDeadlines READ pinDeadlines NOTIFY pinDeadlinesChanged)
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
        FetchingHoldings = BankingThread::FetchHoldings,
        UpdatingBankAccess = BankingThread::UpdateBankAccess,
        ChangingTanMethod = BankingThread::ChangeTanMethod
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
    QVariantMap pinDeadlines() const;
    AccountModel *accounts();
    TransactionModel *transactions();
    HoldingModel *holdings();

    Q_INVOKABLE void addBankAccess(const QString &bankCode, const QString &bankName, const QString &serverUrl,
                                   const QString &userId, const QString &customerId);
    Q_INVOKABLE void refreshBalances();
    /** The balances of one bank access only: no other bank is contacted, no other PIN asked for. */
    Q_INVOKABLE void refreshBankAccess(int bankAccessId);
    Q_INVOKABLE void fetchTransactions(int accountId);
    Q_INVOKABLE void fetchHoldings(int accountId);
    Q_INVOKABLE void removeBankAccess(int bankAccessId);
    /**
     * Asks the bank afresh for its parameters and the access's accounts (BPD
     * and UPD), offers to remove accounts it no longer lists, then fetches the
     * balances. Also brings the accounts of an access that has none.
     */
    Q_INVOKABLE void updateBankAccess(int bankAccessId);
    /** Another of the TAN methods the bank allows, without contacting it; and the TAN medium's name. */
    Q_INVOKABLE void changeTanMethod(int bankAccessId);
    /** Wipes all remembered PINs; the next action with a bank asks for its PIN again. */
    Q_INVOKABLE void forgetPins();
    Q_INVOKABLE void respond(int interactionId, bool accepted, const QString &text, int choice);
    /** Aborts whatever the bank is waiting for: the running operation fails as cancelled. */
    Q_INVOKABLE void cancel();
    /** Deletes all stored data and the key. Also the way out when the data cannot be decrypted. */
    Q_INVOKABLE void resetAllData();
    /** Starts again after banking failed to start, e.g. when the key could not be read. */
    Q_INVOKABLE void retry();

signals:
    void stateChanged();
    void busyChanged();
    void progressTextChanged();
    void pinDeadlinesChanged();
    void interactionRequested(int interactionId, int type, const QString &title, const QString &text,
                              const QStringList &choices, const QString &imageUrl, const QString &presetText,
                              const QString &flickerCode, bool digitsOnly);
    void interactionFinished(int interactionId);
    void operationFinished(int operation, bool success, const QString &message);

private slots:
    void handleKeyAvailable(const QByteArray &key);
    void handleKeyFailed(const QString &message, bool keyMissing);
    void handleKeyDeleted(bool success, const QString &message);
    void handleInitialized(int result, const QString &message);
    void handleCommandStarted(int command);
    void handleCommandFinished(int command, bool success, const QString &message);
    void handleProgress(const QString &text);
    void handleInteractionRequested(int interactionId, int type, const QString &title, const QString &text,
                                    const QStringList &choices, const QImage &image, const QString &presetText,
                                    const QString &flickerCode, bool digitsOnly);
    void handlePinsChanged(const QVariantMap &deadlines);
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
    QVariantMap currentPinDeadlines;
};

#endif // BANKING_H
