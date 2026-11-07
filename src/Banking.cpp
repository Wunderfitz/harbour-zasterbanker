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

#include "Banking.h"

#include "EncryptedConfigStore.h"
#include "TanImageProvider.h"

#include <QDebug>
#include <QVariantMap>

Banking::Banking(const QString &dataDirectory, TanImageProvider *tanImageProvider, QObject *parent)
    : QObject(parent)
    , thread(dataDirectory)
    , tanImageProvider(tanImageProvider)
    , currentState(Starting)
    , currentOperation(NoOperation)
{
    connect(&keyStore, &KeyStore::keyAvailable, this, &Banking::handleKeyAvailable);
    connect(&keyStore, &KeyStore::keyFailed, this, &Banking::handleKeyFailed);
    connect(&keyStore, &KeyStore::keyDeleted, this, &Banking::handleKeyDeleted);

    connect(&thread, &BankingThread::initialized, this, &Banking::handleInitialized);
    connect(&thread, &BankingThread::accountsChanged, &accountModel, &AccountModel::setAccounts);
    connect(&thread, &BankingThread::balancesFetched, &accountModel, &AccountModel::setBalances);
    connect(&thread, &BankingThread::accountsChanged, &transactionModel, &TransactionModel::retainAccounts);
    connect(&thread, &BankingThread::transactionsFetched, &transactionModel, &TransactionModel::setTransactions);
    connect(&thread, &BankingThread::accountsChanged, &holdingModel, &HoldingModel::retainAccounts);
    connect(&thread, &BankingThread::holdingsFetched, &holdingModel, &HoldingModel::setHoldings);
    connect(&thread, &BankingThread::commandStarted, this, &Banking::handleCommandStarted);
    connect(&thread, &BankingThread::commandFinished, this, &Banking::handleCommandFinished);
    connect(&thread, &BankingThread::progress, this, &Banking::handleProgress);
    connect(&thread, &BankingThread::interactionRequested, this, &Banking::handleInteractionRequested);
    connect(&thread, &BankingThread::interactionFinished, this, &Banking::handleInteractionFinished);

    thread.start();
    startUp();
}

Banking::~Banking()
{
    thread.stop();
    thread.wait();
}

Banking::State Banking::state() const
{
    return currentState;
}

QString Banking::stateMessage() const
{
    return currentStateMessage;
}

bool Banking::busy() const
{
    return currentOperation != NoOperation;
}

Banking::Operation Banking::operation() const
{
    return currentOperation;
}

QString Banking::progressText() const
{
    return currentProgressText;
}

TransactionModel *Banking::transactions()
{
    return &transactionModel;
}

HoldingModel *Banking::holdings()
{
    return &holdingModel;
}

AccountModel *Banking::accounts()
{
    return &accountModel;
}

void Banking::addBankAccess(const QString &bankCode, const QString &bankName, const QString &serverUrl,
                            const QString &userId, const QString &customerId)
{
    if (currentState != Ready || busy()) {
        return;
    }
    QVariantMap bankAccess;
    bankAccess.insert(QStringLiteral("bankCode"), bankCode.trimmed());
    bankAccess.insert(QStringLiteral("bankName"), bankName.trimmed());
    bankAccess.insert(QStringLiteral("serverUrl"), serverUrl.trimmed());
    bankAccess.insert(QStringLiteral("userId"), userId.trimmed());
    bankAccess.insert(QStringLiteral("customerId"), customerId.trimmed());
    thread.addBankAccess(bankAccess);
}

void Banking::refreshBalances()
{
    if (currentState == Ready && !busy() && accountModel.rowCount() > 0) {
        thread.fetchBalances();
    }
}

void Banking::refreshBankAccess(int bankAccessId)
{
    if (currentState == Ready && !busy() && accountModel.rowCount() > 0) {
        thread.fetchBalances(static_cast<uint>(bankAccessId));
    }
}

void Banking::fetchTransactions(int accountId)
{
    if (currentState == Ready && !busy()) {
        thread.fetchTransactions(static_cast<uint>(accountId));
    }
}

void Banking::fetchHoldings(int accountId)
{
    if (currentState == Ready && !busy()) {
        thread.fetchHoldings(static_cast<uint>(accountId));
    }
}

void Banking::removeBankAccess(int bankAccessId)
{
    if (currentState == Ready && !busy()) {
        thread.removeBankAccess(static_cast<uint>(bankAccessId));
    }
}

void Banking::respond(int interactionId, bool accepted, const QString &text, int choice)
{
    thread.respond(interactionId, accepted, text, choice);
}

void Banking::cancel()
{
    thread.cancelInteraction();
}

void Banking::resetAllData()
{
    if (busy()) {
        return;
    }
    // First the data, while the key is still there, then the key.
    thread.resetData();
}

void Banking::startUp()
{
    setState(Starting);
    keyStore.requestKey(EncryptedConfigStore::dataExists(thread.dataFilePath()));
}

void Banking::handleKeyAvailable(const QByteArray &key)
{
    // The banking thread wipes its copy once the store has it.
    thread.initialize(key);
}

void Banking::handleKeyFailed(const QString &message, bool keyMissing)
{
    setState(keyMissing ? DataUnreadable : Failed, message);
}

void Banking::handleKeyDeleted(bool success)
{
    if (!success) {
        setState(Failed, tr("The key could not be removed from Sailfish Secrets."));
        return;
    }
    startUp();
}

void Banking::handleInitialized(int result, const QString &message)
{
    switch (result) {
    case BankingThread::InitializationSucceeded:
        setState(Ready);
        break;
    case BankingThread::InitializationDataUnreadable:
        setState(DataUnreadable, message);
        break;
    default:
        setState(Failed, message);
        break;
    }
}

void Banking::handleCommandStarted(int command)
{
    currentOperation = static_cast<Operation>(command);
    currentProgressText.clear();
    emit progressTextChanged();
    emit busyChanged();
}

void Banking::handleCommandFinished(int command, bool success, const QString &message)
{
    currentOperation = NoOperation;
    emit busyChanged();
    emit operationFinished(command, success, message);
    if (command == BankingThread::ResetData) {
        if (success) {
            keyStore.deleteKey();
        } else {
            setState(Failed, message);
        }
    }
}

void Banking::handleProgress(const QString &text)
{
    currentProgressText = text;
    emit progressTextChanged();
}

void Banking::handleInteractionRequested(int interactionId, int type, const QString &title, const QString &text,
                                         const QStringList &choices, const QImage &image)
{
    const QString imageUrl = image.isNull() ? QString() : tanImageProvider->setImage(image);
    emit interactionRequested(interactionId, type, title, text, choices, imageUrl);
}

void Banking::handleInteractionFinished(int interactionId)
{
    tanImageProvider->clear();
    emit interactionFinished(interactionId);
}

void Banking::setState(State state, const QString &message)
{
    if (state != currentState || message != currentStateMessage) {
        currentState = state;
        currentStateMessage = message;
        emit stateChanged();
    }
}
