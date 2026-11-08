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
Tests of the two tools in tools/, run as they are run in the build: as
programs, on files. Needs nothing but Python 3:

    python3 -m unittest discover -s tests/host -v
"""

import gettext
import os
import sqlite3
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
IMPORT_BANKLIST = os.path.join(ROOT, "tools", "import-banklist.py")
PO2MO = os.path.join(ROOT, "tools", "po2mo.py")
INSTITUTES_CSV = os.path.join(ROOT, "tests", "data", "institutes.csv")


def run(*args):
    return subprocess.run([sys.executable, "-I"] + list(args), stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, universal_newlines=True)


class ImportBanklistTest(unittest.TestCase):
    """tools/import-banklist.py on a made-up list (tests/data/institutes.csv)."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.database = os.path.join(cls.folder.name, "institutes.db")
        result = run(IMPORT_BANKLIST, INSTITUTES_CSV, cls.database)
        if result.returncode != 0:
            raise AssertionError(result.stderr)
        connection = sqlite3.connect(cls.database)
        cls.rows = {row[0]: row for row in connection.execute(
            "SELECT blz, bic, name, city, url, last_change, search_key FROM institutes")}
        connection.close()

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_keeps_pintan_with_fints_3(self):
        self.assertEqual(sorted(self.rows), ["10010010", "20020020", "30030030", "40040040", "50050050"])

    def test_merges_rows_that_differ_only_in_number(self):
        connection = sqlite3.connect(self.database)
        count = connection.execute("SELECT COUNT(*) FROM institutes WHERE blz = '10010010'").fetchone()[0]
        connection.close()
        self.assertEqual(count, 1)

    def test_keeps_the_fields(self):
        self.assertEqual(self.rows["10010010"][:6], ("10010010", "TESTDEFF100", "Volksbank Musterstadt eG",
                                                      "München", "https://fints.example.test/volksbank",
                                                      "01.10.2026"))

    def test_not_available_becomes_empty(self):
        self.assertEqual(self.rows["30030030"][1], "")
        self.assertEqual(self.rows["30030030"][3], "")

    def test_search_key_folds_case_and_umlauts(self):
        self.assertEqual(self.rows["20020020"][6],
                         "20020020 testdeff200 gemeinschaftsbank fuer beispiele eg weissenburg")

    def test_rejects_an_unknown_layout(self):
        broken = os.path.join(self.folder.name, "broken.csv")
        with open(broken, "w", encoding="latin-1") as f:
            f.write("a;b;c\r\n1;2;3\r\n")
        result = run(IMPORT_BANKLIST, broken, os.path.join(self.folder.name, "broken.db"))
        self.assertNotEqual(result.returncode, 0)


PO_HEADER = r'''msgid ""
msgstr ""
"Content-Type: text/plain; charset=UTF-8\n"
"Language: de\n"

'''


class Po2MoTest(unittest.TestCase):
    """tools/po2mo.py, checked by reading its output with Python's own gettext."""

    def compile(self, entries, header=PO_HEADER):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        po = os.path.join(folder.name, "test.po")
        mo = os.path.join(folder.name, "test.mo")
        with open(po, "w", encoding="utf-8") as f:
            f.write(header + entries)
        result = run(PO2MO, po, mo)
        if result.returncode != 0:
            return result, None
        with open(mo, "rb") as f:
            return result, gettext.GNUTranslations(f)

    def test_translates(self):
        _, catalogue = self.compile('msgid "Balance"\nmsgstr "Kontostand"\n\n'
                                    'msgid "Line one\\n"\n"line two"\nmsgstr "Zeile eins\\n"\n"Zeile zwei"\n')
        self.assertEqual(catalogue.gettext("Balance"), "Kontostand")
        self.assertEqual(catalogue.gettext("Line one\nline two"), "Zeile eins\nZeile zwei")

    def test_keeps_non_ascii(self):
        _, catalogue = self.compile('msgid "Transfer"\nmsgstr "Überweisung €"\n')
        self.assertEqual(catalogue.gettext("Transfer"), "Überweisung €")

    def test_leaves_out_fuzzy_untranslated_and_obsolete(self):
        _, catalogue = self.compile('#, fuzzy\nmsgid "Fuzzy"\nmsgstr "Unscharf"\n\n'
                                    'msgid "Untranslated"\nmsgstr ""\n\n'
                                    '#~ msgid "Obsolete"\n#~ msgstr "Veraltet"\n')
        self.assertEqual(catalogue.gettext("Fuzzy"), "Fuzzy")
        self.assertEqual(catalogue.gettext("Untranslated"), "Untranslated")
        self.assertEqual(catalogue.gettext("Obsolete"), "Obsolete")

    def test_keeps_matching_formats(self):
        _, catalogue = self.compile('#, c-format\nmsgid "%d of %s"\nmsgstr "%d von %s"\n\n'
                                    '#, c-format\nmsgid "%s has %lu"\nmsgstr "%2$lu bei %1$s"\n')
        self.assertEqual(catalogue.gettext("%d of %s"), "%d von %s")
        self.assertEqual(catalogue.gettext("%s has %lu"), "%2$lu bei %1$s")

    def test_leaves_out_mismatching_formats(self):
        result, catalogue = self.compile('#, c-format\nmsgid "%d of %s"\nmsgstr "%s von %d"\n\n'
                                         '#, c-format\nmsgid "Error %d"\nmsgstr "Fehler"\n\n'
                                         '#, c-format\nmsgid "%s (%d%%)"\nmsgstr "%s (%d %%)"\n')
        self.assertEqual(catalogue.gettext("%d of %s"), "%d of %s")
        self.assertEqual(catalogue.gettext("Error %d"), "Error %d")
        self.assertEqual(catalogue.gettext("%s (%d%%)"), "%s (%d %%)")
        self.assertIn("2 left out for their format", result.stdout)

    def test_requires_a_utf8_header(self):
        result, _ = self.compile('msgid "Balance"\nmsgstr "Kontostand"\n',
                                 header='msgid ""\nmsgstr "Content-Type: text/plain; charset=ISO-8859-1\\n"\n\n')
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
