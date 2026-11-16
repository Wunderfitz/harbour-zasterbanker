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

#include "GwenGui.h"

#include "BankingThread.h"
#include "DebugLog.h"
#include "FlickerCode.h"
#include "LogMasker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QRegExp>
#include <QRegularExpression>

#include <aqbanking/banking.h>
#include <gwenhywfar/db.h>
#include <gwenhywfar/gui_be.h>
#include <gwenhywfar/gwentime.h>
#include <gwenhywfar/i18n.h>
#include <gwenhywfar/logger.h>
#include <gwenhywfar/ssl_cert_descr.h>

#include <cstring>
#include <ctime>

namespace {

/*
 * How long a PIN is remembered after the last command that used it. Each
 * command for the same bank access starts the time again.
 */
const qint64 pinLifetime = 5 * 60 * 1000;

/*
 * A PIN the user entered. Kept beyond the command it was entered in only if
 * the bank accepted it: AqBanking reports that through setPasswordStatus().
 * A PIN that never got that far may be mistyped, and using it again without
 * asking would cost one of the few attempts before the bank locks the access.
 */
struct RememberedPin {
    QByteArray pin;
    bool accepted;
    bool usedByCommand;
    qint64 lastUsed;
};

BankingThread *bankingThread = nullptr;
QMap<QByteArray, RememberedPin> pins;
uint32_t nextBoxId = 1;

// What the last request to confirm in the bank's app during this command was
// about (FinTS segment and login, or AqBanking's whole text): the same again
// means the bank has no confirmation yet (3956).
QString lastApproval;

// AqBanking's message box for app approval (decoupled TAN methods), as
// cbox_voptan.c, _letUserConfirmApproval(), writes it. Compare them when
// upgrading AqBanking. Its plain text is not translated (AqBanking 6.9.2), names
// the FinTS segment and the login, and its "Approved" button is a fuzzy entry
// and stays English, so the app words the box itself.
const char APPROVAL_DOMAIN[] = "aqbanking";
const char APPROVAL_TITLE[] = "Decoupled Mode: Waiting for Approval";
const char APPROVAL_TEXT[] = "Your approval for this communication with your bank server is required.\n"
                             "Please use your smart phone app for this\n"
                             "and click \"Approved\" afterwards (%s, user %s at %s).\n"
                             "\n"
                             "Message from bank server regarding this process:\n"
                             "%s\n";

// For the debug log; LogMasker takes out the login in AqBanking's token
// names ("PIN_<bank code>_<login>") and in its texts.
void log(const QString &text)
{
    DebugLog::write(DebugLog::App, text);
}

QString maskedToken(const QByteArray &token)
{
    return LogMasker::text(QString::fromUtf8(token));
}

const char *passwordStatusName(GWEN_GUI_PASSWORD_STATUS status)
{
    switch (status) {
    case GWEN_Gui_PasswordStatus_Bad: return "rejected";
    case GWEN_Gui_PasswordStatus_Unknown: return "unknown";
    case GWEN_Gui_PasswordStatus_Ok: return "accepted";
    case GWEN_Gui_PasswordStatus_Used: return "used";
    case GWEN_Gui_PasswordStatus_Unused: return "unused";
    case GWEN_Gui_PasswordStatus_Remove: return "to be removed";
    }
    return "?";
}

void wipe(QByteArray &data)
{
    if (!data.isEmpty()) {
        std::memset(data.data(), 0, static_cast<size_t>(data.size()));
    }
    data.clear();
}

// FinTS messages are ISO 8859-1, and AqBanking puts PIN and TAN into them as it
// gets them. The dialog's text arrives as UTF-8, where an umlaut or "ß" is two
// bytes: the bank would see a different PIN, and a long one would no longer fit
// the field (AqBanking then fails with "Unable to encode"). Converts the text in
// place; false, with the text wiped, if it has a character ISO 8859-1 lacks.
bool toLatin1(QByteArray &text)
{
    QString decoded = QString::fromUtf8(text);
    bool representable = true;
    for (const QChar c : decoded) {
        if (c.unicode() > 0xff) {
            representable = false;
            break;
        }
    }
    wipe(text);
    if (representable) {
        text = decoded.toLatin1();
    }
    decoded.fill(QChar(0));
    return representable;
}

/*
 * Milliseconds since boot, including the time the device spent suspended:
 * five minutes with the screen off count as five minutes. QElapsedTimer and
 * CLOCK_MONOTONIC stand still during suspend.
 */
qint64 bootTime()
{
    struct timespec time;
    clock_gettime(CLOCK_BOOTTIME, &time);
    return static_cast<qint64>(time.tv_sec) * 1000 + time.tv_nsec / 1000000;
}

qint64 (*now)() = bootTime;

void forgetPin(QMap<QByteArray, RememberedPin>::iterator pin)
{
    wipe(pin.value().pin);
    pins.erase(pin);
}

/*
 * Gwenhywfar texts may carry an HTML rendition after the plain one, as in
 * "Plain text<html>Rich text</html>". Only the plain part is used; a text that
 * is nothing but HTML loses its tags.
 */
QString plainText(const char *text)
{
    if (!text) {
        return QString();
    }
    QString result = QString::fromUtf8(text);
    const int html = result.indexOf(QStringLiteral("<html>"), 0, Qt::CaseInsensitive);
    if (html > 0) {
        result.truncate(html);
    } else if (html == 0) {
        result.remove(QRegExp(QStringLiteral("<[^>]*>")));
    }
    return result.trimmed();
}

/*
 * For the dialogs that show the bank's own words, the TAN challenge above all,
 * which AqBanking passes on as received, formatting tags included. QML shows
 * these as Text.StyledText. That also knows <img>, which would load from any
 * address, and <a>, and the bank's text is not to be trusted with either: only
 * text formatting gets through, without attributes. Other tags are dropped and
 * everything else is escaped, so that a stray "<" stays a "<".
 */
QString styledText(const char *text)
{
    static const QStringList allowedTags = {
        QStringLiteral("b"), QStringLiteral("br"), QStringLiteral("i"), QStringLiteral("li"), QStringLiteral("ol"),
        QStringLiteral("p"), QStringLiteral("strong"), QStringLiteral("u"), QStringLiteral("ul")
    };
    const QString source = plainText(text);
    QRegExp tag(QStringLiteral("<(/?)([a-z][a-z0-9]*)\\b[^<>]*>"), Qt::CaseInsensitive);
    QString result;
    int position = 0;
    int match;
    while ((match = tag.indexIn(source, position)) >= 0) {
        result += source.mid(position, match - position).toHtmlEscaped();
        if (allowedTags.contains(tag.cap(2).toLower())) {
            result += QLatin1Char('<') + tag.cap(1) + tag.cap(2) + QLatin1Char('>');
        }
        position = match + tag.matchedLength();
    }
    result += source.mid(position).toHtmlEscaped();
    // AqBanking's own line breaks; StyledText takes them for mere spaces.
    result.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return result;
}

int GWENHYWFAR_CB getPassword(GWEN_GUI *, uint32_t flags, const char *token, const char *title, const char *text,
                              char *buffer, int minLen, int maxLen, GWEN_GUI_PASSWORD_METHOD methodId,
                              GWEN_DB_NODE *methodParams, uint32_t)
{
    const bool isTan = flags & GWEN_GUI_INPUT_FLAGS_TAN;
    const QByteArray tokenName(token ? token : "");

    if (!isTan && !tokenName.isEmpty()) {
        GwenGui::forgetExpiredPins();
        auto remembered = pins.find(tokenName);
        if (remembered != pins.end() && remembered.value().pin.size() < maxLen) {
            const QByteArray &pin = remembered.value().pin;
            std::memcpy(buffer, pin.constData(), static_cast<size_t>(pin.size()));
            buffer[pin.size()] = 0;
            remembered.value().usedByCommand = true;
            remembered.value().lastUsed = now();
            log(QStringLiteral("PIN for %1 taken from memory").arg(maskedToken(tokenName)));
            return 0;
        }
    }

    QByteArray image;
    QString mimeType;
    QString flicker;
    // The bank says per TAN method whether its TANs are numeric (HITANS,
    // "Erlaubtes Format": 1 alphanumeric, 2 numeric); FinTS itself allows any.
    const bool digitsOnly = isTan && methodParams
            && qstrcmp(GWEN_DB_GetCharValue(methodParams, "tanMethod/formatId", 0, ""), "2") == 0;
    if (isTan && (methodId & GWEN_Gui_PasswordMethod_Mask) == GWEN_Gui_PasswordMethod_OpticalHHD && methodParams) {
        const int tanMethod = GWEN_DB_GetIntValue(methodParams, "tanMethodId", 0, AB_BANKING_TANMETHOD_TEXT);
        if (tanMethod == AB_BANKING_TANMETHOD_CHIPTAN_OPTIC) {
            // AqBanking's code to send, as hex digits; the frames to show, worked
            // out here on the banking thread.
            flicker = FlickerCode::frames(QString::fromLatin1(GWEN_DB_GetCharValue(methodParams, "challenge", 0, "")));
            if (flicker.isEmpty()) {
                qWarning() << "The flicker code could not be read";
            }
        } else if (tanMethod == AB_BANKING_TANMETHOD_PHOTOTAN || tanMethod == AB_BANKING_TANMETHOD_CHIPTAN_QR) {
            unsigned int size = 0;
            const void *data = GWEN_DB_GetBinValue(methodParams, "imageData", 0, nullptr, 0, &size);
            if (data && size > 0) {
                image = QByteArray(static_cast<const char *>(data), static_cast<int>(size));
                mimeType = QString::fromUtf8(GWEN_DB_GetCharValue(methodParams, "mimeType", 0, ""));
            }
        }
    }

    if (DebugLog::isEnabled()) {
        if (isTan) {
            log(QStringLiteral("TAN asked for: TAN method %1, input mechanism %2, password method 0x%3, format %4, %5 to %6 characters")
                .arg(methodParams ? GWEN_DB_GetIntValue(methodParams, "tanMethodId", 0, -1) : -1)
                .arg(methodParams ? GWEN_DB_GetIntValue(methodParams, "tanMethod/tanInputMechanism", 0, -1) : -1)
                .arg(static_cast<uint>(methodId), 0, 16)
                .arg(QString::fromUtf8(methodParams ? GWEN_DB_GetCharValue(methodParams, "tanMethod/formatId", 0, "") : ""))
                .arg(minLen).arg(maxLen - 1));
        } else {
            log(QStringLiteral("PIN asked for %1, %2 to %3 characters").arg(maskedToken(tokenName)).arg(minLen).arg(maxLen - 1));
        }
    }
    BankingThread::Response response = bankingThread->requestInteraction(
                isTan ? BankingThread::TanInteraction : BankingThread::PinInteraction,
                plainText(title), isTan ? styledText(text) : plainText(text), QStringList(), image, mimeType,
                QString(), flicker, digitsOnly);
    if (!response.accepted) {
        return GWEN_ERROR_USER_ABORTED;
    }
    if (!toLatin1(response.text)) {
        bankingThread->reportError(isTan ? QCoreApplication::translate("GwenGui", "The TAN contains a character that cannot be sent to the bank.")
                                         : QCoreApplication::translate("GwenGui", "The PIN contains a character that cannot be sent to the bank."));
        return GWEN_ERROR_INVALID;
    }
    if (response.text.size() < minLen || response.text.size() >= maxLen) {
        wipe(response.text);
        bankingThread->reportError(isTan ? QCoreApplication::translate("GwenGui", "The TAN has an invalid length.")
                                         : QCoreApplication::translate("GwenGui", "The PIN has an invalid length."));
        return GWEN_ERROR_INVALID;
    }
    std::memcpy(buffer, response.text.constData(), static_cast<size_t>(response.text.size()));
    buffer[response.text.size()] = 0;
    if (!isTan && !tokenName.isEmpty()) {
        auto previous = pins.find(tokenName);
        if (previous != pins.end()) {
            forgetPin(previous);
        }
        RememberedPin pin;
        pin.pin = QByteArray(response.text.constData(), response.text.size());
        pin.accepted = false;
        pin.usedByCommand = true;
        pin.lastUsed = now();
        pins.insert(tokenName, pin);
    }
    wipe(response.text);
    return 0;
}

int GWENHYWFAR_CB setPasswordStatus(GWEN_GUI *, const char *token, const char *, GWEN_GUI_PASSWORD_STATUS status, uint32_t)
{
    if (!token) {
        return 0;
    }
    log(QStringLiteral("AqBanking: PIN for %1 %2").arg(maskedToken(QByteArray(token)), QLatin1String(passwordStatusName(status))));
    auto pin = pins.find(QByteArray(token));
    if (pin == pins.end()) {
        return 0;
    }
    if (status == GWEN_Gui_PasswordStatus_Bad || status == GWEN_Gui_PasswordStatus_Remove) {
        forgetPin(pin);
    } else if (status == GWEN_Gui_PasswordStatus_Ok) {
        pin.value().accepted = true;
    }
    return 0;
}

/*
 * Whether the title is that of AqBanking's box for app approval, in whatever
 * language AqBanking shows it; if so, the bank's message in it, unfiltered,
 * goes to *bankMessage, what it is about to *subject, and *understood says
 * whether the text read as expected.
 */
bool isApprovalRequest(const char *title, const char *text, QString *bankMessage, QString *subject, bool *understood)
{
    const QString heading = QString::fromUtf8(title ? title : "");
    if (heading != QString::fromUtf8(GWEN_I18N_Translate(APPROVAL_DOMAIN, APPROVAL_TITLE))
            && heading != QLatin1String(APPROVAL_TITLE)) {
        return false;
    }
    // AqBanking's text, plain part, laid over its own template: the last of its
    // four values is the bank's message.
    QString plain = QString::fromUtf8(text ? text : "");
    const int html = plain.indexOf(QStringLiteral("<html>"), 0, Qt::CaseInsensitive);
    if (html >= 0) {
        plain.truncate(html);
    }
    bankMessage->clear();
    *subject = QStringLiteral("text:") + plain;
    *understood = false;
    for (const char *source : { GWEN_I18N_Translate(APPROVAL_DOMAIN, APPROVAL_TEXT), APPROVAL_TEXT }) {
        const QStringList parts = QString::fromUtf8(source).split(QStringLiteral("%s"));
        QString pattern;
        for (int i = 0; i < parts.size(); i++) {
            pattern += QRegularExpression::escape(parts.at(i));
            if (i < parts.size() - 1) {
                pattern += QStringLiteral("(.*)");
            }
        }
        const QRegularExpressionMatch match = QRegularExpression(QLatin1Char('^') + pattern + QLatin1Char('$'),
                                                                 QRegularExpression::DotMatchesEverythingOption).match(plain);
        if (match.hasMatch() && match.lastCapturedIndex() == 4) {
            *bankMessage = match.captured(4).trimmed();
            *subject = QStringLiteral("job:") + match.captured(1) + QLatin1Char('/') + match.captured(2);
            *understood = true;
            break;
        }
    }
    return true;
}

int GWENHYWFAR_CB messageBox(GWEN_GUI *, uint32_t, const char *title, const char *text,
                             const char *b1, const char *b2, const char *b3, uint32_t)
{
    QString bankMessage;
    QString subject;
    bool understood = false;
    if (isApprovalRequest(title, text, &bankMessage, &subject, &understood)) {
        // Asked the same again within the command: the bank said it has no
        // confirmation yet (3956), and AqBanking asks once more.
        QString message;
        const bool again = !lastApproval.isNull() && subject == lastApproval;
        lastApproval = subject;
        log(QStringLiteral("App approval asked for (again: %1, text as expected: %2)")
            .arg(again ? QStringLiteral("yes") : QStringLiteral("no"), understood ? QStringLiteral("yes") : QStringLiteral("no")));
        if (again) {
            message = QCoreApplication::translate("GwenGui", "Your bank has not received your confirmation yet.")
                    + QStringLiteral("<br><br>");
        }
        message += QCoreApplication::translate("GwenGui", "Your bank asks you to confirm this in its app. "
                                                          "Confirm there, then come back and tap \"Confirmed\".");
        if (!understood) {
            // not as expected: AqBanking's own text, rather than nothing
            const QString aqbankingText = styledText(text);
            if (!aqbankingText.isEmpty()) {
                message += QStringLiteral("<br><br>") + aqbankingText;
            }
        } else if (!bankMessage.isEmpty()) {
            message += QStringLiteral("<br><br>") + QCoreApplication::translate("GwenGui", "Your bank says:")
                    + QStringLiteral("<br>") + styledText(bankMessage.toUtf8().constData());
        }
        // One answer, the dialog's accept; backing out cancels, as AqBanking's "Abort".
        const BankingThread::Response response = bankingThread->requestInteraction(
                    BankingThread::MessageInteraction,
                    QCoreApplication::translate("GwenGui", "Confirm in your banking app"), message,
                    QStringList() << QCoreApplication::translate("GwenGui", "Confirmed"));
        if (!response.accepted) {
            return GWEN_ERROR_USER_ABORTED;
        }
        // AqBanking goes on with 1 only, its "Approved"
        return response.choice == 1 ? 1 : GWEN_ERROR_USER_ABORTED;
    }

    QStringList buttons;
    for (const char *button : { b1, b2, b3 }) {
        if (!button) {
            break;
        }
        buttons.append(plainText(button));
    }
    if (buttons.isEmpty()) {
        buttons.append(QCoreApplication::translate("GwenGui", "OK"));
    }
    const BankingThread::Response response = bankingThread->requestInteraction(
                BankingThread::MessageInteraction, plainText(title), styledText(text), buttons);
    if (!response.accepted) {
        return GWEN_ERROR_USER_ABORTED;
    }
    return response.choice;
}

int GWENHYWFAR_CB inputBox(GWEN_GUI *, uint32_t, const char *title, const char *text,
                           char *buffer, int minLen, int maxLen, uint32_t)
{
    BankingThread::Response response = bankingThread->requestInteraction(
                BankingThread::TextInteraction, plainText(title), plainText(text));
    if (!response.accepted) {
        return GWEN_ERROR_USER_ABORTED;
    }
    if (response.text.size() < minLen || response.text.size() >= maxLen) {
        wipe(response.text);
        return GWEN_ERROR_INVALID;
    }
    std::memcpy(buffer, response.text.constData(), static_cast<size_t>(response.text.size()));
    buffer[response.text.size()] = 0;
    wipe(response.text);
    return 0;
}

uint32_t GWENHYWFAR_CB showBox(GWEN_GUI *, uint32_t, const char *title, const char *text, uint32_t)
{
    const QString message = plainText(text);
    DebugLog::write(DebugLog::Progress, LogMasker::text(QStringLiteral("box: %1 - %2").arg(plainText(title), message)));
    bankingThread->reportProgress(message.isEmpty() ? plainText(title) : message);
    return nextBoxId++;
}

void GWENHYWFAR_CB hideBox(GWEN_GUI *, uint32_t)
{
}

uint32_t GWENHYWFAR_CB progressStart(GWEN_GUI *, uint32_t, const char *title, const char *, uint64_t, uint32_t)
{
    const QString message = plainText(title);
    DebugLog::write(DebugLog::Progress, LogMasker::text(QStringLiteral("start: ") + message));
    if (!message.isEmpty()) {
        bankingThread->reportProgress(message);
    }
    return nextBoxId++;
}

int GWENHYWFAR_CB progressAdvance(GWEN_GUI *, uint32_t, uint64_t)
{
    return bankingThread->isCancelled() ? GWEN_ERROR_USER_ABORTED : 0;
}

int GWENHYWFAR_CB progressSetTotal(GWEN_GUI *, uint32_t, uint64_t)
{
    return 0;
}

int GWENHYWFAR_CB progressLog(GWEN_GUI *, uint32_t, GWEN_LOGGER_LEVEL level, const char *text)
{
    const QString message = plainText(text);
    // every level: AqBanking's info lines say what it is doing
    DebugLog::write(DebugLog::Progress, LogMasker::text(QStringLiteral("%1: %2").arg(QLatin1String(GWEN_Logger_Level2Name(level)), message)));
    if (level <= GWEN_LoggerLevel_Error) {
        bankingThread->reportError(message);
    } else if (level <= GWEN_LoggerLevel_Notice) {
        bankingThread->reportProgress(message);
    }
    return bankingThread->isCancelled() ? GWEN_ERROR_USER_ABORTED : 0;
}

int GWENHYWFAR_CB progressEnd(GWEN_GUI *, uint32_t)
{
    return 0;
}

/*
 * Gwenhywfar has already verified the chain against the system's CA bundle
 * and the host name. The stock implementation would ask the user what to do
 * with a certificate that failed; a banking app has nothing to ask there.
 * Only a certificate without any finding is accepted.
 */
int GWENHYWFAR_CB checkCert(GWEN_GUI *, const GWEN_SSLCERTDESCR *cert, GWEN_SYNCIO *, uint32_t)
{
    const uint32_t status = GWEN_SslCertDescr_GetStatusFlags(cert);
    if (DebugLog::isEnabled()) {
        const GWEN_TIME *notAfter = GWEN_SslCertDescr_GetNotAfter(cert);
        log(QStringLiteral("Server certificate for \"%1\" (%2), valid until %3, SHA-1 %4: status 0x%5 %6")
            .arg(QString::fromUtf8(GWEN_SslCertDescr_GetCommonName(cert)), QString::fromUtf8(GWEN_SslCertDescr_GetOrganizationName(cert)),
                 notAfter ? QDateTime::fromTime_t(static_cast<uint>(GWEN_Time_toTime_t(notAfter))).toUTC().toString(Qt::ISODate) : QStringLiteral("?"),
                 QString::fromUtf8(GWEN_SslCertDescr_GetFingerPrintSha1(cert)))
            .arg(status, 0, 16).arg(QString::fromUtf8(GWEN_SslCertDescr_GetStatusText(cert))));
    }
    if (status == GWEN_SSL_CERT_FLAGS_OK) {
        return 0;
    }
    const QString details = QString::fromUtf8(GWEN_SslCertDescr_GetStatusText(cert));
    bankingThread->reportError(QCoreApplication::translate("GwenGui", "The bank's server could not prove its identity, the connection was refused. %1")
                               .arg(details).trimmed());
    return GWEN_ERROR_SSL_SECURITY;
}

/*
 * Gwenhywfar and AqBanking log to stderr, and at their more talkative levels
 * also what they are sending and receiving. In release builds nothing of it
 * leaves the app.
 */
int GWENHYWFAR_CB logHook(GWEN_GUI *, const char *domain, GWEN_LOGGER_LEVEL priority, const char *text)
{
    if (DebugLog::isEnabled()) {
        DebugLog::write(DebugLog::Library, LogMasker::text(QStringLiteral("%1 %2: %3").arg(QString::fromUtf8(domain ? domain : ""),
                                                                                          QLatin1String(GWEN_Logger_Level2Name(priority)),
                                                                                          QString::fromUtf8(text ? text : "").trimmed())));
    }
#ifdef QT_DEBUG
    qDebug() << "[" << domain << priority << "]" << text;
#else
    Q_UNUSED(domain)
    Q_UNUSED(priority)
    Q_UNUSED(text)
#endif
    return 1;
}

}

