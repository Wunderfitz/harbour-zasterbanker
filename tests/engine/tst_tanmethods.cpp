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
 * and the TAN medium it stores for a user. The parameters are written into a
 * user the way AqBanking keeps those it got from a bank, and read back through
 * the encrypted store.
 */

#include "Check.h"
#include "AqHbciShim.h"

#include <QFile>

#include <aqbanking/banking.h>
#include <gwenhywfar/db.h>
#include <gwenhywfar/pathmanager.h>

extern "C" {
#include <aqbanking/banking_be.h>
#include <aqbanking/backendsupport/provider_be.h>
#include "aqhbci/banking/user.h"
#include "aqhbci/banking/user_l.h"
#include "aqhbci/msglayer/bpd_l.h"
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

    useInstalledDefinitions();
    ZB_DeleteUser(banking, uid);
}
