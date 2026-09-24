# Windows-L2CAP-Treiber für AAP — Plan

Ziel: alle AAP-Funktionen (ANC, Einstellungen, Schlüssel → exakter Akku und RPA-Prüfung,
Übergabe ans iPhone, Head Tracking) auch unter Windows. Windows lässt L2CAP-Verbindungen nur aus dem
Kernel zu; dafür braucht es einen eigenen KMDF-Profiltreiber. Der Daemon bleibt unverändert bis auf
die bereits vorhandene Naht in `core/src/client/Client_win.cpp` (`SupportsL2CAP`, `ConnectToSocketL2CAP`,
`SocketSend`/`SocketReceive`/`SocketClose`).

## Grundregel: Treiber laufen nur in der VM

Ein Kerneltreiber läuft im Kernel des Systems, das ihn lädt. Ein Container schützt davor nicht (er teilt
den Host-Kernel). Deshalb:

* **Gebaut** wird auf dem Host (Kompilieren lädt nichts), mit der portablen Toolchain in
  `%LOCALAPPDATA%\MyPodsToolchain` plus WDK aus NuGet.
* **Geladen und getestet** wird ausschließlich in einer VirtualBox-VM mit Windows 11: Testsigning und
  ausgeschaltetes Secure Boot nur im Gast, vor jeder Treiberinstallation ein Snapshot, Kernel-Debugger
  (KDNET) vom Host auf den Gast. Ein Bluescreen trifft nur die VM; Rückweg ist der Snapshot.
* Auf dem Host ist Secure Boot mit Speicherintegrität aktiv; testsignierte Treiber laden dort ohnehin
  nicht, und das bleibt so.

Bluetooth in der VM: der interne Intel-Adapter (USB `8087:0036`) wird per USB-Filter an den Gast
durchgereicht. **Solange die VM läuft, hat der Host kein Bluetooth**, also auch keine MX Keys/MX Master —
eine USB- oder Kabel-Tastatur/-Maus bereitlegen. Beim Beenden der VM geht der Adapter an den Host zurück.

## Architektur

```
magicpodscore (User-Mode, unverändert bis auf Client_win.cpp)
   │  CreateFile(\\?\<Interface MyPodsAap>)  IOCTL_MYPODS_AAP_OPEN{BTH_ADDR}
   │  WriteFile = ein AAP-Paket raus, ReadFile = ein AAP-Paket rein, CloseHandle = Kanal zu
   ▼
mypodsaap.sys (KMDF-Profiltreiber)
   │  BRB_L2CA_OPEN_CHANNEL (PSM 0x1001), BRB_L2CA_ACL_TRANSFER (in/out), BRB_L2CA_CLOSE_CHANNEL
   ▼
Windows-Bluetooth-Stack (BthPort/BthEnum) ── Adapter ── AirPods
```

