#!/bin/bash
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
# Builds and runs the tests that need AqBanking, Gwenhywfar and Qt as the app
# has them (tests/engine/), inside the build engine, from a build directory of
# the i486 target, whose binaries run natively:
#
#   cd ~/git/build-harbour-zasterbanker-4.6.0.13-i486
#   ~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-i486 build-shell ../harbour-zasterbanker/tests/run-engine-tests.sh
#
# The third-party libraries have to be built for the target first
# (3rdparty/build-deps.sh). Everything the tests build and install goes into
# engine-tests/ in the current directory. The exit code is the number of
# failed checks.

set -euo pipefail

SOURCES="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$PWD/engine-tests"
INSTALLED="$BUILD/root"

mkdir -p "$BUILD"
cd "$BUILD"

echo "==> Building the tests"
qmake "$SOURCES/tests/engine/engine-tests.pro"
# Linked again each time: make would not notice changed linker flags.
rm -f tst_engine
make -j"$(nproc)"

# AqBanking's data files, as the package installs them (3rdparty/aqbanking.pri).
# A fresh copy each time, so that a file the package lost is lost here too.
echo "==> Installing AqBanking's data files"
if [ -d "$INSTALLED" ]; then
    rm -r "$INSTALLED"
fi
make install INSTALL_ROOT="$INSTALLED" > /dev/null

echo "==> Making the bank list"
python3 "$SOURCES/tools/import-banklist.py" "$SOURCES/tests/data/institutes.csv" "$BUILD/institutes.db"

echo "==> Running the tests"
./tst_engine "$INSTALLED/usr/share/harbour-zasterbanker" "$BUILD/institutes.db"
