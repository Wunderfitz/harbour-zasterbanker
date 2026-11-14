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

#ifndef FLICKERCODE_H
#define FLICKERCODE_H

#include <QString>

/**
 * The flicker code a chipTAN generator reads off the screen (HHD 1.4, optical
 * interface), as the TAN dialog shows it.
 *
 * AqBanking turns the bank's challenge into the code to send (length, start
 * code, data elements and both checksums, as hex digits) and hands it over as
 * the "challenge" of a TAN request with chipTAN optical. Shown, it is preceded
 * by the sync "0FFF", and each byte goes out low half first. Every half-byte is
 * one frame of four bars, bit 0 to bit 3 from left to right after the clock bar,
 * shown twice: with the clock bar white, then black.
 */
class FlickerCode
{
public:
    /**
     * The half-bytes in the order they are shown, sync included, as upper case
     * hex digits; empty if the code is not an even number of hex digits.
     */
    static QString frames(const QString &challenge);
};

#endif // FLICKERCODE_H
