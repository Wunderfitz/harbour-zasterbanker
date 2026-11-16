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
#include <QElapsedTimer>
#include <QHash>
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
        FetchTransactions,
        FetchHoldings,
        UpdateBankAccess,
        ChangeTanMethod
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
    void fetchBalances(uint bankAccessId);
    void fetchTransactions(uint accountId);
    void fetchHoldings(uint accountId);
    void removeBankAccess(uint bankAccessId);
    /*
     * Asks the bank afresh for its parameters and the access's accounts with
     * theirs (BPD and UPD), offers to remove accounts the bank no longer
     * lists, then asks for the balances.
     */
    void updateBankAccess(uint bankAccessId);
    /* Lets the user choose another of the TAN methods the bank allows, and the TAN medium's name with it. */
    void changeTanMethod(uint bankAccessId);
    void resetData();
    /* Wipes all remembered PINs at once; while a command runs, as soon as it ends. */
    void forgetPins();
    void respond(int interactionId, bool accepted, const QString &text, int choice);
    void cancelInteraction();
    void stop();

    QString dataFilePath() const;

    /* Banking thread, called from GwenGui */
    /*
     * presetText: what a text field starts with (TextInteraction);
     * flickerCode: the frames of a chipTAN flicker code (TanInteraction), see FlickerCode;
     * digitsOnly: the bank takes digits only for this TAN (TanInteraction)
     */
    Response requestInteraction(InteractionType type, const QString &title, const QString &text,
                                const QStringList &choices = QStringList(),
                                const QByteArray &image = QByteArray(), const QString &mimeType = QString(),
                                const QString &presetText = QString(), const QString &flickerCode = QString(),
                                bool digitsOnly = false);
    void reportProgress(const QString &text);
    void reportError(const QString &text);
    bool isCancelled() const;

signals:
    void initialized(int result, const QString &message);
    void accountsChanged(const QVariantList &accounts);
    void balancesFetched(const QVariantList &balances);
    void transactionsFetched(uint accountId, const QVariantList &transactions);
    void holdingsFetched(uint accountId, const QVariantList &holdings);
    void commandStarted(int command);
    void commandFinished(int command, bool success, const QString &message);
    void progress(const QString &text);
    /* A challenge image (photoTAN, QR-TAN) arrives decoded, as decoding is this thread's job too. */
    void interactionRequested(int interactionId, int type, const QString &title, const QString &text,
                              const QStringList &choices, const QImage &image, const QString &presetText,
                              const QString &flickerCode, bool digitsOnly);
    /*
     * The bank accesses whose PIN is remembered, by ID (as text), with the time
     * it is wiped at (milliseconds since the epoch); after every command, and
     * when one is wiped.
     */
    void pinsChanged(const QVariantMap &deadlines);
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
    void executeFetchBalances(const Command &command);
    /* Of all bank accesses, or only of the given one. */
    bool retrieveBalances(QString *errorMessage, bool oneBankAccess = false, uint32_t bankAccessId = 0);
    void executeFetchTransactions(const Command &command);
    /* One request for the account's transactions, with the job AqBanking chooses. */
    int requestTransactions(uint32_t accountId, QVariantList *transactions, bool *hasAnswer, bool *jobFailed);
    void executeFetchHoldings(const Command &command);
    void executeRemoveBankAccess(const Command &command);
    void executeUpdateBankAccess(const Command &command);
    /* Asks whether to remove the accounts the bank no longer lists, and does so. */
    void offerToRemoveAccounts(const QList<uint32_t> &accountIds);
    void executeChangeTanMethod(const Command &command);
    /* Emits commandFinished(), and has the debug log say so. */
    void finishCommand(int command, bool success, const QString &message);
    void executeResetData();
    /*
     * At setup, the user chooses if there is more than one method; when
     * changing, always, with the current one named, and the TAN medium's
     * current name in its field.
     */
    bool chooseTanMethod(uint32_t userUid, bool change = false);
    bool chooseTanMedium(uint32_t userUid, const QString &current = QString());
    /* For the debug log: AqBanking's log levels, and what a command is about. */
    void applyLogLevels();
    void logCommand(const Command &command);
    void logBankAccess(uint32_t uid);
    void logAccount(uint32_t accountId);
    /* Emits pinsChanged() if the set of accesses changed, or always if force is set. */
    void publishPins(bool force);
    void publishAccounts();
    void loadStoredBalances();
    void storeBalances(const QVariantList &balances);
    void saveStoredBalances();
    QVariantList storedBalanceList() const;
    void shutDownAqBanking();
    QString failureMessage(int errorCode, const QString &fallback) const;

    const QString dataDirectory;

    mutable QMutex mutex;
    QWaitCondition condition;
    QQueue<Command> commands;
    bool stopRequested;
    bool cancelRequested;
    bool forgetPinsRequested;
    int pendingInteraction;
    bool interactionAnswered;
    Response interactionResponse;
    int nextInteractionId;

    /* Banking thread only */
    AB_BANKING *banking;
    GWEN_GUI *gui;
    EncryptedConfigStore *configStore;
    QString lastError;
    /* How long the running command takes, for the debug log. */
    QElapsedTimer commandTimer;
    /* What pinsChanged() said last. */
    QVariantMap publishedPins;
    /* The last balance of each account, kept in the encrypted store, by account ID. */
    QHash<uint, QVariantMap> storedBalances;
};

#endif // BANKINGTHREAD_H
