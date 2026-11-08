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

#include "FakeBankingThread.h"

namespace FakeBankingThread
{
QList<Request> requests;
QList<BankingThread::Response> answers;
QStringList errors;
QStringList progress;

void reset()
{
    requests.clear();
    answers.clear();
    errors.clear();
    progress.clear();
}

BankingThread::Response answer(const QString &text)
{
    BankingThread::Response response;
    response.accepted = true;
    response.text = text.toUtf8();
    response.choice = 0;
    return response;
}

BankingThread::Response choose(int choice)
{
    BankingThread::Response response;
    response.accepted = true;
    response.choice = choice;
    return response;
}
}

// Only what GwenGui calls, and what a QThread subclass must have.

BankingThread::BankingThread(const QString &dataDirectory, QObject *parent)
    : QThread(parent)
    , dataDirectory(dataDirectory)
{
}

BankingThread::~BankingThread()
{
}

void BankingThread::run()
{
}

BankingThread::Response BankingThread::requestInteraction(InteractionType type, const QString &title, const QString &text,
                                                          const QStringList &choices, const QByteArray &image,
                                                          const QString &mimeType)
{
    FakeBankingThread::Request request;
    request.type = type;
    request.title = title;
    request.text = text;
    request.choices = choices;
    request.image = image;
    request.mimeType = mimeType;
    FakeBankingThread::requests.append(request);
    if (FakeBankingThread::answers.isEmpty()) {
        Response cancelled;
        cancelled.accepted = false;
        cancelled.choice = 0;
        return cancelled;
    }
    return FakeBankingThread::answers.takeFirst();
}

void BankingThread::reportProgress(const QString &text)
{
    FakeBankingThread::progress.append(text);
}

void BankingThread::reportError(const QString &text)
{
    FakeBankingThread::errors.append(text);
}

bool BankingThread::isCancelled() const
{
    return false;
}
