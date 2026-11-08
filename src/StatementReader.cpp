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

#include "StatementReader.h"

#include <QDate>
#include <QVariantMap>

#include <aqbanking/types/balance.h>
#include <aqbanking/types/security.h>
#include <aqbanking/types/transaction.h>
#include <gwenhywfar/gwendate.h>
#include <gwenhywfar/gwentime.h>

namespace {

QDate toQDate(const GWEN_DATE *date)
{
    return date ? QDate(GWEN_Date_GetYear(date), GWEN_Date_GetMonth(date), GWEN_Date_GetDay(date)) : QDate();
}

}

namespace StatementReader
{

QVariantList balances(const AB_IMEXPORTER_CONTEXT *context, const QDateTime &fetched)
{
    QVariantList result;
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
        entry.insert(QStringLiteral("fetched"), fetched);
        const QDate date = toQDate(AB_Balance_GetDate(chosen));
        if (date.isValid()) {
            entry.insert(QStringLiteral("date"), date);
        }
        result.append(entry);
    }
    return result;
}

// The account's statement lines. Other transaction types in there would be
// AqBanking's own bookkeeping, not movements on the account.
QVariantList transactions(const AB_IMEXPORTER_ACCOUNTINFO *info)
{
    QVariantList result;
    AB_TRANSACTION_LIST *list = AB_ImExporterAccountInfo_GetTransactionList(info);
    if (!list) {
        return result;
    }
    for (const AB_TRANSACTION *transaction = AB_Transaction_List_First(list); transaction;
         transaction = AB_Transaction_List_Next(transaction)) {
        const AB_TRANSACTION_TYPE type = AB_Transaction_GetType(transaction);
        if (type != AB_Transaction_TypeStatement && type != AB_Transaction_TypeNotedStatement) {
            continue;
        }
        const GWEN_DATE *booked = AB_Transaction_GetDate(transaction);
        QVariantMap entry;
        entry.insert(QStringLiteral("date"), toQDate(booked ? booked : AB_Transaction_GetValutaDate(transaction)));
        // A line the parser found no amount in still gets shown, as "-": leaving
        // it out would make the statement look complete when it is not.
        const AB_VALUE *value = AB_Transaction_GetValue(transaction);
        entry.insert(QStringLiteral("hasAmount"), value != nullptr);
        entry.insert(QStringLiteral("amount"), value ? AB_Value_GetValueAsDouble(value) : 0.0);
        entry.insert(QStringLiteral("currency"), value ? QString::fromUtf8(AB_Value_GetCurrency(value)) : QString());
        entry.insert(QStringLiteral("name"), QString::fromUtf8(AB_Transaction_GetRemoteName(transaction)));
        entry.insert(QStringLiteral("iban"), QString::fromUtf8(AB_Transaction_GetRemoteIban(transaction)));
        entry.insert(QStringLiteral("purpose"), QString::fromUtf8(AB_Transaction_GetPurpose(transaction)));
        entry.insert(QStringLiteral("kind"), QString::fromUtf8(AB_Transaction_GetTransactionText(transaction)));
        entry.insert(QStringLiteral("pending"), type == AB_Transaction_TypeNotedStatement);
        result.append(entry);
    }
    return result;
}

// What a depot holds. Only one depot is asked for at a time, so everything in
// the context is this one's. The value is the bank's price times the units; a
// position with either missing (bonds, for one: the parser reads neither their
// nominal value nor their price in percent) has no value rather than a wrong one.
QVariantList holdings(const AB_IMEXPORTER_CONTEXT *context)
{
    QVariantList result;
    for (const AB_SECURITY *security = AB_ImExporterContext_GetFirstSecurity(context); security;
         security = AB_Security_List_Next(security)) {
        const AB_VALUE *units = AB_Security_GetUnits(security);
        const AB_VALUE *price = AB_Security_GetUnitPriceValue(security);
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), QString::fromUtf8(AB_Security_GetName(security)));
        entry.insert(QStringLiteral("idKind"), QString::fromUtf8(AB_Security_GetNameSpace(security)));
        entry.insert(QStringLiteral("identifier"), QString::fromUtf8(AB_Security_GetUniqueId(security)));
        entry.insert(QStringLiteral("hasUnits"), units != nullptr);
        entry.insert(QStringLiteral("units"), units ? AB_Value_GetValueAsDouble(units) : 0.0);
        entry.insert(QStringLiteral("hasPrice"), price != nullptr);
        entry.insert(QStringLiteral("price"), price ? AB_Value_GetValueAsDouble(price) : 0.0);
        entry.insert(QStringLiteral("currency"), price ? QString::fromUtf8(AB_Value_GetCurrency(price)) : QString());
        const GWEN_TIME *priceDate = AB_Security_GetUnitPriceDate(security);
        if (priceDate) {
            int day = 0;
            int month = 0;
            int year = 0;
            // Gwenhywfar counts the months of a broken down date from 0.
            GWEN_Time_GetBrokenDownDate(priceDate, &day, &month, &year);
            entry.insert(QStringLiteral("priceDate"), QDate(year, month + 1, day));
        }
        entry.insert(QStringLiteral("hasValue"), units != nullptr && price != nullptr);
        entry.insert(QStringLiteral("value"), units && price ? AB_Value_GetValueAsDouble(units) * AB_Value_GetValueAsDouble(price) : 0.0);
        result.append(entry);
    }
    return result;
}

}
