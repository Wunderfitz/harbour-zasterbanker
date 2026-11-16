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
#include "DebugLog.h"
#include "EncryptedConfigStore.h"
#include "FinTsRegistration.h"
#include "GwenGui.h"
#include "LogMasker.h"
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

#include <aqbanking/account_type.h>
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
// Accounts the bank no longer lists, asked about at once.
const int MAX_UNLISTED_ACCOUNTS = 64;

void wipe(QByteArray &data)
{
    if (!data.isEmpty()) {
        std::memset(data.data(), 0, static_cast<size_t>(data.size()));
    }
    data.clear();
}

// For the debug log, which writes everything in English.
QString commandName(int command)
{
    switch (command) {
    case BankingThread::Initialize: return QStringLiteral("start banking");
    case BankingThread::AddBankAccess: return QStringLiteral("add bank access");
    case BankingThread::FetchBalances: return QStringLiteral("fetch balances");
    case BankingThread::RemoveBankAccess: return QStringLiteral("remove bank access");
    case BankingThread::ResetData: return QStringLiteral("delete all data");
    case BankingThread::FetchTransactions: return QStringLiteral("fetch transactions");
    case BankingThread::FetchHoldings: return QStringLiteral("fetch holdings");
    case BankingThread::UpdateBankAccess: return QStringLiteral("update accounts and bank data");
    case BankingThread::ChangeTanMethod: return QStringLiteral("change TAN method");
    }
    return QString::number(command);
}

QString interactionName(int type)
{
    switch (type) {
    case BankingThread::PinInteraction: return QStringLiteral("PIN");
    case BankingThread::TanInteraction: return QStringLiteral("TAN");
    case BankingThread::TextInteraction: return QStringLiteral("text");
    case BankingThread::ChoiceInteraction: return QStringLiteral("choice");
    case BankingThread::MessageInteraction: return QStringLiteral("message");
    }
    return QString::number(type);
}

QString yesNo(bool value)
{
    return value ? QStringLiteral("yes") : QStringLiteral("no");
}

// Personal values in the log: one * per character, as LogMasker does.
QString masked(const char *value)
{
    return QString(static_cast<int>(value ? std::strlen(value) : 0), QLatin1Char('*'));
}

void log(const QString &text)
{
    DebugLog::write(DebugLog::App, text);
}
}

BankingThread::BankingThread(const QString &dataDirectory, QObject *parent)
    : QThread(parent)
    , dataDirectory(dataDirectory)
    , stopRequested(false)
    , cancelRequested(false)
    , forgetPinsRequested(false)
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

void BankingThread::updateBankAccess(uint bankAccessId)
{
    Command command;
    command.type = UpdateBankAccess;
    command.arguments.insert(QStringLiteral("bankAccessId"), bankAccessId);
    enqueue(command);
}

