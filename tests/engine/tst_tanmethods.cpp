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

/*
 * The TAN methods as the shim lists them, from made-up bank parameters (BPD),
 * and the TAN medium it stores for a user; and the jobs for transactions it
 * finds for an account, from the same parameters and made-up account
 * parameters (UPD), and the CAMT preference it sets. The parameters are written
 * into a user the way AqBanking keeps those it got from a bank, and read back
 * through the encrypted store.
 */

#include "Check.h"
#include "AqHbciShim.h"

#include <QFile>

#include <aqbanking/banking.h>
#include <gwenhywfar/db.h>
#include <gwenhywfar/msgengine.h>
#include <gwenhywfar/pathmanager.h>

extern "C" {
#include <aqbanking/banking_be.h>
#include <aqbanking/backendsupport/provider_be.h>
#include "aqhbci/banking/account.h"
#include "aqhbci/banking/user.h"
#include "aqhbci/banking/user_l.h"
#include "aqhbci/msglayer/bpd_l.h"

// From aqhbci's joblayer/job_l.h and ajobs/jobgettrans_camt_l.h, which C++
// cannot include: result_l.h, which they pull in, names a parameter "or".
typedef struct AH_JOB AH_JOB;
AH_JOB *AH_Job_GetTransactionsCAMT_new(AB_PROVIDER *pro, AB_USER *u, AB_ACCOUNT *account);
GWEN_DB_NODE *AH_Job_GetArguments(const AH_JOB *j);
void AH_Job_free(AH_JOB *j);
}

namespace {

const char *const BACKEND = "aqhbci";

// aqhbci looks for its message definitions (hbci.xml) where the package puts
// them, and defines that path anew each time the provider starts, dropping it
// again when it ends. Defined beforehand, the provider adds its own after the
// tests' (complaining that the path exists), so the installed copy is found.
QByteArray definitions;

void useInstalledDefinitions()
{
    GWEN_PathManager_DefinePath("aqhbci", "xmldatadir");
    GWEN_PathManager_InsertPath("aqhbci", "aqhbci", "xmldatadir", definitions.constData());
}

// One tanMethod group of HITANS, as AqBanking's message engine stores it: numbers
// as numbers, the alphanumeric fields as text.
void addMethod(GWEN_DB_NODE *version, int function, const char *name, const char *mediumField, const char *needed)
{
    GWEN_DB_NODE *method = GWEN_DB_GetGroup(version, GWEN_PATH_FLAGS_CREATE_GROUP, "tanMethod");
    GWEN_DB_SetIntValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, "function", function);
    GWEN_DB_SetIntValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, "process", 2);
    GWEN_DB_SetCharValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, "methodId", name);
    GWEN_DB_SetCharValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, "methodName", name);
    if (mediumField) {
        GWEN_DB_SetCharValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, mediumField, needed);
    }
}

bool giveBankParameters(AB_BANKING *banking, uint32_t uid)
{
    useInstalledDefinitions();
    AB_PROVIDER *provider = AB_Banking_BeginUseProvider(banking, BACKEND);
    AB_USER *user = nullptr;
    int rv = AB_Provider_GetUser(provider, uid, 1, 0, &user);
    if (rv >= 0) {
        AH_BPD *bpd = AH_Bpd_new();
        GWEN_DB_NODE *jobs = AH_Bpd_GetBpdJobs(bpd, AH_User_GetHbciVersion(user));
        // HKTAN version 6 calls the field needTanMediumId, a digit
        GWEN_DB_NODE *version6 = GWEN_DB_GetGroup(jobs, GWEN_DB_FLAGS_DEFAULT, "HITANS/6");
        addMethod(version6, 920, "BestSign", "needTanMediumId", "2");
        addMethod(version6, 942, "mobileTAN", "needTanMediumId", "1");
        addMethod(version6, 972, "chipTAN", "needTanMediumId", "0");
        addMethod(version6, 982, "photoTAN", nullptr, nullptr);
        // Version 2 calls it tanMediumIdNeeded, a number; a method only described
        // there keeps its version 2 description.
        GWEN_DB_NODE *version2 = GWEN_DB_GetGroup(jobs, GWEN_DB_FLAGS_DEFAULT, "HITANS/2");
        GWEN_DB_NODE *method = GWEN_DB_GetGroup(version2, GWEN_PATH_FLAGS_CREATE_GROUP, "tanMethod");
        GWEN_DB_SetIntValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, "function", 900);
        GWEN_DB_SetIntValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, "process", 2);
        GWEN_DB_SetCharValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, "methodName", "iTAN");
        GWEN_DB_SetIntValue(method, GWEN_DB_FLAGS_OVERWRITE_VARS, "tanMediumIdNeeded", 2);
        // Transactions as CAMT (HKCAZ) and as MT940 (HKKAZ). Whether an account
        // has MT940 is up to its UPD, CAMT only needs a format the installed
        // profiles know.
        GWEN_DB_NODE *camt = GWEN_DB_GetGroup(jobs, GWEN_DB_FLAGS_DEFAULT, "HICAZS/1");
        GWEN_DB_SetIntValue(camt, GWEN_DB_FLAGS_OVERWRITE_VARS, "StoreDays", 90);
        GWEN_DB_SetCharValue(camt, GWEN_DB_FLAGS_OVERWRITE_VARS, "supportedFormat",
                             "urn:iso:std:iso:20022:tech:xsd:camt.052.001.08");
        GWEN_DB_NODE *mt940 = GWEN_DB_GetGroup(jobs, GWEN_DB_FLAGS_DEFAULT, "HIKAZS/7");
        GWEN_DB_SetIntValue(mt940, GWEN_DB_FLAGS_OVERWRITE_VARS, "storeDays", 90);
        AH_User_SetBpd(user, bpd);
        AH_Bpd_free(bpd);
        AH_User_AddTanMethod(user, 920);
        rv = AB_Provider_EndExclUseUser(provider, user, 0);
        AB_User_free(user);
    }
    AB_Banking_EndUseProvider(banking, provider);
    return rv >= 0;
}

