# MyPods

AirPods Pro 3 auf CachyOS so nutzen, wie sie sich an einem Mac anfuehlen:
Popup mit Animation beim Deckeloeffnen, exakter Akkustand, ANC-Umschaltung,
Ohrerkennung mit Auto-Pause, automatisches Audio-Routing.

Privates Projekt, nur fuer AirPods Pro 3 (A3063/A3064/A3065), nur Linux.
Windows ist bewusst ausgeklammert — siehe [docs/PLAN.md](docs/PLAN.md).

## Aufbau

| Verzeichnis | Inhalt |
|-------------|--------|
| `core/` | Daemon: AAP ueber L2CAP (PSM 0x1001) + BLE-Advertisements, WebSocket-API auf Port 2020. Uebernommen aus MagicPodsCore. |
| `ui/` | Qt6/QML-Oberflaeche: Tray, Einstellungen, Deckel-auf-Popup. Uebernommen aus MagicPodsLinux. Startet den Daemon selbst. |
| `tools/sniff.py` | Mitschnitt/Dekodierung der Apple-BLE-Advertisements. Braucht kein root. |
| `docs/PLAN.md` | Protokollrecherche und Umsetzungsplan. |
| `docs/core-api-reference.md` | WebSocket-API des Daemons. |
| `captures/` | Eigene Hardware-Mitschnitte (werden zu Testvektoren). |

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
python3 tools/sniff.py --log captures/pro3.txt --seconds 120
```

Waehrend des Scans Deckel mehrfach oeffnen/schliessen und Pods ein-/aussetzen.
Damit wird belegt, ob die Model-ID `0x2027` stimmt und wie sich der Lid-Zaehler verhaelt.

## Herkunft und Lizenz

GPL-3.0, weil der uebernommene Code GPL-3.0 ist. Details: [CREDITS.md](CREDITS.md).
