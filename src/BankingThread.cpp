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

#include "BankingThread.h"

#include "AqHbciShim.h"
#include "EncryptedConfigStore.h"
#include "FinTsRegistration.h"
#include "GwenGui.h"
#include "StatementReader.h"

#include <QDataStream>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QMutexLocker>
#include <QSet>

#include <aqbanking/banking_online.h>
#include <aqbanking/types/account_spec.h>
#include <aqbanking/types/imexporter_context.h>
#include <aqbanking/types/transaction.h>
#include <gwenhywfar/dbio.h>
#include <gwenhywfar/gwendate.h>
#include <gwenhywfar/gwenhywfar.h>
#include <gwenhywfar/logger.h>
#include <gwenhywfar/plugin.h>

#include <cstring>

// AqBanking's parser for bank statements, linked in from swift.a. It is a
// plugin that Gwenhywfar would otherwise look for on disk.
extern "C" GWEN_PLUGIN *dbio_swift_factory(GWEN_PLUGIN_MANAGER *pm, const char *modName, const char *fileName);

namespace {

// Where the last balances are kept in the encrypted store, and their format.
const QString BALANCES_NAME = QStringLiteral("balances");
const qint32 BALANCES_VERSION = 1;

// How far back transactions are asked for. Under PSD2, banks generally need no
// TAN up to this age, and most ask for one or refuse anything older.
const int TRANSACTION_DAYS = 90;

// AqBanking's name for this application; it keys the app's own settings group.
const char APP_NAME[] = "ZasterBanker";
// Sent to the bank with every dialog, at most five characters. Bump with the spec.
const char FINTS_APP_VERSION[] = "0.5";
const char DATA_FILE[] = "aqbanking.zbs";
const int MAX_TAN_METHODS = 32;
// FinTS allows 32 characters for the name of a TAN medium.
const int MAX_TAN_MEDIUM_LENGTH = 32;
const int MAX_USERS = 64;

void wipe(QByteArray &data)
{
    if (!data.isEmpty()) {
        std::memset(data.data(), 0, static_cast<size_t>(data.size()));
    }
    data.clear();
}
}

BankingThread::BankingThread(const QString &dataDirectory, QObject *parent)
    : QThread(parent)
    , dataDirectory(dataDirectory)
    , stopRequested(false)
    , cancelRequested(false)
    , pendingInteraction(0)
    , interactionAnswered(false)
    , nextInteractionId(0)
    , banking(nullptr)
    , gui(nullptr)
    , configStore(nullptr)
    , collectingBankMessages(false)
{
    interactionResponse.accepted = false;
    interactionResponse.choice = 0;
}

BankingThread::~BankingThread()
{
    stop();
    wait();
}

QString BankingThread::dataFilePath() const
{
    return dataDirectory + QLatin1Char('/') + QLatin1String(DATA_FILE);
}

void BankingThread::initialize(const QByteArray &key)
{
    Command command;
    command.type = Initialize;
    command.key = key;
    enqueue(command);
}

void BankingThread::addBankAccess(const QVariantMap &bankAccess)
{
    Command command;
    command.type = AddBankAccess;
    command.arguments = bankAccess;
    enqueue(command);
}

void BankingThread::fetchBalances()
{
    Command command;
    command.type = FetchBalances;
    enqueue(command);
}

void BankingThread::fetchBalances(uint bankAccessId)
{
    Command command;
    command.type = FetchBalances;
    command.arguments.insert(QStringLiteral("bankAccessId"), bankAccessId);
    enqueue(command);
}

void BankingThread::fetchTransactions(uint accountId)
{
    Command command;
    command.type = FetchTransactions;
    command.arguments.insert(QStringLiteral("accountId"), accountId);
    enqueue(command);
}

void BankingThread::fetchHoldings(uint accountId)
{
    Command command;
    command.type = FetchHoldings;
    command.arguments.insert(QStringLiteral("accountId"), accountId);
    enqueue(command);
}

void BankingThread::retrieveAccounts(uint bankAccessId)
{
    Command command;
    command.type = RetrieveAccounts;
    command.arguments.insert(QStringLiteral("bankAccessId"), bankAccessId);
    enqueue(command);
}

void BankingThread::removeBankAccess(uint bankAccessId)
{
    Command command;
    command.type = RemoveBankAccess;
    command.arguments.insert(QStringLiteral("bankAccessId"), bankAccessId);
    enqueue(command);
}

