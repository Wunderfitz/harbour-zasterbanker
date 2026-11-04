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
Compiles a gettext catalogue (.po) into the binary form gettext reads (.mo),
for the translations of Gwenhywfar and AqBanking. The build engine has no
msgfmt, so 3rdparty/build-deps.sh uses this instead.

It does what "msgfmt --check-format" does with these catalogues: fuzzy,
untranslated and obsolete entries are left out, and so is any translation of
a printf format ("c-format") whose conversions differ from the original's -
the libraries hand these strings to snprintf, and a mismatch there would read
the wrong arguments. A left-out text simply stays English. Plural forms and
message contexts are not supported; neither catalogue uses them.

Usage: tools/po2mo.py <input.po> <output.mo>
"""

import re
import struct
import sys

ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "a": "\a", "b": "\b", "f": "\f",
           "v": "\v", "\\": "\\", "\"": "\"", "'": "'", "?": "?"}

# A printf conversion: optional argument position, flags, width, precision,
# length modifier, conversion character.
CONVERSION = re.compile(r"%(?:(\d+)\$)?[-+ #0']*(?:\*|\d+)?(?:\.(?:\*|\d+))?"
                        r"(hh|h|ll|l|L|q|j|z|Z|t)?([diouxXeEfFgGaAcspn%])")


def unquote(line):
    """The text of one quoted PO string, C escapes resolved."""
    line = line.strip()
    if len(line) < 2 or line[0] != "\"" or line[-1] != "\"":
        raise ValueError("not a quoted string: " + line)
    text, result, i = line[1:-1], [], 0
    while i < len(text):
        c = text[i]
        if c != "\\":
            result.append(c)
            i += 1
            continue
        e = text[i + 1]
        if e in ESCAPES:
            result.append(ESCAPES[e])
            i += 2
        elif e in "01234567":
            digits = re.match(r"[0-7]{1,3}", text[i + 1:]).group(0)
            result.append(chr(int(digits, 8)))
            i += 1 + len(digits)
        elif e == "x":
            digits = re.match(r"[0-9a-fA-F]+", text[i + 2:]).group(0)
            result.append(chr(int(digits, 16)))
            i += 2 + len(digits)
        else:
            raise ValueError("unknown escape \\" + e)
    return "".join(result)


def parse(path):
    """The entries of a PO file: (flags, msgid, msgstr, line number)."""
    entries = []
    entry = {"flags": set(), "fields": {}, "current": None, "line": 0}

    def finish():
        fields = entry["fields"]
        if "msgid" in fields:
            if any(key not in ("msgid", "msgstr") for key in fields):
                raise ValueError("line %d: plural forms and contexts are not supported" % entry["line"])
            entries.append((entry["flags"], fields["msgid"], fields.get("msgstr", ""), entry["line"]))
        entry.update(flags=set(), fields={}, current=None, line=0)

    with open(path, encoding="utf-8") as po:
        for number, line in enumerate(po, 1):
            line = line.strip()
            if line.startswith("#~"):
                continue
            # A comment, a blank line or a new keyword after the translation
            # starts the next entry.
            if entry["current"] is not None and entry["current"].startswith("msgstr") and not line.startswith("\""):
                finish()
            if not line:
                continue
            if line.startswith("#"):
                if line.startswith("#,"):
                    entry["flags"] |= {flag.strip() for flag in line[2:].split(",")}
            elif line.startswith("\""):
                if entry["current"] is None:
                    raise ValueError("line %d: string without keyword" % number)
                entry["fields"][entry["current"]] += unquote(line)
            else:
                keyword, _, rest = line.partition(" ")
                if keyword not in ("msgctxt", "msgid", "msgid_plural", "msgstr") and not keyword.startswith("msgstr["):
                    raise ValueError("line %d: unexpected %s" % (number, keyword))
                if not entry["line"]:
                    entry["line"] = number
                entry["current"] = keyword
                entry["fields"][keyword] = unquote(rest)
    finish()
    return entries


def conversions(text):
    """The conversions a printf format consumes, in argument order."""
    found = [(m.group(1), (m.group(2) or "") + m.group(3)) for m in CONVERSION.finditer(text)
             if m.group(3) != "%"]
    if any(position for position, _ in found):
        if not all(position for position, _ in found):
            return None
        found.sort(key=lambda item: int(item[0]))
        if [int(position) for position, _ in found] != list(range(1, len(found) + 1)):
            return None
    return [conversion for _, conversion in found]


def write_mo(path, catalogue):
    """The GNU .mo format: header, two string tables, no hash table."""
    keys = sorted(catalogue, key=lambda k: k.encode("utf-8"))
    ids = [k.encode("utf-8") for k in keys]
    strs = [catalogue[k].encode("utf-8") for k in keys]
    count = len(keys)
    ids_table = 7 * 4
    strs_table = ids_table + count * 8
    data = strs_table + count * 8
    offsets, blob = [], b""
    for item in ids + strs:
        offsets.append((len(item), data + len(blob)))
        blob += item + b"\0"
    with open(path, "wb") as mo:
        mo.write(struct.pack("<7I", 0x950412de, 0, count, ids_table, strs_table, 0, data))
        for length, offset in offsets:
            mo.write(struct.pack("<2I", length, offset))
        mo.write(blob)


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        sys.exit(2)
    catalogue, skipped = {}, 0
    for flags, msgid, msgstr, line in parse(sys.argv[1]):
        if not msgstr or "fuzzy" in flags:
            continue
        if msgid and "c-format" in flags:
            wanted = conversions(msgid)
            if wanted is None or conversions(msgstr) != wanted:
                print("%s:%d: format does not match the original, left out" % (sys.argv[1], line),
                      file=sys.stderr)
                skipped += 1
                continue
        catalogue[msgid] = msgstr
    if "" not in catalogue or "charset=UTF-8" not in catalogue[""]:
        print("%s: no UTF-8 header entry" % sys.argv[1], file=sys.stderr)
        sys.exit(1)
    write_mo(sys.argv[2], catalogue)
    print("%s: %d translations, %d left out for their format" % (sys.argv[2], len(catalogue) - 1, skipped))


if __name__ == "__main__":
    main()
