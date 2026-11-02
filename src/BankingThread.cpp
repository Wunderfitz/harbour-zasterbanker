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

#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QSet>

#include <aqbanking/banking_online.h>
#include <aqbanking/types/account_spec.h>
#include <aqbanking/types/balance.h>
#include <aqbanking/types/imexporter_context.h>
#include <aqbanking/types/transaction.h>
#include <gwenhywfar/gwenhywfar.h>
#include <gwenhywfar/logger.h>

#include <cstring>

namespace {
// AqBanking's name for this application; it keys the app's own settings group.
const char APP_NAME[] = "ZasterBanker";
// Sent to the bank with every dialog, at most five characters. Bump with the spec.
const char FINTS_APP_VERSION[] = "0.5";
const char DATA_FILE[] = "aqbanking.zbs";
const int MAX_TAN_METHODS = 32;
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
        emit progress(text);
    }
}

void BankingThread::reportError(const QString &text)
{
    if (!text.isEmpty()) {
        lastError = text;
        emit progress(text);
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

    forever {
        Command command;
        {
            QMutexLocker locker(&mutex);
            while (commands.isEmpty() && !stopRequested) {
                condition.wait(&mutex);
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
            executeFetchBalances();
            break;
        case RemoveBankAccess:
            executeRemoveBankAccess(command);
            break;
        case ResetData:
            executeResetData();
            break;
        }
        GwenGui::forgetPins();
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
    uint32_t uid = 0;
    int rv = ZB_AddPinTanUser(banking,
                              arguments.value(QStringLiteral("bankCode")).toString().toUtf8().constData(),
                              arguments.value(QStringLiteral("userId")).toString().toUtf8().constData(),
                              arguments.value(QStringLiteral("customerId")).toString().toUtf8().constData(),
                              arguments.value(QStringLiteral("serverUrl")).toString().toUtf8().constData(),
                              arguments.value(QStringLiteral("bankName")).toString().toUtf8().constData(),
                              &uid);
    if (rv < 0) {
        emit commandFinished(AddBankAccess, false, failureMessage(rv, tr("The bank access could not be created.")));
        return;
    }

    // A failed setup leaves nothing behind: the half-made user goes again.
    reportProgress(tr("Connecting to the bank..."));
    rv = ZB_GetSysId(banking, uid);
    if (rv < 0) {
        const QString message = failureMessage(rv, tr("The bank did not accept the connection."));
        ZB_DeleteUser(banking, uid);
        emit commandFinished(AddBankAccess, false, message);
        return;
    }

    if (!chooseTanMethod(uid)) {
        const QString message = failureMessage(GWEN_ERROR_USER_ABORTED, QString());
        ZB_DeleteUser(banking, uid);
        emit commandFinished(AddBankAccess, false, message);
        return;
    }

    reportProgress(tr("Retrieving your accounts..."));
    rv = ZB_GetAccounts(banking, uid);
    if (rv < 0) {
        const QString message = failureMessage(rv, tr("The bank did not list your accounts."));
        ZB_DeleteUser(banking, uid);
        emit commandFinished(AddBankAccess, false, message);
        return;
    }

    publishAccounts();
    // Within the same command, so the PIN entered for the setup is reused.
    // Balances that do not come are no reason to undo the setup.
    QString balanceError;
    retrieveBalances(&balanceError);
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
    return true;
}

void BankingThread::executeFetchBalances()
{
    emit commandStarted(FetchBalances);
    if (!banking) {
        emit commandFinished(FetchBalances, false, tr("Banking is not available."));
        return;
    }
    QString message;
    const bool success = retrieveBalances(&message);
    emit commandFinished(FetchBalances, success, message);
}

bool BankingThread::retrieveBalances(QString *errorMessage)
{
    AB_ACCOUNT_SPEC_LIST *specs = nullptr;
    int rv = AB_Banking_GetAccountSpecList(banking, &specs);
    if (rv < 0 || !specs) {
        *errorMessage = tr("There are no accounts.");
        return false;
    }

    AB_TRANSACTION_LIST2 *jobs = AB_Transaction_List2_new();
    for (AB_ACCOUNT_SPEC *spec = AB_AccountSpec_List_First(specs); spec; spec = AB_AccountSpec_List_Next(spec)) {
        AB_TRANSACTION *job = AB_Transaction_new();
        AB_Transaction_SetCommand(job, AB_Transaction_CommandGetBalance);
        AB_Transaction_SetUniqueAccountId(job, AB_AccountSpec_GetUniqueId(spec));
        AB_Transaction_List2_PushBack(jobs, job);
    }
    AB_AccountSpec_List_free(specs);

    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    reportProgress(tr("Retrieving balances..."));
    rv = AB_Banking_SendCommands(banking, jobs, context);
    AB_Transaction_List2_freeAll(jobs);

    QVariantList balances;
    for (AB_IMEXPORTER_ACCOUNTINFO *info = AB_ImExporterContext_GetFirstAccountInfo(context); info;
         info = AB_ImExporterAccountInfo_List_Next(info)) {
        AB_BALANCE_LIST *list = AB_ImExporterAccountInfo_GetBalanceList(info);
        if (!list) {
            continue;
        }
        // The booked balance is what the bank statement says; the others
        // (noted, disposable, ...) are only a fallback.
        const AB_BALANCE *chosen = nullptr;
        for (const AB_BALANCE *balance = AB_Balance_List_First(list); balance; balance = AB_Balance_List_Next(balance)) {
            if (!chosen || AB_Balance_GetType(balance) == AB_Balance_TypeBooked) {
                chosen = balance;
            }
        }
        const AB_VALUE *value = chosen ? AB_Balance_GetValue(chosen) : nullptr;
        if (!value) {
            continue;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("accountId"), AB_ImExporterAccountInfo_GetAccountId(info));
        entry.insert(QStringLiteral("balance"), AB_Value_GetValueAsDouble(value));
        entry.insert(QStringLiteral("currency"), QString::fromUtf8(AB_Value_GetCurrency(value)));
        const GWEN_DATE *date = AB_Balance_GetDate(chosen);
        if (date) {
            entry.insert(QStringLiteral("date"), QDate(GWEN_Date_GetYear(date), GWEN_Date_GetMonth(date), GWEN_Date_GetDay(date)));
        }
        balances.append(entry);
    }
    AB_ImExporterContext_free(context);

    if (!balances.isEmpty()) {
        emit balancesFetched(balances);
    }
    const bool success = rv >= 0 && !balances.isEmpty();
    if (!success) {
        *errorMessage = failureMessage(rv < 0 ? rv : GWEN_ERROR_NO_DATA, tr("The bank sent no balances."));
    }
    return success;
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
            account.insert(QStringLiteral("type"), AB_AccountSpec_GetType(spec));
            accounts.append(account);
        }
        AB_AccountSpec_List_free(specs);
    }
    emit accountsChanged(accounts);
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