void BankingThread::resetData()
{
    Command command;
    command.type = ResetData;
    enqueue(command);
}

void BankingThread::enqueue(const Command &command)
{
    QMutexLocker locker(&mutex);
    commands.enqueue(command);
    condition.wakeAll();
}

void BankingThread::respond(int interactionId, bool accepted, const QString &text, int choice)
{
    QMutexLocker locker(&mutex);
    if (interactionId != pendingInteraction) {
        return;
    }
    interactionResponse.accepted = accepted;
    interactionResponse.text = text.toUtf8();
    interactionResponse.choice = choice;
    interactionAnswered = true;
    condition.wakeAll();
}

void BankingThread::cancelInteraction()
{
    QMutexLocker locker(&mutex);
    cancelRequested = true;
    condition.wakeAll();
}

void BankingThread::stop()
{
    QMutexLocker locker(&mutex);
    stopRequested = true;
    cancelRequested = true;
    condition.wakeAll();
}

bool BankingThread::isCancelled() const
{
    QMutexLocker locker(&mutex);
    return cancelRequested;
}

BankingThread::Response BankingThread::requestInteraction(InteractionType type, const QString &title, const QString &text,
                                                          const QStringList &choices, const QByteArray &image,
                                                          const QString &mimeType)
{
    Response response;
    response.accepted = false;
    response.choice = 0;

    QImage decodedImage;
    if (!image.isEmpty()) {
        const QByteArray format = mimeType.startsWith(QStringLiteral("image/")) ? mimeType.mid(6).toLatin1() : QByteArray();
        decodedImage = QImage::fromData(image, format.isEmpty() ? nullptr : format.constData());
        if (decodedImage.isNull()) {
            decodedImage = QImage::fromData(image);
        }
        if (decodedImage.isNull()) {
            qWarning() << "Could not decode the TAN challenge image of type" << mimeType;
        }
    }

    int interactionId;
    {
        QMutexLocker locker(&mutex);
        if (cancelRequested) {
            return response;
        }
        interactionId = ++nextInteractionId;
        pendingInteraction = interactionId;
        interactionAnswered = false;
    }

    emit interactionRequested(interactionId, type, title, text, choices, decodedImage);

    {
        QMutexLocker locker(&mutex);
        while (!interactionAnswered && !cancelRequested) {
            condition.wait(&mutex);
        }
        if (interactionAnswered) {
            response = interactionResponse;
        }
        wipe(interactionResponse.text);
        pendingInteraction = 0;
        interactionAnswered = false;
    }

    emit interactionFinished(interactionId);
    return response;
}

void BankingThread::reportProgress(const QString &text)
{
    if (!text.isEmpty()) {
        collectBankMessage(text);
        emit progress(text);
    }
}

void BankingThread::reportError(const QString &text)
{
    if (!text.isEmpty()) {
        collectBankMessage(text);
        lastError = text;
        emit progress(text);
    }
}

// AqBanking logs every result code the bank sends as "HBCI: 0020 - text (S)",
// "S" for the answer to one segment, "M" for the whole message, whatever its
// level (notice, warning or error). Not translated, so this is how to tell them
// from AqBanking's own messages. Without them, a fetch that finds nothing says
// nothing about why: the bank's warnings (3xxx) fail neither the job nor the call.
void BankingThread::collectBankMessage(const QString &text)
{
    static const QString prefix = QStringLiteral("HBCI: ");
    if (collectingBankMessages && text.startsWith(prefix)) {
        const QString message = text.mid(prefix.length());
        if (!bankMessages.contains(message)) {
            bankMessages.append(message);
        }
    }
}

