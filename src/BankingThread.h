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

#ifndef BANKINGTHREAD_H
#define BANKINGTHREAD_H

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>
#include <QWaitCondition>

#include <aqbanking/banking.h>

class EncryptedConfigStore;
struct GWEN_GUI;

/**
 * Owns AqBanking and does everything with it.
 *
 * AqBanking is synchronous and not thread-safe: a FinTS dialog blocks for as
 * long as the bank takes, and asks for PIN and TAN by calling back into
 * Gwenhywfar's GUI layer from the middle of it. So it gets a thread of its own,
 * which works off a queue of commands one at a time. The GUI callbacks
 * (GwenGui) end up in request*(), which hand the question to the UI thread as a
 * signal and then wait until respond() or cancelInteraction() brings the answer.
 *
 * Everything this class emits arrives queued on the UI thread, where Banking
 * turns it into properties and signals for QML.
 */
class BankingThread : public QThread
{
    Q_OBJECT

public:
    enum CommandType {
        Initialize,
        AddBankAccess,
        FetchBalances,
        RemoveBankAccess,
        ResetData,
        FetchTransactions
    };

    enum InitializationResult {
        InitializationSucceeded,
        InitializationDataUnreadable,
        InitializationFailed
    };

    enum InteractionType {
        PinInteraction,
        TanInteraction,
        TextInteraction,
        ChoiceInteraction,
        MessageInteraction
    };

    struct Response {
        bool accepted;
        QByteArray text;
        int choice;
    };

    explicit BankingThread(const QString &dataDirectory, QObject *parent = nullptr);
    ~BankingThread();

    /* UI thread */
    void initialize(const QByteArray &key);
    void addBankAccess(const QVariantMap &bankAccess);
    void fetchBalances();
    void fetchTransactions(uint accountId);
    void removeBankAccess(uint bankAccessId);
    void resetData();
    void respond(int interactionId, bool accepted, const QString &text, int choice);
    void cancelInteraction();
    void stop();

    QString dataFilePath() const;

    /* Banking thread, called from GwenGui */
    Response requestInteraction(InteractionType type, const QString &title, const QString &text,
                                const QStringList &choices = QStringList(),
                                const QByteArray &image = QByteArray(), const QString &mimeType = QString());
    void reportProgress(const QString &text);
    void reportError(const QString &text);
    bool isCancelled() const;

signals:
    void initialized(int result, const QString &message);
    void accountsChanged(const QVariantList &accounts);
    void balancesFetched(const QVariantList &balances);
    void transactionsFetched(uint accountId, const QVariantList &transactions);
    void commandStarted(int command);
    void commandFinished(int command, bool success, const QString &message);
    void progress(const QString &text);
    /* A challenge image (photoTAN, QR-TAN) arrives decoded, as decoding is this thread's job too. */
    void interactionRequested(int interactionId, int type, const QString &title, const QString &text,
                              const QStringList &choices, const QImage &image);
    void interactionFinished(int interactionId);

protected:
    void run() override;

private:
    struct Command {
        CommandType type;
        QByteArray key;
        QVariantMap arguments;
    };

    void enqueue(const Command &command);
    void executeInitialize(Command &command);
    void executeAddBankAccess(const Command &command);
    void executeFetchBalances();
    bool retrieveBalances(QString *errorMessage);
    void executeFetchTransactions(const Command &command);
    void executeRemoveBankAccess(const Command &command);
    void executeResetData();
    bool chooseTanMethod(uint32_t userUid);
    void publishAccounts();
    void shutDownAqBanking();
    QString failureMessage(int errorCode, const QString &fallback) const;

    const QString dataDirectory;

    mutable QMutex mutex;
    QWaitCondition condition;
    QQueue<Command> commands;
    bool stopRequested;
    bool cancelRequested;
    int pendingInteraction;
    bool interactionAnswered;
    Response interactionResponse;
    int nextInteractionId;

    /* Banking thread only */
    AB_BANKING *banking;
    GWEN_GUI *gui;
    EncryptedConfigStore *configStore;
    QString lastError;
};

#endif // BANKINGTHREAD_H