QString storedTanMedium(AB_BANKING *banking, uint32_t uid)
{
    useInstalledDefinitions();
    AB_PROVIDER *provider = AB_Banking_BeginUseProvider(banking, BACKEND);
    AB_USER *user = nullptr;
    QString result = QStringLiteral("(no user)");
    if (AB_Provider_GetUser(provider, uid, 1, 1, &user) >= 0) {
        const char *medium = AH_User_GetTanMediumId(user);
        result = medium ? QString::fromUtf8(medium) : QStringLiteral("(none)");
        AB_User_free(user);
    }
    AB_Banking_EndUseProvider(banking, provider);
    return result;
}

// An account of the user, as the account list brings it (an IBAN, a FinTS
// account with suffix), with the jobs its UPD lists for it.
uint32_t addAccount(AB_BANKING *banking, uint32_t uid, const char *iban, const QList<QByteArray> &updJobs)
{
    useInstalledDefinitions();
    AB_PROVIDER *provider = AB_Banking_BeginUseProvider(banking, BACKEND);
    AB_ACCOUNT *account = AB_Provider_CreateAccountObject(provider);
    AB_Account_SetUserId(account, uid);
    AB_Account_SetBankCode(account, "12345678");
    AB_Account_SetAccountNumber(account, *iban ? QByteArray(iban).right(10).constData() : "1234567890");
    AB_Account_SetIban(account, iban);
    AB_Account_SetBic(account, "TESTDEFFXXX");
    AB_Account_SetAccountType(account, AB_AccountType_Bank);
    AH_Account_AddFlags(account, AH_BANK_FLAGS_KTV2);
    int rv = AB_Provider_AddAccount(provider, account, 1);
    uint32_t accountUid = rv >= 0 ? AB_Account_GetUniqueId(account) : 0;
    AB_Account_free(account);

    AB_USER *user = nullptr;
    if (accountUid && AB_Provider_GetUser(provider, uid, 1, 0, &user) >= 0) {
        // where AqBanking files the UPD it got for an account
        const QByteArray group = QByteArray("uaid-") + QByteArray::number(accountUid, 16).rightJustified(8, '0');
        GWEN_DB_NODE *upd = GWEN_DB_GetGroup(AH_User_GetUpd(user), GWEN_DB_FLAGS_OVERWRITE_GROUPS, group.constData());
        for (const QByteArray &job : updJobs) {
            GWEN_DB_NODE *updJob = GWEN_DB_GetGroup(upd, GWEN_PATH_FLAGS_CREATE_GROUP, "updjob");
            GWEN_DB_SetCharValue(updJob, GWEN_DB_FLAGS_OVERWRITE_VARS, "job", job.constData());
        }
        if (AB_Provider_EndExclUseUser(provider, user, 0) < 0) {
            accountUid = 0;
        }
        AB_User_free(user);
    } else {
        accountUid = 0;
    }
    AB_Banking_EndUseProvider(banking, provider);
    return accountUid;
}

