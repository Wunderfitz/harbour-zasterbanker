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
 * What the bank sends, read the way the app reads it: AqBanking starts on the
 * encrypted store, its real importers parse sample statements with the
 * profiles the package installs, the result is merged as the aqhbci jobs merge
 * it, and StatementReader turns it into what the pages show.
 *
 * The samples are made up, in the formats FinTS banks send: MT940 (booked),
 * MT942 (pending), CAMT 052.001.08 and .02, and MT535 for a depot.
 */

#include "Check.h"
#include "EncryptedConfigStore.h"
#include "StatementReader.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <aqbanking/banking.h>
#include <aqbanking/banking_imex.h>
#include <aqbanking/types/balance.h>
#include <aqbanking/types/imexporter_context.h>
#include <aqbanking/types/transaction.h>
#include <gwenhywfar/gwendate.h>
#include <gwenhywfar/pathmanager.h>

extern "C" {
#include <aqbanking/backendsupport/provider_be.h>
}

namespace {

const uint32_t ACCOUNT_ID = 42;

QByteArray lines(std::initializer_list<const char *> parts)
{
    QByteArray result;
    for (const char *part : parts) {
        result += part;
        result += "\r\n";
    }
    return result;
}

// As AH_Job_GetTransactions and its CAMT twin do: a scratch context, merged
// into the account's entry with the type of statement.
QVariantList importTransactions(AB_BANKING *banking, const char *importer, const char *profile, const QByteArray &data,
                                AB_TRANSACTION_TYPE type, int *result)
{
    AB_IMEXPORTER_CONTEXT *scratch = AB_ImExporterContext_new();
    *result = AB_Banking_ImportFromBufferLoadProfile(banking, importer, scratch, profile, nullptr,
                                                    reinterpret_cast<const uint8_t *>(data.constData()), data.size());
    AB_IMEXPORTER_ACCOUNTINFO *info = AB_ImExporterAccountInfo_new();
    AB_ImExporterAccountInfo_SetAccountId(info, ACCOUNT_ID);
    AB_Provider_MergeContextsSetTypeAndFreeSrc(info, scratch, type);
    const QVariantList transactions = StatementReader::transactions(info);
    AB_ImExporterAccountInfo_free(info);
    return transactions;
}

QVariantMap byName(const QVariantList &entries, const QString &name)
{
    for (const QVariant &entry : entries) {
        if (entry.toMap().value(QStringLiteral("name")).toString() == name) {
            return entry.toMap();
        }
    }
    return QVariantMap();
}

void mt940Tests(AB_BANKING *banking)
{
    const QByteArray statement = lines({
        ":20:STARTUMS",
        ":25:10010010/1234567890",
        ":28C:0",
        ":60F:C261101EUR1000,00",
        ":61:2611031103DR42,50NMSCNONREF",
        ":86:106?00KARTENZAHLUNG?20SVWZ+Einkauf Markt am 03.11.",
        "?30TESTDEFF100?31DE89370400440532013000",
        "?32Bio Markt GmbH",
        ":61:2611051105CR2500,00NMSCNONREF",
        ":86:166?00GUTSCHRIFT?20SVWZ+Gehalt November",
        "?31DE02100100100006820101?32Arbeitgeber AG",
        ":62F:C261105EUR3457,50",
        "-"
    });
    int rv = 0;
    const QVariantList transactions = importTransactions(banking, "swift", "fints940", statement, AB_Transaction_TypeStatement, &rv);
    Check::that("MT940 is read", rv >= 0, QString::number(rv));
    Check::equal("both lines arrive", transactions.size(), 2);

    const QVariantMap card = byName(transactions, QStringLiteral("Bio Markt GmbH"));
    Check::equal("MT940: a debit is negative", card.value(QStringLiteral("amount")), -42.5);
    Check::equal("MT940: it has an amount", card.value(QStringLiteral("hasAmount")), true);
    Check::equal("MT940: currency", card.value(QStringLiteral("currency")), QStringLiteral("EUR"));
    Check::equal("MT940: date", card.value(QStringLiteral("date")), QDate(2026, 11, 3));
    Check::equal("MT940: IBAN", card.value(QStringLiteral("iban")), QStringLiteral("DE89370400440532013000"));
    Check::equal("MT940: kind", card.value(QStringLiteral("kind")), QStringLiteral("KARTENZAHLUNG"));
    Check::that("MT940: purpose", card.value(QStringLiteral("purpose")).toString().contains(QStringLiteral("Einkauf Markt am 03.11.")),
                card.value(QStringLiteral("purpose")).toString());
    Check::equal("MT940: booked", card.value(QStringLiteral("pending")), false);

    const QVariantMap salary = byName(transactions, QStringLiteral("Arbeitgeber AG"));
    Check::equal("MT940: a credit is positive", salary.value(QStringLiteral("amount")), 2500.0);
    Check::equal("MT940: its date", salary.value(QStringLiteral("date")), QDate(2026, 11, 5));
}

void mt942Tests(AB_BANKING *banking)
{
    const QByteArray statement = lines({
        ":20:STARTDISPE",
        ":25:10010010/1234567890",
        ":28C:0",
        ":34F:EURD0,",
        ":13:2611071200",
        ":61:2611071107DR19,99NMSCNONREF",
        ":86:105?00LASTSCHRIFT?20SVWZ+Abo November",
        "?32Streaming GmbH",
        ":90D:1EUR19,99",
        ":90C:0EUR0,",
        "-"
    });
    int rv = 0;
    const QVariantList transactions = importTransactions(banking, "swift", "fints942", statement, AB_Transaction_TypeNotedStatement, &rv);
    Check::that("MT942 is read", rv >= 0, QString::number(rv));
    const QVariantMap pending = byName(transactions, QStringLiteral("Streaming GmbH"));
    Check::equal("MT942: the line arrives", transactions.size(), 1);
    Check::equal("MT942: pending", pending.value(QStringLiteral("pending")), true);
    Check::equal("MT942: amount", pending.value(QStringLiteral("amount")), -19.99);
    // Without a currency of its own; the page falls back to the account's.
    Check::equal("MT942: no currency", pending.value(QStringLiteral("currency")), QString());
}

const char CAMT_08[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document xmlns="urn:iso:std:iso:20022:tech:xsd:camt.052.001.08">
 <BkToCstmrAcctRpt>
  <GrpHdr><MsgId>TEST-1</MsgId><CreDtTm>2026-11-08T10:00:00</CreDtTm></GrpHdr>
  <Rpt>
   <Id>TEST-1-1</Id>
   <CreDtTm>2026-11-08T10:00:00</CreDtTm>
   <Acct><Id><IBAN>DE02100100100006820101</IBAN></Id><Ccy>EUR</Ccy></Acct>
   <Ntry>
    <Amt Ccy="EUR">2500.00</Amt>
    <CdtDbtInd>CRDT</CdtDbtInd>
    <Sts><Cd>BOOK</Cd></Sts>
    <BookgDt><Dt>2026-11-05</Dt></BookgDt>
    <ValDt><Dt>2026-11-05</Dt></ValDt>
    <NtryDtls><TxDtls>
     <Amt Ccy="EUR">2500.00</Amt>
     <CdtDbtInd>CRDT</CdtDbtInd>
     <RltdPties>
      <Dbtr><Pty><Nm>Arbeitgeber AG</Nm></Pty></Dbtr>
      <DbtrAcct><Id><IBAN>DE89370400440532013000</IBAN></Id></DbtrAcct>
     </RltdPties>
     <RmtInf><Ustrd>Gehalt November</Ustrd></RmtInf>
    </TxDtls></NtryDtls>
   </Ntry>
   <Ntry>
    <Amt Ccy="EUR">42.50</Amt>
    <CdtDbtInd>DBIT</CdtDbtInd>
    <Sts><Cd>BOOK</Cd></Sts>
    <BookgDt><Dt>2026-11-03</Dt></BookgDt>
    <ValDt><Dt>2026-11-03</Dt></ValDt>
    <NtryDtls><TxDtls>
     <Amt Ccy="EUR">42.50</Amt>
     <CdtDbtInd>DBIT</CdtDbtInd>
     <RltdPties>
      <Cdtr><Pty><Nm>Bio Markt GmbH</Nm></Pty></Cdtr>
      <CdtrAcct><Id><IBAN>DE89370400440532013000</IBAN></Id></CdtrAcct>
     </RltdPties>
     <RmtInf><Ustrd>Einkauf Markt am 03.11.</Ustrd></RmtInf>
    </TxDtls></NtryDtls>
   </Ntry>
  </Rpt>
 </BkToCstmrAcctRpt>
</Document>
)";

const char CAMT_02[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document xmlns="urn:iso:std:iso:20022:tech:xsd:camt.052.001.02">
 <BkToCstmrAcctRpt>
  <GrpHdr><MsgId>TEST-2</MsgId><CreDtTm>2026-11-08T10:00:00</CreDtTm></GrpHdr>
  <Rpt>
   <Id>TEST-2-1</Id>
   <CreDtTm>2026-11-08T10:00:00</CreDtTm>
   <Acct><Id><IBAN>DE02100100100006820101</IBAN></Id><Ccy>EUR</Ccy></Acct>
   <Ntry>
    <Amt Ccy="EUR">42.50</Amt>
    <CdtDbtInd>DBIT</CdtDbtInd>
    <Sts>BOOK</Sts>
    <BookgDt><Dt>2026-11-03</Dt></BookgDt>
    <ValDt><Dt>2026-11-03</Dt></ValDt>
    <NtryDtls><TxDtls>
     <RltdPties>
      <Cdtr><Nm>Bio Markt GmbH</Nm></Cdtr>
      <CdtrAcct><Id><IBAN>DE89370400440532013000</IBAN></Id></CdtrAcct>
     </RltdPties>
     <RmtInf><Ustrd>Einkauf Markt am 03.11.</Ustrd></RmtInf>
    </TxDtls></NtryDtls>
   </Ntry>
  </Rpt>
 </BkToCstmrAcctRpt>
</Document>
)";

void camtTests(AB_BANKING *banking)
{
    int rv = 0;
    QVariantList transactions = importTransactions(banking, "xml", "camt_052_001_08", QByteArray(CAMT_08), AB_Transaction_TypeStatement, &rv);
    Check::that("CAMT 052.001.08 is read", rv >= 0, QString::number(rv));
    Check::equal("CAMT .08: both entries arrive", transactions.size(), 2);
    QVariantMap salary = byName(transactions, QStringLiteral("Arbeitgeber AG"));
    Check::equal("CAMT .08: a credit is positive", salary.value(QStringLiteral("amount")), 2500.0);
    Check::equal("CAMT .08: currency", salary.value(QStringLiteral("currency")), QStringLiteral("EUR"));
    Check::equal("CAMT .08: date", salary.value(QStringLiteral("date")), QDate(2026, 11, 5));
    Check::equal("CAMT .08: IBAN of the payer", salary.value(QStringLiteral("iban")), QStringLiteral("DE89370400440532013000"));
    Check::that("CAMT .08: purpose", salary.value(QStringLiteral("purpose")).toString().contains(QStringLiteral("Gehalt November")),
                salary.value(QStringLiteral("purpose")).toString());
    QVariantMap card = byName(transactions, QStringLiteral("Bio Markt GmbH"));
    Check::equal("CAMT .08: a debit is negative", card.value(QStringLiteral("amount")), -42.5);
    Check::equal("CAMT .08: name of the payee", card.value(QStringLiteral("name")), QStringLiteral("Bio Markt GmbH"));

    transactions = importTransactions(banking, "xml", "camt_052_001_02", QByteArray(CAMT_02), AB_Transaction_TypeStatement, &rv);
    Check::that("CAMT 052.001.02 is read", rv >= 0, QString::number(rv));
    card = byName(transactions, QStringLiteral("Bio Markt GmbH"));
    Check::equal("CAMT .02: the entry arrives", transactions.size(), 1);
    Check::equal("CAMT .02: amount", card.value(QStringLiteral("amount")), -42.5);
    Check::equal("CAMT .02: date", card.value(QStringLiteral("date")), QDate(2026, 11, 3));
}

void mt535Tests(AB_BANKING *banking)
{
    // Positions in units with a price, one with an umlaut and its price dated by
    // 98C (date and time), and a bond in nominal value with a price in percent.
    const QByteArray statement = lines({
        ":16R:GENL",
        ":28E:1/ONLY",
        ":13A::STAT//535",
        ":20C::SEME//NONREF",
        ":23G:NEWM",
        ":98A::STAT//20261106",
        ":22F::STTY//CUST",
        ":97A::SAFE//10010010/0001234567",
        ":17B::ACTI//Y",
        ":16S:GENL",
        ":16R:SUBSAFE",
        ":16R:FIN",
        ":35B:ISIN DE0007164600",
        "/DE/716460",
        "SAP SE",
        ":90B::MRKT//ACTU/EUR123,45",
        ":98A::PRIC//20261105",
        ":93B::AGGR//UNIT/10,",
        ":16S:FIN",
        ":16R:FIN",
        ":35B:ISIN DE0008430026",
        "/DE/843002",
        // FinTS speaks ISO-8859-1; AqBanking hands out UTF-8.
        "M\xfcnchener R\xfc" "ck AG",
        ":90B::MRKT//ACTU/EUR500,",
        ":98C::PRIC//20261104173000",
        ":93B::AGGR//UNIT/2,5",
        ":16S:FIN",
        ":16R:FIN",
        ":35B:ISIN DE0001102580",
        "Bundesanleihe",
        ":90A::MRKT//PRCT/101,25",
        ":93B::AGGR//FAMT/1000,",
        ":16S:FIN",
        ":16S:SUBSAFE",
        "-"
    });
    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    const int rv = AB_Banking_ImportFromBufferLoadProfile(banking, "swift", context, "SWIFT-MT535", nullptr,
                                                          reinterpret_cast<const uint8_t *>(statement.constData()), statement.size());
    const QVariantList holdings = StatementReader::holdings(context);
    AB_ImExporterContext_free(context);
    Check::that("MT535 is read", rv >= 0, QString::number(rv));
    Check::equal("MT535: all three positions arrive", holdings.size(), 3);

    const QVariantMap shares = byName(holdings, QStringLiteral("SAP SE"));
    Check::equal("MT535: identifier", shares.value(QStringLiteral("identifier")), QStringLiteral("DE0007164600"));
    Check::equal("MT535: units", shares.value(QStringLiteral("units")), 10.0);
    Check::equal("MT535: price (patched)", shares.value(QStringLiteral("price")), 123.45);
    Check::equal("MT535: currency", shares.value(QStringLiteral("currency")), QStringLiteral("EUR"));
    Check::equal("MT535: price date by 98A, its day too (patched)", shares.value(QStringLiteral("priceDate")), QDate(2026, 11, 5));
    Check::equal("MT535: value is units times price", shares.value(QStringLiteral("value")), 1234.5);

    const QVariantMap umlaut = byName(holdings, QString::fromUtf8("M\xc3\xbcnchener R\xc3\xbc" "ck AG"));
    Check::that("MT535: umlauts in a name survive (patched)", !umlaut.isEmpty(),
                [&holdings]() { QStringList n; for (const QVariant &h : holdings) n << h.toMap().value(QStringLiteral("name")).toString(); return n.join(QLatin1Char('|')); }());
    Check::equal("MT535: fractional units", umlaut.value(QStringLiteral("units")), 2.5);
    Check::equal("MT535: price date by 98C (patched)", umlaut.value(QStringLiteral("priceDate")), QDate(2026, 11, 4));
    Check::equal("MT535: its value", umlaut.value(QStringLiteral("value")), 1250.0);

    // The known gap: neither nominal values nor prices in percent are read.
    const QVariantMap bond = byName(holdings, QStringLiteral("Bundesanleihe"));
    Check::equal("MT535: a bond has no value rather than a wrong one", bond.value(QStringLiteral("hasValue")), false);
}

void addBalance(AB_IMEXPORTER_ACCOUNTINFO *info, AB_BALANCE_TYPE type, double amount, const QDate &date)
{
    AB_BALANCE *balance = AB_Balance_new();
    AB_Balance_SetType(balance, type);
    AB_VALUE *value = AB_Value_fromDouble(amount);
    AB_Value_SetCurrency(value, "EUR");
    AB_Balance_SetValue(balance, value);
    AB_Value_free(value);
    if (date.isValid()) {
        GWEN_DATE *gwenDate = GWEN_Date_fromGregorian(date.year(), date.month(), date.day());
        AB_Balance_SetDate(balance, gwenDate);
        GWEN_Date_free(gwenDate);
    }
    AB_ImExporterAccountInfo_AddBalance(info, balance);
}

void balanceTests()
{
    AB_IMEXPORTER_CONTEXT *context = AB_ImExporterContext_new();
    AB_IMEXPORTER_ACCOUNTINFO *both = AB_ImExporterAccountInfo_new();
    AB_ImExporterAccountInfo_SetAccountId(both, 1);
    addBalance(both, AB_Balance_TypeNoted, 99.0, QDate(2026, 11, 8));
    addBalance(both, AB_Balance_TypeBooked, 1234.56, QDate(2026, 11, 7));
    addBalance(both, AB_Balance_TypeDisposable, 5000.0, QDate());
    AB_ImExporterContext_AddAccountInfo(context, both);
    AB_IMEXPORTER_ACCOUNTINFO *noted = AB_ImExporterAccountInfo_new();
    AB_ImExporterAccountInfo_SetAccountId(noted, 2);
    addBalance(noted, AB_Balance_TypeNoted, -10.0, QDate());
    AB_ImExporterContext_AddAccountInfo(context, noted);
    AB_IMEXPORTER_ACCOUNTINFO *none = AB_ImExporterAccountInfo_new();
    AB_ImExporterAccountInfo_SetAccountId(none, 3);
    AB_ImExporterContext_AddAccountInfo(context, none);

    const QDateTime fetched(QDate(2026, 11, 8), QTime(9, 30), Qt::UTC);
    const QVariantList balances = StatementReader::balances(context, fetched);
    AB_ImExporterContext_free(context);

    Check::equal("balances: only accounts with one", balances.size(), 2);
    const QVariantMap first = balances.value(0).toMap();
    const QVariantMap second = balances.value(1).toMap();
    Check::equal("balances: the booked one is taken", first.value(QStringLiteral("balance")), 1234.56);
    Check::equal("balances: with its date", first.value(QStringLiteral("date")), QDate(2026, 11, 7));
    Check::equal("balances: and currency", first.value(QStringLiteral("currency")), QStringLiteral("EUR"));
    Check::equal("balances: and the time fetched", first.value(QStringLiteral("fetched")), fetched);
    Check::equal("balances: otherwise the first", second.value(QStringLiteral("balance")), -10.0);
    Check::equal("balances: of its account", second.value(QStringLiteral("accountId")), 2u);
    Check::that("balances: without a date if the bank gave none", !second.contains(QStringLiteral("date")));
}

}

