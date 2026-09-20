# Herkunft des Codes

MyPods ist keine Neuentwicklung. Der Grossteil stammt aus bestehenden GPL-3.0-Projekten,
lokal kopiert und angepasst. Das Projekt steht deshalb ebenfalls unter GPL-3.0.

| Herkunft | Autoren | Was uebernommen wurde |
|----------|---------|-----------------------|
| [MagicPodsCore](https://github.com/steam3d/MagicPodsCore) | Aleksandr Maslov, Andrei Litvintsev | `core/` komplett: AAP-SDK, BLE-Advertisement-Auswertung inkl. AES-Entschluesselung und RPA-Pruefung, Geraete-/Capability-Modell, WebSocket-API, Tests |
| [MagicPodsLinux](https://github.com/steam3d/MagicPodsLinux) | Aleksandr Maslov | `ui/` komplett: Qt6/QML-Oberflaeche, `PopupAnimation.qml`, Animations-Sprites, Tray |
| [AirPodsDesktop](https://github.com/SpriteOvO/AirPodsDesktop) | SpriteOvO | Referenz fuer die Continuity-Nachrichtentypen und Model-IDs (`Source/Core/AppleCP.*`); dort liegen auch die Popup-Videos |
| [LibrePods](https://github.com/librepods-org/librepods) | Kavish Devar u. a. | Protokolldokumentation (`docs/AAP Definitions.md`); geplante Uebernahme: Ohrerkennung, Metadaten-Parser, Umbenennen |

Protokollwissen ausserdem aus:

* Heinze, Classen, Rohrbach: *MagicPairing: Apple's Take on Securing Bluetooth Peripherals*, WiSec 2020 (arXiv:2005.07255)
* Celosia, Cunche: *Discontinued Privacy: Personal Data Leaks in Apple Bluetooth-Low-Energy Continuity Protocols*
* Martin u. a.: *Handoff All Your Privacy*, arXiv:1904.10600

Apple, AirPods und macOS sind Marken von Apple Inc. Dieses Projekt steht in keiner
Verbindung zu Apple.
