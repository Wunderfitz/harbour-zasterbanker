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

#ifndef AQHBCISHIM_H
#define AQHBCISHIM_H

/*
 * The few things Zaster Banker needs from AqBanking's FinTS backend (aqhbci)
 * that AqBanking's public API does not offer: setting up a PIN/TAN user, the
 * initial handshake, and listing and choosing TAN methods.
 *
 * AqBanking's own aqhbci-tool4 does the same through its "control" commands;
 * the functions here follow those commands (adduser, getsysid, getaccounts,
 * listitanmodes, setitanmode, deluser) line by line, but report structured
 * results instead of printing them. They use aqhbci's internal headers, which
 * is possible because AqBanking is linked statically - and which means this
 * file has to be checked against the control commands whenever AqBanking is
 * upgraded.
 *
 * All functions return 0 or a positive count on success, a negative
 * Gwenhywfar error code otherwise. Only the banking thread calls them.
 */

#include <aqbanking/banking.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZB_NAME_LENGTH 128

typedef struct {
    /* As aqhbci counts it: job version * 1000 + security function, e.g. 6902 */
    int id;
    char name[ZB_NAME_LENGTH];
    /* Offered by the bank to this user (and not just known to the bank) */
    int allowed;
    int selected;
    /* The bank's "TAN medium name required": 0 not allowed, 1 optional, 2 required */
    int tanMediumNeeded;
} ZB_TAN_METHOD;

typedef struct {
    uint32_t uid;
    char bankCode[ZB_NAME_LENGTH];
    char userId[ZB_NAME_LENGTH];
    char userName[ZB_NAME_LENGTH];
} ZB_USER_INFO;

int ZB_AddPinTanUser(AB_BANKING *ab, const char *bankCode, const char *userId, const char *customerId,
                     const char *serverUrl, const char *userName, uint32_t *pUid);

/* Checks the server's certificate and fetches the system ID: the handshake. */
int ZB_GetSysId(AB_BANKING *ab, uint32_t uid);

/* Fetches the accounts the bank lists for the user and adds them to AqBanking. */
int ZB_GetAccounts(AB_BANKING *ab, uint32_t uid);

int ZB_ListTanMethods(AB_BANKING *ab, uint32_t uid, ZB_TAN_METHOD *methods, int maxMethods);
int ZB_SelectTanMethod(AB_BANKING *ab, uint32_t uid, int methodId);
int ZB_SetTanMediumId(AB_BANKING *ab, uint32_t uid, const char *tanMediumId);

int ZB_ListUsers(AB_BANKING *ab, ZB_USER_INFO *users, int maxUsers);
int ZB_GetAccountUser(AB_BANKING *ab, uint32_t accountUid, uint32_t *pUserUid);

/*
 * Sets (prefer != 0) or clears AqBanking's account flag to fetch transactions as
 * CAMT where the bank offers that; with it, AqBanking falls back to MT940 only if
 * the bank does not offer CAMT for the account. Without it, AqBanking only ever
 * tries MT940, and gives up silently if the bank does not list that job for the
 * account. Rewrites the store only if the flag changes.
 */
int ZB_SetCamtDownload(AB_BANKING *ab, uint32_t accountUid, int prefer);

/*
 * Which jobs for transactions the bank's parameter data offer for the account,
 * as a combination of the flags below: CAMT (HKCAZ), and the job AqBanking uses
 * without the CAMT flag (MT940, HKKAZ, or a credit card's own). Contacts no one.
 */
#define ZB_TRANSACTION_JOB_CAMT  1
#define ZB_TRANSACTION_JOB_OTHER 2
int ZB_TransactionJobs(AB_BANKING *ab, uint32_t accountUid);

/*
 * Asks the bank for the SEPA details of the account's user if the account has no
 * BIC yet: fetching the account list does not bring one, and banks reject requests
 * for transactions without it (9160). Returns 1 if it asked, 0 if there was no need.
 */
int ZB_EnsureAccountBic(AB_BANKING *ab, uint32_t accountUid);

/* Deletes the user together with all of its accounts. */
int ZB_DeleteUser(AB_BANKING *ab, uint32_t uid);

#ifdef __cplusplus
}
#endif

#endif /* AQHBCISHIM_H */
