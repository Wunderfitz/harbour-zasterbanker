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

#include "AqHbciShim.h"

#include <aqbanking/banking_be.h>
#include <aqbanking/backendsupport/account.h>
#include <aqbanking/backendsupport/provider.h>
#include <aqbanking/backendsupport/user.h>
#include <aqbanking/types/imexporter_context.h>

#include "aqhbci/banking/account.h"
#include "aqhbci/banking/provider_online.h"
#include "aqhbci/banking/user.h"
#include "aqhbci/tan/tanmethod.h"

#include <gwenhywfar/error.h>
#include <gwenhywfar/url.h>

#include <string.h>

#define ZB_AQHBCI_BACKEND "aqhbci"
#define ZB_HBCI_VERSION_300 300

static void copyString(char *target, const char *source)
{
    if (source) {
        strncpy(target, source, ZB_NAME_LENGTH - 1);
        target[ZB_NAME_LENGTH - 1] = 0;
    } else {
        target[0] = 0;
    }
}

int ZB_AddPinTanUser(AB_BANKING *ab, const char *bankCode, const char *userId, const char *customerId,
                     const char *serverUrl, const char *userName, uint32_t *pUid)
{
    AB_PROVIDER *pro;
    AB_USER *user;
    GWEN_URL *url;
    int rv;

    if (!(bankCode && *bankCode && userId && *userId && serverUrl && *serverUrl)) {
        return GWEN_ERROR_INVALID;
    }
    url = GWEN_Url_fromString(serverUrl);
    if (!url) {
        return GWEN_ERROR_BAD_ADDRESS;
    }

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        GWEN_Url_free(url);
        return GWEN_ERROR_NOT_FOUND;
    }

    /* As in AH_Control_AddUser() */
    user = AB_Provider_CreateUserObject(pro);
    AB_User_SetUserName(user, userName ? userName : bankCode);
    AB_User_SetCountry(user, "de");
    AB_User_SetBankCode(user, bankCode);
    AB_User_SetUserId(user, userId);
    AB_User_SetCustomerId(user, (customerId && *customerId) ? customerId : userId);
    AH_User_SetTokenType(user, "pintan");
    AH_User_SetTokenName(user, NULL);
    AH_User_SetTokenContextId(user, 1);
    AH_User_SetHbciVersion(user, ZB_HBCI_VERSION_300);
    AH_User_SetServerUrl(user, url);
    AH_User_SetCryptMode(user, AH_CryptMode_Pintan);
    GWEN_Url_free(url);

    rv = AB_Provider_AddUser(pro, user);
    if (rv >= 0) {
        *pUid = AB_User_GetUniqueId(user);
        rv = 0;
    }
    AB_User_free(user);
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}

int ZB_GetSysId(AB_BANKING *ab, uint32_t uid)
{
    AB_PROVIDER *pro;
    AB_USER *user = NULL;
    AB_IMEXPORTER_CONTEXT *ctx;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }
    rv = AB_Provider_GetUser(pro, uid, 1, 1, &user);
    if (rv >= 0) {
        /* As in AH_Control_GetSysId(): certificate first, then the system ID */
        rv = AH_Provider_GetCert(pro, user, 1, 0, 1);
        if (rv >= 0) {
            ctx = AB_ImExporterContext_new();
            rv = AH_Provider_GetSysId(pro, user, ctx, 1, 0, 1);
            AB_ImExporterContext_free(ctx);
        }
        AB_User_free(user);
    }
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}

int ZB_GetAccounts(AB_BANKING *ab, uint32_t uid)
{
    AB_PROVIDER *pro;
    AB_USER *user = NULL;
    AB_IMEXPORTER_CONTEXT *ctx;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }
    rv = AB_Provider_GetUser(pro, uid, 1, 1, &user);
    if (rv >= 0) {
        ctx = AB_ImExporterContext_new();
        rv = AH_Provider_GetAccounts(pro, user, ctx, 1, 0, 1);
        AB_ImExporterContext_free(ctx);
        AB_User_free(user);
    }
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}

int ZB_ListTanMethods(AB_BANKING *ab, uint32_t uid, ZB_TAN_METHOD *methods, int maxMethods)
{
    AB_PROVIDER *pro;
    AB_USER *user = NULL;
    const AH_TAN_METHOD_LIST *descriptions;
    const AH_TAN_METHOD *method;
    int count = 0;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }
    rv = AB_Provider_GetUser(pro, uid, 1, 1, &user);
    if (rv < 0) {
        AB_Banking_EndUseProvider(ab, pro);
        return rv;
    }

    /* As in AH_Control_ListItanModes(). A bank describes every method once per
     * HKTAN version it supports; only the newest description of each counts. */
    descriptions = AH_User_GetTanMethodDescriptions(user);
    method = descriptions ? AH_TanMethod_List_First(descriptions) : NULL;
    while (method && count < maxMethods) {
        const int function = AH_TanMethod_GetFunction(method);
        const int version = AH_TanMethod_GetGvVersion(method);
        const int process = AH_TanMethod_GetProcess(method);
        const char *name = AH_TanMethod_GetMethodName(method);
        int existing = -1;
        int i;

        for (i = 0; i < count; i++) {
            if (methods[i].id % 1000 == function) {
                existing = i;
            }
        }
        /* aqhbci handles the two-step processes 1 and 2 only */
        if ((process == 1 || process == 2) && (existing < 0 || methods[existing].id / 1000 < version)) {
            ZB_TAN_METHOD *target = existing < 0 ? &methods[count++] : &methods[existing];
            target->id = version * 1000 + function;
            copyString(target->name, (name && *name) ? name : AH_TanMethod_GetMethodId(method));
            target->allowed = AH_User_HasTanMethod(user, function) ? 1 : 0;
            target->selected = (AH_User_GetSelectedTanMethod(user) % 1000 == function) ? 1 : 0;
        }
        method = AH_TanMethod_List_Next(method);
    }

    AB_User_free(user);
    AB_Banking_EndUseProvider(ab, pro);
    return count;
}

