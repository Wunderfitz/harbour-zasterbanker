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
# Emulator test: open the installed app directly on the bank search page,
# type "volksbank" one key at a time with pauses (results come back in
# between), then take a screenshot. With working focus, the field shows the
# whole word. The installed main QML is patched temporarily and restored.
set -u
K=~/SailfishOS/vmshare/ssh/private_keys/sdk
OPTS="-i $K -p 2223 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
# Usage: tests/search-focus.sh <screenshot.png>, with the emulator running and
# the 5.1 i486 build deployed to it. Needs root SSH into the emulator.
OUT=${1:?screenshot file}
MAIN=/usr/share/harbour-zasterbanker/qml/harbour-zasterbanker.qml

ssh $OPTS root@127.0.0.1 "cp $MAIN /root/main.qml.orig && sed -i 's|initialPage: Component { OverviewPage {} }|initialPage: Component { InstituteSearchPage {} }|' $MAIN && grep -c InstituteSearchPage $MAIN"
ssh $OPTS defaultuser@127.0.0.1 'pkill -x harbour-zasterb; export XDG_RUNTIME_DIR=/run/user/100000 WAYLAND_DISPLAY=../../display/wayland-0; (nohup timeout 60 sailjail -p harbour-zasterbanker.desktop /usr/bin/harbour-zasterbanker > /tmp/zb.log 2>&1 &)'
sleep 8
for c in v o l k s b a n k; do
    VBoxManage controlvm SailfishOS-5.1.0.11 keyboardputstring "$c"
    sleep 1.5
done
sleep 1
VBoxManage controlvm SailfishOS-5.1.0.11 screenshotpng "$OUT"
ssh $OPTS root@127.0.0.1 "cp /root/main.qml.orig $MAIN && pkill -x harbour-zasterb; grep -c InstituteSearchPage $MAIN"
