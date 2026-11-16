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
 * The debug log: what LogMasker leaves of made-up FinTS messages as they go
 * over the wire (an outgoing one with its signed part inside HNVSD, PIN and
 * TAN; an incoming one with result codes, bank and account parameters, a
 * balance, SEPA details and a TAN challenge; statements in CAMT, MT940 and
 * MT535; unknown segments, binary data, a message cut short), of free text and
 * of the personal data the app registers; and DebugLog keeping entries only
 * while enabled, in memory, and forgetting them when switched off.
 */

#include "Check.h"
#include "DebugLog.h"
#include "LogMasker.h"

#include <QCoreApplication>

namespace {

QString stars(int count)
{
    return QString(count, QLatin1Char('*'));
}

void contains(const char *name, const QString &text, const QString &part)
{
    Check::that(name, text.contains(part), text.contains(part) ? part : QStringLiteral("\"%1\" not in %2").arg(part, text));
}

void lacks(const char *name, const QString &text, const QStringList &parts)
{
    QStringList found;
    for (const QString &part : parts) {
        if (text.contains(part, Qt::CaseInsensitive)) {
            found.append(part);
        }
    }
    Check::that(name, found.isEmpty(), found.isEmpty() ? QString() : QStringLiteral("%1 in %2").arg(found.join(QStringLiteral(", ")), text));
}

QByteArray binary(const QByteArray &data)
{
    return '@' + QByteArray::number(data.size()) + '@' + data;
}

void testOutgoing()
{
    Check::section("debug log: a message sent, PIN and TAN included");
    const QByteArray inner =
            "HNSHK:2:4+PIN:2+912+1234567+1+1+1::abcSYSIDxyz+1+1:20261116:120000+1:999:1+6:10:16+280:50010517:max.mustermann:S:0:0'"
            "HKIDN:3:2+280:50010517+kunde4711+abcSYSIDxyz+1'"
            "HKVVB:4:3+12+5+0+ABCDEF0123456789XYZ+0.5'"
            "HKTAN:5:6+4+HKIDN+++++++++Mein Handy'"
            "HKCAZ:6:1+DE12500105170648489890:GENODEF1XYZ:0648489890::280:50010517+urn:iso:std:iso:20022:tech:xsd:camt.052.001.08+N+20260818+20261116'"
            "HNSHA:7:2+1234567++G3h31m?+P!n:654321'";
    const QByteArray message = "HNHBK:1:3+000000000400+300+0+1'"
            "HNVSK:998:3+PIN:2+998+1+1::abcSYSIDxyz+1:20261116:120000+2:2:13:@8@00000000:5:1+280:50010517:max.mustermann:V:0:0+0'"
            "HNVSD:999:1+" + binary(inner) + "'"
            "HNHBS:8:1+1'";
    const QString masked = LogMasker::finTsMessage(message);
    contains("the message header is kept", masked, QStringLiteral("HNHBK:1:3+000000000400+300+0+1'|"));
    contains("the encryption header keeps all but login and system ID", masked,
             QStringLiteral("|HNVSK:998:3+PIN:2+998+1+1::***********+1:20261116:120000+2:2:13:@8@00000000:5:1+280:50010517:**************:V:0:0+0'|"));
    contains("the signed part inside HNVSD is masked as a message of its own", masked,
             QStringLiteral("|HNVSD:999:1+@%1@HNSHK:2:4+PIN:2+912+1234567+1+1+1::***********+1+1:20261116:120000+1:999:1+6:10:16+280:50010517:**************:S:0:0'|").arg(inner.size()));
    contains("customer and system ID are masked, the bank code is not", masked, QStringLiteral("|HKIDN:3:2+280:50010517+*********+***********+1'|"));
    contains("the registration number is masked", masked, QStringLiteral("|HKVVB:4:3+12+5+0+*******************+0.5'|"));
    contains("the TAN medium is masked, the TAN process kept", masked, QStringLiteral("|HKTAN:5:6+4+HKIDN+++++++++**********'|"));
    contains("a request names the account masked, its dates and format kept", masked,
             QStringLiteral("|HKCAZ:6:1+**********************:***********:**********::***:********+urn:iso:std:iso:20022:tech:xsd:camt.052.001.08+N+20260818+20261116'|"));
    contains("PIN and TAN are named, not shown", masked, QStringLiteral("|HNSHA:7:2+1234567++[PIN]:[TAN]''|HNHBS:8:1+1'"));
    lacks("nothing personal is left", masked, QStringList() << QStringLiteral("mustermann") << QStringLiteral("kunde4711")
          << QStringLiteral("SYSID") << QStringLiteral("G3h31m") << QStringLiteral("P!n") << QStringLiteral("654321")
          << QStringLiteral("ABCDEF0123") << QStringLiteral("Handy") << QStringLiteral("0648489890"));
}

void testIncoming()
{
    Check::section("debug log: a message received");
    const QByteArray message = "HNHBK:1:3+000000000999+300+4711abc+1+4711abc:1'"
            "HIRMG:2:2+0010::Nachricht entgegengenommen.'"
            "HIRMS:3:2:3+0020::Auftrag ausgef\xfchrt.+3920::Zugelassene Zwei-Schritt-Verfahren f\xfcr den Benutzer.:912:913'"
            "HIRMS:4:2:4+9210::Konto 1234567890 unbekannt.:1234567890'"
            "HIBPA:5:3+12+280:50010517+Testbank eG+1+1+300+0'"
            "HITANS:6:7:4+1+1+1+J:N:0:912:2:HHD1.4:HHD:1.4:chipTAN optisch:6:1:TAN-Nummer:3:J:2:N:0:0:N:N:00:0:N:1'"
            "HIUPA:7:4:4+max.mustermann+5+0+Max Mustermann'"
            "HIUPD:8:6:4+0648489890::280:50010517+DE12500105170648489890+kunde4711+1+EUR+Mustermann, Max++Girokonto+T:2000,:EUR+HKSAL:1+HKKAZ:1+HKCAZ:1+HKTAN:1'"
            "HISAL:9:7:5+DE12500105170648489890:GENODEF1XYZ:0648489890::280:50010517+Girokonto+EUR+C:1234,56:EUR:20261116:120000+D:12,5:EUR:20261116'"
            "HISPA:10:1:5+J:DE12500105170648489890:GENODEF1XYZ:0648489890::280:50010517'"
            "HITAN:11:6:5+4++ref123+Bitte best\xe4tigen Sie die \xdc" "berweisung an Erika Musterfrau ?+ 100,00 EUR++20261116:130000+Mein Handy'"
            "HNHBS:12:1+1'";
    const QString masked = LogMasker::finTsMessage(message);
    contains("result codes keep code and text", masked,
             QStringLiteral("|HIRMS:3:2:3+0020::Auftrag ausgeführt.+3920::Zugelassene Zwei-Schritt-Verfahren für den Benutzer.:912:913'|"));
    contains("a number in a result code's text and parameters is masked", masked,
             QStringLiteral("|HIRMS:4:2:4+9210::Konto ********** unbekannt.:**********'|"));
    contains("the bank's parameters are kept", masked, QStringLiteral("|HIBPA:5:3+12+280:50010517+Testbank eG+1+1+300+0'|"));
    contains("the bank's TAN methods are kept", masked, QStringLiteral("|HITANS:6:7:4+1+1+1+J:N:0:912:2:HHD1.4:HHD:1.4:chipTAN optisch:6:1:"));
    contains("user parameters keep only version and usage", masked, QStringLiteral("|HIUPA:7:4:4+**************+5+0+**************'|"));
    contains("account parameters keep bank code, type, currency and jobs", masked,
             QStringLiteral("|HIUPD:8:6:4+**********::280:50010517+**********************+*********+1+EUR+***************++*********+*:*****:***+HKSAL:1+HKKAZ:1+HKCAZ:1+HKTAN:1'|"));
    contains("a balance keeps currency, date and time", masked,
             QStringLiteral("+EUR+*:*******:EUR:20261116:120000+*:****:EUR:20261116'|"));
    contains("SEPA details keep the BIC and bank code", masked,
             QStringLiteral("|HISPA:10:1:5+J:**********************:GENODEF1XYZ:**********::280:50010517'|"));
    contains("a TAN challenge is masked, process and order reference kept", masked,
             QStringLiteral("|HITAN:11:6:5+4++ref123+%1++20261116:130000+**********'|").arg(stars(69)));
    lacks("nothing personal is left", masked, QStringList() << QStringLiteral("mustermann") << QStringLiteral("musterfrau")
          << QStringLiteral("0648489890") << QStringLiteral("DE1250") << QStringLiteral("1234,56") << QStringLiteral("12,5")
          << QStringLiteral("2000,") << QStringLiteral("100,00") << QStringLiteral("Girokonto"));
}

void testStatements()
{
    Check::section("debug log: statements");
    const QByteArray camt =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Document xmlns=\"urn:iso:std:iso:20022:tech:xsd:camt.052.001.08\">\n"
            "  <BkToCstmrAcctRpt><GrpHdr><MsgId>MSG4711</MsgId><CreDtTm>2026-11-16T12:00:00</CreDtTm></GrpHdr>\n"
            "  <Rpt><Acct><Id><IBAN>DE12500105170648489890</IBAN></Id></Acct>\n"
            "  <Ntry><Amt Ccy=\"EUR\">100.00</Amt><CdtDbtInd>CRDT</CdtDbtInd><Sts><Cd>BOOK</Cd></Sts>\n"
            "  <BookgDt><Dt>2026-11-10</Dt></BookgDt><NtryDtls><TxDtls><RltdPties><Dbtr><Pty><Nm>Erika M\xc3\xbcsterfrau</Nm></Pty></Dbtr></RltdPties>\n"
            "  <Purp><Cd>SALA</Cd></Purp><RmtInf><Ustrd>Miete November</Ustrd></RmtInf></TxDtls></NtryDtls></Ntry></Rpt>\n"
            "  </BkToCstmrAcctRpt>\n"
            "</Document>\n";
    const QString camtMasked = LogMasker::finTsMessage(
                "HICAZ:5:1:3+DE12500105170648489890:GENODEF1XYZ:0648489890::280:50010517+urn:iso:std:iso:20022:tech:xsd:camt.052.001.08+"
                + binary(camt) + "'");
    contains("CAMT: the format is kept", camtMasked, QStringLiteral("+urn:iso:std:iso:20022:tech:xsd:camt.052.001.08+@"));
    contains("CAMT: the structure stays, the amount is masked, its currency not", camtMasked,
             QStringLiteral("<Ntry><Amt Ccy=\"EUR\">******</Amt><CdtDbtInd>CRDT</CdtDbtInd><Sts><Cd>BOOK</Cd></Sts><BookgDt><Dt>**********</Dt></BookgDt>"));
    contains("CAMT: the namespace and the statement's time are kept", camtMasked,
             QStringLiteral("<Document xmlns=\"urn:iso:std:iso:20022:tech:xsd:camt.052.001.08\"><BkToCstmrAcctRpt><GrpHdr><MsgId>*******</MsgId><CreDtTm>2026-11-16T12:00:00</CreDtTm>"));
    contains("CAMT: names and purposes are masked, the purpose code as well", camtMasked,
             QStringLiteral("<Nm>****************</Nm></Pty></Dbtr></RltdPties><Purp><Cd>****</Cd></Purp><RmtInf><Ustrd>**************</Ustrd>"));
    lacks("CAMT: nothing personal is left", camtMasked, QStringList() << QStringLiteral("sterfrau") << QStringLiteral("Miete")
          << QStringLiteral("SALA") << QStringLiteral("100.00") << QStringLiteral("0648489890") << QStringLiteral("MSG4711"));

    const QByteArray mt940 = "\r\n:20:STARTUMS\r\n:25:50010517/0648489890\r\n:28C:0\r\n:60F:C261101EUR1234,56\r\n"
            ":61:2611101110CR100,00NMSCNONREF\r\n:86:166?00GUTSCHRIFT?20Miete November?32Erika Musterfrau\r\n"
            ":62F:C261110EUR1334,56\r\n-";
    const QString mt940Masked = LogMasker::finTsMessage("HIKAZ:5:7:3+" + binary(mt940) + "'");
    contains("MT940: tags are kept, their contents masked", mt940Masked,
             QStringLiteral("HIKAZ:5:7:3+@%1@\n:20:********\n:25:*******************\n:28C:*\n:60F:*****************\n").arg(mt940.size()));
    contains("MT940: the purpose keeps its subfield marks", mt940Masked,
             QStringLiteral("\n:86:***?00**********?20**************?32****************\n:62F:"));
    contains("MT940: the end mark stays", mt940Masked, QStringLiteral("\n-'"));
    lacks("MT940: nothing personal is left", mt940Masked, QStringList() << QStringLiteral("Musterfrau") << QStringLiteral("Miete")
          << QStringLiteral("0648489890") << QStringLiteral("1234,56"));

    const QByteArray mt535 = ":16R:GENL\r\n:28E:1/ONLY\r\n:16S:GENL\r\n:16R:FIN\r\n:35B:ISIN DE0005140008\r\nDeutsche Bank AG\r\n:16S:FIN\r\n-";
    const QString mt535Masked = LogMasker::finTsMessage("HIWPD:5:6:3+" + binary(mt535) + "'");
    contains("MT535: block names are kept, the rest masked", mt535Masked,
             QStringLiteral(":16R:GENL\n:28E:******\n:16S:GENL\n:16R:FIN\n:35B:*****************\n****************\n:16S:FIN\n-'"));
}

void testOddities()
{
    Check::section("debug log: unknown segments, binary data, broken messages");
    Check::equal("an unknown segment keeps only its header",
                 LogMasker::finTsMessage("HKXYZ:3:1+geheim+da?:ta:x'"), QStringLiteral("HKXYZ:3:1+******+*****:*'"));
    Check::equal("binary data that is not text is described",
                 LogMasker::finTsMessage(QByteArray("HITAN:4:6:3+4++ref+Text+") + binary(QByteArray("\x89PNG\r\n\x1a\n\0\0", 10)) + "'"),
                 QStringLiteral("HITAN:4:6:3+4++ref+****+@10@[binary, 10 bytes]'"));
    Check::equal("an incomplete segment at the end is masked as a whole",
                 LogMasker::finTsMessage("HNHBS:2:1+1'HKIDN:3:2+280:5001"), QStringLiteral("HNHBS:2:1+1'|%1").arg(stars(18)));
    Check::equal("a line break after the last segment is not one",
                 LogMasker::finTsMessage("HNHBS:2:1+1'\r\n"), QStringLiteral("HNHBS:2:1+1'"));
}

void testText()
{
    Check::section("debug log: free text and known personal data");
    Check::equal("IBAN, e-mail, amount and long numbers are masked in text",
                 LogMasker::text(QStringLiteral("Konto DE12 5001 0517 0648 4898 90 von max@example.com: 100,00 EUR, Kunde 4711123, Fehler 9210 (-1)")),
                 QStringLiteral("Konto %1 von %2: %3, Kunde %4, Fehler 9210 (-1)").arg(stars(27), stars(15), stars(10), stars(7)));
    Check::equal("a result line is kept", LogMasker::text(QStringLiteral("HBCI: 3920 - Zugelassene TAN-Verfahren: 912, 913 (S)")),
                 QStringLiteral("HBCI: 3920 - Zugelassene TAN-Verfahren: 912, 913 (S)"));
    LogMasker::clearPersonalData();
    LogMasker::addPersonalData(QStringList() << QStringLiteral("max.mustermann") << QStringLiteral("0064711") << QStringLiteral("abc"));
    LogMasker::addPersonalData(QStringList() << QStringLiteral("Mustermann, Maximilian Max"), true);
    Check::equal("a login is masked wherever it appears", LogMasker::text(QStringLiteral("Handling user \"Max.Mustermann\"")),
                 QStringLiteral("Handling user \"%1\"").arg(stars(14)));
    Check::equal("an owner's name is masked as a whole and by its words, short ones and parts of words not",
                 LogMasker::text(QStringLiteral("Mustermann, Maximilian Max; Herr Mustermann; Mustermannstr.; Max")),
                 QStringLiteral("%1; Herr %2; Mustermannstr.; Max").arg(stars(26), stars(10)));
    Check::equal("an account number is masked with and without its leading zeros, values under four characters not",
                 LogMasker::text(QStringLiteral("Konto 0064711, 64711 and 4711 abc")), QStringLiteral("Konto *******, ***** and 4711 abc"));
    Check::equal("known personal data is masked in FinTS messages as well",
                 LogMasker::finTsMessage("HIRMS:3:2:3+3938::Zugang f\xfcr max.mustermann gesperrt.'"),
                 QStringLiteral("HIRMS:3:2:3+3938::Zugang für %1 gesperrt.'").arg(stars(14)));
    LogMasker::clearPersonalData();
    Check::equal("a TAN challenge keeps only its shape",
                 LogMasker::everything(QStringLiteral("<b>Überweisung</b> an DE12, 100,00 EUR")),
                 QStringLiteral("<b>***********</b> ** ****, ***,** ***"));
}

void testLog()
{
    Check::section("debug log: in memory while enabled");
    DebugLog log;
    DebugLog::write(DebugLog::App, QStringLiteral("before"));
    QCoreApplication::processEvents();
    Check::equal("nothing is recorded while off", log.count(), 0);
    log.setEnabled(true);
    QCoreApplication::processEvents();
    const int start = log.count();
    Check::that("switched on, it starts with what the app runs on", start >= 4, QString::number(start));
    DebugLog::write(DebugLog::App, QStringLiteral("something happened"));
    DebugLog::writeFinTsMessage(0, "HNHBS:2:1+1'", 12);
    const QByteArray received = "HKIDN:2:2+280:50010517+kunde4711+0+1'";
    DebugLog::writeFinTsMessage(1, received.constData(), static_cast<unsigned int>(received.size()));
    QCoreApplication::processEvents();
    Check::equal("entries arrive on the UI thread", log.count(), start + 3);
    const QString text = log.text();
    Check::that("the text has every entry on a line with time and source",
                text.contains(QRegExp(QStringLiteral("\\n\\d{4}-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d{3} app: something happened\\n"))), text);
    contains("a message sent is recorded as such", text, QStringLiteral(" sent: HNHBS:2:1+1'\n"));
    contains("a message received is recorded masked", text, QStringLiteral(" received: HKIDN:2:2+280:50010517+*********+*+1'\n"));
    const QModelIndex last = log.index(log.count() - 1);
    Check::equal("the list shows the source", log.data(last, DebugLog::SourceRole), QVariant(QStringLiteral("received")));
    Check::equal("a short entry is not shortened", log.data(last, DebugLog::ShortenedRole), QVariant(false));
    DebugLog::write(DebugLog::App, QString(5000, QLatin1Char('x')));
    QCoreApplication::processEvents();
    const QModelIndex longEntry = log.index(log.count() - 1);
    Check::equal("a long entry is shortened in the list", log.data(longEntry, DebugLog::ShortenedRole), QVariant(true));
    Check::equal("its length is known", log.data(longEntry, DebugLog::LengthRole), QVariant(5000));
    Check::that("the copied text has all of it", log.text().contains(QString(5000, QLatin1Char('x'))));
    log.setEnabled(false);
    Check::equal("switched off, the log is gone", log.count(), 0);
    DebugLog::write(DebugLog::App, QStringLiteral("after"));
    QCoreApplication::processEvents();
    Check::equal("and nothing more is recorded", log.count(), 0);
}

}

void runDebugLogTests()
{
    testOutgoing();
    testIncoming();
    testStatements();
    testOddities();
    testText();
    testLog();
}
