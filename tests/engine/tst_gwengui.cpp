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
 * GwenGui, driven through Gwenhywfar's own GUI API as AqBanking drives it, with
 * a stand-in for the banking thread: the PIN memory's rules, the filter on the
 * bank's texts, the TAN image and flicker code, message boxes (AqBanking's
 * request to confirm in the bank's app reworded) and the certificate check.
 */

#include "Check.h"
#include "FakeBankingThread.h"
#include "FlickerCode.h"
#include "GwenGui.h"

#include <aqbanking/banking.h>
#include <gwenhywfar/db.h>
#include <gwenhywfar/error.h>
#include <gwenhywfar/gui.h>
#include <gwenhywfar/ssl_cert_descr.h>
#include <gwenhywfar/ssl_cert_flags.h>

using FakeBankingThread::answer;
using FakeBankingThread::answers;
using FakeBankingThread::requests;

namespace {

const qint64 MINUTE = 60 * 1000;

qint64 fakeTime = 0;

qint64 fakeClock()
{
    return fakeTime;
}

struct Entry {
    int result;
    QString text;
    QByteArray bytes; // as AqBanking gets them
};

// What AqBanking does when it needs the PIN of a bank access.
Entry askPin(const char *token, int minLength = 4)
{
    char buffer[64];
    buffer[0] = 0;
    const int rv = GWEN_Gui_GetPassword(0, token, "PIN", "Bitte geben Sie Ihre PIN ein.", buffer, minLength,
                                        sizeof(buffer), GWEN_Gui_PasswordMethod_Text, nullptr, 0);
    return Entry { rv, QString::fromUtf8(buffer), QByteArray(buffer) };
}

Entry askTan(const char *text, GWEN_GUI_PASSWORD_METHOD method = GWEN_Gui_PasswordMethod_Text,
             GWEN_DB_NODE *params = nullptr)
{
    char buffer[64];
    buffer[0] = 0;
    const int rv = GWEN_Gui_GetPassword(GWEN_GUI_INPUT_FLAGS_TAN, "TAN_pintan_alice", "TAN", text, buffer, 1,
                                        sizeof(buffer), method, params, 0);
    return Entry { rv, QString::fromUtf8(buffer), QByteArray(buffer) };
}

void bankSays(const char *token, GWEN_GUI_PASSWORD_STATUS status)
{
    GWEN_Gui_SetPasswordStatus(token, nullptr, status, 0);
}

// Whether the next request for the token is answered from memory.
bool remembered(const char *token)
{
    const int before = requests.size();
    answers.clear();
    const Entry entry = askPin(token);
    return requests.size() == before && entry.result == 0;
}

// The worked example of the HHD 1.4 specification, as AqBanking turns the
// bank's challenge into the code to send, and the frames it is shown as: the
// sync 0FFF in front, each byte's low half-byte first.
const char FLICKER_CODE[] = "1784011049063F059876543210041234567844312C303019";
const char FLICKER_FRAMES[] = "F0FF714810019460F350896745230140214365874413C2030391";

const char ALICE[] = "PIN_pintan_alice";
const char BOB[] = "PIN_pintan_bob";
const char CAROL[] = "PIN_pintan_carol";

void pinTests()
{
    FakeBankingThread::reset();
    GwenGui::forgetPins();
    fakeTime = 10 * MINUTE;

    answers << answer(QStringLiteral("12345"));
    Entry entry = askPin(ALICE);
    Check::that("the PIN is asked for", requests.size() == 1 && requests.first().type == BankingThread::PinInteraction);
    Check::equal("and handed to AqBanking", entry.text, QStringLiteral("12345"));
    Check::equal("with the dialog's text", requests.first().text, QStringLiteral("Bitte geben Sie Ihre PIN ein."));
    Check::that("reused within the same command", remembered(ALICE));
    bankSays(ALICE, GWEN_Gui_PasswordStatus_Ok);
    GwenGui::finishCommand();
    Check::that("an accepted PIN is kept for the next command", remembered(ALICE));
    Check::that("it is handed over unchanged", askPin(ALICE).text == QStringLiteral("12345"));
    GwenGui::finishCommand();

    FakeBankingThread::reset();
    answers << answer(QStringLiteral("99999"));
    askPin(BOB);
    GwenGui::finishCommand();
    Check::that("a PIN the bank did not confirm is gone after its command", !remembered(BOB));
    Check::that("then the user is asked again", requests.size() == 2 && requests.last().type == BankingThread::PinInteraction,
                QString::number(requests.size()));
    FakeBankingThread::reset();

    answers << answer(QStringLiteral("99999"));
    askPin(BOB);
    bankSays(BOB, GWEN_Gui_PasswordStatus_Ok);
    GwenGui::finishCommand();
    Check::that("bank accesses are kept apart", remembered(ALICE) && remembered(BOB));
    GwenGui::finishCommand();

    bankSays(ALICE, GWEN_Gui_PasswordStatus_Bad);
    Check::that("a PIN the bank rejects is gone at once", !remembered(ALICE));
    Check::that("and only that one", remembered(BOB));
    GwenGui::finishCommand();

    // Five minutes after the end of the last command that used it.
    FakeBankingThread::reset();
    GwenGui::forgetPins();
    fakeTime = 100 * MINUTE;
    answers << answer(QStringLiteral("24680"));
    askPin(CAROL);
    bankSays(CAROL, GWEN_Gui_PasswordStatus_Ok);
    fakeTime += 3 * MINUTE;  // waiting for the TAN, say
    GwenGui::finishCommand();
    Check::equal("the time starts when the command ends", GwenGui::forgetExpiredPins(), 5 * MINUTE);
    fakeTime += 4 * MINUTE;
    Check::equal("and is counted down", GwenGui::forgetExpiredPins(), 1 * MINUTE);
    Check::that("still kept after 4 minutes", remembered(CAROL));
    GwenGui::finishCommand();
    Check::equal("using it starts the time again", GwenGui::forgetExpiredPins(), 5 * MINUTE);
    fakeTime += 5 * MINUTE - 1;
    GwenGui::finishCommand();  // another command, which did not use it
    Check::equal("other commands do not", GwenGui::forgetExpiredPins(), qint64(1));
    fakeTime += 1;
    Check::equal("expired after 5 minutes", GwenGui::forgetExpiredPins(), qint64(-1));
    Check::that("and asked for again", !remembered(CAROL));

    answers << answer(QStringLiteral("24680"));
    askPin(CAROL);
    bankSays(CAROL, GWEN_Gui_PasswordStatus_Ok);
    GwenGui::finishCommand();
    fakeTime += 6 * MINUTE;
    Check::that("an expired PIN is never used, even if not wiped yet", !remembered(CAROL));

    answers << answer(QStringLiteral("24680"));
    askPin(CAROL);
    bankSays(CAROL, GWEN_Gui_PasswordStatus_Ok);
    GwenGui::finishCommand();
    GwenGui::forgetPins();
    Check::that("forgetPins() wipes them all", !remembered(CAROL));
    GwenGui::finishCommand();

    FakeBankingThread::reset();
    entry = askPin(ALICE);
    Check::equal("cancelling the PIN dialog aborts", entry.result, GWEN_ERROR_USER_ABORTED);
    Check::that("and keeps nothing", !remembered(ALICE));
    GwenGui::finishCommand();

    FakeBankingThread::reset();
    answers << answer(QStringLiteral("12"));
    entry = askPin(ALICE, 5);
    Check::equal("a PIN too short is refused", entry.result, GWEN_ERROR_INVALID);
    Check::that("and reported", FakeBankingThread::errors.size() == 1);
    Check::that("and not kept", !remembered(ALICE));
    GwenGui::finishCommand();

    // FinTS messages are ISO 8859-1: an umlaut must reach AqBanking as one byte,
    // not as UTF-8's two (found 2026-11-11, "Unable to encode" for a long PIN).
    FakeBankingThread::reset();
    const QString special = QStringLiteral("äöüÄÖÜß!$%&/()=?+#,.-:");
    answers << answer(special);
    entry = askPin(ALICE);
    Check::equal("a PIN with umlauts and special characters is accepted", entry.result, 0);
    Check::equal("and handed over in ISO 8859-1", entry.bytes, special.toLatin1());
    bankSays(ALICE, GWEN_Gui_PasswordStatus_Ok);
    GwenGui::finishCommand();
    entry = askPin(ALICE);
    Check::that("and remembered as such", requests.size() == 1 && entry.bytes == special.toLatin1());
    GwenGui::finishCommand();
    GwenGui::forgetPins();

    FakeBankingThread::reset();
    answers << answer(QStringLiteral("12€45"));
    entry = askPin(ALICE);
    Check::equal("a PIN with a character ISO 8859-1 lacks is refused", entry.result, GWEN_ERROR_INVALID);
    Check::that("and reported", FakeBankingThread::errors.size() == 1);
    Check::that("and not kept", !remembered(ALICE));
    GwenGui::finishCommand();
    GwenGui::forgetPins();
}

// What the overview shows: the PINs kept, with their time left.
void rememberedPinTests()
{
    FakeBankingThread::reset();
    GwenGui::forgetPins();
    fakeTime = 100 * MINUTE;
    Check::that("No PIN is reported as remembered at first", GwenGui::rememberedPins().isEmpty());

    answers << answer(QStringLiteral("12345"));
    askPin(ALICE);
    Check::that("nor one not yet accepted by the bank", GwenGui::rememberedPins().isEmpty());
    bankSays(ALICE, GWEN_Gui_PasswordStatus_Ok);
    Check::that("nor one in use by the running command", GwenGui::rememberedPins().isEmpty());
    GwenGui::finishCommand();
    QMap<QByteArray, qint64> pins = GwenGui::rememberedPins();
    Check::equal("A PIN kept after its command is reported", pins.keys().size(), 1);
    Check::equal("by its token, with 5 minutes left", pins.value(ALICE), 5 * MINUTE);
    fakeTime += 2 * MINUTE;
    Check::equal("then 3", GwenGui::rememberedPins().value(ALICE), 3 * MINUTE);
    fakeTime += 3 * MINUTE;
    GwenGui::forgetExpiredPins();
    Check::that("and none once it is wiped", GwenGui::rememberedPins().isEmpty());

    answers << answer(QStringLiteral("12345"));
    askPin(ALICE);
    bankSays(ALICE, GWEN_Gui_PasswordStatus_Ok);
    GwenGui::finishCommand();
    GwenGui::forgetPins();
    Check::that("Forgetting the PINs leaves none to report", GwenGui::rememberedPins().isEmpty());
    Check::that("and the next command asks again", !remembered(ALICE));
}

void tanTests()
{
    FakeBankingThread::reset();
    answers << answer(QStringLiteral("123456")) << answer(QStringLiteral("654321"));
    Entry entry = askTan("Bitte TAN eingeben");
    Check::that("the TAN is asked for", requests.size() == 1 && requests.first().type == BankingThread::TanInteraction);
    Check::equal("and handed to AqBanking", entry.text, QStringLiteral("123456"));
    entry = askTan("Bitte TAN eingeben");
    Check::that("a TAN is never remembered", requests.size() == 2 && entry.text == QStringLiteral("654321"));

    // The bank's challenge: formatting gets through, anything that could load
    // or link does not, and the rest is escaped.
    FakeBankingThread::reset();
    answers << answer(QStringLiteral("1"));
    askTan("Bitte <b>TAN</b> eingeben:<br/><img src=\"https://evil.example/x.png\"><a href=\"https://evil.example\">hier</a>"
           " 1 < 2 & 3 > 2<p style=\"color:red\">Absatz</p><SCRIPT>alert(1)</SCRIPT>\nZeile");
    Check::equal("the challenge is filtered", requests.value(0).text,
                 QStringLiteral("Bitte <b>TAN</b> eingeben:<br>hier 1 &lt; 2 &amp; 3 &gt; 2<p>Absatz</p>alert(1)<br>Zeile"));

    FakeBankingThread::reset();
    answers << answer(QStringLiteral("1"));
    askTan("Klartext<html><b>Reich</b> formatiert</html>");
    Check::equal("of a text with an HTML rendition, the plain one is shown", requests.value(0).text, QStringLiteral("Klartext"));

    // photoTAN and QR-TAN bring their image along.
    const QByteArray png("\x89PNG\r\n\x1a\nnot really", 18);
    GWEN_DB_NODE *params = GWEN_DB_Group_new("params");
    GWEN_DB_SetIntValue(params, GWEN_DB_FLAGS_OVERWRITE_VARS, "tanMethodId", AB_BANKING_TANMETHOD_PHOTOTAN);
    GWEN_DB_SetBinValue(params, GWEN_DB_FLAGS_OVERWRITE_VARS, "imageData", png.constData(), png.size());
    GWEN_DB_SetCharValue(params, GWEN_DB_FLAGS_OVERWRITE_VARS, "mimeType", "image/png");
    FakeBankingThread::reset();
    answers << answer(QStringLiteral("1"));
    askTan("Bitte scannen", GWEN_Gui_PasswordMethod_OpticalHHD, params);
    Check::that("the photoTAN image is passed on", requests.value(0).image == png);
    Check::equal("with its type", requests.value(0).mimeType, QStringLiteral("image/png"));

    GWEN_DB_SetIntValue(params, GWEN_DB_FLAGS_OVERWRITE_VARS, "tanMethodId", AB_BANKING_TANMETHOD_CHIPTAN_OPTIC);
    GWEN_DB_SetCharValue(params, GWEN_DB_FLAGS_OVERWRITE_VARS, "challenge", FLICKER_CODE);
    FakeBankingThread::reset();
    answers << answer(QStringLiteral("1"));
    entry = askTan("Bitte Flickercode lesen", GWEN_Gui_PasswordMethod_OpticalHHD, params);
    Check::that("a flicker code is not taken for an image", requests.value(0).image.isEmpty());
    Check::equal("but passed on as the frames to show", requests.value(0).flickerCode, QString::fromLatin1(FLICKER_FRAMES));
    Check::equal("and its TAN handed to AqBanking", entry.text, QStringLiteral("1"));
    GWEN_DB_Group_free(params);

    // Whether the bank takes digits only for the TAN, as its parameters say per method.
    params = GWEN_DB_Group_new("params");
    GWEN_DB_SetIntValue(params, GWEN_DB_FLAGS_OVERWRITE_VARS, "tanMethodId", AB_BANKING_TANMETHOD_TEXT);
    GWEN_DB_SetCharValue(params, GWEN_DB_FLAGS_OVERWRITE_VARS, "tanMethod/formatId", "2");
    FakeBankingThread::reset();
    answers << answer(QStringLiteral("1"));
    askTan("Bitte TAN eingeben", GWEN_Gui_PasswordMethod_Text, params);
    Check::that("A numeric TAN method gets the digit keyboard", requests.value(0).digitsOnly);
    GWEN_DB_SetCharValue(params, GWEN_DB_FLAGS_OVERWRITE_VARS, "tanMethod/formatId", "1");
    FakeBankingThread::reset();
    answers << answer(QStringLiteral("a1"));
    askTan("Bitte TAN eingeben", GWEN_Gui_PasswordMethod_Text, params);
    Check::that("an alphanumeric one the whole keyboard", !requests.value(0).digitsOnly);
    GWEN_DB_Group_free(params);
    FakeBankingThread::reset();
    answers << answer(QStringLiteral("a1"));
    askTan("Bitte TAN eingeben");
    Check::that("as does one the bank says nothing about", !requests.value(0).digitsOnly);
}

void flickerCodeTests()
{
    Check::equal("The flicker frames of the specification's example", FlickerCode::frames(QString::fromLatin1(FLICKER_CODE)),
                 QString::fromLatin1(FLICKER_FRAMES));
    Check::equal("lower case hex digits as well",
                 FlickerCode::frames(QString::fromLatin1(FLICKER_CODE).toLower()), QString::fromLatin1(FLICKER_FRAMES));
    Check::equal("An odd number of digits is no code", FlickerCode::frames(QStringLiteral("123")), QString());
    Check::equal("neither is anything but hex digits", FlickerCode::frames(QStringLiteral("12G4")), QString());
    Check::equal("nor nothing", FlickerCode::frames(QString()), QString());
}

void messageTests()
{
    FakeBankingThread::reset();
    answers << FakeBankingThread::choose(1);
    int rv = GWEN_Gui_MessageBox(GWEN_GUI_MSG_FLAGS_TYPE_INFO, "Freigabe",
                                 "Bitte in der App <i>freigeben</i><img src=\"x\">", "Erledigt", "Abbrechen", nullptr, 0);
    Check::equal("a message box returns the button chosen", rv, 1);
    Check::that("it is shown as a message", requests.value(0).type == BankingThread::MessageInteraction);
    Check::equal("with its buttons", requests.value(0).choices.join(QLatin1Char('|')), QStringLiteral("Erledigt|Abbrechen"));
    Check::equal("and its text filtered", requests.value(0).text, QStringLiteral("Bitte in der App <i>freigeben</i>"));

    FakeBankingThread::reset();
    answers << FakeBankingThread::choose(0);
    GWEN_Gui_MessageBox(GWEN_GUI_MSG_FLAGS_TYPE_INFO, "Hinweis", "Text", nullptr, nullptr, nullptr, 0);
    Check::that("a message without buttons gets one", requests.value(0).choices.size() == 1);

    FakeBankingThread::reset();
    rv = GWEN_Gui_MessageBox(GWEN_GUI_MSG_FLAGS_TYPE_INFO, "Freigabe", "Text", "Erledigt", nullptr, nullptr, 0);
    Check::equal("closing a message box aborts", rv, GWEN_ERROR_USER_ABORTED);
}

// AqBanking's request to confirm in the bank's app, as cbox_voptan.c,
// _letUserConfirmApproval(), shows it (untranslated, as here in the C locale).
int askApproval(const char *job, const char *challenge)
{
    const QString plain = QString::fromUtf8(
                "Your approval for this communication with your bank server is required.\n"
                "Please use your smart phone app for this\n"
                "and click \"Approved\" afterwards (%1, user %2 at %3).\n"
                "\n"
                "Message from bank server regarding this process:\n"
                "%4\n").arg(QString::fromLatin1(job), QStringLiteral("tester"), QStringLiteral("12345678"),
                            QString::fromUtf8(challenge));
    const QByteArray text = (plain + QStringLiteral("<html><p>Your approval ...</p></html>")).toUtf8();
    return GWEN_Gui_MessageBox(GWEN_GUI_MSG_FLAGS_TYPE_INFO | GWEN_GUI_MSG_FLAGS_CONFIRM_B1,
                               "Decoupled Mode: Waiting for Approval", text.constData(), "Approved", "Abort", nullptr, 0);
}

void approvalTests()
{
    const QString notYet = QStringLiteral("Your bank has not received your confirmation yet.");
    FakeBankingThread::reset();
    answers << FakeBankingThread::choose(1);
    int rv = askApproval("HKIDN", "Bitte best\xc3\xa4tigen Sie den Vorgang in Ihrer <b>S-pushTAN</b>-App.<img src=\"x\">");
    Check::equal("Confirming in the bank's app goes on as AqBanking's \"Approved\"", rv, 1);
    const FakeBankingThread::Request request = requests.value(0);
    Check::equal("its box gets the app's own title", request.title, QStringLiteral("Confirm in your banking app"));
    Check::equal("and one answer, the app's", request.choices.join(QLatin1Char('|')), QStringLiteral("Confirmed"));
    Check::that("the bank's message is shown, filtered",
                request.text.endsWith(QString::fromUtf8("Your bank says:<br>Bitte best\xc3\xa4tigen Sie den Vorgang in Ihrer <b>S-pushTAN</b>-App.")),
                request.text);
    Check::that("without the FinTS segment and the login", !request.text.contains(QStringLiteral("HKIDN"))
                && !request.text.contains(QStringLiteral("tester")), request.text);
    Check::that("and at first without saying it is not there yet", !request.text.contains(notYet));

    answers << FakeBankingThread::choose(1);
    askApproval("HKIDN", "Bitte freigeben");
    Check::that("Asked the same again, the bank has no confirmation yet", requests.value(1).text.startsWith(notYet),
                requests.value(1).text);
    answers << FakeBankingThread::choose(1);
    askApproval("HKCAZ", "Bitte freigeben");
    Check::that("an approval of another request is a new one", !requests.value(2).text.contains(notYet));
    GwenGui::finishCommand();
    answers << FakeBankingThread::choose(1);
    askApproval("HKCAZ", "Bitte freigeben");
    Check::that("so is the same in the next command", !requests.value(3).text.contains(notYet));

    rv = askApproval("HKIDN", "Bitte freigeben");
    Check::equal("Backing out aborts, as AqBanking's \"Abort\"", rv, GWEN_ERROR_USER_ABORTED);

    FakeBankingThread::reset();
    answers << FakeBankingThread::choose(1);
    GWEN_Gui_MessageBox(GWEN_GUI_MSG_FLAGS_TYPE_INFO, "Decoupled Mode: Waiting for Approval",
                        "Some other wording <b>HKIDN</b><img src=\"x\">", "Approved", "Abort", nullptr, 0);
    Check::that("A text that does not read as expected is shown as AqBanking wrote it, filtered",
                requests.value(0).text.endsWith(QStringLiteral("<br><br>Some other wording <b>HKIDN</b>")), requests.value(0).text);
    GwenGui::finishCommand();
}

void certificateTests()
{
    GWEN_SSLCERTDESCR *cert = GWEN_SslCertDescr_new();
    FakeBankingThread::reset();
    GWEN_SslCertDescr_SetStatusFlags(cert, GWEN_SSL_CERT_FLAGS_OK);
    Check::equal("a certificate without findings is accepted", GWEN_Gui_CheckCert(cert, nullptr, 0), 0);
    Check::that("without asking", requests.isEmpty() && FakeBankingThread::errors.isEmpty());

    GWEN_SslCertDescr_SetStatusFlags(cert, GWEN_SSL_CERT_FLAGS_EXPIRED);
    GWEN_SslCertDescr_SetStatusText(cert, "expired");
    answers << FakeBankingThread::choose(0);
    Check::equal("an expired one is refused", GWEN_Gui_CheckCert(cert, nullptr, 0), GWEN_ERROR_SSL_SECURITY);
    Check::that("without asking either", requests.isEmpty());
    Check::that("and the reason reported", FakeBankingThread::errors.size() == 1 && FakeBankingThread::errors.first().contains(QStringLiteral("expired")),
                FakeBankingThread::errors.join(QLatin1Char('|')));

    GWEN_SslCertDescr_SetStatusFlags(cert, GWEN_SSL_CERT_FLAGS_OK | GWEN_SSL_CERT_FLAGS_EXPIRED);
    Check::equal("so is one with a finding besides OK", GWEN_Gui_CheckCert(cert, nullptr, 0), GWEN_ERROR_SSL_SECURITY);
    GWEN_SslCertDescr_free(cert);
}

}

void runGwenGuiTests()
{
    Check::section("GwenGui");
    BankingThread thread(QStringLiteral("/nonexistent"));
    GWEN_GUI *gui = GwenGui::create(&thread);
    GWEN_Gui_SetGui(gui);
    GwenGui::setClock(fakeClock);

    pinTests();
    rememberedPinTests();
    tanTests();
    flickerCodeTests();
    messageTests();
    approvalTests();
    certificateTests();

    GwenGui::setClock(nullptr);
    GWEN_Gui_SetGui(nullptr);
    GwenGui::destroy(gui);
}
