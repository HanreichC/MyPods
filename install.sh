#!/bin/sh
# Builds MyPods in the Podman container and installs it for the current user: the program in
# ~/.local/opt/mypods, a start menu entry, autostart. No root needed. Run it again to update.
set -eu
cd "$(dirname "$0")"

DEST="$HOME/.local/opt/mypods"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
AUTOSTART="${XDG_CONFIG_HOME:-$HOME/.config}/autostart"
UNITS="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"

# Cached layers make this quick; rerunning it picks up changes to the Dockerfile
podman build -q -t mypods-dev -f .devcontainer/Dockerfile .devcontainer
podman run --rm -v "$PWD:/workspace" -w /workspace mypods-dev \
    sh -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)'

systemctl --user stop mypods-core.service 2>/dev/null || true
pkill -x mypods || true
pkill -x mypodscore || true
# MyPods was called MagicPods: what an install from then left behind goes
pkill -x magicpods || true
pkill -x magicpodscore || true
rm -f "$DEST/magicpods" "$DEST/modules/magicpodscore" "$DATA/icons/magicpods.png" \
      "$DATA/applications/app.magicpods.desktop" "$AUTOSTART/app.magicpods.desktop"

mkdir -p "$DEST/modules" "$DATA/applications" "$DATA/icons" "$AUTOSTART" "$UNITS"
install -m755 build/mypods "$DEST/"
install -m755 build/modules/mypodscore "$DEST/modules/"
install -m644 ui/src/app/qml/assets/images/logo-512.png "$DATA/icons/mypods.png"

# The desktop entry and the unit are the package's (packaging/linux), with the paths under $DEST.
# The same file name as DesktopManager's, so the switch in the settings recognizes it
sed -e "s|/usr/lib/mypods|$DEST|" -e "s|^Icon=.*|Icon=$DATA/icons/mypods.png|" \
    packaging/linux/app.mypods.desktop > "$DATA/applications/app.mypods.desktop"
sed "s|^Exec=.*|& --hidden|" "$DATA/applications/app.mypods.desktop" > "$AUTOSTART/app.mypods.desktop"

# The daemon runs as a user service, apart from the UI: ear detection and switching keep working
# when the tray app quits, and systemd restarts it after a crash. The UI starts it through systemctl
# (BackendManager) instead of starting it itself.
sed "s|/usr/lib/mypods|$DEST|" packaging/linux/mypods-core.service > "$UNITS/mypods-core.service"
if systemctl --user daemon-reload 2>/dev/null; then
    systemctl --user enable --now mypods-core.service
else
    rm -f "$UNITS/mypods-core.service" # no systemd: the UI starts the daemon itself
fi

grep -qE '^[[:space:]]*DeviceID[[:space:]]*=[[:space:]]*bluetooth:004C' /etc/bluetooth/main.conf 2>/dev/null || cat <<'HINT'

For automatic switching between the iPhone and this computer, BlueZ has to present itself as an
Apple device (once, needs root):
  sudo sed -i '/^\[General\]/a DeviceID = bluetooth:004C:0000:0000' /etc/bluetooth/main.conf
  sudo systemctl restart bluetooth
HINT

setsid -f "$DEST/mypods" >/dev/null 2>&1
echo "MyPods is installed and running. From now on it starts at login."