void BankingThread::changeTanMethod(uint bankAccessId)
{
    Command command;
    command.type = ChangeTanMethod;
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

void BankingThread::forgetPins()
{
    QMutexLocker locker(&mutex);
    forgetPinsRequested = true;
    condition.wakeAll();
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
                                                          const QString &mimeType, const QString &presetText,
                                                          const QString &flickerCode, bool digitsOnly)
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

    if (DebugLog::isEnabled()) {
        // A TAN challenge may name payee and amount: of it, only the shape.
        log(QStringLiteral("Dialog %1 (%2): \"%3\" - %4%5%6%7")
            .arg(interactionId).arg(interactionName(type), LogMasker::text(title),
                 type == TanInteraction ? LogMasker::everything(text) : LogMasker::text(text),
                 choices.isEmpty() ? QString() : QStringLiteral(" - choices: ") + LogMasker::text(choices.join(QStringLiteral(" | "))),
                 image.isEmpty() ? QString() : QStringLiteral(" - image: %1 bytes of %2, %3")
                                                .arg(image.size()).arg(mimeType, decodedImage.isNull() ? QStringLiteral("not readable")
                                                                                                       : QStringLiteral("%1x%2").arg(decodedImage.width()).arg(decodedImage.height())),
                 flickerCode.isEmpty() ? (digitsOnly ? QStringLiteral(" - digits only") : QString())
                                       : QStringLiteral(" - flicker code of %1 frames").arg(flickerCode.length())));
    }
    emit interactionRequested(interactionId, type, title, text, choices, decodedImage, presetText, flickerCode,
                              digitsOnly);

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

    // never what was typed, nor, for PIN and TAN, how long it was
    QString answer = QStringLiteral("cancelled");
    if (response.accepted) {
        switch (type) {
        case ChoiceInteraction:
        case MessageInteraction:
            answer = QStringLiteral("answered with choice %1").arg(response.choice);
            break;
        case TextInteraction:
            answer = QStringLiteral("answered with %1 characters").arg(response.text.size());
            break;
        default:
            answer = QStringLiteral("answered");
            break;
        }
    }
    log(QStringLiteral("Dialog %1 %2").arg(interactionId).arg(answer));
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
    applyLogLevels();
    gui = GwenGui::create(this);
    // Every FinTS message, to the debug log, which masks it, if it is on.
    ZB_SetMessageLogHook(DebugLog::writeFinTsMessage);
    LogMasker::addPersonalData(QStringList() << QStringLiteral(FINTS_REGISTRATION_NUMBER));
    GWEN_Gui_SetGui(gui);

    GWEN_PLUGIN_MANAGER *dbioPlugins = GWEN_PluginManager_FindPluginManager(GWEN_DBIO_PLUGIN_NAME);
    GWEN_PluginManager_AddPlugin(dbioPlugins, dbio_swift_factory(dbioPlugins, "swift", nullptr));

    forever {
        Command command;
        bool haveCommand = false;
        bool forget = false;
        {
            QMutexLocker locker(&mutex);
            if (commands.isEmpty() && !stopRequested && !forgetPinsRequested) {
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
            forget = forgetPinsRequested;
            forgetPinsRequested = false;
            if (!commands.isEmpty()) {
                command = commands.dequeue();
                cancelRequested = false;
                haveCommand = true;
            }
        }
        if (forget) {
            GwenGui::forgetPins();
        }
        if (!haveCommand) {
            GwenGui::forgetExpiredPins();
            publishPins(forget);
            continue;
        }
        lastError.clear();
        applyLogLevels();
        commandTimer.start();
        logCommand(command);

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
        case UpdateBankAccess:
            executeUpdateBankAccess(command);
            break;
        case ChangeTanMethod:
            executeChangeTanMethod(command);
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
        // the times start again when the command ends
        publishPins(true);
    }

    shutDownAqBanking();
    ZB_SetMessageLogHook(nullptr);
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
        log(QStringLiteral("The stored data cannot be decrypted (%1)").arg(loadResult));
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
        log(QStringLiteral("AB_Banking_Init failed: %1").arg(rv));
        emit initialized(InitializationFailed, tr("The banking library could not be started (%1).").arg(rv));
        return;
    }

    loadStoredBalances();
    // The TAN input mechanism follows the selected TAN method (chipTAN QR and
    // optical); accesses set up before the app set it get it now.
    ZB_USER_INFO users[MAX_USERS];
    const int userCount = ZB_ListUsers(banking, users, MAX_USERS);
    for (int i = 0; i < userCount; i++) {
        const int mechanism = ZB_UpdateTanMechanism(banking, users[i].uid);
        if (mechanism < 0) {
            qWarning() << "Could not set the TAN input mechanism" << mechanism;
        }
    }
    log(QStringLiteral("Banking started: %1 bank accesses, %2 stored balances").arg(qMax(userCount, 0)).arg(storedBalances.size()));
    emit initialized(InitializationSucceeded, QString());
    publishAccounts();
    for (int i = 0; i < userCount; i++) {
        logBankAccess(users[i].uid);
    }
}

