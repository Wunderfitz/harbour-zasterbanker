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

#include "DebugLog.h"

#include "LogMasker.h"

#include <QAtomicInt>
#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QSysInfo>

#include <aqbanking/banking.h>
#include <gcrypt.h>
#include <gnutls/gnutls.h>
#include <gwenhywfar/gwenhywfar.h>

#ifndef ZB_VERSION
#define ZB_VERSION "unknown"
#endif

namespace {

// About 8 MB: room for a few days of statements, which are the largest part.
// The oldest entries go first.
const qint64 MAX_CHARACTERS = 4 * 1000 * 1000;
// What the list shows of an entry, and what it shows at all: a long statement
// takes seconds to lay out. Copying always takes everything.
const int PREVIEW_LENGTH = 600;
const int SHOWN_LENGTH = 20000;

QAtomicInt enabledFlag(0);
QMutex pendingMutex;
QList<DebugLog::Entry> pending;
bool takeScheduled = false;
DebugLog *instance = nullptr;

QtMessageHandler previousHandler = nullptr;
// Qt's warnings while writing one would come back here.
thread_local bool inMessageHandler = false;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    // qDebug() is only used in debug builds, where it repeats AqBanking's log
    if (type != QtDebugMsg && !inMessageHandler && DebugLog::isEnabled()) {
        inMessageHandler = true;
        const char *level = type == QtInfoMsg ? "info" : (type == QtWarningMsg ? "warning" : "critical");
        DebugLog::write(DebugLog::QtMessage, LogMasker::text(QStringLiteral("%1: %2").arg(QLatin1String(level), message)));
        inMessageHandler = false;
    }
    if (previousHandler) {
        previousHandler(type, context, message);
    }
}

QString version(void (*get)(int *, int *, int *, int *))
{
    int major = 0;
    int minor = 0;
    int patchlevel = 0;
    int build = 0;
    get(&major, &minor, &patchlevel, &build);
    return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patchlevel);
}

// e.g. NAME="Jolla C2" from /etc/hw-release, if Sailjail lets the app read it
QString hardware()
{
    QFile file(QStringLiteral("/etc/hw-release"));
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("unknown");
    }
    QStringList details;
    for (const QByteArray &line : file.readAll().split('\n')) {
        if (line.startsWith("NAME=") || line.startsWith("VERSION_ID=")) {
            details.append(QString::fromUtf8(line.mid(line.indexOf('=') + 1)).remove(QLatin1Char('"')));
        }
    }
    return details.isEmpty() ? QStringLiteral("unknown") : details.join(QLatin1Char(' '));
}

}

DebugLog::DebugLog(QObject *parent)
    : QAbstractListModel(parent)
    , characters(0)
    , dropped(0)
{
    QMutexLocker locker(&pendingMutex);
    instance = this;
}

DebugLog::~DebugLog()
{
    enabledFlag.store(0);
    QMutexLocker locker(&pendingMutex);
    instance = nullptr;
    pending.clear();
}

bool DebugLog::isEnabled()
{
    return enabledFlag.load() != 0;
}

void DebugLog::write(Source source, const QString &text)
{
    if (!isEnabled()) {
        return;
    }
    Entry entry;
    entry.time = QDateTime::currentDateTime();
    entry.source = source;
    entry.text = text;
    QMutexLocker locker(&pendingMutex);
    if (!instance) {
        return;
    }
    pending.append(entry);
    // one call for whatever arrives until the UI thread gets round to it
    if (!takeScheduled) {
        takeScheduled = true;
        QMetaObject::invokeMethod(instance, "takePending", Qt::QueuedConnection);
    }
}

void DebugLog::writeFinTsMessage(int received, const char *data, unsigned int size)
{
    if (!isEnabled() || !data) {
        return;
    }
    // no copy: the message holds the PIN and the TAN
    const QByteArray message = QByteArray::fromRawData(data, static_cast<int>(size));
    write(received ? Received : Sent, LogMasker::finTsMessage(message));
}

void DebugLog::installMessageHandler()
{
    previousHandler = qInstallMessageHandler(messageHandler);
}

bool DebugLog::enabled() const
{
    return isEnabled();
}

void DebugLog::setEnabled(bool enabled)
{
    if (enabled == isEnabled()) {
        return;
    }
    enabledFlag.store(enabled ? 1 : 0);
    if (enabled) {
        writeSystemInformation();
    } else {
        {
            QMutexLocker locker(&pendingMutex);
            pending.clear();
        }
        beginResetModel();
        entries.clear();
        characters = 0;
        dropped = 0;
        endResetModel();
        emit countChanged();
    }
    emit enabledChanged();
}

int DebugLog::count() const
{
    return entries.size();
}