namespace GwenGui
{

GWEN_GUI *create(BankingThread *thread)
{
    bankingThread = thread;
    GWEN_GUI *gui = GWEN_Gui_new();
    GWEN_Gui_SetGetPasswordFn(gui, getPassword);
    GWEN_Gui_SetSetPasswordStatusFn(gui, setPasswordStatus);
    GWEN_Gui_SetMessageBoxFn(gui, messageBox);
    GWEN_Gui_SetInputBoxFn(gui, inputBox);
    GWEN_Gui_SetShowBoxFn(gui, showBox);
    GWEN_Gui_SetHideBoxFn(gui, hideBox);
    GWEN_Gui_SetProgressStartFn(gui, progressStart);
    GWEN_Gui_SetProgressAdvanceFn(gui, progressAdvance);
    GWEN_Gui_SetProgressSetTotalFn(gui, progressSetTotal);
    GWEN_Gui_SetProgressLogFn(gui, progressLog);
    GWEN_Gui_SetProgressEndFn(gui, progressEnd);
    GWEN_Gui_SetCheckCertFn(gui, checkCert);
    GWEN_Gui_SetLogHookFn(gui, logHook);
    return gui;
}

void destroy(GWEN_GUI *gui)
{
    forgetPins();
    GWEN_Gui_free(gui);
    bankingThread = nullptr;
}

void finishCommand()
{
    lastApproval.clear();
    const qint64 finished = now();
    for (auto pin = pins.begin(); pin != pins.end();) {
        if (!pin.value().accepted) {
            wipe(pin.value().pin);
            pin = pins.erase(pin);
            continue;
        }
        // The time starts when the command ends, not when it first needed the
        // PIN: waiting for a TAN does not use up the five minutes.
        if (pin.value().usedByCommand) {
            pin.value().lastUsed = finished;
            pin.value().usedByCommand = false;
        }
        ++pin;
    }
}

qint64 forgetExpiredPins()
{
    const qint64 current = now();
    qint64 next = -1;
    for (auto pin = pins.begin(); pin != pins.end();) {
        const qint64 remaining = pin.value().lastUsed + pinLifetime - current;
        if (remaining <= 0) {
            wipe(pin.value().pin);
            pin = pins.erase(pin);
            continue;
        }
        if (next < 0 || remaining < next) {
            next = remaining;
        }
        ++pin;
    }
    return next;
}

void setClock(qint64 (*clock)())
{
    now = clock ? clock : bootTime;
}

QMap<QByteArray, qint64> rememberedPins()
{
    QMap<QByteArray, qint64> result;
    const qint64 current = now();
    for (auto pin = pins.constBegin(); pin != pins.constEnd(); ++pin) {
        // A PIN in use by the running command, or not yet accepted, is not kept yet.
        if (pin.value().accepted && !pin.value().usedByCommand) {
            result.insert(pin.key(), qMax<qint64>(0, pin.value().lastUsed + pinLifetime - current));
        }
    }
    return result;
}

void forgetPins()
{
    for (auto pin = pins.begin(); pin != pins.end(); ++pin) {
        wipe(pin.value().pin);
    }
    pins.clear();
}

}