// What AqBanking's CAMT job (HKCAZ) would identify the account by: the KTI's
// fields it has arguments for, as "field=value", in the KTI's order.
QStringList camtAccountArguments(AB_BANKING *banking, uint32_t uid, uint32_t accountUid)
{
    useInstalledDefinitions();
    AB_PROVIDER *provider = AB_Banking_BeginUseProvider(banking, BACKEND);
    AB_USER *user = nullptr;
    AB_ACCOUNT *account = nullptr;
    QStringList result;
    if (AB_Provider_GetUser(provider, uid, 1, 1, &user) >= 0
            && AB_Provider_GetAccount(provider, accountUid, 1, 1, &account) >= 0) {
        AH_JOB *job = AH_Job_GetTransactionsCAMT_new(provider, user, account);
        if (job) {
            GWEN_DB_NODE *arguments = AH_Job_GetArguments(job);
            for (const char *field : {"iban", "bic", "accountId", "accountSubId", "country", "bankCode"}) {
                if (GWEN_DB_VariableExists(arguments, field)) {
                    const QString value = GWEN_DB_GetValueTypeByPath(arguments, field, 0) == GWEN_DB_NodeType_ValueInt
                                              ? QString::number(GWEN_DB_GetIntValue(arguments, field, 0, 0))
                                              : QString::fromUtf8(GWEN_DB_GetCharValue(arguments, field, 0, ""));
                    result.append(QStringLiteral("%1=%2").arg(QLatin1String(field), value));
                }
            }
            AH_Job_free(job);
        } else {
            result.append(QStringLiteral("(no job)"));
        }
    }
    AB_Account_free(account);
    if (user) {
        AB_User_free(user);
    }
    AB_Banking_EndUseProvider(banking, provider);
    return result;
}

// The signature's end (HNSHA) with the PIN, as AqBanking's message engine
// writes it into a PIN/TAN message (msgcrypt_pintan_sign.c), or the error.
QByteArray signatureWithPin(AB_BANKING *banking, uint32_t uid, const QByteArray &pin)
{
    useInstalledDefinitions();
    AB_PROVIDER *provider = AB_Banking_BeginUseProvider(banking, BACKEND);
    AB_USER *user = nullptr;
    QByteArray result("(no user)");
    if (AB_Provider_GetUser(provider, uid, 1, 1, &user) >= 0) {
        GWEN_MSGENGINE *engine = AH_User_GetMsgEngine(user);
        GWEN_MsgEngine_SetProtocolVersion(engine, 300);
        GWEN_MsgEngine_SetMode(engine, "pintan");
        GWEN_XMLNODE *segment = GWEN_MsgEngine_FindNodeByPropertyStrictProto(engine, "SEG", "id", 0, "SigTail");
        GWEN_DB_NODE *values = GWEN_DB_Group_new("values");
        GWEN_DB_SetIntValue(values, GWEN_DB_FLAGS_DEFAULT, "head/seq", 4);
        GWEN_DB_SetCharValue(values, GWEN_DB_FLAGS_DEFAULT, "ctrlref", "1234567");
        GWEN_DB_SetCharValue(values, GWEN_DB_FLAGS_DEFAULT, "pin", pin.constData());
        GWEN_BUFFER *buffer = GWEN_Buffer_new(0, 256, 0, 1);
        const int rv = segment ? GWEN_MsgEngine_CreateMessageFromNode(engine, segment, buffer, values) : -1;
        result = rv == 0 ? QByteArray(GWEN_Buffer_GetStart(buffer), static_cast<int>(GWEN_Buffer_GetUsedBytes(buffer)))
                         : QByteArray("error ") + QByteArray::number(rv);
        GWEN_Buffer_free(buffer);
        GWEN_DB_Group_free(values);
        AB_User_free(user);
    }
    AB_Banking_EndUseProvider(banking, provider);
    return result;
}

QVariant prefersCamt(AB_BANKING *banking, uint32_t accountUid)
{
    useInstalledDefinitions();
    AB_PROVIDER *provider = AB_Banking_BeginUseProvider(banking, BACKEND);
    AB_ACCOUNT *account = nullptr;
    QVariant result;
    if (AB_Provider_GetAccount(provider, accountUid, 1, 1, &account) >= 0) {
        result = (AH_Account_GetFlags(account) & AH_BANK_FLAGS_PREFER_CAMT_DOWNLOAD) != 0;
        AB_Account_free(account);
    }
    AB_Banking_EndUseProvider(banking, provider);
    return result;
}

