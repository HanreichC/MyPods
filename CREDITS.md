# Herkunft des Codes

MyPods ist keine Neuentwicklung. Der Grossteil stammt aus bestehenden GPL-3.0-Projekten,
lokal kopiert und angepasst. Das Projekt steht deshalb ebenfalls unter GPL-3.0.

| Herkunft | Autoren | Was uebernommen wurde |
|----------|---------|-----------------------|
| [MagicPodsCore](https://github.com/steam3d/MagicPodsCore) | Aleksandr Maslov, Andrei Litvintsev | `core/` komplett: AAP-SDK, BLE-Advertisement-Auswertung inkl. AES-Entschluesselung und RPA-Pruefung, Geraete-/Capability-Modell, WebSocket-API, Tests |
| [MagicPodsLinux](https://github.com/steam3d/MagicPodsLinux) | Aleksandr Maslov | `ui/` komplett: Qt6/QML-Oberflaeche, `PopupAnimation.qml`, Animations-Sprites, Tray |
| [AirPodsDesktop](https://github.com/SpriteOvO/AirPodsDesktop) | SpriteOvO | Referenz fuer die Continuity-Nachrichtentypen und Model-IDs (`Source/Core/AppleCP.*`); dort liegen auch die Popup-Videos |
| [LibrePods](https://github.com/librepods-org/librepods) | Kavish Devar u. a. | Protokolldokumentation (`docs/AAP Definitions.md`); geplante Uebernahme: Ohrerkennung, Metadaten-Parser, Umbenennen |

Mitgelieferte Bibliotheken (unveraendert, damit der Build ohne Netz laeuft; Lizenztext jeweils daneben):

| Bibliothek | Version | Lizenz | Ort |
|------------|---------|--------|-----|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT | `core/dependencies/json/`, `ui/src/app/dependencies/nlohmann/` |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | MIT | `core/dependencies/toml++/vendor/` |
| [sdbus-c++](https://github.com/Kistler-Group/sdbus-cpp) | 1.6.0 | LGPL-2.1 mit Ausnahme | `core/dependencies/sdbus-cpp/vendor/` |
| [uSockets](https://github.com/uNetworking/uSockets) | 0.8.7 | Apache-2.0 | `core/dependencies/uSockets/vendor/` |
| [uWebSockets](https://github.com/uNetworking/uWebSockets) | 20.58.0 | Apache-2.0 | `core/dependencies/uWebSockets/vendor/` |
| [Inter](https://github.com/rsms/inter) (Schrift) | 4.1 | SIL OFL 1.1 | `ui/src/app/fonts/` |

Protokollwissen ausserdem aus:

* Heinze, Classen, Rohrbach: *MagicPairing: Apple's Take on Securing Bluetooth Peripherals*, WiSec 2020 (arXiv:2005.07255)
* Celosia, Cunche: *Discontinued Privacy: Personal Data Leaks in Apple Bluetooth-Low-Energy Continuity Protocols*
* Martin u. a.: *Handoff All Your Privacy*, arXiv:1904.10600

Apple, AirPods und macOS sind Marken von Apple Inc. Dieses Projekt steht in keiner
Verbindung zu Apple.
