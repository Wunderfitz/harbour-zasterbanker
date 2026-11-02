#!/usr/bin/env python3
#
# Copyright (C) 2026 Sebastian J. Wolf and other contributors
#
# This file is part of Zaster Banker.
#
# Zaster Banker is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Zaster Banker is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Zaster Banker. If not, see <http://www.gnu.org/licenses/>.
#
"""
Turns the FinTS institute list of the Deutsche Kreditwirtschaft (the CSV export
of "fints_institute NEU mit BIC Master.xlsx") into the SQLite database the app
ships as db/fints_institutes.db.

Only institutes offering PIN/TAN access with FinTS 3.0 are kept, as that is
the only access Zaster Banker supports. Rows that differ in nothing but the
running number are merged.

Usage: tools/import-banklist.py <path to the CSV> [<output database>]
"""

import csv
import os
import sqlite3
import sys

COL_BLZ = 1
COL_BIC = 2
COL_NAME = 3
COL_CITY = 4
COL_PINTAN_URL = 24
COL_PINTAN_VERSION = 25
COL_LAST_CHANGE = 26

# What the spreadsheet the list comes from writes into an empty cell that a
# formula refers to ("#NV" is German Excel's #N/A).
MISSING = ("#NV", "#N/A")


def fold(text):
    """Search key: lower case, umlauts spelled out, so 'Muenchen' finds 'München'."""
    text = text.lower()
    for umlaut, spelled in (("ä", "ae"), ("ö", "oe"), ("ü", "ue"), ("ß", "ss")):
        text = text.replace(umlaut, spelled)
    return text


def main():
    if len(sys.argv) < 2:
        print(__doc__.strip(), file=sys.stderr)
        return 1
    source = sys.argv[1]
    target = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "db", "fints_institutes.db")

    with open(source, encoding="latin-1", newline="") as f:
        rows = list(csv.reader(f, delimiter=";"))
    header, rows = rows[0], rows[1:]
    if header[COL_BLZ] != "BLZ" or not header[COL_PINTAN_URL].startswith("PIN/TAN"):
        print("Unexpected column layout: %s" % header, file=sys.stderr)
        return 1

    institutes = []
    seen = set()
    for row in rows:
        url = row[COL_PINTAN_URL].strip()
        version = row[COL_PINTAN_VERSION].strip()
        blz = row[COL_BLZ].strip()
        if not url.startswith("https://") or version != "FinTS V3.0":
            continue
        if len(blz) != 8 or not blz.isdigit():
            continue
        bic, city = (value.strip() for value in (row[COL_BIC], row[COL_CITY]))
        entry = (blz, "" if bic in MISSING else bic, row[COL_NAME].strip(), "" if city in MISSING else city, url,
                 row[COL_LAST_CHANGE].strip())
        if entry in seen:
            continue
        seen.add(entry)
        institutes.append(entry)

    os.makedirs(os.path.dirname(os.path.abspath(target)), exist_ok=True)
    if os.path.exists(target):
        os.remove(target)
    db = sqlite3.connect(target)
    db.execute("""CREATE TABLE institutes (
                      id INTEGER PRIMARY KEY,
                      blz TEXT NOT NULL,
                      bic TEXT NOT NULL,
                      name TEXT NOT NULL,
                      city TEXT NOT NULL,
                      url TEXT NOT NULL,
                      last_change TEXT NOT NULL,
                      search_key TEXT NOT NULL)""")
    db.executemany("INSERT INTO institutes (blz, bic, name, city, url, last_change, search_key) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?)",
                   [e + (fold(" ".join((e[0], e[1], e[2], e[3]))),) for e in institutes])
    db.execute("CREATE INDEX institutes_blz ON institutes (blz)")
    db.commit()
    db.execute("VACUUM")
    db.close()
    print("Wrote %d institutes to %s" % (len(institutes), os.path.abspath(target)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