void BankingThread::executeAddBankAccess(const Command &command)
{
    emit commandStarted(AddBankAccess);
    if (!banking) {
        finishCommand(AddBankAccess, false, tr("Banking is not available."));
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
    log(QStringLiteral("Bank access %1 created: %2").arg(uid).arg(rv));
    if (rv < 0) {
        finishCommand(AddBankAccess, false, failureMessage(rv, tr("The bank access could not be created.")));
        return;
    }

    // A failed setup leaves nothing behind: the half-made user goes again.
    reportProgress(tr("Connecting to the bank..."));
    rv = ZB_GetSysId(banking, uid);
    log(QStringLiteral("Handshake (system ID): %1").arg(rv));
    if (rv < 0) {
        const QString message = failureMessage(rv, tr("The bank did not accept the connection."));
        ZB_DeleteUser(banking, uid);
        finishCommand(AddBankAccess, false, message);
        return;
    }

    if (!chooseTanMethod(uid)) {
        const QString message = failureMessage(GWEN_ERROR_USER_ABORTED, QString());
        ZB_DeleteUser(banking, uid);
        finishCommand(AddBankAccess, false, message);
        return;
    }

    reportProgress(tr("Retrieving your accounts..."));
    rv = ZB_GetAccounts(banking, uid, nullptr, 0, nullptr);
    log(QStringLiteral("Account list: %1").arg(rv));
    // A bank access without accounts would be of no use (found 2026-11-11 at a
    // Sparkasse: the setup went through, and no account came).
    if (rv <= 0) {
        const QString message = rv < 0 ? failureMessage(rv, tr("The bank did not list your accounts."))
                                        : tr("The bank listed no accounts for this access.");
        ZB_DeleteUser(banking, uid);
        finishCommand(AddBankAccess, false, message);
        return;
    }

    publishAccounts();
    logBankAccess(uid);
    // Within the same command, so the PIN entered for the setup is reused.
    // Only the new bank access's: the others would ask for their PINs.
    // Balances that do not come are no reason to undo the setup.
    QString balanceError;
    retrieveBalances(&balanceError, true, uid);
    finishCommand(AddBankAccess, true, QString());
}

bool BankingThread::chooseTanMethod(uint32_t userUid, bool change)
{
    ZB_TAN_METHOD methods[MAX_TAN_METHODS];
    const int count = ZB_ListTanMethods(banking, userUid, methods, MAX_TAN_METHODS);
    QList<ZB_TAN_METHOD> allowed;
    QStringList known;
    for (int i = 0; i < count; i++) {
        if (methods[i].allowed) {
            allowed.append(methods[i]);
        }
        known.append(QStringLiteral("%1 \"%2\"%3%4, medium name %5")
                     .arg(methods[i].id).arg(QString::fromUtf8(methods[i].name),
                                             methods[i].allowed ? QStringLiteral(" allowed") : QString(),
                                             methods[i].selected ? QStringLiteral(" selected") : QString(),
                                             methods[i].tanMediumNeeded == 2 ? QStringLiteral("required")
                                             : (methods[i].tanMediumNeeded == 1 ? QStringLiteral("optional") : QStringLiteral("not allowed"))));
    }
    log(QStringLiteral("TAN methods (%1): %2").arg(count).arg(LogMasker::text(known.join(QStringLiteral("; ")))));
    // None means the bank does without two-step TANs for this user; AqBanking
    // then sticks to the single-step procedure on its own.
    if (allowed.isEmpty()) {
        if (change) {
            const Response response = requestInteraction(MessageInteraction, tr("TAN method"),
                                                         tr("Your bank named no TAN method for this access at the last contact. "
                                                            "Update the accounts and bank data to ask it again."));
            return response.accepted;
        }
        return true;
    }

    int chosen = 0;
    if (allowed.size() > 1 || change) {
        QStringList names;
        QString current;
        for (const ZB_TAN_METHOD &method : allowed) {
            names.append(QString::fromUtf8(method.name));
            if (method.selected) {
                current = QString::fromUtf8(method.name);
            }
        }
        QString text;
        if (change) {
            // Offered from what the bank allowed at the last contact, which every
            // contact renews: no connection, so it works even if the current
            // method no longer does.
            text = (current.isEmpty() ? QString() : tr("You currently use %1.").arg(current) + QLatin1Char(' '))
                    + tr("Which of the TAN methods your bank allows for this access do you want to use? "
                         "If one is missing, update the accounts and bank data first.");
        } else {
            text = tr("Your bank offers several ways to confirm with a TAN. Which one do you use?");
        }
        const Response response = requestInteraction(ChoiceInteraction, tr("TAN method"), text, names);
        if (!response.accepted || response.choice < 0 || response.choice >= allowed.size()) {
            return false;
        }
        chosen = response.choice;
    }
    const int rv = ZB_SelectTanMethod(banking, userUid, allowed.at(chosen).id);
    log(QStringLiteral("TAN method %1 selected: %2").arg(allowed.at(chosen).id).arg(rv));
    if (rv < 0) {
        qWarning() << "Selecting TAN method" << allowed.at(chosen).id << "failed" << rv;
        // At setup, AqBanking goes on with the method it chose itself, as before.
        if (change) {
            lastError = tr("The TAN method could not be stored (%1).").arg(rv);
            return false;
        }
    }

    char medium[ZB_NAME_LENGTH];
    const QString currentMedium = ZB_GetTanMediumId(banking, userUid, medium, sizeof(medium)) >= 0
            ? QString::fromLatin1(medium) : QString();
    switch (allowed.at(chosen).tanMediumNeeded) {
    case 2:
        // Only when required: an optional name the user would have to look up for nothing.
        return chooseTanMedium(userUid, currentMedium);
    case 0:
        // The bank allows none with this method; one set for another must not go along.
        if (!currentMedium.isEmpty() && ZB_SetTanMediumId(banking, userUid, nullptr) < 0) {
            qWarning() << "Clearing the TAN medium failed";
        }
        return true;
    default:
        return true;
    }
}

// Some banks (Deutsche Bank and Postbank for BestSign, for one) want the name of
// the device the TAN comes from with every TAN, and refuse the dialog without it.
// AqBanking cannot ask the bank for the list of names, so the user types it.
bool BankingThread::chooseTanMedium(uint32_t userUid, const QString &current)
{
    QString hint;
    QString preset = current;
    forever {
        const Response response = requestInteraction(TextInteraction, tr("TAN medium"),
                                                     hint + tr("Your bank needs the name of the device or app you confirm with. "
                                                               "You find it in the security settings of your online banking or "
                                                               "of the bank's app, e.g. under \"Sicherheitsverfahren\" for BestSign. "
                                                               "Enter it exactly as shown there, capitals included."),
                                                     QStringList(), QByteArray(), QString(), preset);
        if (!response.accepted) {
            return false;
        }
        const QString name = QString::fromUtf8(response.text).trimmed();
        preset = name;
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
        log(QStringLiteral("TAN medium name of %1 characters stored: %2").arg(latin1.size()).arg(rv));
        if (rv < 0) {
            qWarning() << "Setting the TAN medium failed" << rv;
            lastError = tr("The name of the TAN medium could not be stored (%1).").arg(rv);
            return false;
        }
        return true;
    }
}

void BankingThread::executeFetchBalances(const Command &command)
{
    emit commandStarted(FetchBalances);
    if (!banking) {
        finishCommand(FetchBalances, false, tr("Banking is not available."));
        return;
    }
    QString message;
    const QVariant bankAccessId = command.arguments.value(QStringLiteral("bankAccessId"));
    const bool success = bankAccessId.isValid() ? retrieveBalances(&message, true, bankAccessId.toUInt())
                                                : retrieveBalances(&message);
    finishCommand(FetchBalances, success, message);
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
    const int jobCount = AB_Transaction_List2_GetSize(jobs);
    rv = AB_Banking_SendCommands(banking, jobs, context);
    if (DebugLog::isEnabled()) {
        QStringList statuses;
        AB_TRANSACTION_LIST2_ITERATOR *it = AB_Transaction_List2_First(jobs);
        for (AB_TRANSACTION *job = it ? AB_Transaction_List2Iterator_Data(it) : nullptr; job;
             job = AB_Transaction_List2Iterator_Next(it)) {
            statuses.append(QStringLiteral("account %1 %2").arg(AB_Transaction_GetUniqueAccountId(job))
                            .arg(QLatin1String(AB_Transaction_Status_toString(AB_Transaction_GetStatus(job)))));
        }
        AB_Transaction_List2Iterator_free(it);
        log(QStringLiteral("Balance jobs for %1 accounts sent: %2 (%3)").arg(jobCount).arg(rv).arg(statuses.join(QStringLiteral(", "))));
    }
    AB_Transaction_List2_freeAll(jobs);

    const QVariantList balances = StatementReader::balances(context, QDateTime::currentDateTimeUtc());
    AB_ImExporterContext_free(context);
    log(QStringLiteral("Balances read: %1").arg(balances.size()));

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
        finishCommand(FetchTransactions, false, tr("Banking is not available."));
        return;
    }
    const uint accountId = command.arguments.value(QStringLiteral("accountId")).toUInt();
    logAccount(accountId);

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
    log(QStringLiteral("Jobs for transactions: %1 (CAMT %2, other %3, UPD lists HKCAZ %4); CAMT first: %5 (%6)")
        .arg(jobs).arg(yesNo(jobs >= 0 && (jobs & ZB_TRANSACTION_JOB_CAMT)), yesNo(jobs >= 0 && (jobs & ZB_TRANSACTION_JOB_OTHER)),
                       yesNo(jobs >= 0 && (jobs & ZB_TRANSACTION_JOB_CAMT_LISTED)), yesNo(camtFirst))
        .arg(preferred));

    reportProgress(tr("Retrieving transactions..."));
    const int bicResult = ZB_EnsureAccountBic(banking, accountId);
    log(QStringLiteral("SEPA details for the BIC: %1").arg(bicResult == 0 ? QStringLiteral("not needed") : QString::number(bicResult)));
    if (bicResult < 0) {
        // Not the end yet: the bank may do without, and if not, its answer says so.
        qWarning() << "Could not fetch the SEPA details of this account" << bicResult;
    } else if (bicResult > 0) {
        publishAccounts();
    }
    lastError.clear();

    QVariantList transactions;
    bool hasAnswer = false;
    bool jobFailed = false;
    int rv = requestTransactions(accountId, &transactions, &hasAnswer, &jobFailed);
    // A bank that rejects the request does not fail the job or the call, it only
    // reports an error: without transactions, that is not "no transactions".
    bool success = rv >= 0 && hasAnswer && !jobFailed && (!transactions.isEmpty() || lastError.isEmpty());
    log(QStringLiteral("Transactions: sent %1, answer %2, job failed %3, %4 transactions, last error: %5")
        .arg(rv).arg(yesNo(hasAnswer), yesNo(jobFailed)).arg(transactions.size())
        .arg(lastError.isEmpty() ? QStringLiteral("none") : LogMasker::text(lastError)));

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
    finishCommand(FetchTransactions, success, message);
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
        finishCommand(FetchHoldings, false, tr("Banking is not available."));
        return;
    }
    const uint accountId = command.arguments.value(QStringLiteral("accountId")).toUInt();
    logAccount(accountId);

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
    log(QStringLiteral("Holdings: sent %1, job %2, %3 holdings, last error: %4")
        .arg(rv).arg(QLatin1String(AB_Transaction_Status_toString(status))).arg(holdings.size())
        .arg(lastError.isEmpty() ? QStringLiteral("none") : LogMasker::text(lastError)));

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
    finishCommand(FetchHoldings, success, message);
}

