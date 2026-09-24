# MyPods

**Use AirPods and other Bluetooth headphones on Linux the way they feel on a Mac.**

MyPods is a Linux (and, with fewer features, Windows) desktop app and background daemon for Apple
AirPods and other Bluetooth headphones. It shows the familiar lid-open popup with an animation, reads the exact battery level,
switches noise control, pauses playback when you take a pod out, and hands the AirPods back and
forth between your iPhone and your computer, just like "Connect to This Mac: Automatically".

[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
![Platform: Linux | Windows](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg)
![Qt 6.9+](https://img.shields.io/badge/Qt-6.9%2B-41cd52.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599c.svg)

> [!NOTE]
> MyPods is a personal project in active development, running on CachyOS (Arch-based).
> It has been tested with **AirPods Max** and **Parrot Zik 2.0**. AirPods Pro 3 support is
> implemented from protocol research but has **not been tested on real hardware** yet, and
> neither have the other models listed below. Features marked *unverified* fall into the same
> category.

> [!WARNING]
> **This project is vibe-coded.** Large parts of the new code were written with an AI coding
> assistant and reviewed by hand, on top of the existing MagicPods code base. Expect rough
> edges, and read the code before you rely on it.

---

## Table of contents

- [Features](#features)
- [Supported devices](#supported-devices)
- [How it works](#how-it-works)
- [Requirements](#requirements)
- [Installation](#installation)
- [Windows](#windows)
- [One-time system setup](#one-time-system-setup)
- [Usage](#usage)
- [Configuration](#configuration)
- [Building from source](#building-from-source)
- [Testing](#testing)
- [WebSocket API](#websocket-api)
- [Known limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [Security](#security)
- [Credits](#credits)
- [License](#license)

---

## Features

### AirPods and Beats

| On a Mac | In MyPods |
|----------|-----------|
| Popup with animation when you open the case | Same popup, driven by the BLE advertisements the AirPods broadcast. Works before they are even connected. |
| Exact battery in 1 % steps for left, right and case | Battery decrypted from the advertisement payload and read over AAP, including charging state. |
| Noise control: Off / Transparency / Adaptive / Noise Cancellation | Same modes, plus the adaptive noise level slider (0–100). |
| Automatic ear detection | Taking a pod out pauses playback, putting it back resumes it (MPRIS). The switch is stored on the AirPods themselves. |
| "Connect to This Mac: Automatically" | When a player starts here (browser, Spotify, anything with MPRIS), MyPods takes the AirPods over from your iPhone: connects if needed, performs Apple's smart-routing handoff, enables A2DP and makes them the default output. When the iPhone starts playing, the computer pauses and falls back to its speakers. It never takes over during a call on the other device. |
| "Moved to iPhone — Move back" banner | The device page shows "Playing on iPhone — Move here". |
| "When Last Connected to This Mac" | Same option; switching then happens only manually. |
| Spatial Audio: Off / Fixed / Head Tracked | PipeWire filter chain with an HRTF (libmysofa). Head orientation streams from the AirPods. |
| Equalizer (Music app) | The Apple Music presets (Acoustic, Bass Booster, Classical, Rock, Vocal Booster …), applied in front of the AirPods. |
| Conversation Awareness | On/off and current state. |
| AirPods module in the menu bar / Control Center | Clicking the tray icon opens a popup right under the panel: battery, noise control, Conversation Awareness, "Move here", what's playing with play/pause/skip, and the output volume. A double click opens the full window. |
| Press speed, press-and-hold duration, volume swipe, tone volume, personalized volume, mute/end call, ANC with one AirPod | Same settings, written to the AirPods. |

Also available: Bluetooth codec display, battery level in the tray icon, autostart,
a Steam Deck / gamescope mode inherited from MagicPods, and English UI strings with
translation support via Qt Linguist.

### Parrot Zik 2.0

Controlled through the headphones' own RFCOMM XML API. Just pair it; MyPods recognizes it by its
service UUID.

- Battery level and charging state
- Noise control: ANC and Street Mode, each in normal or max strength. There is deliberately no
  "Off" option, because it mutes the Zik 2.
- Head detection (auto-pause)
- Concert Hall effect with room size (silent, living, jazz, concert) and angle (30°–180°)
- Equalizer on the headphone's DSP, using the same presets as the AirPods equalizer
- Smart Audio Tune, ANC during phone calls, voice prompts, auto connection, auto power off

### Samsung Galaxy Buds

Battery and noise control, inherited from MagicPodsCore (Galaxy Buds, Buds+, Live, Pro, Buds2,
Buds2 Pro, FE, Core, Buds3, Buds3 Pro, Buds3 FE, Buds4, Buds4 Pro).

### Any other headset

Any paired device with the Hands-Free profile shows its battery level (as reported through HFP)
and the active Bluetooth codec.

---

## Supported devices

| Family | Models | Status |
|--------|--------|--------|
| AirPods Max | Max (`0x200A`), Max USB-C (`0x201F`), Max 2 (`0x202D`) | **Tested** (AirPods Max). Max 2 popup trigger is *unverified*. |
| AirPods Pro | AirPods Pro 3 (A3063 / A3064 / A3065, model ID `0x2027`) | Implemented, *not tested on hardware* |
| Other AirPods | AirPods 1–4, AirPods 4 ANC, AirPods Pro, Pro 2, Pro 2 USB-C | Recognized by the inherited AAP stack, not tested by this project |
| Beats | Powerbeats Pro / Pro 2 / 3 / 4 / Fit, Beats Fit Pro, Studio Buds / Buds+, Studio Pro, Studio 3, Solo 3 / Pro / 4 / Buds, Flex, BeatsX | Recognized by the inherited AAP stack, not tested by this project |
| Parrot | Zik 2.0 | **Tested** |
| Samsung | Galaxy Buds series (see above) | Inherited from MagicPodsCore, not tested by this project |
| Generic | Any Hands-Free (HFP) headset | Battery and codec only |

Linux gets every feature. Windows gets the subset that works without a kernel driver, see [Windows](#windows).

---

## How it works

Apple headphones talk to their host over two separate channels, and most tools only use one of them.
MyPods uses both.

```
                 ┌──────────────────────── AirPods ────────────────────────┐
                 │                                                         │
   BLE advertisements (Continuity,                     L2CAP, PSM 0x1001
   "Proximity Pairing", type 0x07)                     (AAP / "AACP")
   no connection needed                                needs a classic connection
                 │                                                         │
                 ▼                                                         ▼
 ┌───────────────────────────────── magicpodscore (daemon) ─────────────────────────────────┐
 │  lid state, coarse battery,       │  exact battery, ANC, ear detection, settings,       │
 │  AES-decrypted exact battery,     │  IRK/ENC key exchange, smart routing (handoff),     │
 │  RPA check → popup trigger        │  head tracking                                      │
 │                                                                                          │
 │  BlueZ (D-Bus) · PulseAudio/PipeWire · MPRIS (D-Bus) · PipeWire filter chain (EQ, HRTF)  │
 │  settings in ~/.config/mypods/config.toml                                                │
 └───────────────────────────────────────┬──────────────────────────────────────────────────┘
                                         │ WebSocket, JSON, port 2020
                                         ▼
                        ┌──────── magicpods (Qt 6 / QML UI) ────────┐
                        │ tray icon, device pages, settings,        │
                        │ lid-open popup animation                  │
                        │ starts the daemon on launch               │
                        └───────────────────────────────────────────┘
```

1. **BLE advertisements.** AirPods constantly broadcast Apple manufacturer data (`0x004C`).
   The Proximity Pairing message contains the model, lid counter, in-ear and in-case bits,
   coarse battery nibbles and a 16-byte AES-128 encrypted block with the exact battery.
   This is what makes the popup appear instantly, even before the AirPods are connected.
2. **AAP over L2CAP.** Once connected, the daemon opens an L2CAP socket on PSM `0x1001`, sends the
   handshake and subscribes to notifications. From then on the AirPods push battery, noise control,
   ear detection and more, and accept settings changes.
3. **Keys.** To recognize *your* AirPods among rotating BLE addresses (RPA) and to decrypt the
   battery block, MyPods needs the IRK and ENC key. It requests them from the AirPods over AAP once
   and stores them. No iCloud involved.
4. **Audio switching.** The AirPods relay Apple's "smart routing" messages between their sources.
   MyPods participates in that exchange, and watches MPRIS players to decide when to take over.
5. **Effects.** Equalizer and spatial audio run as a PipeWire filter-chain sink (`mypods_fx`)
   in front of the headphones. Head tracking rotates the virtual speakers via `pw-cli`.

A detailed write-up of the protocols and design decisions is in [docs/PLAN.md](docs/PLAN.md)
(German).

---

## Requirements

**Runtime (host)**

- Linux with **BlueZ** and **PipeWire** (with `pipewire-pulse`, since the daemon uses libpulse)
- **Qt 6.9 or newer**: `qt6-base`, `qt6-declarative`, `qt6-websockets`, `qt6-svg`
- `libpulse`, `openssl`, `systemd-libs`
- `libmysofa` (only for spatial audio; the default HRTF is read from `/usr/share/libmysofa/default.sofa`)
- An MPRIS-capable media player for auto-pause, automatic switching and the tray popup's now playing (browsers, Spotify, mpv with `mpv-mpris`, …)
- `pactl` (part of `libpulse`) for the tray popup's volume slider
- Optional: `layer-shell-qt` (ships with KDE Plasma) to place the tray popup under the panel on Wayland

**Build**

- **Podman** — the build runs inside a container, nothing is installed on the host

The container is based on Arch Linux so the binaries match an Arch-based host (Arch, CachyOS,
EndeavourOS, …). On other distributions, build natively instead (see
[Building from source](#building-from-source)).

On Arch-based systems, the runtime packages are:

```bash
sudo pacman -S --needed bluez pipewire pipewire-pulse libpulse openssl \
    qt6-base qt6-declarative qt6-websockets qt6-svg libmysofa layer-shell-qt
```

---

## Installation

### AppImage (any distribution)

Download `MyPods-x86_64.AppImage` from the [latest release](https://github.com/HanreichC/MyPods/releases/latest),
then run `chmod +x MyPods-x86_64.AppImage && ./MyPods-x86_64.AppImage`. Qt is bundled; BlueZ, PipeWire
and libmysofa still come from the host. Every push to `main` publishes a new build.
To build it yourself: `podman run --rm -v "$PWD:/workspace:Z" -w /workspace docker.io/library/ubuntu:22.04 ./appimage.sh`

### From source (Arch-based)

```bash
git clone https://github.com/HanreichC/MyPods.git
cd MyPods
./install.sh
```

`install.sh` builds MyPods in the Podman container and installs it for the current user.
No root required.

| What | Where |
|------|-------|
| UI binary | `~/.local/opt/mypods/magicpods` |
| Daemon binary | `~/.local/opt/mypods/modules/magicpodscore` |
| Start menu entry | `~/.local/share/applications/app.magicpods.desktop` |
| Autostart entry (starts hidden in the tray) | `~/.config/autostart/app.magicpods.desktop` |
| Icon | `~/.local/share/icons/magicpods.png` |

Run the script again to update. It stops the running instance, reinstalls and starts it again.

**Uninstall**

```bash
pkill -x magicpods; pkill -x magicpodscore
rm -rf ~/.local/opt/mypods \
       ~/.local/share/applications/app.magicpods.desktop \
       ~/.config/autostart/app.magicpods.desktop \
       ~/.local/share/icons/magicpods.png
rm -rf ~/.config/mypods   # optional: settings and stored AirPods keys
```

---

## Windows

Download `MyPods-x64.msi` from the [latest release](https://github.com/HanreichC/MyPods/releases/latest)
and install it. It installs to `C:\Program Files\MyPods`, adds a Start menu entry and starts MyPods
hidden in the tray at login. Settings live in `%APPDATA%\mypods\config.toml`.

Windows only lets kernel-mode drivers open L2CAP channels, and every AirPods setting travels over
L2CAP (AAP). MyPods ships no driver, so on Windows it does what user space allows:

| Feature | Windows |
|---------|---------|
| Device list, connect/disconnect, Bluetooth on/off | Yes (WinRT, Bluetooth audio driver) |
| Lid-open popup with animation | Yes, from the BLE advertisements |
| AirPods battery in the app and tray | Yes, from the advertisements: 10 % steps, 1 % with imported keys |
| Ear detection (pause/resume) | Yes, from the advertisements; on/off is stored locally |
| Tray popup: now playing, media keys, volume | Yes (system media sessions, Core Audio) |
| Parrot Zik 2.0, Galaxy Buds (RFCOMM) | Yes, all features |
| HFP battery of other headsets | Yes |
| Noise control, Conversation Awareness, press/swipe settings | No, needs AAP |
| Automatic switching with the iPhone ("Move here") | No, needs AAP |
| Spatial audio, equalizer for AirPods | No, would need an audio driver (APO) |
| Codec display and switching | No, Windows exposes neither |

**Recognizing your AirPods.** On Linux MyPods fetches the AirPods' IRK and ENC keys over AAP. Without
AAP it treats the AirPods of your paired model within about a meter as yours, which gets confused
when a second pair of the same model is right next to you. If you also use MyPods on Linux, copy the
`irk` and `enc` lines from your AirPods' table in `~/.config/mypods/config.toml` into the same table
in `%APPDATA%\mypods\config.toml` (the table name is the AirPods' address). MyPods then verifies them
properly and shows the exact battery.

---

## One-time system setup

For automatic switching between your iPhone and this computer, BlueZ must identify itself as an
Apple device. Otherwise the iPhone and the AirPods do not include the computer in the handoff.
Everything else works without this step.

```bash
sudo sed -i '/^\[General\]/a DeviceID = bluetooth:004C:0000:0000' /etc/bluetooth/main.conf
sudo systemctl restart bluetooth
```

Then reconnect the AirPods once. `install.sh` reminds you if the line is missing.

> [!TIP]
> Setting the Apple DeviceID also unlocks additional features on the AirPods side,
> such as multipoint handoff behavior.

---

## Usage

1. **Pair your headphones** with the system as usual. For AirPods, hold the button on the case
   until the light flashes white, then pair through your desktop's Bluetooth settings or
   `bluetoothctl`.
2. **Start MyPods** from the start menu (it also starts automatically at login). The UI launches
   the daemon by itself.
3. **Open the case** near your computer. The popup appears with the animation and battery levels.
4. **Click the tray icon** for the popup: battery, noise control, Conversation Awareness, what's
   playing with media controls, and the volume. It closes on a second click, Esc or a click
   elsewhere. Without connected headphones, a click opens the window instead.
5. **Double-click the tray icon** (or choose **MyPods Settings…** in the popup) for the full
   window: ear detection, automatic switching, spatial audio, equalizer and all device-specific
   settings. Right-click opens the context menu with connect/disconnect and Exit.

Command-line options:

```bash
magicpods --hidden            # start minimized to the tray (used by autostart)
magicpodscore --version       # print daemon version
magicpodscore --selftest      # run the built-in byte-level checks, no hardware needed
```

Starting `magicpods` a second time brings the existing window to the front instead of launching
another instance.

---

## Configuration

Settings are stored by the daemon in TOML:

```
${XDG_CONFIG_HOME:-~/.config}/mypods/config.toml
```

Most options are set through the UI. Global options live in the `[magicpods]` table:

| Key | Default | Meaning |
|-----|---------|---------|
| `animation` | `true` | Enables the BLE scan used for the lid-open popup. The scan only runs while AirPods are actually paired, and it leaves the discovery transport at its default so paired Bluetooth LE mice and keyboards keep reconnecting — see [Troubleshooting](#troubleshooting). |
| `logLevel` | Info | Daemon log verbosity (debug builds always log at debug level). |

Per-device settings (for example the stored `irk`/`enc` keys, switching mode, spatial audio and
equalizer choice) are saved in a table named after the device. Deleting the keys forces MyPods to
request them again on the next connection.

---

## Building from source

### With Podman (recommended)

Nothing gets installed globally; the toolchain lives in the dev container
([.devcontainer/Dockerfile](.devcontainer/Dockerfile)).

```bash
podman build -t mypods-dev -f .devcontainer/Dockerfile .devcontainer
podman run --rm -v "$PWD:/workspace" -w /workspace mypods-dev \
    sh -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)'
```

Output:

- `build/magicpods` — the UI
- `build/modules/magicpodscore` — the daemon

Run `./build/magicpods` on the host. The UI looks for the daemon in `modules/` next to its own
binary and starts it.

For a debug build, use `-DCMAKE_BUILD_TYPE=Debug`. This enables debug logging and runs the
self-tests on daemon startup.

### Windows

Visual Studio 2022 or newer with the C++ workload, Qt 6.9 for MSVC (with Qt WebSockets) and vcpkg.
From a *Developer PowerShell*:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH=C:\Qt\6.9.3\msvc2022_64 `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build
build\modules\magicpodscore.exe --selftest
```

vcpkg installs OpenSSL, libuv and zlib from [vcpkg.json](vcpkg.json). The MSI is built by
[.github/workflows/windows.yml](.github/workflows/windows.yml) with WiX 5 from
[packaging/windows/MyPods.wxs](packaging/windows/MyPods.wxs).

### In VS Code / VSCodium

The repository ships a dev container definition
([.devcontainer/devcontainer.json](.devcontainer/devcontainer.json)). With a dev container
extension installed, run **Devcontainer: Open Folder in container**. The container uses host
networking and the host's system D-Bus socket, so the daemon can talk to BlueZ from inside it.

### Natively

On a distribution other than Arch, install the equivalents of the packages in the Dockerfile
(CMake ≥ 3.22, a C++20 compiler, pkg-config, Qt 6.9+ incl. Qt Linguist tools, BlueZ headers,
libpulse, OpenSSL, libsystemd) and run the same two CMake commands directly. All other libraries
(sdbus-c++, uWebSockets, uSockets, nlohmann/json, toml++) are vendored and build offline.

---

## Testing

MyPods follows a simple rule: every piece of non-trivial logic has one runnable check, no test
framework required. All but one run without hardware.

```bash
# Daemon: AAP battery/ANC parsing, BLE advertisement decoding, smart routing packets,
# audio effect chain, Galaxy Buds and Parrot Zik protocol
./build/modules/magicpodscore --selftest

# Emulated AirPods Max through the real audio path (spatial audio, head tracking, EQ, routing).
# Needs PipeWire, no Bluetooth; briefly switches the default sink to a fake headphones sink.
./build/modules/magicpodscore --emulate-airpods

# BLE advertisement decoder in the sniffing tool, against known captures
python3 tools/sniff.py --selftest
```

Two additional standalone checks for the ANC and battery wire paths live in
[core/src/tests/AncSelfCheck.cpp](core/src/tests/AncSelfCheck.cpp) and
[core/src/tests/BatterySelfCheck.cpp](core/src/tests/BatterySelfCheck.cpp); the compile command
is at the top of each file.

One check does need hardware, because the behavior it guards only exists on a real adapter:

```bash
# With MyPods running: fails if a discovery session is held although no Apple device is
# paired, which is what stops Bluetooth LE mice and keyboards from reconnecting
sh tools/check_no_idle_discovery.sh

# With MyPods running: remove the headphones in bluetoothctl, start this, then pair them;
# passes once the core pushes a device list containing them, without a restart
python3 tools/check_new_pairing.py AA:BB:CC:DD:EE:FF
```

### Capturing from real hardware

[tools/sniff.py](tools/sniff.py) records and decodes Apple BLE advertisements through BlueZ over
D-Bus. It needs no root.

```bash
python3 tools/sniff.py                                         # live view
python3 tools/sniff.py --log captures/pro3.txt --seconds 120   # record to file
python3 tools/sniff.py --all                                   # also show other Apple message types
```

While it runs, open and close the lid a few times and take the pods in and out. The captures
confirm model IDs and the lid counter behavior, and become test vectors for the parser.

AirPods Max have no charging case. For them, the popup trigger is the high nibble of byte 8
(`== 8`) instead of the case state.

---

## WebSocket API

The UI and the daemon are separate processes that talk JSON over a WebSocket on port `2020`.
This makes it possible to write alternative front ends, scripts or integrations.

```bash
# Example with websocat
echo '{"method":"GetAll"}' | websocat -n1 ws://localhost:2020/
```

Available methods: `GetAll`, `GetDevices`, `ConnectDevice`, `DisconnectDevice`,
`GetActiveDeviceInfo`, `SetCapabilities`, `GetDefaultBluetoothAdapter`,
`EnableDefaultBluetoothAdapter`, `DisableDefaultBluetoothAdapter`, `GetSettingsAll`,
`GetSettings`, `GetSetting`, `SetSetting`. The daemon also broadcasts changes (capabilities,
connection state, active device, adapter state, settings, popup trigger) to every connected client.

The full reference with request and response examples is in
[docs/core-api-reference.md](docs/core-api-reference.md).

---

## Known limitations

- **Generic HRTF.** Spatial audio uses the generic KEMAR HRTF shipped with libmysofa. Apple
  personalizes it from a scan of your ears. You can replace the SOFA file with a personal one.
- **Head tracking calibration.** Interpreting the head-tracking stream is a heuristic taken from
  LibrePods and not yet calibrated on real hardware.
- **No takeover for non-MPRIS audio.** Games and system sounds deliberately do not trigger
  automatic switching, same as on a Mac.
- **iPhone handoff** requires the Apple DeviceID in BlueZ (see
  [One-time system setup](#one-time-system-setup)).
- **Not implemented:** heart rate (AirPods Pro 3), Find My and the case speaker. These parts of
  Apple's protocol have not been reverse engineered publicly.
- **Binary compatibility.** Binaries built by `install.sh` target Arch-based systems.
- **Tray popup placement.** Wayland doesn't let apps position their windows, so the popup is a
  layer-shell surface anchored to the top-right corner: right for a panel at the top, wrong for
  one at the bottom. The AppImage is built without `layer-shell-qt`, so there the compositor
  places the popup. On X11 it opens at the pointer, for a panel on any edge.
- **Tray double click** is two clicks within the system's double-click interval, since the KDE
  tray protocol (StatusNotifierItem) has none. The popup opens on the first click and gives way
  to the window on the second.

---

## Troubleshooting

**The popup does not appear.**
Check that the `animation` setting is enabled and that Bluetooth is on. Run
`python3 tools/sniff.py` and open the case; if nothing shows up, the adapter is not receiving the
advertisements. The popup also needs the IRK/ENC keys, which are fetched the first time the
AirPods connect to MyPods.

**My Bluetooth mouse or keyboard stopped reconnecting on its own.**
This was fixed in the lid-open popup's BLE scan: it no longer sets an LE-only discovery
filter. An LE-only discovery scans without a gap and the kernel never gets to finish the
allowlist auto-connect that paired Bluetooth LE devices need, so they stay disconnected for
as long as MyPods scans. The default transport interleaves BR/EDR inquiry with the LE scan
and leaves exactly those gaps, at no measurable cost to advertisement throughput. If you
still see it on an older build, `animation = false` disables the scan.

**Noise control and settings are missing.**
These need an active classic connection (A2DP/HFP). Connect the AirPods first. If they still do not
show up, make sure no other tool (for example LibrePods) holds the L2CAP channel.

**Automatic switching does not work.**
Verify the `DeviceID = bluetooth:004C:0000:0000` line in `/etc/bluetooth/main.conf`, restart
Bluetooth and reconnect the AirPods. Switching is also skipped while a call is active on the other
device, and only MPRIS players trigger it.

**The UI says it cannot reach the daemon.**
Something else might be using port 2020, or an old daemon is still running:
`pkill -x magicpodscore` and start MyPods again. Run
`~/.local/opt/mypods/modules/magicpodscore` in a terminal to see its log.

**Spatial audio has no effect.**
Check that `/usr/share/libmysofa/default.sofa` exists (package `libmysofa`) and that applications
play to the `mypods_fx` sink.

---

## Security

- The daemon's WebSocket API has **no authentication**, and it listens on port 2020 on all network
  interfaces. Anyone who can reach that port can read device state and change settings. Block the
  port in your firewall if your machine is on an untrusted network.
- The AirPods' IRK and ENC keys are stored in plain text in `~/.config/mypods/config.toml`.
  Keep that file private.

---

## Credits

MyPods builds on existing open-source work. Most of the code comes from these GPL-3.0 projects,
copied locally and adapted:

- [MagicPodsCore](https://github.com/steam3d/MagicPodsCore) by Aleksandr Maslov and Andrei
  Litvintsev: the daemon, AAP stack, BLE decoding, device and capability model, WebSocket API
- [MagicPodsLinux](https://github.com/steam3d/MagicPodsLinux) by Aleksandr Maslov: the Qt/QML UI,
  popup animation, sprites and tray
- [LibrePods](https://github.com/librepods-org/librepods): protocol documentation, smart routing
  and head tracking
- [AirPodsDesktop](https://github.com/SpriteOvO/AirPodsDesktop) by SpriteOvO: Continuity message
  types and model IDs
- [zik2ctl](https://github.com/kradhub/zik2ctl) and [pyParrotZik](https://github.com/m0sia/pyParrotZik):
  Parrot Zik 2.0 protocol

The full list, including vendored libraries and academic papers, is in [CREDITS.md](CREDITS.md).

---

## License

MyPods is licensed under the [GNU General Public License v3.0](LICENSE), because the code it is
based on is GPL-3.0. Vendored libraries keep their own licenses (MIT, Apache-2.0, LGPL-2.1 with
exception, SIL OFL 1.1), see [CREDITS.md](CREDITS.md).

Apple, AirPods, Beats and macOS are trademarks of Apple Inc. Parrot and Zik are trademarks of
Parrot. Samsung and Galaxy Buds are trademarks of Samsung Electronics Co., Ltd.
This project is not affiliated with or endorsed by any of them.
