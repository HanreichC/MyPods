# MyPods

AirPods Pro 3 auf CachyOS so nutzen, wie sie sich an einem Mac anfuehlen:
Popup mit Animation beim Deckeloeffnen, exakter Akkustand, ANC-Umschaltung,
Ohrerkennung mit Auto-Pause, automatisches Audio-Routing.

Privates Projekt fuer AirPods Pro 3 (A3063/A3064/A3065) und AirPods Max
(Max `0x200A`, Max USB-C `0x201F`, Max 2 `0x202D`), nur Linux.
Windows ist bewusst ausgeklammert — siehe [docs/PLAN.md](docs/PLAN.md).

Ausserdem: **Parrot Zik 2.0** ueber dessen RFCOMM-XML-API — Akku, ANC / Street-Mode (normal/max; kein „Aus“, das macht den Zik 2 stumm),
Kopferkennung, Concert Hall (Raum, Winkel), Equalizer auf dem Kopfhoerer-DSP, Smart Audio Tune,
ANC im Telefonat, Sprachansagen, Auto-Verbinden, Auto-Aus. Einfach koppeln, MyPods erkennt ihn am Dienst.

## Wie am Mac

| Mac | MyPods |
|-----|--------|
| "Mit diesem Mac verbinden: Automatisch" — AirPods wechseln zu dem Geraet, das gerade abspielt | Startet hier ein Player (YouTube im Browser, Spotify …, alles mit MPRIS), holt MyPods die AirPods vom iPhone: verbinden falls noetig, Apples Smart-Routing-Uebernahme, A2DP an, Standard-Ausgang. Startet das iPhone, pausiert der Laptop und faellt auf die Lautsprecher zurueck. Nicht waehrend eines Anrufs auf dem anderen Geraet. |
| Banner "Zu iPhone bewegt — Zurueck" | Geraeteseite: "Playing on iPhone — Move here" |
| "Wenn zuletzt mit diesem Mac verbunden" | Gleiche Einstellung, dann nur manuell |
| Automatische Ohrerkennung | Pod raus = Pause, wieder rein = weiter (MPRIS); der Schalter wird auf den AirPods gespeichert |
| 3D-Audio: Aus / Fixiert / Kopfbewegung | PipeWire-Filter-Chain mit HRTF (libmysofa), Kopfbewegung kommt von den AirPods |
| Equalizer (Musik-App) | Apple-Music-Presets vor den AirPods |

**Einmalig noetig (root):** BlueZ muss sich als Apple-Geraet melden, sonst nehmen iPhone und AirPods den
Laptop nicht in die Umschaltung auf. Ohne diese Zeile funktioniert alles andere, nur das Umschalten nicht.

```bash
sudo sed -i '/^\[General\]/a DeviceID = bluetooth:004C:0000:0000' /etc/bluetooth/main.conf
sudo systemctl restart bluetooth
```

Danach die AirPods einmal neu verbinden. Grenzen: Das 3D-Audio am Laptop nutzt eine generische HRTF (Apple
vermisst dein Ohr), die Auswertung der Kopfbewegung ist eine Heuristik aus LibrePods und noch nicht an
Pro-3-Hardware geeicht, und Audio ohne MPRIS (Spiele, Systemtoene) loest bewusst keine Uebernahme aus — wie am Mac.

## Aufbau

| Verzeichnis | Inhalt |
|-------------|--------|
| `core/` | Daemon: AAP ueber L2CAP (PSM 0x1001) + BLE-Advertisements, WebSocket-API auf Port 2020. Uebernommen aus MagicPodsCore. |
| `ui/` | Qt6/QML-Oberflaeche: Tray, Einstellungen, Deckel-auf-Popup. Uebernommen aus MagicPodsLinux. Startet den Daemon selbst. |
| `tools/sniff.py` | Mitschnitt/Dekodierung der Apple-BLE-Advertisements. Braucht kein root. |
| `docs/PLAN.md` | Protokollrecherche und Umsetzungsplan. |
| `docs/core-api-reference.md` | WebSocket-API des Daemons. |
| `captures/` | Eigene Hardware-Mitschnitte (werden zu Testvektoren). |

## Installieren

```bash
./install.sh
```

Baut im Container, installiert nach `~/.local/opt/mypods`, legt Startmenue- und Autostart-Eintrag
an und startet die App. Erneut ausfuehren = Update. Kein root noetig.

## Bauen

Nichts wird global installiert — Toolchain steckt im Dev-Container (Podman, Arch-Basis,
damit die Binaries auf dem Host laufen):

```bash
podman build -t mypods-dev -f .devcontainer/Dockerfile .devcontainer
podman run --rm -v "$PWD:/workspace" -w /workspace mypods-dev \
    sh -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)'
```

Ergebnis: `build/magicpods` (UI) und `build/modules/magicpodscore` (Daemon).
Gestartet wird auf dem Host `./build/magicpods`; die UI startet den Daemon selbst.

In VS Code OSS stattdessen: *Devcontainer: Open Folder in container*.

## Zuerst pruefen (vor jeder Zeile neuem Code)

```bash
python3 tools/sniff.py --selftest                      # Dekoder gegen bekannte Captures
./build/modules/magicpodscore --selftest               # AAP/Smart-Routing/Effekt-Chain, ohne Hardware
python3 tools/sniff.py --log captures/pro3.txt --seconds 120
```

Waehrend des Scans Deckel mehrfach oeffnen/schliessen und Pods ein-/aussetzen.
Damit wird belegt, ob die Model-ID `0x2027` stimmt und wie sich der Lid-Zaehler verhaelt.

AirPods Max haben kein Lade-Case: Als Popup-Trigger nimmt MagicPodsCore dort das High-Nibble
von Byte 8 (== 8) statt des Case-Zustands. Fuer Max 2 ist das nur uebernommen, nicht an echter
Hardware belegt:

```bash
python3 tools/sniff.py --log captures/max2.txt --seconds 120
```

## Herkunft und Lizenz

GPL-3.0, weil der uebernommene Code GPL-3.0 ist. Details: [CREDITS.md](CREDITS.md).