// Everything about a bank access the bank decides, asked for afresh: its
// parameters (BPD), the accounts with theirs (UPD) and the TAN methods it
// allows. In AqBanking, that is all one request, the account list's. It also
// brings the accounts of an access that has none, which the setup of one that
// brought none fails on, but one set up before that rule may have.
void BankingThread::executeUpdateBankAccess(const Command &command)
{
    emit commandStarted(UpdateBankAccess);
    if (!banking) {
        finishCommand(UpdateBankAccess, false, tr("Banking is not available."));
        return;
    }
    const uint32_t uid = command.arguments.value(QStringLiteral("bankAccessId")).toUInt();
    logBankAccess(uid);
    reportProgress(tr("Updating accounts and bank data..."));
    lastError.clear();
    uint32_t unlisted[MAX_UNLISTED_ACCOUNTS];
    int unlistedCount = 0;
    int rv = ZB_GetAccounts(banking, uid, unlisted, MAX_UNLISTED_ACCOUNTS, &unlistedCount);
    // the bank's descriptions of its TAN methods may have changed
    const int mechanism = ZB_UpdateTanMechanism(banking, uid);
    log(QStringLiteral("Account list: %1, %2 accounts no longer listed; TAN input mechanism: %3").arg(rv).arg(unlistedCount).arg(mechanism));
    publishAccounts();
    logBankAccess(uid);
    if (rv > 0 && unlistedCount > 0) {
        QList<uint32_t> accountIds;
        for (int i = 0; i < unlistedCount; i++) {
            accountIds.append(unlisted[i]);
        }
        offerToRemoveAccounts(accountIds);
        rv = ZB_CountAccounts(banking, uid);
        log(QStringLiteral("Accounts left: %1").arg(rv));
    }
    QString message;
    if (rv > 0) {
        QString balanceError;
        retrieveBalances(&balanceError, true, uid);
    } else {
        message = rv < 0 ? failureMessage(rv, tr("The bank did not list your accounts."))
                         : tr("The bank listed no accounts for this access.");
    }
    finishCommand(UpdateBankAccess, rv > 0, message);
}

