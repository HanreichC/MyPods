# MyPods — Recherche & Umsetzungsplan

Ziel: AirPods Pro 3 auf CachyOS (und Windows) so nutzen, wie sie sich an einem Mac anfühlen —
inklusive Deckel-auf-Popup mit Animation, exaktem Akkustand, ANC-Steuerung, Ohrerkennung
und automatischem Audio-Routing. Nur AirPods Pro 3 (A3063/A3064/A3065), nichts anderes.

Stand: 2026-09-20. Alles unten ist aus öffentlichem Reverse Engineering + den vier lokalen Repos
belegt; alles was mit **(H)** markiert ist, muss an echter Hardware verifiziert werden.

---

## 1. Wie Apples Software funktioniert

Apple benutzt **zwei getrennte Kanäle**. Das ist der Schlüssel zum Verständnis — die meisten
Tools können nur einen davon.

### 1.1 Kanal A — BLE Advertising (Continuity / "Proximity Pairing")

Die AirPods senden permanent BLE-Advertisements mit Apple-Manufacturer-ID `0x004C`.
Nachrichtentyp `0x07` = Proximity Pairing. Genau daraus baut macOS/iOS das Deckel-auf-Popup —
**ohne verbunden zu sein**. Das ist der Grund, warum das Popup sofort kommt, bevor irgendetwas
gekoppelt ist.

Layout (27 Byte, verifiziert in `MagicPodsCore/src/device/capabilities/aap/AppAnimationCapability.cpp`
und `librepods/linux/ble/blemanager.cpp`):

| Offset | Inhalt |
|--------|--------|
| 0 | `0x07` Nachrichtentyp |
| 1 | `0x19` Restlänge (25) |
| 2 | `0x00` = Pairing-Mode (noch nie gekoppelt), `0x01` = gekoppelt/normal |
| 3–4 | Model-ID, **AirPods Pro 3 = Bytes `27 20`** (= `0x2027`) **(H)** |
| 5 | Statusbits: bit5 = welcher Pod ist primary (L/R), bit6 = dieser Pod im Case, bit4 = ein Pod im Case, bit2 = beide im Case, bit1/bit3 = in-ear L/R |
| 6 | Akku grob: je ein Nibble pro Pod (`0..10` = 0–100 %, `15` = unbekannt) |
| 7 | High-Nibble = Ladeflags (L/R/Case), Low-Nibble = Case-Akku grob |
| 8 | bits0–2 = **Lid-Open-Counter**, bit3 = Lid-State |
| 9 | Gehäusefarbe |
| 10 | Connection-State (`0x00` disconnected, `0x04` idle, `0x05` music, `0x06` call …) |
| 11–26 | **AES-128-ECB verschlüsselte Payload (16 Byte)** → exakter Akku in 1-%-Schritten |

Zwei Dinge, die Apple "magisch" wirken lassen und die man nachbauen muss:

1. **Der Popup-Trigger.** Nicht der Lid-State allein. MagicPods leitet ihn ab aus
   `beide Pods im Case && zwei Pods aktiv && (Byte8 & 0x0F) <= 8` — der Counter zählt bei jedem
   Deckel-Öffnen hoch und läuft nach ein paar Sekunden über den Schwellwert, was das Popup wieder
   schließt. LibrePods liest stattdessen Counter (bits0–2) + Lid-Bit (bit3). Beide Lesarten
   erzeugen dasselbe Verhalten; **(H)** wir verifizieren an der Hardware und nehmen die, die stabil
   ist.
2. **Exakter Akku.** Die groben 10-%-Nibbles sind das, was jedes billige Tool anzeigt. Apple zeigt
   1-%-Schritte, weil die letzten 16 Byte mit einem gerätespezifischen Key entschlüsselt werden:
   `encPayload[0] == 0x04|0x14`, dann `[1]`/`[2]` = Pods, `[3]` = Case, Bit7 jeweils = lädt.

Die BLE-Adresse ist eine rotierende RPA. Um zu wissen, dass ein Advertisement von *meinen*
AirPods kommt, braucht man den **IRK**; für die verschlüsselte Payload den **ENC-Key**. Beide
bekommt man **von den AirPods selbst** über Kanal B (siehe 1.2), kein iCloud nötig.

### 1.2 Kanal B — AAP/AACP über L2CAP, PSM `0x1001`

Sobald klassisch verbunden (A2DP/HFP), öffnet Apple parallel einen L2CAP-Socket auf PSM `0x1001`.
Darüber läuft alles, was "Einstellungen" ist. Unverschlüsselt, reine Hex-Frames.