void BankingThread::run()
{
    GWEN_Init();
    GWEN_Logger_SetLevel(GWEN_LOGDOMAIN, GWEN_LoggerLevel_Error);
    GWEN_Logger_SetLevel(AQBANKING_LOGDOMAIN, GWEN_LoggerLevel_Error);
    GWEN_Logger_SetLevel("aqhbci", GWEN_LoggerLevel_Error);
    gui = GwenGui::create(this);
    GWEN_Gui_SetGui(gui);

    GWEN_PLUGIN_MANAGER *dbioPlugins = GWEN_PluginManager_FindPluginManager(GWEN_DBIO_PLUGIN_NAME);
    GWEN_PluginManager_AddPlugin(dbioPlugins, dbio_swift_factory(dbioPlugins, "swift", nullptr));

    forever {
        Command command;
        {
            QMutexLocker locker(&mutex);
            while (commands.isEmpty() && !stopRequested) {
                // Remembered PINs are wiped when their time is up, not merely
                // left unused. The wait does not count time the device spends
                // suspended, so it wakes at least every 15 seconds while there
                // is a PIN: a few seconds after the device wakes up, a PIN that
                // expired meanwhile is gone.
                const qint64 nextExpiry = GwenGui::forgetExpiredPins();
                if (nextExpiry < 0) {
                    condition.wait(&mutex);
                } else {
                    condition.wait(&mutex, static_cast<unsigned long>(qMin<qint64>(nextExpiry, 15000)));
                }
            }
            if (stopRequested) {
                break;
            }
            command = commands.dequeue();
            cancelRequested = false;
        }
        lastError.clear();

        switch (command.type) {
        case Initialize:
            executeInitialize(command);
            break;
        case AddBankAccess:
            executeAddBankAccess(command);
            break;
        case FetchBalances:
            executeFetchBalances(command);
            break;
        case FetchTransactions:
            executeFetchTransactions(command);
            break;
        case FetchHoldings:
            executeFetchHoldings(command);
            break;
        case RemoveBankAccess:
            executeRemoveBankAccess(command);
            break;
        case RetrieveAccounts:
            executeRetrieveAccounts(command);
            break;
        case ResetData:
            executeResetData();
            break;
        }
        if (command.type == RemoveBankAccess || command.type == ResetData) {
            GwenGui::forgetPins();
        } else {
            GwenGui::finishCommand();
        }
    }

    shutDownAqBanking();
    GWEN_Gui_SetGui(nullptr);
    GwenGui::destroy(gui);
    gui = nullptr;
    GWEN_Fini();
}

void BankingThread::executeInitialize(Command &command)
{
    if (banking) {
        wipe(command.key);
        emit initialized(InitializationSucceeded, QString());
        publishAccounts();
        return;
    }

    QDir().mkpath(dataDirectory);
    configStore = new EncryptedConfigStore(dataFilePath(), command.key);
    wipe(command.key);
    const EncryptedConfigStore::LoadResult loadResult = configStore->load();
    if (loadResult == EncryptedConfigStore::DecryptionFailed || loadResult == EncryptedConfigStore::ReadFailed) {
        delete configStore;
        configStore = nullptr;
        emit initialized(InitializationDataUnreadable, tr("Your stored data cannot be decrypted."));
        return;
    }
    configStore->registerAsDirPlugin();

    banking = AB_Banking_new(APP_NAME, QFile::encodeName(dataDirectory).constData(), 0);
    AB_Banking_RuntimeConfig_SetCharValue(banking, "fintsRegistrationKey", FINTS_REGISTRATION_NUMBER);
    AB_Banking_RuntimeConfig_SetCharValue(banking, "fintsApplicationVersionString", FINTS_APP_VERSION);
    const int rv = AB_Banking_Init(banking);
    if (rv < 0) {
        qWarning() << "AB_Banking_Init failed" << rv;
        AB_Banking_free(banking);
        banking = nullptr;
        delete configStore;
        configStore = nullptr;
        emit initialized(InitializationFailed, tr("The banking library could not be started (%1).").arg(rv));
        return;
    }

    loadStoredBalances();
    emit initialized(InitializationSucceeded, QString());
    publishAccounts();
}

