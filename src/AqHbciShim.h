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
 * listitanmodes, setitanmode, settanmediumid, delaccount, deluser) line by
 * line, but report structured results instead of printing them. They use aqhbci's internal headers, which
 * is possible because AqBanking is linked statically - and which means this
 * file has to be checked against the control commands whenever AqBanking is
 * upgraded.
 *
 * All functions return 0 or a positive count on success, a negative
 * Gwenhywfar error code otherwise. Only the banking thread calls them.
 */

#include <aqbanking/banking.h>
#include <gwenhywfar/db.h>

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
    /* The rest only for the debug log: */
    char customerId[ZB_NAME_LENGTH];
    char server[ZB_NAME_LENGTH];
    int hbciVersion;
    /* As in ZB_TAN_METHOD, 0 if none was selected */
    int tanMethod;
    int tanMechanism;
    int hasSystemId;
    int hasTanMedium;
    int bpdVersion;
    int updVersion;
} ZB_USER_INFO;

int ZB_AddPinTanUser(AB_BANKING *ab, const char *bankCode, const char *userId, const char *customerId,
                     const char *serverUrl, const char *userName, uint32_t *pUid);

/* Checks the server's certificate and fetches the system ID: the handshake. */
int ZB_GetSysId(AB_BANKING *ab, uint32_t uid);

/*
 * Fetches the accounts the bank lists for the user and adds them to AqBanking,
 * and returns how many accounts the user has then (or an error).
 * - It asks for the bank's parameters (BPD) and the whole account list with the
 *   user's parameters (UPD) afresh, version 0 both, as AH_Provider_GetAccounts()
 *   does: AqBanking otherwise sends the versions it has, and a bank that thinks
 *   them current sends neither. The answer also brings the TAN methods the bank
 *   allows the user now (3920).
 * - If the list holds no accounts, it asks for the accounts' SEPA details
 *   (HKSPA, as "aqhbci-tool4 getaccsepa" and python-fints do), from whose
 *   answer AqBanking adds accounts as well; that request failing is no error.
 * - AqBanking only ever adds and updates accounts. If unlisted is given, it
 *   receives up to maxUnlisted unique IDs of the user's accounts that the
 *   bank's list did not name, and *pUnlistedCount their number: always none if
 *   the list named no account at all, or if the request failed.
 */
int ZB_GetAccounts(AB_BANKING *ab, uint32_t uid, uint32_t *unlisted, int maxUnlisted, int *pUnlistedCount);

/*
 * The accounts the account list segments (HIUPD, "AccountData") among a job's
 * responses name, as the unique IDs of the stored accounts AqBanking matches
 * them with (IBAN first, then bank code, account number and suffix, each with
 * the account type, as jobqueue_account.c does), 0 where none matches. Returns
 * how many such segments there are; more than maxListed is an error.
 */
int ZB_ListedAccounts(AB_BANKING *ab, GWEN_DB_NODE *responses, uint32_t *listed, int maxListed);

/*
 * The user's accounts that are not among the listedCount IDs in listed, up to
 * maxUnlisted of them; returns their number. None if listedCount < 1: an empty
 * list says nothing about which accounts the bank still has.
 */
int ZB_UnlistedAccounts(AB_BANKING *ab, uint32_t uid, const uint32_t *listed, int listedCount,
                        uint32_t *unlisted, int maxUnlisted);

/* Deletes one account, as AqBanking would with the user deleted. */
int ZB_DeleteAccount(AB_BANKING *ab, uint32_t accountUid);

/* How many accounts AqBanking has for the user. */
int ZB_CountAccounts(AB_BANKING *ab, uint32_t uid);

int ZB_ListTanMethods(AB_BANKING *ab, uint32_t uid, ZB_TAN_METHOD *methods, int maxMethods);
/* Also sets the TAN input mechanism for it, see ZB_UpdateTanMechanism(). */
int ZB_SelectTanMethod(AB_BANKING *ab, uint32_t uid, int methodId);

/*
 * The TAN input mechanism (AB_BANKING_TANMETHOD_*) for a TAN method, by its
 * technical ID and its name, or 0 to leave it to AqBanking: chipTAN optical
 * (flicker code) for IDs like "HHD1.4OPT" or names with "optisch" or "optic",
 * chipTAN QR for "Q1S", IDs ending in "QR" and names like "chipTAN-QR".
 * AqBanking looks at the method's ZKA name first, which is "HHD" for every
 * chipTAN variant, and takes that for text input: Sparkassen's chipTAN QR came
 * as a start code to type into the generator. GnuCash and KMyMoney users choose
 * the mechanism by hand ("aqhbci-tool4 settanmechanism").
 */
int ZB_TanMechanismFor(const char *methodId, const char *methodName);

/*
 * Sets the user's TAN input mechanism to ZB_TanMechanismFor() the selected
 * TAN method, rewriting the store only if it changes. Returns the mechanism.
 */
int ZB_UpdateTanMechanism(AB_BANKING *ab, uint32_t uid);
int ZB_SetTanMediumId(AB_BANKING *ab, uint32_t uid, const char *tanMediumId);
/* The name of the TAN medium stored for the user into buffer, "" if there is none. */
int ZB_GetTanMediumId(AB_BANKING *ab, uint32_t uid, char *buffer, int size);

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
 * as a combination of the flags below: CAMT (HKCAZ), which AqBanking creates
 * going by the bank's parameters (BPD) alone; the job it uses without the CAMT
 * flag (MT940, HKKAZ, or a credit card's own, DKKKU, HKKKU), which it only
 * creates if the account's parameters (UPD) list it; and whether the UPD lists
 * HKCAZ as well. Contacts no one.
 */
#define ZB_TRANSACTION_JOB_CAMT        1
#define ZB_TRANSACTION_JOB_OTHER       2
#define ZB_TRANSACTION_JOB_CAMT_LISTED 4
int ZB_TransactionJobs(AB_BANKING *ab, uint32_t accountUid);

/*
 * Whether to ask for CAMT first, given ZB_TransactionJobs()'s answer (or an
 * error): yes, unless the account's UPD lists another job for transactions but
 * not HKCAZ. Then that job is what the bank offers for the account, as AqBanking
 * itself decides for balances and MT940 by the UPD (Postbank's credit cards list
 * DKKKU, and answer a CAMT request with 9010).
 */
int ZB_PreferCamtFor(int transactionJobs);

/*
 * Asks the bank for the SEPA details of the account's user if the account has no
 * BIC yet: fetching the account list does not bring one, and banks reject requests
 * for transactions without it (9160). Returns 1 if it asked, 0 if there was no need.
 */
int ZB_EnsureAccountBic(AB_BANKING *ab, uint32_t accountUid);

/* Deletes the user together with all of its accounts. */
int ZB_DeleteUser(AB_BANKING *ab, uint32_t uid);

/*
 * Has AqBanking call hook with every FinTS message it sends (received 0) or
 * receives (1), complete as it goes over the wire, PIN and TAN included; NULL
 * stops it. AqBanking only has this hook through its no-disk-logs patch, which
 * writes no message to disk. Called on the thread running the dialog.
 */
void ZB_SetMessageLogHook(void (*hook)(int received, const char *data, unsigned int size));

#ifdef __cplusplus
}
#endif

#endif /* AQHBCISHIM_H */