int transactionJobs(AB_BANKING *banking, uint32_t accountUid)
{
    useInstalledDefinitions();
    return ZB_TransactionJobs(banking, accountUid);
}

int setCamtDownload(AB_BANKING *banking, uint32_t accountUid, int prefer)
{
    useInstalledDefinitions();
    return ZB_SetCamtDownload(banking, accountUid, prefer);
}

// The jobs the fetch of transactions chooses between, and the flag that chooses.
void transactionJobTests(AB_BANKING *banking, uint32_t uid)
{
    Check::section("Jobs for transactions");
    const uint32_t both = addAccount(banking, uid, "DE02120300000000202051", {"HKSAL", "HKCAZ", "HKKAZ"});
    const uint32_t camtOnly = addAccount(banking, uid, "DE02500105170137075030", {"HKSAL", "HKCAZ"});
    const uint32_t mt940Only = addAccount(banking, uid, "DE89370400440532013000", {"HKSAL", "HKKAZ"});
    // A credit card at Postbank: its own jobs for balance and transactions
    const uint32_t creditCard = addAccount(banking, uid, "DE75512108001245126199", {"DKKKS", "DKKKU"});
    // No jobs at all in the UPD
    const uint32_t unlisted = addAccount(banking, uid, "DE12500105170648489890", {});
    // An account the bank gave no IBAN for
    const uint32_t noIban = addAccount(banking, uid, "", {"HKSAL", "HKCAZ"});
    if (!Check::that("Six accounts are added", both && camtOnly && mt940Only && creditCard && unlisted && noIban)) {
        return;
    }
    Check::equal("The shim counts them", ZB_CountAccounts(banking, uid), 6);
    Check::equal("and none for an unknown bank access", ZB_CountAccounts(banking, 0x7fffffff), 0);
    const int camt = ZB_TRANSACTION_JOB_CAMT;
    const int other = ZB_TRANSACTION_JOB_OTHER;
    const int listed = ZB_TRANSACTION_JOB_CAMT_LISTED;
    Check::equal("CAMT and MT940 where the UPD lists both", transactionJobs(banking, both), camt | other | listed);
    Check::equal("CAMT only where it lists HKCAZ only", transactionJobs(banking, camtOnly), camt | listed);
    Check::equal("MT940, and CAMT by the BPD, where it lists HKKAZ only", transactionJobs(banking, mt940Only), camt | other);
    Check::equal("The credit card job, and CAMT by the BPD, for a credit card", transactionJobs(banking, creditCard), camt | other);
    Check::equal("CAMT by the BPD alone where the UPD lists nothing", transactionJobs(banking, unlisted), camt);
    Check::that("An unknown account is an error", transactionJobs(banking, 0x7fffffff) < 0);

    Check::equal("CAMT first where the UPD lists both", ZB_PreferCamtFor(camt | other | listed), 1);
    Check::equal("CAMT first where it lists HKCAZ only", ZB_PreferCamtFor(camt | listed), 1);
    Check::equal("CAMT first where it lists nothing", ZB_PreferCamtFor(camt), 1);
    Check::equal("MT940 where it lists HKKAZ only", ZB_PreferCamtFor(camt | other), 0);
    Check::equal("CAMT first if the jobs are unknown", ZB_PreferCamtFor(-1), 1);

    // AqBanking's CAMT job, as patched (aqbanking-6.9.2-camt-account.patch)
    Check::equal("The CAMT request names the account by IBAN and BIC only", camtAccountArguments(banking, uid, both),
                 QStringList({QStringLiteral("iban=DE02120300000000202051"), QStringLiteral("bic=TESTDEFFXXX")}));
    Check::equal("Without an IBAN, by its account number and bank code",
                 camtAccountArguments(banking, uid, noIban),
                 QStringList({QStringLiteral("bic=TESTDEFFXXX"), QStringLiteral("accountId=1234567890"),
                              QStringLiteral("country=280"), QStringLiteral("bankCode=12345678")}));

    Check::equal("No CAMT preference at first", prefersCamt(banking, both), false);
    int rv = setCamtDownload(banking, both, 1);
    Check::that("CAMT is preferred", rv >= 0, QString::number(rv));
    Check::equal("The preference is stored", prefersCamt(banking, both), true);
    rv = setCamtDownload(banking, both, 1);
    Check::that("Preferring it again is fine", rv >= 0 && prefersCamt(banking, both).toBool(), QString::number(rv));
    rv = setCamtDownload(banking, both, 0);
    Check::that("The preference is cleared", rv >= 0, QString::number(rv));
    Check::equal("It reads back cleared", prefersCamt(banking, both), false);
    Check::equal("The other account is left alone", prefersCamt(banking, camtOnly), false);
    Check::equal("The jobs offered stay the same", transactionJobs(banking, both), camt | other | listed);
}

