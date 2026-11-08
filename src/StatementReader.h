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

#ifndef STATEMENTREADER_H
#define STATEMENTREADER_H

#include <QDateTime>
#include <QVariantList>

#include <aqbanking/types/imexporter_context.h>

/**
 * Reads what the bank sent - balances, statement lines, securities - out of
 * the context AqBanking leaves it in, into the maps the models take.
 *
 * Only the banking thread uses it, right after AB_Banking_SendCommands(); the
 * tests feed it statements through AqBanking's own importers.
 */
namespace StatementReader
{
/**
 * The balance of each account in the context that has one: the booked one if
 * the bank sent it, otherwise the first. "fetched" goes with each, for the
 * date shown when the bank gave none.
 */
QVariantList balances(const AB_IMEXPORTER_CONTEXT *context, const QDateTime &fetched);
/** The account's statement lines, booked and pending. */
QVariantList transactions(const AB_IMEXPORTER_ACCOUNTINFO *info);
/** What a depot holds: everything in the context, as only one depot is asked for at a time. */
QVariantList holdings(const AB_IMEXPORTER_CONTEXT *context);
}

#endif // STATEMENTREADER_H