int ZB_SelectTanMethod(AB_BANKING *ab, uint32_t uid, int methodId)
{
    AB_PROVIDER *pro;
    AB_USER *user = NULL;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }
    /* As in AH_Control_SetItanMode(): lock, modify, write back on unlock */
    rv = AB_Provider_GetUser(pro, uid, 1, 0, &user);
    if (rv >= 0) {
        if (!AH_User_HasTanMethod(user, methodId % 1000)) {
            AB_Provider_EndExclUseUser(pro, user, 1);
            rv = GWEN_ERROR_INVALID;
        } else {
            AH_User_SetSelectedTanMethod(user, methodId);
            rv = AB_Provider_EndExclUseUser(pro, user, 0);
            if (rv < 0) {
                AB_Provider_EndExclUseUser(pro, user, 1);
            }
        }
        AB_User_free(user);
    }
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}

int ZB_ListUsers(AB_BANKING *ab, ZB_USER_INFO *users, int maxUsers)
{
    AB_PROVIDER *pro;
    AB_USER_LIST *list;
    AB_USER *user;
    int count = 0;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }
    list = AB_User_List_new();
    rv = AB_Provider_ReadUsers(pro, list);
    if (rv >= 0 || rv == GWEN_ERROR_NOT_FOUND) {
        user = AB_User_List_First(list);
        while (user && count < maxUsers) {
            users[count].uid = AB_User_GetUniqueId(user);
            copyString(users[count].bankCode, AB_User_GetBankCode(user));
            copyString(users[count].userId, AB_User_GetUserId(user));
            copyString(users[count].userName, AB_User_GetUserName(user));
            count++;
            user = AB_User_List_Next(user);
        }
        rv = count;
    }
    AB_User_List_free(list);
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}

int ZB_GetAccountUser(AB_BANKING *ab, uint32_t accountUid, uint32_t *pUserUid)
{
    AB_PROVIDER *pro;
    AB_ACCOUNT *account = NULL;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }
    rv = AB_Provider_GetAccount(pro, accountUid, 1, 1, &account);
    if (rv >= 0) {
        *pUserUid = AB_Account_GetUserId(account);
        AB_Account_free(account);
    }
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}

int ZB_PreferCamtDownload(AB_BANKING *ab, uint32_t accountUid)
{
    AB_PROVIDER *pro;
    AB_ACCOUNT *account = NULL;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }
    /* Read first: changing an account rewrites the encrypted store. */
    rv = AB_Provider_GetAccount(pro, accountUid, 1, 1, &account);
    if (rv >= 0 && !(AH_Account_GetFlags(account) & AH_BANK_FLAGS_PREFER_CAMT_DOWNLOAD)) {
        AB_Account_free(account);
        account = NULL;
        rv = AB_Provider_GetAccount(pro, accountUid, 1, 0, &account);
        if (rv >= 0) {
            AH_Account_AddFlags(account, AH_BANK_FLAGS_PREFER_CAMT_DOWNLOAD);
            rv = AB_Provider_EndExclUseAccount(pro, account, 0);
        }
    }
    AB_Account_free(account);
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}

int ZB_EnsureAccountBic(AB_BANKING *ab, uint32_t accountUid)
{
    AB_PROVIDER *pro;
    AB_ACCOUNT *account = NULL;
    AB_USER *user = NULL;
    AB_IMEXPORTER_CONTEXT *ctx;
    const char *bic;
    uint32_t userUid;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }
    rv = AB_Provider_GetAccount(pro, accountUid, 1, 1, &account);
    if (rv >= 0) {
        bic = AB_Account_GetBic(account);
        userUid = AB_Account_GetUserId(account);
        if (bic && *bic) {
            rv = 0;
        } else {
            /* as "aqhbci-tool4 getaccsepa" does */
            rv = AB_Provider_GetUser(pro, userUid, 1, 1, &user);
            if (rv >= 0) {
                ctx = AB_ImExporterContext_new();
                rv = AH_Provider_GetAccountSepaInfo(pro, user, ctx, 1, 0, 1);
                AB_ImExporterContext_free(ctx);
                AB_User_free(user);
                if (rv >= 0) {
                    rv = 1;
                }
            }
        }
        AB_Account_free(account);
    }
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}

int ZB_DeleteUser(AB_BANKING *ab, uint32_t uid)
{
    AB_PROVIDER *pro;
    AB_ACCOUNT_LIST *accounts;
    AB_ACCOUNT *account;
    int rv;

    pro = AB_Banking_BeginUseProvider(ab, ZB_AQHBCI_BACKEND);
    if (!pro) {
        return GWEN_ERROR_NOT_FOUND;
    }

    /* AB_Provider_DeleteUser() refuses while accounts still refer to the user */
    accounts = AB_Account_List_new();
    rv = AB_Provider_ReadAccounts(pro, accounts);
    if (rv >= 0 || rv == GWEN_ERROR_NOT_FOUND) {
        rv = 0;
        account = AB_Account_List_First(accounts);
        while (account && rv >= 0) {
            if (AB_Account_GetUserId(account) == uid) {
                rv = AB_Provider_DeleteAccount(pro, AB_Account_GetUniqueId(account));
            }
            account = AB_Account_List_Next(account);
        }
    }
    AB_Account_List_free(accounts);

    if (rv >= 0) {
        rv = AB_Provider_DeleteUser(pro, uid);
    }
    AB_Banking_EndUseProvider(ab, pro);
    return rv;
}