void BankingThread::executeAddBankAccess(const Command &command)
{
    emit commandStarted(AddBankAccess);
    if (!banking) {
        emit commandFinished(AddBankAccess, false, tr("Banking is not available."));
        return;
    }

    const QVariantMap &arguments = command.arguments;
    // Every step's result lines, so that a setup that ends without accounts can
    // show what the bank said.
    startCollectingBankMessages();
    uint32_t uid = 0;
    int rv = ZB_AddPinTanUser(banking,
                              arguments.value(QStringLiteral("bankCode")).toString().toUtf8().constData(),
                              arguments.value(QStringLiteral("userId")).toString().toUtf8().constData(),
                              arguments.value(QStringLiteral("customerId")).toString().toUtf8().constData(),
                              arguments.value(QStringLiteral("serverUrl")).toString().toUtf8().constData(),
                              arguments.value(QStringLiteral("bankName")).toString().toUtf8().constData(),
                              &uid);
    if (rv < 0) {
        reportBankMessages();
        emit commandFinished(AddBankAccess, false, failureMessage(rv, tr("The bank access could not be created.")));
        return;
    }

    // A failed setup leaves nothing behind: the half-made user goes again.
    reportProgress(tr("Connecting to the bank..."));
    rv = ZB_GetSysId(banking, uid);
    if (rv < 0) {
        const QString message = failureMessage(rv, tr("The bank did not accept the connection."));
        ZB_DeleteUser(banking, uid);
        reportBankMessages();
        emit commandFinished(AddBankAccess, false, message);
        return;
    }

    if (!chooseTanMethod(uid)) {
        const QString message = failureMessage(GWEN_ERROR_USER_ABORTED, QString());
        ZB_DeleteUser(banking, uid);
        reportBankMessages();
        emit commandFinished(AddBankAccess, false, message);
        return;
    }

    reportProgress(tr("Retrieving your accounts..."));
    rv = ZB_GetAccounts(banking, uid);
    // A bank access without accounts would be of no use (found 2026-11-11 at a
    // Sparkasse: the setup went through, and no account came).
    if (rv <= 0) {
        const QString message = rv < 0 ? failureMessage(rv, tr("The bank did not list your accounts."))
                                        : tr("The bank listed no accounts for this access.");
        ZB_DeleteUser(banking, uid);
        reportBankMessages();
        emit commandFinished(AddBankAccess, false, message);
        return;
    }

    publishAccounts();
    // Within the same command, so the PIN entered for the setup is reused.
    // Only the new bank access's: the others would ask for their PINs.
    // Balances that do not come are no reason to undo the setup.
    QString balanceError;
    retrieveBalances(&balanceError, true, uid);
    reportBankMessages();
    emit commandFinished(AddBankAccess, true, QString());
}

bool BankingThread::chooseTanMethod(uint32_t userUid)
{
    ZB_TAN_METHOD methods[MAX_TAN_METHODS];
    const int count = ZB_ListTanMethods(banking, userUid, methods, MAX_TAN_METHODS);
    QList<ZB_TAN_METHOD> allowed;
    for (int i = 0; i < count; i++) {
        if (methods[i].allowed) {
            allowed.append(methods[i]);
        }
    }
    // None means the bank does without two-step TANs for this user; AqBanking
    // then sticks to the single-step procedure on its own.
    if (allowed.isEmpty()) {
        return true;
    }

    int chosen = 0;
    if (allowed.size() > 1) {
        QStringList names;
        for (const ZB_TAN_METHOD &method : allowed) {
            names.append(QString::fromUtf8(method.name));
        }
        const Response response = requestInteraction(ChoiceInteraction, tr("TAN method"),
                                                     tr("Your bank offers several ways to confirm with a TAN. Which one do you use?"),
                                                     names);
        if (!response.accepted || response.choice < 0 || response.choice >= allowed.size()) {
            return false;
        }
        chosen = response.choice;
    }
    const int rv = ZB_SelectTanMethod(banking, userUid, allowed.at(chosen).id);
    if (rv < 0) {
        qWarning() << "Selecting TAN method" << allowed.at(chosen).id << "failed" << rv;
    }
    // Only when required: an optional name the user would have to look up for nothing.
    if (allowed.at(chosen).tanMediumNeeded == 2) {
        return chooseTanMedium(userUid);
    }
    return true;
}

// Some banks (Deutsche Bank and Postbank for BestSign, for one) want the name of
// the device the TAN comes from with every TAN, and refuse the dialog without it.
// AqBanking cannot ask the bank for the list of names, so the user types it.
bool BankingThread::chooseTanMedium(uint32_t userUid)
{
    QString hint;
    forever {
        const Response response = requestInteraction(TextInteraction, tr("TAN medium"),
                                                     hint + tr("Your bank needs the name of the device or app you confirm with. "
                                                               "You find it in the security settings of your online banking or "
                                                               "of the bank's app, e.g. under \"Sicherheitsverfahren\" for BestSign. "
                                                               "Enter it exactly as shown there, capitals included."));
        if (!response.accepted) {
            return false;
        }
        const QString name = QString::fromUtf8(response.text).trimmed();
        if (name.isEmpty() || name.length() > MAX_TAN_MEDIUM_LENGTH) {
            hint = tr("The name can have up to %1 characters.").arg(MAX_TAN_MEDIUM_LENGTH) + QLatin1Char(' ');
            continue;
        }
        // AqBanking sends it as stored, and FinTS messages are ISO 8859-1.
        const QByteArray latin1 = name.toLatin1();
        if (QString::fromLatin1(latin1) != name) {
            hint = tr("The name contains a character that cannot be sent to the bank.") + QLatin1Char(' ');
            continue;
        }
        const int rv = ZB_SetTanMediumId(banking, userUid, latin1.constData());
        if (rv < 0) {
            qWarning() << "Setting the TAN medium failed" << rv;
            return false;
        }
        return true;
    }
}