```
Handshake (ohne den antworten die AirPods auf gar nichts):
  00 00 04 00 01 00 02 00 00 00 00 00 00 00 00 00
Extended Init (nötig für Adaptive/CA bei Pro 2/Pro 3/AirPods 4 ANC):
  04 00 04 00 4d 00 0e 00 00 00 00 00 00 00        (MagicPodsCore-Variante)
  04 00 04 00 4d 00 ff 00 00 00 00 00 00 00        (LibrePods-Capture)
Notifications abonnieren:
  04 00 04 00 0f 00 ff ff ff ff ff
```

Danach pusht die Hardware von selbst:

| Zweck | Frame |
|-------|-------|
| Akku (exakt, pro Komponente + Ladestatus) | `04 00 04 00 04 00 <count> (<comp> 01 <level> <status> 01)*` |
| Ohrerkennung | `04 00 04 00 06 00 <primary> <secondary>` (`00` im Ohr, `01` draußen, `02` im Case) |
| Noise-Control | `04 00 04 00 09 00 0D <mode>` (`01` off, `02` ANC, `03` Transparenz, `04` adaptiv) |
| Conversational Awareness | `04 00 04 00 4B 00 02 00 01 <level>` (Level → Lautstärke absenken) |
| Metadaten (Name, **Modellnummer A3063…**, Seriennummer, Firmware) | `04 00 04 00 1d <nullterminierte Strings>` |
| **IRK + ENC-Key anfordern** | senden: `04 00 04 00 30 00 05 00` → Antwort `04 00 04 00 31 00 02 …` (TLV `0x01`=IRK 16 B, `0x04`=ENC 16 B) |

Schreibend genauso: ANC setzen, umbenennen (`04 00 04 00 1A 00 01 <len> 00 <name>`),
Adaptive-Noise-Level 0–100 (`… 09 00 2E <level>`), CA an/aus, Stem-Belegung, Hearing-Aid,
Transparenz-Feintuning.

**Trick:** Meldet sich der Host per DID-Profil als Apple (`DeviceID = bluetooth:004C:0000:0000`
in `/etc/bluetooth/main.conf`), schalten die AirPods zusätzliche Features frei (Loud Sound
Reduction, Accessibility, Multipoint-Handoff inkl. "Move to iPhone"-Verhalten).

### 1.3 Was Apple noch macht — und was wir davon brauchen