// AqBanking keeps an account the bank no longer lists (closed, say), so the
// user is asked. Removing one changes nothing at the bank.
void BankingThread::offerToRemoveAccounts(const QList<uint32_t> &accountIds)
{
    QStringList names;
    for (uint32_t accountId : accountIds) {
        AB_ACCOUNT_SPEC *spec = nullptr;
        QString name;
        if (AB_Banking_GetAccountSpecByUniqueId(banking, accountId, &spec) >= 0 && spec) {
            const QString accountName = QString::fromUtf8(AB_AccountSpec_GetAccountName(spec));
            const QString iban = QString::fromUtf8(AB_AccountSpec_GetIban(spec));
            const QString number = QString::fromUtf8(AB_AccountSpec_GetAccountNumber(spec));
            const QString identifier = iban.isEmpty() ? number : iban;
            name = accountName.isEmpty() || accountName == identifier
                    ? identifier : QStringLiteral("%1 (%2)").arg(accountName, identifier);
            AB_AccountSpec_free(spec);
        }
        // The dialog shows styled text, and these are the bank's names.
        names.append((name.isEmpty() ? tr("Account %1").arg(accountId) : name).toHtmlEscaped());
    }
    const Response response = requestInteraction(
                MessageInteraction, tr("Accounts no longer listed"),
                tr("Your bank no longer lists these accounts for this access:") + QStringLiteral("<br><br>")
                + names.join(QStringLiteral("<br>")) + QStringLiteral("<br><br>")
                + tr("Remove them from Zaster Banker? This changes nothing at the bank."),
                QStringList() << tr("Remove") << tr("Keep"));
    if (!response.accepted || response.choice != 1) {
        return;
    }
    for (uint32_t accountId : accountIds) {
        const int rv = ZB_DeleteAccount(banking, accountId);
        log(QStringLiteral("Account %1 removed: %2").arg(accountId).arg(rv));
        if (rv < 0) {
            qWarning() << "Removing an account failed" << rv;
        }
    }
    publishAccounts();
}