void BankingThread::executeFetchBalances(const Command &command)
{
    emit commandStarted(FetchBalances);
    if (!banking) {
        emit commandFinished(FetchBalances, false, tr("Banking is not available."));
        return;
    }
    QString message;
    const QVariant bankAccessId = command.arguments.value(QStringLiteral("bankAccessId"));
    const bool success = bankAccessId.isValid() ? retrieveBalances(&message, true, bankAccessId.toUInt())
                                                : retrieveBalances(&message);
    emit commandFinished(FetchBalances, success, message);
}

bool BankingThread::retrieveBalances(QString *errorMessage, bool oneBankAccess, uint32_t bankAccessId)
{
    AB_ACCOUNT_SPEC_LIST *specs = nullptr;
    int rv = AB_Banking_GetAccountSpecList(banking, &specs);
    if (rv < 0 || !specs) {
        *errorMessage = tr("There are no accounts.");
        return false;
    }

    AB_TRANSACTION_LIST2 *jobs = AB_Transaction_List2_new();
    for (AB_ACCOUNT_SPEC *spec = AB_AccountSpec_List_First(specs); spec; spec = AB_AccountSpec_List_Next(spec)) {
        if (oneBankAccess) {
            uint32_t userUid = 0;
            if (ZB_GetAccountUser(banking, AB_AccountSpec_GetUniqueId(spec), &userUid) < 0 || userUid != bankAccessId) {
                continue;
            }
        }
        AB_TRANSACTION *job = AB_Transaction_new();
        AB_Transaction_SetCommand(job, AB_Transaction_CommandGetBalance);
        AB_Transaction_SetUniqueAccountId(job, AB_AccountSpec_GetUniqueId(spec));
        AB_Transaction_List2_PushBack(jobs, job);
    }
    AB_AccountSpec_List_free(specs);
    if (AB_Transaction_List2_GetSize(jobs) == 0) {
        AB_Transaction_List2_free(jobs);
        *errorMessage = tr("There are no accounts.");
        return false;
    }

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    reportProgress(tr("Retrieving balances..."));
    rv = AB_Banking_SendCommands(banking, jobs, context);
    AB_Transaction_List2_freeAll(jobs);

    const QVariantList balances = StatementReader::balances(context, QDateTime::currentDateTimeUtc());
    AB_ImExporterContext_free(context);

    if (!balances.isEmpty()) {
        storeBalances(balances);
        emit balancesFetched(balances);
    }
    const bool success = rv >= 0 && !balances.isEmpty();
    if (!success) {
        *errorMessage = failureMessage(rv < 0 ? rv : GWEN_ERROR_NO_DATA, tr("The bank sent no balances."));
    }
    return success;
}

void BankingThread::executeFetchTransactions(const Command &command)
{
    emit commandStarted(FetchTransactions);
    if (!banking) {
        emit commandFinished(FetchTransactions, false, tr("Banking is not available."));
        return;
    }
    const uint accountId = command.arguments.value(QStringLiteral("accountId")).toUInt();

    // CAMT first, as some banks no longer offer MT940 at all; but not where the
    // account's UPD lists only another job, such as a credit card's.
    const int jobs = ZB_TransactionJobs(banking, accountId);
    if (jobs < 0) {
        qWarning() << "Could not tell which jobs the bank offers for this account" << jobs;
    }
    const bool camtFirst = ZB_PreferCamtFor(jobs);
    const int preferred = ZB_SetCamtDownload(banking, accountId, camtFirst ? 1 : 0);
    if (preferred < 0) {
        qWarning() << "Could not set the CAMT preference for this account" << preferred;
    }

    reportProgress(tr("Retrieving transactions..."));
    const int bicResult = ZB_EnsureAccountBic(banking, accountId);
    if (bicResult < 0) {
        // Not the end yet: the bank may do without, and if not, its answer says so.
        qWarning() << "Could not fetch the SEPA details of this account" << bicResult;
    } else if (bicResult > 0) {
        publishAccounts();
    }
    lastError.clear();
    startCollectingBankMessages();

    QVariantList transactions;
    bool hasAnswer = false;
    bool jobFailed = false;
    int rv = requestTransactions(accountId, &transactions, &hasAnswer, &jobFailed);
    // A bank that rejects the request does not fail the job or the call, it only
    // reports an error: without transactions, that is not "no transactions".
    bool success = rv >= 0 && hasAnswer && !jobFailed && (!transactions.isEmpty() || lastError.isEmpty());

    // Before the result, so that the page has them when it learns the outcome.
    reportBankMessages();
    if (success) {
        emit transactionsFetched(accountId, transactions);
    }

    // A job that fails without an error message never got as far as the bank's
    // answer being read, or was never sent at all: AqBanking only tells its log.
    QString message;
    if (!success) {
        if (jobFailed && lastError.isEmpty()) {
            message = hasAnswer ? tr("The bank's answer could not be read.")
                                : tr("The bank does not offer transactions for this account.");
        } else {
            message = failureMessage(rv < 0 ? rv : GWEN_ERROR_GENERIC, tr("The transactions could not be retrieved."));
        }
    }
    emit commandFinished(FetchTransactions, success, message);
}

