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

#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>

/**
 * The debug log, for problem reports: every FinTS message sent and received,
 * what the app does, AqBanking's own log, Qt's warnings - each with the time,
 * and all of it masked (LogMasker). Exposed as the "debugLog" context property,
 * a list model for DebugLogPage.
 *
 * In memory only, never written anywhere: it is gone when the app ends, and
 * cleared when debug logging is switched off. While it is off, nothing is
 * recorded. Whether it is on is the user's setting (AppSettings), applied at
 * start.
 *
 * write() is thread-safe; entries reach the model on the UI thread, a batch at
 * a time. Masking is the writer's job, so that it costs the writing thread
 * (the banking thread, mostly), not the UI.
 */
class DebugLog : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Source {
        App,
        Sent,
        Received,
        Library,
        Progress,
        QtMessage
    };

    enum Roles {
        TimeRole = Qt::UserRole + 1,
        SourceRole,
        // the text, cut short for the list
        PreviewRole,
        // whether PreviewRole is
        ShortenedRole,
        // the text as far as the page shows it at all
        TextRole,
        // whether TextRole has all of it
        CompleteRole,
        LengthRole
    };

    struct Entry {
        QDateTime time;
        Source source;
        QString text;
    };

    explicit DebugLog(QObject *parent = nullptr);
    ~DebugLog();

    /** Any thread: whether anything is recorded. Cheap. */
    static bool isEnabled();
    /** Any thread: records text, if enabled. It must be masked already. */
    static void write(Source source, const QString &text);
    /** Any thread; AqBanking's hook: a FinTS message as it went over the wire, masked here. */
    static void writeFinTsMessage(int received, const char *data, unsigned int size);
    /** Has Qt's warnings recorded as well, while enabled. */
    static void installMessageHandler();

    bool enabled() const;
    /** Starts with what the app runs on; off, the log is cleared. */
    void setEnabled(bool enabled);
    int count() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** The whole log as text, one entry per line (a FinTS message's statements may take several). */
    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void copyToClipboard() const;

signals:
    void enabledChanged();
    void countChanged();

private slots:
    void takePending();

private:
    void writeSystemInformation();
    static QString sourceName(Source source);
    static QString line(const Entry &entry);

    QList<Entry> entries;
    qint64 characters;
    int dropped;
};

#endif // DEBUGLOG_H
