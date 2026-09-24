#!/bin/sh
# Runs every check that needs no hardware; the first failure stops it. Used by appimage.sh (CI) and by hand:
#   sh tools/run_checks.sh [build dir]        (default: build, as configured by the README)
# Qt comes from $QT (a Qt prefix such as /opt/qt/6.9.3/gcc_64) or else from pkg-config.
set -eu
cd "$(dirname "$0")/.."
BUILD=${1:-build}
CXX=${CXX:-g++}
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

if [ -n "${QT:-}" ]; then
    QTCORE="-I$QT/include -I$QT/include/QtCore -L$QT/lib -lQt6Core -Wl,-rpath,$QT/lib"
    QMLTEST=$QT/bin/qmltestrunner
else
    QTCORE=$(pkg-config --cflags --libs Qt6Core)
    QMLTEST=$(command -v qmltestrunner6 || command -v qmltestrunner || echo /usr/lib/qt6/bin/qmltestrunner)
fi

"$BUILD/modules/magicpodscore" --selftest
python3 tools/sniff.py --selftest
python3 tools/check_translations.py

$CXX -std=c++20 -Icore/src -Icore/dependencies/json/include core/src/tests/AncSelfCheck.cpp \
    core/src/sdk/aap/setters/AapSetAnc.cpp core/src/sdk/aap/setters/AapRequest.cpp \
    core/src/sdk/aap/setters/AapInitExt.cpp core/src/sdk/aap/watchers/AapAncWatcher.cpp \
    core/src/sdk/aap/watchers/AapWatcher.cpp -o "$OUT/anc"
"$OUT/anc"
$CXX -std=c++20 -Icore/src -Icore/dependencies/json/include core/src/tests/BatterySelfCheck.cpp \
    core/src/sdk/aap/watchers/AapBatteryWatcher.cpp core/src/sdk/aap/watchers/AapWatcher.cpp \
    core/src/device/DeviceBattery.cpp -o "$OUT/battery"
"$OUT/battery"
# shellcheck disable=SC2086 # QTCORE is a list of flags
$CXX -std=c++20 -fPIC -Iui/src/app/cpp ui/tests/LowBatteryCheck.cpp $QTCORE -o "$OUT/lowbat"
"$OUT/lowbat"
# shellcheck disable=SC2086
$CXX -std=c++20 -fPIC -Iui/src/app/cpp ui/tests/ActionsCheck.cpp $QTCORE -o "$OUT/actions"
"$OUT/actions"

# QML: the picker against the sources, with the qmldir the build generated
mkdir -p "$OUT/qml/magicpods"
sed /^prefer/d "$BUILD/qml/magicpods/qmldir" > "$OUT/qml/magicpods/qmldir"
ln -s "$PWD/ui/src" "$OUT/qml/magicpods/src"
QT_QPA_PLATFORM=offscreen "$QMLTEST" -import "$OUT/qml" -input ui/tests/tst_picker.qml

echo "All checks passed"