int BankingThread::requestTransactions(uint32_t accountId, QVariantList *transactions, bool *hasAnswer, bool *jobFailed)
{
    AB_TRANSACTION *job = AB_Transaction_new();
    AB_Transaction_SetCommand(job, AB_Transaction_CommandGetTransactions);
    AB_Transaction_SetUniqueAccountId(job, accountId);
    const QDate firstDay = QDate::currentDate().addDays(-TRANSACTION_DAYS);
    GWEN_DATE *firstDate = GWEN_Date_fromGregorian(firstDay.year(), firstDay.month(), firstDay.day());
    AB_Transaction_SetFirstDate(job, firstDate);
    GWEN_Date_free(firstDate);
    AB_TRANSACTION_LIST2 *jobs = AB_Transaction_List2_new();
    AB_Transaction_List2_PushBack(jobs, job);

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    const int rv = AB_Banking_SendCommands(banking, jobs, context);
    *jobFailed = AB_Transaction_GetStatus(job) == AB_Transaction_StatusError;
    AB_Transaction_List2_freeAll(jobs);

    // AB_Banking_SendCommands() also returns 0 when the connection failed, and
    // no transactions may simply mean there were none. But the job only creates
    // the account's entry in the context once the bank has answered.
    *hasAnswer = false;
    transactions->clear();
    for (const AB_IMEXPORTER_ACCOUNTINFO *info = AB_ImExporterContext_GetFirstAccountInfo(context); info;
         info = AB_ImExporterAccountInfo_List_Next(info)) {
        if (AB_ImExporterAccountInfo_GetAccountId(info) == accountId) {
            *hasAnswer = true;
            *transactions += StatementReader::transactions(info);
        }
    }
    AB_ImExporterContext_free(context);
    return rv;
}

void BankingThread::executeFetchHoldings(const Command &command)
{
    emit commandStarted(FetchHoldings);
    if (!banking) {
        emit commandFinished(FetchHoldings, false, tr("Banking is not available."));
        return;
    }
    const uint accountId = command.arguments.value(QStringLiteral("accountId")).toUInt();

    reportProgress(tr("Retrieving holdings..."));
    lastError.clear();

    AB_TRANSACTION *job = AB_Transaction_new();
    AB_Transaction_SetCommand(job, AB_Transaction_CommandGetDepot);
    AB_Transaction_SetUniqueAccountId(job, accountId);
    AB_TRANSACTION_LIST2 *jobs = AB_Transaction_List2_new();
    AB_Transaction_List2_PushBack(jobs, job);

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    const int rv = AB_Banking_SendCommands(banking, jobs, context);
    const AB_TRANSACTION_STATUS status = AB_Transaction_GetStatus(job);
    AB_Transaction_List2_freeAll(jobs);

    const QVariantList holdings = StatementReader::holdings(context);
    AB_ImExporterContext_free(context);

    // Unlike with transactions, the context has no entry for the account to prove
    // an answer. The job does: it only ends up accepted if the bank said it carried
    // out the request or has no entries, which is what an empty depot looks like.
    const bool success = rv >= 0 && (!holdings.isEmpty() || (status == AB_Transaction_StatusAccepted && lastError.isEmpty()));

    if (success) {
        emit holdingsFetched(accountId, holdings);
    }

    QString message;
    if (!success) {
        // Error without a message: never sent, as AqBanking found no job for this
        // account in the bank's parameters, and only tells its log.
        message = status == AB_Transaction_StatusError && lastError.isEmpty()
                ? tr("The bank does not offer the holdings of this account.")
                : failureMessage(rv < 0 ? rv : GWEN_ERROR_GENERIC, tr("The holdings could not be retrieved."));
    }
    emit commandFinished(FetchHoldings, success, message);
}

