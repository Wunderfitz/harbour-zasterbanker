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

#ifndef FAKEBANKINGTHREAD_H
#define FAKEBANKINGTHREAD_H

#include "BankingThread.h"

#include <QList>

/*
 * The tests link the real GwenGui against a stand-in for the parts of
 * BankingThread it calls (FakeBankingThread.cpp): each question it would hand
 * to the UI is recorded here, and answered from a script.
 */
namespace FakeBankingThread
{
struct Request {
    BankingThread::InteractionType type;
    QString title;
    QString text;
    QStringList choices;
    QByteArray image;
    QString mimeType;
};

/** What GwenGui asked, oldest first. */
extern QList<Request> requests;
/** The answers to give, oldest first; when there is none left, the user cancels. */
extern QList<BankingThread::Response> answers;
/** What GwenGui reported with reportError() and reportProgress(). */
extern QStringList errors;
extern QStringList progress;

void reset();
BankingThread::Response answer(const QString &text);
BankingThread::Response choose(int choice);
}

#endif // FAKEBANKINGTHREAD_H
