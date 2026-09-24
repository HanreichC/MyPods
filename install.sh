#!/bin/sh
# Baut MyPods im Podman-Container und installiert es fuer den aktuellen Benutzer:
# Programm nach ~/.local/opt/mypods, Startmenue-Eintrag, Autostart. Kein root noetig.
# Erneut ausfuehren = Update.
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
pkill -x magicpods || true
pkill -x magicpodscore || true

mkdir -p "$DEST/modules" "$DATA/applications" "$DATA/icons" "$AUTOSTART" "$UNITS"
install -m755 build/magicpods "$DEST/"
install -m755 build/modules/magicpodscore "$DEST/modules/"
install -m644 ui/src/app/qml/assets/images/logo-512.png "$DATA/icons/magicpods.png"

# Gleicher Dateiname wie DesktopManager, damit der Schalter in den Einstellungen ihn erkennt
cat > "$DATA/applications/app.magicpods.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=MyPods
Comment=The control center for your Bluetooth headphones
Exec=$DEST/magicpods
Icon=$DATA/icons/magicpods.png
Terminal=false
Categories=Utility;
StartupWMClass=MyPods
EOF
sed "s|^Exec=.*|& --hidden|" "$DATA/applications/app.magicpods.desktop" > "$AUTOSTART/app.magicpods.desktop"

# Der Daemon laeuft als User-Service, unabhaengig von der Oberflaeche: Ohrerkennung und Umschalten
# bleiben aktiv, wenn das Tray-Programm beendet wird, und systemd startet ihn nach einem Absturz neu.
# Die UI startet ihn ueber systemctl (BackendManager), statt ihn selbst zu starten.
cat > "$UNITS/mypods-core.service" <<EOF
[Unit]
Description=MyPods daemon for AirPods and Bluetooth headphones
After=pipewire.service pipewire-pulse.service

[Service]
ExecStart=$DEST/modules/magicpodscore
Restart=on-failure
RestartSec=3

[Install]
WantedBy=default.target
EOF
if systemctl --user daemon-reload 2>/dev/null; then
    systemctl --user enable --now mypods-core.service
else
    rm -f "$UNITS/mypods-core.service" # kein systemd: die UI startet den Daemon wie bisher selbst
fi

grep -qE '^[[:space:]]*DeviceID[[:space:]]*=[[:space:]]*bluetooth:004C' /etc/bluetooth/main.conf 2>/dev/null || cat <<'HINT'

Fuer automatisches Umschalten zwischen iPhone und diesem Laptop muss sich BlueZ als Apple-Geraet
melden (einmalig, braucht root):
  sudo sed -i '/^\[General\]/a DeviceID = bluetooth:004C:0000:0000' /etc/bluetooth/main.conf
  sudo systemctl restart bluetooth
HINT

setsid -f "$DEST/magicpods" >/dev/null 2>&1
echo "MyPods installiert und gestartet. Startet ab jetzt automatisch beim Anmelden."