// Without contacting the bank: see chooseTanMethod().
void BankingThread::executeChangeTanMethod(const Command &command)
{
    emit commandStarted(ChangeTanMethod);
    if (!banking) {
        finishCommand(ChangeTanMethod, false, tr("Banking is not available."));
        return;
    }
    lastError.clear();
    logBankAccess(command.arguments.value(QStringLiteral("bankAccessId")).toUInt());
    const bool changed = chooseTanMethod(command.arguments.value(QStringLiteral("bankAccessId")).toUInt(), true);
    finishCommand(ChangeTanMethod, changed, changed ? QString()
                         : (lastError.isEmpty() ? failureMessage(GWEN_ERROR_USER_ABORTED, QString()) : lastError));
}

void BankingThread::executeRemoveBankAccess(const Command &command)
{
    emit commandStarted(RemoveBankAccess);
    if (!banking) {
        finishCommand(RemoveBankAccess, false, tr("Banking is not available."));
        return;
    }
    const int rv = ZB_DeleteUser(banking, command.arguments.value(QStringLiteral("bankAccessId")).toUInt());
    log(QStringLiteral("Bank access removed: %1").arg(rv));
    publishAccounts();
    finishCommand(RemoveBankAccess, rv >= 0, rv >= 0 ? QString() : failureMessage(rv, tr("The bank access could not be removed.")));
}