* **Andocken:** Ein Profiltreiber braucht ein PDO von BthEnum, um an die Profilschnittstelle
  (`GUID_BTHDDI_PROFILE_DRIVER_INTERFACE`, BRBs) zu kommen. Weg 1 (wie
  [BthPS3](https://github.com/nefarius/BthPS3)): einen *lokalen* Dienst mit eigener GUID registrieren
  (`BluetoothSetLocalServiceInfo`), BthEnum legt dafür ein PDO an, die INF des Treibers passt darauf.
  Von dort aus Kanäle zu beliebigen gekoppelten Geräten öffnen. Weg 2 als Rückfall: das PDO des
  AAP-Dienstes der AirPods selbst (`BluetoothSetServiceState` mit der AAP-UUID), falls die AirPods ihn im
  SDP-Eintrag führen. D2 klärt, welcher Weg trägt.
* **Ein Kanal pro Handle.** Pakete bleiben Pakete (L2CAP ist nachrichtenorientiert wie `SOCK_SEQPACKET`
  unter Linux), ReadFile liefert genau ein empfangenes Paket.
* **Sicherheit (Vertrauensgrenze User→Kernel):**
  * Nur PSM `0x1001` ist erlaubt, fest im Treiber; kein beliebiger L2CAP-Zugang für jeden Prozess.
  * Nur Adressen gekoppelter Geräte.
  * Alle Puffergrößen gegen die ausgehandelte MTU geprüft, keine Längen aus dem User-Mode ungeprüft
    übernommen; METHOD_BUFFERED für das OPEN-IOCTL.
  * Zugriff per SDDL auf SYSTEM, Administratoren und interaktive Benutzer (der Daemon läuft als Benutzer).
* **Lizenz:** Vorlage ist Microsofts `bthecho`-Beispiel (Windows-driver-samples, MIT). Der Treiber ist
  ein eigenes Programm hinter einer IOCTL-Schnittstelle; der GPL-Daemon spricht nur mit ihm.

## Phasen

| | Phase | Ergebnis | Wo |
|---|---|---|---|
| D0 | Testumgebung | VirtualBox, Win11-Gast, Testsigning, Secure Boot aus, KDNET, USB-Filter für den Adapter, Snapshot „sauber“ | Host (Installation) + VM |
| D1 | Toolchain | WDK aus NuGet in der portablen Toolchain; Treiber, INF und Katalog bauen und testsignieren | Host |
| D2 | Machbarkeits-Spike | Treiber lädt auf dem Dienst-PDO, öffnet PSM 0x1001 zu den AirPods Max, sendet den AAP-Handshake und bekommt eine Antwort | VM |
| D3 | Treiber vollständig | Lesen/Schreiben, Trennung durch AirPods/Reichweite/Adapter aus, sauberes Entladen | VM |
| D4 | Daemon-Anbindung | `Client_win.cpp`: `SupportsL2CAP()` = Treiber-Interface vorhanden, Kanal über IOCTL. Dann zeigen sich unverändert ANC, Einstellungen, Schlüssel, exakter Akku, Head Tracking | Host-Build, Test in VM |
| D5 | Härtung | Driver Verifier (Standardregeln + DDI-Konformität), Code-Analyse (`/analyze`), Stress: 1000× Öffnen/Schließen, AirPods aus dem Case/zurück, Adapter aus/an | VM |
| D6 | Verteilung | optionales Treiberpaket im MSI; Signierung siehe unten | — |

Jede Phase endet mit einem laufbaren Check: D2 ein kleines Testprogramm (Handshake → Antwort), D4 der
bestehende Daemon mit echten AirPods, D5 ein Skript, das die Stressfälle unter Driver Verifier fährt.

## Stand

| Phase | Stand |
|---|---|
| D0 | offen: VirtualBox installieren, Windows-11-ISO, VM anlegen |
| D1 | ✅ `driver/windows/build.ps1` baut `mypodsaap.sys` (`/W4 /WX`, `/analyze` mit den WDK-Treiberregeln, Befunde brechen den Build ab), stempelt die INF, erzeugt den Katalog, testsigniert beides und baut `aaptool.exe`. Ausgabe: `build\driver\` |
| D2 | Code fertig, ungetestet: Treiber `driver/windows/mypodsaap.c`, Check `aaptool test <Adresse>` |
| D4 | Code fertig, ungetestet: `Client_win.cpp` nutzt den Treiber, sobald dessen Interface existiert; ohne Treiber unverändert (auf dem Host geprüft) |

Dateien in `driver/windows/`: `mypodsaap.c` (Treiber), `mypodsaap.h` (IOCTL/GUIDs, auch vom Daemon genutzt),
`mypodsaap.inf`, `aaptool.cpp` (Dienst registrieren, D2-Check), `install.ps1` (nur in der VM; bricht
auf echter Hardware ab), `build.ps1`.

## Offene Punkte und Risiken

* **Übergabe ans iPhone:** unter Linux nötig: BlueZ meldet sich mit Apples DeviceID
  (`bluetooth:004C:0000:0000`). Ob sich der Device-ID-Eintrag von Windows ändern lässt, ist offen;
  ohne ihn funktionieren ANC, Einstellungen und Head Tracking trotzdem, nur die automatische Übergabe
  eventuell nicht.
* **Weg 1 vs. Weg 2** (siehe Architektur): entscheidet D2.
* **VirtualBox auf Hyper-V:** Docker/WSL2 halten Hyper-V aktiv, VirtualBox läuft dann über die
  Hyper-V-Plattform (langsamer). Für Treiberentwicklung reicht das; USB-Durchreichung funktioniert.
* **Signierung für normale Nutzer:** Testsignierte Treiber laden nur mit Testsigning, das Secure Boot
  ausschließt. Für echte Verteilung braucht es ein EV-Code-Signing-Zertifikat (≈ 300–500 €/Jahr) und
  Microsofts Attestation-Signierung über das Partner Center. Entscheidung erst nach D5.
* **EQ und Spatial Audio** bleiben außen vor; die bräuchten einen Audio-Treiber (APO), ein eigenes Projekt.
