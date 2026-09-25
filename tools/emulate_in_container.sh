#!/bin/sh
# Runs `magicpodscore --emulate-airpods` against a private PipeWire, for the dev container (no host audio needed):
#   podman run --rm -v "$PWD:/workspace" -w /workspace <dev image> sh tools/emulate_in_container.sh [build dir]
set -eu
BUILD=${1:-build}
export XDG_RUNTIME_DIR="$(mktemp -d)"
mkdir -p /run/dbus
dbus-daemon --system --fork # the daemon opens the system bus for BlueZ, even though nothing answers there
exec dbus-run-session -- sh -c '
    pipewire >/dev/null 2>&1 & sleep 1
    wireplumber >/dev/null 2>&1 & pipewire-pulse >/dev/null 2>&1 & sleep 2
    "$0/modules/magicpodscore" --emulate-airpods' "$BUILD"
