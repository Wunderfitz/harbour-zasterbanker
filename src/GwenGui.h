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

#ifndef GWENGUI_H
#define GWENGUI_H

#include <QtGlobal>

#include <gwenhywfar/gui.h>

class BankingThread;

/**
 * Gwenhywfar's GUI layer, implemented on top of BankingThread.
 *
 * AqBanking talks to the user only through these callbacks: PIN and TAN
 * entry, message boxes (also the "confirm in your banking app" of decoupled
 * TAN methods), progress and the check of the bank's TLS certificate. They are
 * all called on the banking thread.
 *
 * A PIN is kept in memory, per bank access, for five minutes after the last
 * command that used it, so that the user does not have to enter it for every
 * action, and only if the bank accepted it. Then, or as soon as the bank
 * rejects it, it is wiped. It is never written anywhere.
 */
namespace GwenGui
{
GWEN_GUI *create(BankingThread *thread);
void destroy(GWEN_GUI *gui);
/**
 * To be called after each command: forgets the PINs the bank did not accept,
 * and starts the five minutes again for those the command used.
 */
void finishCommand();
/**
 * Wipes the PINs whose time is up. Returns the milliseconds until the next
 * one is due, or -1 if no PIN is remembered.
 */
qint64 forgetExpiredPins();
/** Wipes all remembered PINs. */
void forgetPins();

/**
 * For the tests only: the clock the five minutes are measured with, in
 * milliseconds. nullptr restores the real one, time since boot.
 */
void setClock(qint64 (*clock)());
}

#endif // GWENGUI_H