// For a bank access that has no accounts: the setup of one that brought none
// fails, but one set up before that rule, or whose bank listed none later, is
// shown without them, with this to try again.
void BankingThread::executeRetrieveAccounts(const Command &command)
{
    emit commandStarted(RetrieveAccounts);
    if (!banking) {
        emit commandFinished(RetrieveAccounts, false, tr("Banking is not available."));
        return;
    }
    const uint32_t uid = command.arguments.value(QStringLiteral("bankAccessId")).toUInt();
    startCollectingBankMessages();
    reportProgress(tr("Retrieving your accounts..."));
    lastError.clear();
    const int rv = ZB_GetAccounts(banking, uid);
    publishAccounts();
    QString message;
    if (rv > 0) {
        QString balanceError;
        retrieveBalances(&balanceError, true, uid);
    } else {
        message = rv < 0 ? failureMessage(rv, tr("The bank did not list your accounts."))
                         : tr("The bank listed no accounts for this access.");
    }
    reportBankMessages();
    emit commandFinished(RetrieveAccounts, rv > 0, message);
}

void BankingThread::startCollectingBankMessages()
{
    bankMessages.clear();
    collectingBankMessages = true;
}

void BankingThread::reportBankMessages()
{
    collectingBankMessages = false;
    emit bankMessagesReported(bankMessages);
}

void BankingThread::executeRemoveBankAccess(const Command &command)
{
    emit commandStarted(RemoveBankAccess);
    if (!banking) {
        emit commandFinished(RemoveBankAccess, false, tr("Banking is not available."));
        return;
    }
    const int rv = ZB_DeleteUser(banking, command.arguments.value(QStringLiteral("bankAccessId")).toUInt());
    publishAccounts();
    emit commandFinished(RemoveBankAccess, rv >= 0, rv >= 0 ? QString() : failureMessage(rv, tr("The bank access could not be removed.")));
}

void BankingThread::executeResetData()
{
    emit commandStarted(ResetData);
    shutDownAqBanking();
    storedBalances.clear();
    const bool removed = !QFile::exists(dataFilePath()) || QFile::remove(dataFilePath());
    emit accountsChanged(QVariantList());
    emit commandFinished(ResetData, removed, removed ? QString() : tr("The stored data could not be removed."));
}