void BankingThread::executeResetData()
{
    emit commandStarted(ResetData);
    shutDownAqBanking();
    storedBalances.clear();
    const bool removed = !QFile::exists(dataFilePath()) || QFile::remove(dataFilePath());
    emit accountsChanged(QVariantList());
    finishCommand(ResetData, removed, removed ? QString() : tr("The stored data could not be removed."));
}

void BankingThread::publishPins(bool force)
{
    const QMap<QByteArray, qint64> remembered = GwenGui::rememberedPins();
    QVariantMap deadlines;
    if (!remembered.isEmpty() && banking) {
        // AqBanking's token name for a PIN/TAN user's PIN (AH_User_MkPasswdName())
        ZB_USER_INFO users[MAX_USERS];
        const int userCount = ZB_ListUsers(banking, users, MAX_USERS);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (int i = 0; i < userCount; i++) {
            const QByteArray token = QByteArray("PIN_") + QByteArray(users[i].bankCode) + '_' + QByteArray(users[i].userId);
            const auto pin = remembered.constFind(token);
            if (pin != remembered.constEnd()) {
                deadlines.insert(QString::number(users[i].uid), static_cast<double>(now + pin.value()));
            }
        }
    }
    if (force || deadlines.keys() != publishedPins.keys()) {
        publishedPins = deadlines;
        emit pinsChanged(deadlines);
    }
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
    QStringList personal;
    for (int i = 0; i < userCount; i++) {
        users.insert(userInfos[i].uid, userInfos[i]);
        personal << QString::fromUtf8(userInfos[i].userId) << QString::fromUtf8(userInfos[i].customerId);
    }
    QStringList owners;

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
            personal << QString::fromUtf8(AB_AccountSpec_GetIban(spec)) << QString::fromUtf8(AB_AccountSpec_GetAccountNumber(spec));
            owners << QString::fromUtf8(AB_AccountSpec_GetOwnerName(spec));
        }
        AB_AccountSpec_List_free(specs);
    }
    // whatever the debug log shows, these are masked in it
    LogMasker::addPersonalData(personal);
    LogMasker::addPersonalData(owners, true);
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

void BankingThread::finishCommand(int command, bool success, const QString &message)
{
    log(QStringLiteral("Finished %1 after %2 ms: %3%4").arg(commandName(command)).arg(commandTimer.elapsed())
        .arg(success ? QStringLiteral("succeeded") : QStringLiteral("failed"),
             message.isEmpty() ? QString() : QStringLiteral(" - ") + LogMasker::text(message)));
    emit commandFinished(command, success, message);
}

/*
 * AqBanking's and Gwenhywfar's own log: errors only, unless the debug log is
 * on, which takes notices as well. Their more talkative levels would dump what
 * they send, PIN and TAN included, where nothing masks it.
 */
void BankingThread::applyLogLevels()
{
    const GWEN_LOGGER_LEVEL level = DebugLog::isEnabled() ? GWEN_LoggerLevel_Notice : GWEN_LoggerLevel_Error;
    GWEN_Logger_SetLevel(GWEN_LOGDOMAIN, level);
    GWEN_Logger_SetLevel(AQBANKING_LOGDOMAIN, level);
    GWEN_Logger_SetLevel("aqhbci", level);
}

