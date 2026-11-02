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
 * The PIN is kept in memory for the duration of one command, so that the
 * several dialogs of a setup ask for it once, and wiped when the command ends.
 * It is never written anywhere.
 */
namespace GwenGui
{
GWEN_GUI *create(BankingThread *thread);
void destroy(GWEN_GUI *gui);
/** Wipes the PINs remembered during the command that just ended. */
void forgetPins();
}

#endif // GWENGUI_H