void runStatementTests(const QString &installedData)
{
    Check::section("Statements");
    balanceTests();

    QTemporaryDir folder;
    EncryptedConfigStore store(folder.path() + QStringLiteral("/aqbanking.zbs"), QByteArray(32, 's'));
    store.load();
    store.registerAsDirPlugin();
    AB_BANKING *banking = AB_Banking_new("ZasterBankerTests", QFile::encodeName(folder.path()).constData(), 0);
    const int rv = AB_Banking_Init(banking);
    if (!Check::that("AqBanking starts on the encrypted store", rv >= 0, QString::number(rv))) {
        AB_Banking_free(banking);
        return;
    }
    // Where the package puts AqBanking's data; the libraries look below
    // /usr/share/harbour-zasterbanker, the tests where they installed it.
    GWEN_PathManager_InsertPath("aqbanking", "aqbanking", "datadir",
                                QFile::encodeName(installedData + QStringLiteral("/aqbanking/data")).constData());

    mt940Tests(banking);
    mt942Tests(banking);
    camtTests(banking);
    mt535Tests(banking);

    AB_Banking_Fini(banking);
    AB_Banking_free(banking);
    Check::that("AqBanking wrote nothing of its own", QDir(folder.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries)
                == QStringList(QStringLiteral("aqbanking.zbs")),
                QDir(folder.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).join(QLatin1Char(',')));
}