* **MagicPairing** (WiSec'20-Paper von Heinze/Classen/Rohrbach): Apples iCloud-basierte
  Schlüsselableitung, damit AirPods auf allen Geräten eines Accounts sofort funktionieren.
  **Brauchen wir nicht** — normales BT-Pairing reicht, der Popup hängt nicht daran.
* **Erst-Pairing**: Am Mac erscheint das Popup, weil die AirPods im Pairing-Mode advertisen
  (`Byte2 == 0x00`). Für Nicht-Apple-Hosts muss der Case-Knopf gedrückt werden, damit sie
  klassisch discoverable sind. Das Popup können wir trotzdem zeigen (Advertisement ist da) —
  nur "Verbinden" läuft dann über normales BlueZ-Pairing.
* **Audio-Switch / Ear-Detection-Pause**: reine Client-Logik, machen wir selbst (PipeWire + MPRIS).
* **Herzfrequenz (Pro 3)**: Kein Standard-BT-HRM-Profil, läuft proprietär. Community-Stand: noch
  nicht reverse engineered (LibrePods Issue #308, offen). → Forschungsthema, nicht Scope P1–P4.
* **Spatial Audio mit Head-Tracking**: Head-Tracking-Stream ist bekannt
  (`04 00 04 00 17 00 …`, Orientierung ab Offset 43). HRTF-Rendering übernimmt PipeWire selbst
  (filter-chain `sofa`/`spatializer` + libmysofa, auf CachyOS vorhanden) → umgesetzt, siehe 3.6.
* **Automatisches Umschalten** („Mit diesem Mac verbinden: Automatisch"): AirPods relayen
  Smart-Routing-Nachrichten zwischen ihren Quellen (AAP `0x10`/`0x11`, OPACK-kodiert), melden die
  Audioquelle (`0x0E`), verbundene Geräte (`0x2E`) und „owns connection" (Control `0x06`).
  Reverse Engineering: LibrePods (Android, `AACPManager.kt`). Setzt Apple-DeviceID in BlueZ voraus.
* **Find My / Case-Sound**: nicht reverse engineered. → out of scope.

---

## 2. Was die vier Repos taugen

Alle vier sind **GPL-3.0** → MyPods wird GPL-3.0, dann dürfen wir Code und Assets direkt übernehmen.

| Repo | Sprache/Stack | Kann | Für uns |
|------|---------------|------|---------|
| **AirPodsDesktop** | Qt 6.8, Windows, C++ | BLE-Advertisement-Parsing (`Core/AppleCP.*`), Popup mit Video-Animation, Ear-Detection aus dem Advertisement, Low-Latency-Mode, Quick-Connect über WinRT. **Kein AAP** — kann also kein ANC. Kennt Pro 3 bereits (`0x2027`). | **Assets + Windows-Teil**: `Source/Resource/Video/AirPods_Pro_3.avi` (2,8 MB Popup-Animation), `Image/Animation/AirPods_Pro_3.png`, `QuickConnect_win.cpp` (Audio-Endpoint-Handling), `AppleCP.h` als Zweitmeinung beim Advertisement-Parsing |
| **MagicPods-Windows** | — | Nur README, **Closed Source**. Wichtige Info daraus: volle AirPods-Kontrolle unter Windows geht nur über deren **MagicAAP-Kerneltreiber** (unsigniert → Testmode, oder community-signiert). | Kein Code. Aber die Erkenntnis: Windows + AAP = Kerneltreiber, kein Userspace-Weg. |
| **MagicPodsCore** | C++17, CMake, BlueZ (sd-bus) + roher L2CAP-Socket, uWebSockets | Der beste offene AAP-Stack: `src/sdk/aap/` (Setter/Watcher für ANC, Adaptive Noise, CA, Personalized Volume, Press Speed/Hold, Volume Swipe, One-Pod-ANC), `AppAnimationCapability` (AES-Entschlüsselung + RPA-Verify + Popup-Trigger), Pro 3 (`0x2027`) inkl. `AapInitExt`, Settings-Persistenz, WebSocket-JSON-API, Unit-Tests mit echten Captures. | **Fundament des Daemons.** |
| **MagicPodsLinux** | Qt 6.8 + QML, spricht den Core über WebSocket an | `PopupAnimation.qml` (438 Zeilen fertiges OSD inkl. Ein-/Ausblende-Animation, gamescope-Handling), Sprite-Sheets `assets/animations/76_8231.png` = 5280×1650 = Pro-3-Animation, Tray, Settings-Seiten. | **Fundament der UI.** |

**Lücken, die keines der vier Repos schließt** (das ist der eigentliche Grund, warum MyPods existiert):

* MagicPodsCore hat **keine Ohrerkennung für AirPods** (nur Galaxy Buds) → kein Auto-Pause.
* MagicPodsCore hat kein Umbenennen, keine Metadaten (Modellnummer/Firmware), kein Hearing-Aid.
* MagicPodsLinux zeigt das Popup, aber ohne "Verbinden"-Button-Flow wie am Mac.
* AirPodsDesktop kann unter Windows nichts konfigurieren (kein AAP).
* Kein Tool macht automatisches Audio-Routing sauber (Pods raus → Boxen, Pods rein → zurück).

Fünftes Repo, das du nicht hast, aber das die Lücken füllt: **LibrePods** (GPL-3.0,
`github.com/librepods-org/librepods`) — Ohrerkennung, CA-Volume-Ducking, Rename, Metadaten,
IRK/ENC-Request, Hearing-Aid, Handoff. Liegt als Referenz-Clone unter
`/tmp/.../scratchpad/librepods`; für die Arbeit dauerhaft nach `~/Dokumente/GitHub/librepods` klonen.

---

## 3. Der Plan

### 3.1 Architektur — entschieden

Entschieden am 2026-09-20: **Code wird lokal herüberkopiert, kein Fork, kein Subtree.**
Privatnutzung, deshalb keine Upstream-Pflege. **Windows ist gestrichen** (siehe P4).

```
MyPods/
  core/   Daemon, 1:1 aus MagicPodsCore kopiert (GPL-3.0)
          AAP über L2CAP PSM 0x1001, BLE-Advertisements, AES-Payload, RPA-Prüfung,
          Settings, WebSocket-API auf :2020, Tests mit echten Captures
  ui/     Qt6/QML aus MagicPodsLinux kopiert (GPL-3.0)
          Tray, Einstellungen, PopupAnimation.qml, Pro-3-Sprite 76_8231.png;
          startet den Daemon selbst und spricht ihn über WebSocket an
  tools/  sniff.py — Advertisement-Mitschnitt, ohne root, mit Selbsttest
  docs/   dieser Plan + WebSocket-API des Daemons
```

Beides zusammen baut ein Top-Level-CMake (`add_subdirectory(core)` + `add_subdirectory(ui)`);
Toolchain steckt im Podman-Devcontainer auf Arch-Basis, damit die Binaries auf dem
CachyOS-Host laufen.

Was in `core/` fehlt und von uns dazukommt (Frames stehen in Abschnitt 1.2):
Ohrerkennung (`… 06 00`), Metadaten/Modellnummer (`… 1d`), Umbenennen (`… 1A`),
Auto-Pause über MPRIS, Audio-Routing über PipeWire.

### 3.2 Phasen

**P0 — Fundament & Hardware-Wahrheit**
1. ✅ Devcontainer (Podman, Arch-Basis) mit CMake, g++, Qt6, bluez-libs, libpulse, openssl.
   Nichts global installiert; Build im Container, Ausführung auf dem Host.
2. ✅ `core/` und `ui/` herüberkopiert, Top-Level-CMake, GPL-3.0 + CREDITS.md.
3. ✅ `tools/sniff.py`: liest Apple-Advertisements über BlueZ-D-Bus (ohne root), dekodiert
   Model, Akku, Lid-Zähler, Statusbits, Popup-Bedingung; `--selftest` prüft den Dekoder
   gegen vier echte Captures aus `core/src/tests/TestsAapBle.cpp`.
4. ⬜ **(H)** Mit echter Hardware: `python3 tools/sniff.py --log captures/pro3.txt --seconds 120`,
   dabei Deckel mehrfach öffnen/schließen, Pods ein-/aussetzen, Case laden.
   Klärt: Model-ID `27 20`? Welche Lid-Lesart stimmt? Welche Farbe?
5. ⬜ AirPods koppeln: Case-Knopf → `bluetoothctl`; `/etc/bluetooth/main.conf` um
   `DeviceID = bluetooth:004C:0000:0000` ergänzen; PipeWire auf AAC prüfen
   (libfdk-aac ist vorhanden).
6. ⬜ L2CAP-Rohtest: Socket auf PSM `0x1001`, Handshake senden, Antwort dumpen.
   Wenn das geht, ist alles Weitere nur noch Parsing.

**P1 — Linux-MVP: das Popup** (das Kernerlebnis)
7. Core bauen, mit Pro 3 verbinden, IRK+ENC per `04 00 04 00 30 00 05 00` holen und persistieren.
8. Popup-Trigger aus dem Advertisement + exakter Akku aus der entschlüsselten Payload.
9. UI: `PopupAnimation.qml` übernehmen, Sprite-Sheet `76_8231.png`, Akku-Ringe, Gerätename.
10. "Verbinden"-Button → `ConnectDevice` (BlueZ) → Audio landet auf den Pods. Ab hier fühlt es
   sich zum ersten Mal wie ein Mac an.

**P2 — Mac-Parität**
11. Noise-Control (Off/Transparenz/Adaptiv/ANC) inkl. Adaptive-Noise-Slider 0–100.
12. Ohrerkennung → Auto-Pause/Resume über MPRIS (beide raus = pause, wieder rein = play).
13. Auto-Audio-Routing: Pods verbunden → Default-Sink; beide raus/Deckel zu → zurück auf Boxen.
14. Conversational Awareness inkl. Volume-Ducking beim Sprechen.
15. Tray-Icon mit Akkuständen, Umbenennen, Settings-Fenster, Autostart (systemd user service).

**P3 — Politur & Paket**
16. Low-Battery-Notifications, Hotkeys, Light/Dark, i18n (de/en).
17. Packaging für CachyOS: PKGBUILD (AUR-tauglich) + AppImage als Fallback.

**P4 — Windows: zurückgestellt**
Bewusst gestrichen (Entscheidung 2026-09-20). Begründung bleibt im Plan, falls es später
doch kommt: Windows hat **keine Userspace-L2CAP-API**, AAP ginge nur über einen
KMDF-Profiltreiber (Vorlage: Microsoft `bthecho`, Referenz: nefarius/BthPS3) inklusive
Signatur-Problematik. Ohne Treiber wären nur BLE-Features möglich (Popup, Akku,
Ohrerkennung) — Code dafür liegt fertig in AirPodsDesktop.

*Nachtrag 2026-09-24:* Die treiberlose Stufe ist umgesetzt (Branch `windows`, siehe README →
Windows). Die Plattformschicht (BlueZ/PulseAudio/MPRIS bzw. WinRT/Core Audio/SMTC) hat je
eine Implementierung, alles darüber ist gemeinsam. L2CAP meldet unter Windows
`Client::SupportsL2CAP() == false`; ein späterer Treiber wird nur in `Client_win.cpp` angebunden.

**P5 — Forschung (optional)**
18. Herzfrequenz Pro 3: eigene Captures (Apple-DID-Spoof + Notifications auf `FF FF FF FF FF`),
    unbekannte Opcodes protokollieren. Offen in der gesamten Community — hier wäre MyPods der Erste.

### 3.3 Prüfbarkeit

Jede nicht-triviale Logik bekommt genau einen laufbaren Check, ohne Framework:
`core/tests/` erweitert MagicPodsCores bestehende `TestsAapBle` um Pro-3-Vektoren aus unseren
eigenen Captures — Advertisement → erwarteter Akku/Lid/Popup-Zustand, AAP-Frame → erwartetes Event.
Reine Byte-Logik, läuft ohne Hardware, fällt um, wenn jemand am Parser dreht.

### 3.4 Risiken

| Risiko | Wirkung | Umgang |
|--------|---------|--------|
| Pro-3-Advertisement weicht ab (neue Felder) | Popup/Akku falsch | P0 Schritt 2 klärt das vor jeder Zeile UI-Code |
| Firmware-Update ändert Frames | Features fallen aus | Frames zentral in einer Header-Datei, nicht verstreut |
| BLE-Dauerscan stört Audio/andere LE-Geräte | Aussetzer | Passiver HCI-Scan (in MagicPodsCore vorhanden) statt aktiver Discovery, ggf. Duty-Cycling |
| Windows ohne Treiber | kein ANC | Bewusst als Stufe 16 vs. 17 getrennt |
| GPL | MyPods muss GPL-3.0 sein | Ist ohnehin kein Problem, Repo entsprechend lizenzieren |
| Herzfrequenz nie geknackt | Feature fehlt | Nicht auf dem kritischen Pfad, P5 |

### 3.5 Nächster Schritt

P0 Schritt 4–6, sobald die AirPods da sind. Bis dahin ist alles Vorbereitbare vorbereitet:
Der Baum steht, der Dekoder ist getestet, der Devcontainer baut. Erst die Captures
entscheiden, ob die Annahmen aus Abschnitt 1 für die Pro 3 stimmen — danach P1.

### 3.6 Stand 2026-09-22: Mac-Parität Audio

Umgesetzt im Daemon, ohne neue Abhängigkeit (MPRIS über sdbus-c++, PipeWire als Kindprozess):

* `AapAudioSwitchCapability` — Übernahme bei Wiedergabestart (MPRIS), Freigabe bei
  `SetOwnershipToFalse`/fremder Audioquelle (Pause, A2DP aus, Lautsprecher), manuell „Move here".
* `AapEarDetectionCapability` — `0x06` → Pause/Weiter, Schalter = Control `0x0A`.
* `AapSpatialAudioCapability` / `AapEqualizerCapability` + `audio/AudioEffects` — filter-chain
  (EQ-Biquads → SOFA-Spatializer), Head-Tracking dreht die virtuellen Lautsprecher über `pw-cli`.
* Check: `magicpodscore --selftest` (Smart-Routing-Bytes gegen LibrePods, Parser, Chain-Aufbau).

**(H)** an echter Hardware offen: Übernahme mit iPhone (braucht DeviceID), Vorzeichen/Skalierung
der Kopfbewegung, ob die Pro 3 den Head-Tracking-Start von LibrePods akzeptieren.

---

## Quellen

* Proximity-Pairing-/AAP-Dokumentation: LibrePods (`docs/AAP Definitions.md`), GPL-3.0
* `MagicPodsCore/src/device/capabilities/aap/AppAnimationCapability.cpp` (Popup-Trigger, AES-Payload)
* `AirPodsDesktop/Source/Core/AppleCP.{h,cpp}` (Continuity-Typen, Model-IDs)
* Heinze, Classen, Rohrbach: *MagicPairing: Apple's Take on Securing Bluetooth Peripherals*, WiSec 2020, arXiv:2005.07255
* Celosia, Cunche: *Discontinued Privacy: Personal Data Leaks in Apple Bluetooth-Low-Energy Continuity Protocols*
* Martin et al.: *Handoff All Your Privacy*, arXiv:1904.10600
* LibrePods Android `AACPManager.kt`, `AirPodsService.kt`, `HeadOrientation.kt` (Smart Routing, Takeover, Head-Tracking)
* PipeWire filter-chain `sofa`/`spatializer`, `bq_*` (Doku: `man libpipewire-module-filter-chain`)
* Microsoft: *Bluetooth Echo L2CAP Profile Driver* (bthecho), nefarius/BthPS3 — Windows-L2CAP-Weg