void DebugLog::takePending()
{
    QList<Entry> arrived;
    {
        QMutexLocker locker(&pendingMutex);
        arrived.swap(pending);
        takeScheduled = false;
    }
    if (arrived.isEmpty() || !isEnabled()) {
        return;
    }
    beginInsertRows(QModelIndex(), entries.size(), entries.size() + arrived.size() - 1);
    for (const Entry &entry : arrived) {
        characters += entry.text.size();
    }
    entries.append(arrived);
    endInsertRows();

    int surplus = 0;
    qint64 remaining = characters;
    while (remaining > MAX_CHARACTERS && surplus < entries.size() - 1) {
        remaining -= entries.at(surplus).text.size();
        surplus++;
    }
    if (surplus > 0) {
        beginRemoveRows(QModelIndex(), 0, surplus - 1);
        entries.erase(entries.begin(), entries.begin() + surplus);
        characters = remaining;
        dropped += surplus;
        endRemoveRows();
    }
    emit countChanged();
}

int DebugLog::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : entries.size();
}

QVariant DebugLog::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries.size()) {
        return QVariant();
    }
    const Entry &entry = entries.at(index.row());
    switch (role) {
    case TimeRole:
        return entry.time.toString(QStringLiteral("HH:mm:ss.zzz"));
    case SourceRole:
        return sourceName(entry.source);
    case PreviewRole:
        return entry.text.size() > PREVIEW_LENGTH ? entry.text.left(PREVIEW_LENGTH) + QChar(0x2026) : entry.text;
    case ShortenedRole:
        return entry.text.size() > PREVIEW_LENGTH;
    case TextRole:
        return entry.text.left(SHOWN_LENGTH);
    case CompleteRole:
        return entry.text.size() <= SHOWN_LENGTH;
    case LengthRole:
        return entry.text.size();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> DebugLog::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(TimeRole, "time");
    roles.insert(SourceRole, "source");
    roles.insert(PreviewRole, "preview");
    roles.insert(ShortenedRole, "shortened");
    roles.insert(TextRole, "text");
    roles.insert(CompleteRole, "complete");
    roles.insert(LengthRole, "length");
    return roles;
}

QString DebugLog::text() const
{
    QString result = QStringLiteral("Zaster Banker debug log, %1 entries").arg(entries.size());
    if (dropped > 0) {
        result += QStringLiteral(" (the oldest %1 dropped to save memory)").arg(dropped);
    }
    result += QLatin1Char('\n');
    result.reserve(static_cast<int>(characters + entries.size() * 40 + result.size()));
    for (const Entry &entry : entries) {
        result += line(entry);
        result += QLatin1Char('\n');
    }
    return result;
}

void DebugLog::copyToClipboard() const
{
    QGuiApplication::clipboard()->setText(text());
}

QString DebugLog::sourceName(Source source)
{
    switch (source) {
    case App:
        return QStringLiteral("app");
    case Sent:
        return QStringLiteral("sent");
    case Received:
        return QStringLiteral("received");
    case Library:
        return QStringLiteral("aqbanking");
    case Progress:
        return QStringLiteral("progress");
    case QtMessage:
        return QStringLiteral("qt");
    }
    return QString();
}

QString DebugLog::line(const Entry &entry)
{
    return entry.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")) + QLatin1Char(' ') + sourceName(entry.source)
            + QStringLiteral(": ") + entry.text;
}

// What a problem report needs to know first; nothing personal in it.
void DebugLog::writeSystemInformation()
{
    const QDateTime now = QDateTime::currentDateTime();
    write(App, QStringLiteral("Debug logging started. Personal data is masked: one * per character, PIN and TAN as [PIN] and [TAN]."));
    write(App, QStringLiteral("Zaster Banker %1, Qt %2 (built with %3)")
          .arg(QLatin1String(ZB_VERSION), QLatin1String(qVersion()), QLatin1String(QT_VERSION_STR)));
    write(App, QStringLiteral("System: %1, %2 %3, %4; device: %5")
          .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion(),
               QSysInfo::currentCpuArchitecture(), hardware()));
    write(App, QStringLiteral("Libraries: AqBanking %1, Gwenhywfar %2, GnuTLS %3, libgcrypt %4")
          .arg(version(AB_Banking_GetVersion), version(GWEN_Version), QLatin1String(gnutls_check_version(nullptr)),
               QLatin1String(gcry_check_version(nullptr))));
    write(App, QStringLiteral("Locale %1, LANG %2, time zone %3 (UTC%4%5 minutes)")
          .arg(QLocale::system().name(), QString::fromLocal8Bit(qgetenv("LANG")), now.timeZoneAbbreviation(),
               now.offsetFromUtc() >= 0 ? QStringLiteral("+") : QString())
          .arg(now.offsetFromUtc() / 60));
}
