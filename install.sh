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

# Desktop-Eintrag und Unit sind dieselben wie im Paket (packaging/linux), nur mit den Pfaden unter $DEST.
# Gleicher Dateiname wie DesktopManager, damit der Schalter in den Einstellungen ihn erkennt
sed -e "s|/usr/lib/mypods|$DEST|" -e "s|^Icon=.*|Icon=$DATA/icons/mypods.png|" \
    packaging/linux/app.mypods.desktop > "$DATA/applications/app.mypods.desktop"
sed "s|^Exec=.*|& --hidden|" "$DATA/applications/app.mypods.desktop" > "$AUTOSTART/app.mypods.desktop"

# Der Daemon laeuft als User-Service, unabhaengig von der Oberflaeche: Ohrerkennung und Umschalten
# bleiben aktiv, wenn das Tray-Programm beendet wird, und systemd startet ihn nach einem Absturz neu.
# Die UI startet ihn ueber systemctl (BackendManager), statt ihn selbst zu starten.
sed "s|/usr/lib/mypods|$DEST|" packaging/linux/mypods-core.service > "$UNITS/mypods-core.service"
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

setsid -f "$DEST/mypods" >/dev/null 2>&1
echo "MyPods installiert und gestartet. Startet ab jetzt automatisch beim Anmelden."