void BankingThread::logCommand(const Command &command)
{
    if (!DebugLog::isEnabled()) {
        return;
    }
    const QVariantMap &arguments = command.arguments;
    QString details;
    switch (command.type) {
    case AddBankAccess: {
        const QString userId = arguments.value(QStringLiteral("userId")).toString();
        const QString customerId = arguments.value(QStringLiteral("customerId")).toString();
        LogMasker::addPersonalData(QStringList() << userId << customerId);
        details = QStringLiteral("bank code %1, server %2, login of %3 characters, customer ID of %4 characters")
                .arg(arguments.value(QStringLiteral("bankCode")).toString(), arguments.value(QStringLiteral("serverUrl")).toString())
                .arg(userId.length()).arg(customerId.length());
        break;
    }
    case FetchTransactions:
    case FetchHoldings:
        details = QStringLiteral("account %1").arg(arguments.value(QStringLiteral("accountId")).toUInt());
        break;
    case FetchBalances:
        details = arguments.contains(QStringLiteral("bankAccessId"))
                ? QStringLiteral("bank access %1").arg(arguments.value(QStringLiteral("bankAccessId")).toUInt())
                : QStringLiteral("all bank accesses");
        break;
    case RemoveBankAccess:
    case UpdateBankAccess:
    case ChangeTanMethod:
        details = QStringLiteral("bank access %1").arg(arguments.value(QStringLiteral("bankAccessId")).toUInt());
        break;
    default:
        break;
    }
    log(QStringLiteral("Starting %1%2").arg(commandName(command.type), details.isEmpty() ? QString() : QStringLiteral(": ") + details));
}

// What AqBanking knows about a bank access and its accounts, without anything personal.
void BankingThread::logBankAccess(uint32_t uid)
{
    if (!DebugLog::isEnabled() || !banking) {
        return;
    }
    ZB_USER_INFO users[MAX_USERS];
    const int userCount = ZB_ListUsers(banking, users, MAX_USERS);
    for (int i = 0; i < userCount; i++) {
        const ZB_USER_INFO &user = users[i];
        if (user.uid != uid) {
            continue;
        }
        log(QStringLiteral("Bank access %1: bank code %2, server %3, FinTS %4, login %5, customer ID %6, system ID %7, "
                           "TAN method %8, TAN input mechanism %9, TAN medium name %10, BPD version %11, UPD version %12")
            .arg(uid).arg(QString::fromUtf8(user.bankCode), QString::fromUtf8(user.server)).arg(user.hbciVersion)
            .arg(masked(user.userId), masked(user.customerId), yesNo(user.hasSystemId)).arg(user.tanMethod).arg(user.tanMechanism)
            .arg(yesNo(user.hasTanMedium)).arg(user.bpdVersion).arg(user.updVersion));
    }
    AB_ACCOUNT_SPEC_LIST *specs = nullptr;
    if (AB_Banking_GetAccountSpecList(banking, &specs) >= 0 && specs) {
        for (AB_ACCOUNT_SPEC *spec = AB_AccountSpec_List_First(specs); spec; spec = AB_AccountSpec_List_Next(spec)) {
            uint32_t userUid = 0;
            if (ZB_GetAccountUser(banking, AB_AccountSpec_GetUniqueId(spec), &userUid) >= 0 && userUid == uid) {
                logAccount(AB_AccountSpec_GetUniqueId(spec));
            }
        }
        AB_AccountSpec_List_free(specs);
    }
}

void BankingThread::logAccount(uint32_t accountId)
{
    if (!DebugLog::isEnabled() || !banking) {
        return;
    }
    AB_ACCOUNT_SPEC *spec = nullptr;
    if (AB_Banking_GetAccountSpecByUniqueId(banking, accountId, &spec) < 0 || !spec) {
        log(QStringLiteral("Account %1: unknown").arg(accountId));
        return;
    }
    uint32_t userUid = 0;
    ZB_GetAccountUser(banking, accountId, &userUid);
    const int type = AB_AccountSpec_GetType(spec);
    log(QStringLiteral("Account %1 of bank access %2: type %3 (%4), currency %5, IBAN %6, BIC %7, account number %8, "
                       "sub-account %9, owner %10, name %11")
        .arg(accountId).arg(userUid).arg(type).arg(QLatin1String(AB_AccountType_toChar(static_cast<AB_ACCOUNT_TYPE>(type))),
                                                    QString::fromUtf8(AB_AccountSpec_GetCurrency(spec)),
                                                    masked(AB_AccountSpec_GetIban(spec)),
                                                    QString::fromUtf8(AB_AccountSpec_GetBic(spec)),
                                                    masked(AB_AccountSpec_GetAccountNumber(spec)),
                                                    masked(AB_AccountSpec_GetSubAccountNumber(spec)),
                                                    masked(AB_AccountSpec_GetOwnerName(spec)),
                                                    masked(AB_AccountSpec_GetAccountName(spec))));
    AB_AccountSpec_free(spec);
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