void BankingThread::publishAccounts()
{
    QVariantList accounts;
    if (!banking) {
        emit accountsChanged(accounts);
        return;
    }

    QMap<uint, ZB_USER_INFO> users;
    ZB_USER_INFO userInfos[MAX_USERS];
    const int userCount = ZB_ListUsers(banking, userInfos, MAX_USERS);
    for (int i = 0; i < userCount; i++) {
        users.insert(userInfos[i].uid, userInfos[i]);
    }

    AB_ACCOUNT_SPEC_LIST *specs = nullptr;
    if (AB_Banking_GetAccountSpecList(banking, &specs) >= 0 && specs) {
        for (AB_ACCOUNT_SPEC *spec = AB_AccountSpec_List_First(specs); spec; spec = AB_AccountSpec_List_Next(spec)) {
            const uint32_t accountId = AB_AccountSpec_GetUniqueId(spec);
            uint32_t userUid = 0;
            ZB_GetAccountUser(banking, accountId, &userUid);
            QVariantMap account;
            account.insert(QStringLiteral("accountId"), accountId);
            account.insert(QStringLiteral("bankAccessId"), userUid);
            account.insert(QStringLiteral("bankName"), users.contains(userUid)
                           ? QString::fromUtf8(users.value(userUid).userName)
                           : QString::fromUtf8(AB_AccountSpec_GetBankCode(spec)));
            account.insert(QStringLiteral("bankCode"), QString::fromUtf8(AB_AccountSpec_GetBankCode(spec)));
            account.insert(QStringLiteral("accountNumber"), QString::fromUtf8(AB_AccountSpec_GetAccountNumber(spec)));
            account.insert(QStringLiteral("iban"), QString::fromUtf8(AB_AccountSpec_GetIban(spec)));
            account.insert(QStringLiteral("name"), QString::fromUtf8(AB_AccountSpec_GetAccountName(spec)));
            account.insert(QStringLiteral("owner"), QString::fromUtf8(AB_AccountSpec_GetOwnerName(spec)));
            account.insert(QStringLiteral("currency"), QString::fromUtf8(AB_AccountSpec_GetCurrency(spec)));
            account.insert(QStringLiteral("isDepot"), AB_AccountSpec_GetType(spec) == AB_AccountType_Investment);
            accounts.append(account);
        }
        AB_AccountSpec_List_free(specs);
    }
    // A bank access without accounts as an entry of its own (accountId 0), so
    // that the overview shows it, to retrieve them again or to remove it.
    QSet<uint> withAccounts;
    for (const QVariant &account : accounts) {
        withAccounts.insert(account.toMap().value(QStringLiteral("bankAccessId")).toUInt());
    }
    for (auto user = users.constBegin(); user != users.constEnd(); ++user) {
        if (!withAccounts.contains(user.key())) {
            QVariantMap placeholder;
            placeholder.insert(QStringLiteral("accountId"), 0u);
            placeholder.insert(QStringLiteral("bankAccessId"), user.key());
            placeholder.insert(QStringLiteral("bankName"), QString::fromUtf8(user.value().userName));
            placeholder.insert(QStringLiteral("bankCode"), QString::fromUtf8(user.value().bankCode));
            placeholder.insert(QStringLiteral("noAccounts"), true);
            accounts.append(placeholder);
        }
    }
    emit accountsChanged(accounts);

    // The last known balances, until new ones arrive. Those of accounts that
    // are gone (a bank access was removed) are not kept any longer.
    QSet<uint> accountIds;
    for (const QVariant &account : accounts) {
        accountIds.insert(account.toMap().value(QStringLiteral("accountId")).toUInt());
    }
    bool pruned = false;
    for (auto balance = storedBalances.begin(); balance != storedBalances.end();) {
        if (accountIds.contains(balance.key())) {
            ++balance;
        } else {
            balance = storedBalances.erase(balance);
            pruned = true;
        }
    }
    if (pruned) {
        saveStoredBalances();
    }
    if (!storedBalances.isEmpty()) {
        emit balancesFetched(storedBalanceList());
    }
}

QVariantList BankingThread::storedBalanceList() const
{
    QVariantList balances;
    for (const QVariantMap &balance : storedBalances) {
        balances.append(balance);
    }
    return balances;
}

void BankingThread::loadStoredBalances()
{
    storedBalances.clear();
    const QByteArray data = configStore->appData(BALANCES_NAME);
    if (data.isEmpty()) {
        return;
    }
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_5_6);
    qint32 version = 0;
    QVariantList balances;
    stream >> version;
    if (version != BALANCES_VERSION) {
        return;
    }
    stream >> balances;
    if (stream.status() != QDataStream::Ok) {
        return;
    }
    for (const QVariant &balance : balances) {
        const QVariantMap entry = balance.toMap();
        storedBalances.insert(entry.value(QStringLiteral("accountId")).toUInt(), entry);
    }
}

/*
 * Balances fetched just now replace the stored ones of their accounts; the
 * others stay as they were, as on the overview.
 */
void BankingThread::storeBalances(const QVariantList &balances)
{
    for (const QVariant &balance : balances) {
        const QVariantMap entry = balance.toMap();
        storedBalances.insert(entry.value(QStringLiteral("accountId")).toUInt(), entry);
    }
    saveStoredBalances();
}

void BankingThread::saveStoredBalances()
{
    if (!configStore) {
        return;
    }
    QByteArray data;
    {
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_6);
        stream << BALANCES_VERSION << storedBalanceList();
    }
    if (!configStore->setAppData(BALANCES_NAME, data)) {
        qWarning() << "The balances could not be stored";
    }
}

void BankingThread::shutDownAqBanking()
{
    if (banking) {
        AB_Banking_Fini(banking);
        AB_Banking_free(banking);
        banking = nullptr;
    }
    delete configStore;
    configStore = nullptr;
}

QString BankingThread::failureMessage(int errorCode, const QString &fallback) const
{
    if (errorCode == GWEN_ERROR_USER_ABORTED) {
        return tr("Cancelled.");
    }
    if (!lastError.isEmpty()) {
        return lastError;
    }
    return tr("%1 (Error %2)").arg(fallback).arg(errorCode);
}