const ZB_TAN_METHOD *find(const ZB_TAN_METHOD *methods, int count, int id)
{
    for (int i = 0; i < count; i++) {
        if (methods[i].id == id) {
            return &methods[i];
        }
    }
    return nullptr;
}

QVariant mediumNeeded(const ZB_TAN_METHOD *methods, int count, int id)
{
    const ZB_TAN_METHOD *method = find(methods, count, id);
    return method ? QVariant(method->tanMediumNeeded) : QVariant();
}

}

// Runs on the AqBanking the statement tests started.
void tanMethodTests(AB_BANKING *banking, const QString &installedData)
{
    Check::section("TAN methods");
    definitions = QFile::encodeName(installedData + QStringLiteral("/aqbanking/data/aqbanking/backends/aqhbci"));

    uint32_t uid = 0;
    useInstalledDefinitions();
    int rv = ZB_AddPinTanUser(banking, "12345678", "tester", "", "https://bank.example/fints", "Test bank", &uid);
    if (!Check::that("A bank access is added", rv >= 0 && uid != 0, QString::number(rv))
            || !Check::that("The bank's parameters are stored", giveBankParameters(banking, uid))) {
        return;
    }

    ZB_TAN_METHOD methods[16];
    useInstalledDefinitions();
    const int count = ZB_ListTanMethods(banking, uid, methods, 16);
    Check::equal("Every method is listed once", count, 5);
    Check::equal("BestSign needs the name of the TAN medium", mediumNeeded(methods, count, 6920), 2);
    Check::equal("An optional name is reported as optional", mediumNeeded(methods, count, 6942), 1);
    Check::equal("A name not allowed is reported as such", mediumNeeded(methods, count, 6972), 0);
    Check::equal("No such field means no name", mediumNeeded(methods, count, 6982), 0);
    Check::equal("HKTAN version 2's field is read as well", mediumNeeded(methods, count, 2900), 2);
    const ZB_TAN_METHOD *bestSign = find(methods, count, 6920);
    Check::that("Only the method the bank allows this user is allowed",
                bestSign && bestSign->allowed && !find(methods, count, 6942)->allowed);

    Check::equal("No TAN medium at first", storedTanMedium(banking, uid), QStringLiteral("(none)"));
    useInstalledDefinitions();
    rv = ZB_SetTanMediumId(banking, uid, "Mein Smartphone");
    Check::that("The TAN medium is stored", rv >= 0, QString::number(rv));
    Check::equal("It reads back as entered", storedTanMedium(banking, uid), QStringLiteral("Mein Smartphone"));
    useInstalledDefinitions();
    ZB_SetTanMediumId(banking, uid, "");
    Check::equal("An empty name removes it", storedTanMedium(banking, uid), QStringLiteral("(none)"));

    transactionJobTests(banking, uid);

    // The PIN in the message, as GwenGui hands it over (ISO 8859-1): FinTS's own
    // syntax characters are escaped with "?", the others go as they are.
    Check::section("PIN in the message");
    const QString pin = QStringLiteral("äöüÄÖÜß!$%&/()=?+#,.-:");
    Check::equal("special characters are escaped by AqBanking", signatureWithPin(banking, uid, pin.toLatin1()),
                 QByteArray("HNSHA:4:2+1234567++") + QStringLiteral("äöüÄÖÜß!$%&/()=???+#,.-?:'").toLatin1());
    Check::equal("so are the separators", signatureWithPin(banking, uid, "ab+c:d'e"),
                 QByteArray("HNSHA:4:2+1234567++ab?+c?:d?'e'"));
    Check::equal("18 umlauts fit as ISO 8859-1",
                 signatureWithPin(banking, uid, QStringLiteral("ÄÖÜßäöüÄÖÜßäöüÄÖÜß").toLatin1()).left(19),
                 QByteArray("HNSHA:4:2+1234567++"));
    Check::that("but not as UTF-8, which AqBanking cannot encode (35 bytes at most)",
                signatureWithPin(banking, uid, QStringLiteral("ÄÖÜßäöüÄÖÜßäöüÄÖÜß").toUtf8()).startsWith("error"));

    useInstalledDefinitions();
    ZB_DeleteUser(banking, uid);
}
