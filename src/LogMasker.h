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

#ifndef LOGMASKER_H
#define LOGMASKER_H

#include <QByteArray>
#include <QString>
#include <QStringList>

/**
 * Takes the personal data out of what goes into the debug log, which users
 * copy into problem reports: names, account numbers, IBANs, balances,
 * transactions, the login, PIN and TAN.
 *
 * Personal data is replaced by one "*" per character, so that the structure
 * stays readable and lengths can still be compared: "DE12500105170648489890"
 * becomes "**********************". PIN and TAN become "[PIN]" and "[TAN]", as
 * not even their length is anybody's business.
 *
 * FinTS messages are masked by their structure: everything but the segment
 * headers is masked unless a rule for the segment says it is not personal
 * (the bank's parameters, the protocol's own fields, result codes). Unknown
 * segments are masked as a whole. Free text (AqBanking's messages, the bank's
 * texts in the result codes) is masked by its shape: IBANs, numbers of five
 * digits or more, amounts with a currency, e-mail addresses. On top of both,
 * any personal data the app knows of (registered with addPersonalData()) is
 * masked wherever it appears.
 *
 * Thread-safe; called on whichever thread writes to the log.
 */
namespace LogMasker
{
/**
 * A FinTS 3.0 message, complete as it went over the wire: its segments,
 * masked, separated by "|". The encrypted part (HNVSD) is shown inside it,
 * and statements inside the segments by their own structure (CAMT XML,
 * MT940/MT942/MT535).
 */
QString finTsMessage(const QByteArray &message);
/** Free text, e.g. AqBanking's log lines or a bank's message. */
QString text(const QString &text);
/** Every letter and digit masked, markup and punctuation kept: for a TAN challenge. */
QString everything(const QString &text);
/**
 * Values that are personal wherever they turn up: logins, account numbers,
 * IBANs, owners' names. With words set, each word of four letters or more is
 * masked on its own as well, for names.
 */
void addPersonalData(const QStringList &values, bool words = false);
/** For the tests. */
void clearPersonalData();
}

#endif // LOGMASKER_H
