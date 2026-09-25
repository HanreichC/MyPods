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
| "Connect to This Mac: Automatically" | When a player starts here (browser, Spotify, anything with MPRIS), MyPods takes the AirPods over from your iPhone: connects if needed, performs Apple's smart-routing handoff, enables A2DP and makes them the default output. When the iPhone starts playing, the computer pauses and falls back to its speakers. It never takes over during a call on the other device. The iPhone's banner names this computer by its Bluetooth name. |
| "Moved to iPhone — Move back" banner | The device page shows "Playing on iPhone — Move here". |
| Conversation Awareness lowers the volume while you speak | Same: while the AirPods report that you're speaking, the computer lowers their volume (to 30 % of its level by default, adjustable) and brings it back afterwards. |
| Digital Crown direction (AirPods Max), sleep detection, "Connect to This Mac" automatically | Same switches, for the AirPods that report them. |
| Several headphones connected: pick which one to control | A picker at the top of the device page; the tray and popup follow it. The headphones connected last become the active ones. |
| Press and hold: which modes to cycle through, "Off" allowed or not | Same settings, written to the AirPods. |
| Microphone: Automatic / Always Left / Always Right | Same, for the AirPods that report it. |
| Loud Sound Reduction, customized transparency (amplification, balance, tone, ambient noise reduction, conversation boost) | Same, for AirPods Pro 2 and 3 over their ATT channel. *Unverified on hardware.* |
| Hearing Aid on/off | Same switch, once the hearing test was set up on an iPhone. |
| Battery in the menu bar and in Bluetooth settings | The exact level is handed to BlueZ, so the desktop's Bluetooth applet and UPower show it too. |
| "When Last Connected to This Mac" | Same option; switching then happens only manually. |
| Spatial Audio: Off / Fixed / Head Tracked | PipeWire filter chain with an HRTF (libmysofa), equalized so centered sound stays neutral (within 0.75 dB from 31 Hz to 8 kHz, measured). Head orientation streams from the AirPods. |
| Equalizer (Music app) | The Apple Music presets (Acoustic, Bass Booster, Classical, Rock, Vocal Booster …), applied in front of the AirPods without clipping. |
| Apple tunes the sound for each model | Headphone correction: measured [AutoEQ](https://github.com/jaakkopasanen/AutoEq) filters that bring the model to the Harman target, for AirPods 1–4, Pro, Pro 2, Max and most Beats (not Pro 3 or Max 2, nobody measured them yet). |
| — | Crossfeed (bs2b style) for stereo music without spatial audio: each ear also hears a bit of the other channel's bass, as with speakers. Mono stays untouched. |
| Spatial Audio for films (Dolby Atmos) | "Surround (7.1)": the chain takes 5.1/7.1 and places seven virtual speakers (ITU layout, sides at 90°, rears at 135°), all of them head tracked; the LFE goes to both ears. Stereo players are then upmixed by PipeWire. |
| — | Lookahead limiter at the end of the chain (with `swh-plugins`): spatial audio then plays 7 dB louder, and head turns past 45° no longer clip (measured: −1.0 dBFS peak where the chain without it reached +0.9 dBFS). |
| — | Loudness compensation (ISO 226): the quieter you listen, the more bass comes back, following the volume live. |
| Headphone Accommodations (audiogram) | Hearing profile: an audiogram per ear (six thresholds from 250 Hz to 8 kHz) becomes a separate EQ for each ear (half-gain rule, at most 20 dB). |
| — | Your own correction: an AutoEQ or Equalizer APO `ParametricEQ.txt` (setting `eqFile`) replaces the built-in one, for your own measurement, another target, or models nobody measured yet. |
| — | "Compare without effects (A/B)": switches every effect off but keeps the pre-gain, so both sides play equally loud and louder never passes for better. |
| Conversation Awareness | On/off and current state. |
| AirPods module in the menu bar / Control Center | Clicking the tray icon opens a popup right under the panel: battery, noise control, Conversation Awareness, "Move here", what's playing with play/pause/skip, and the output volume. A double click opens the full window. |
| Press speed, press-and-hold duration, volume swipe, tone volume, personalized volume, mute/end call, ANC with one AirPod | Same settings, written to the AirPods. |
| Rename the AirPods; model number, serial number and firmware in the "About" pane | Same, in the "Device" section of the device page. The name is stored on the AirPods, so every paired device sees it. |
| "Low battery" notification | A desktop notification when an AirPod or the Max drops to 10 %. It comes back after charging or once the level is above 20 %. |

Also available: Bluetooth codec display, battery level in the tray icon, autostart,
[keyboard shortcuts](#keyboard-shortcuts) for noise control and handoff, a Steam Deck / gamescope
mode inherited from MagicPods, and an English and German UI (Qt Linguist).

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
Buds2 Pro, FE, Core, Buds3, Buds3 Pro, Buds3 FE, Buds4, Buds4 Pro), plus ear detection: taking a bud
out pauses, putting it back resumes (switchable, stored locally; not on the Buds Core).

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
   On Linux the scan pauses while AirPods are connected: AAP then delivers the same information,
   and the scan's classic inquiry can make A2DP audio stutter.
2. **AAP over L2CAP.** Once connected, the daemon opens an L2CAP socket on PSM `0x1001`, sends the
   handshake and subscribes to notifications. From then on the AirPods push battery, noise control,
   ear detection and more, and accept settings changes. If the AirPods close the channel while the
   Bluetooth link stays up, the daemon opens it again (up to five times per connection).
   AirPods Pro 2 and 3 keep a few more settings (Loud Sound Reduction, customized transparency)
   as GATT characteristics, which the daemon reads and writes over a second L2CAP channel (ATT, PSM `0x1F`).
3. **Keys.** To recognize *your* AirPods among rotating BLE addresses (RPA) and to decrypt the
   battery block, MyPods needs the IRK and ENC key. It asks the AirPods for them over AAP on every
   connection and stores them when they change. No iCloud involved.
4. **Audio switching.** The AirPods relay Apple's "smart routing" messages between their sources.
   MyPods participates in that exchange, and watches MPRIS players to decide when to take over.
5. **Effects.** Equalizer, headphone correction, crossfeed, loudness, hearing profile and spatial
   audio run as a PipeWire filter-chain sink (`mypods_fx`) in front of the headphones: a pre-gain per
   input channel, then spatial audio or crossfeed, then the filters per ear, then the limiter. The
   pre-gain takes off exactly as much as the chain can boost (computed from the summed filter
   curves), so nothing clips. Head tracking and the volume (for the loudness compensation) update the
   running chain via `pw-cli`.

A detailed write-up of the protocols and design decisions is in [docs/PLAN.md](docs/PLAN.md)
(German).

---

## Requirements

**Runtime (host)**

- Linux with **BlueZ** and **PipeWire** (with `pipewire-pulse`, since the daemon uses libpulse)
- **Qt 6.9 or newer**: `qt6-base`, `qt6-declarative`, `qt6-websockets`, `qt6-svg`
- `libpulse`, `openssl`, `systemd-libs`
- `libmysofa` (only for spatial audio; the default HRTF is read from `/usr/share/libmysofa/default.sofa`)
- Optional: `swh-plugins` for the lookahead limiter (LADSPA `fast_lookahead_limiter_1913.so`, found in
  `$LADSPA_PATH` or `/usr/lib/ladspa`). Without it spatial audio plays 7 dB quieter to stay clean.
- An MPRIS-capable media player for auto-pause, automatic switching and the tray popup's now playing (browsers, Spotify, mpv with `mpv-mpris`, …)
- `pactl` (part of `libpulse`) for the tray popup's volume slider
- Optional: `layer-shell-qt` (ships with KDE Plasma) to place the tray popup under the panel on Wayland
- A system tray. KDE Plasma, Cinnamon, XFCE and most panels have one; GNOME needs the
  *AppIndicator and KStatusNotifierItem Support* extension. Without a tray, MyPods opens as a window
  and the daemon keeps working in the background.
- For the battery level in the system's Bluetooth settings: a BlueZ with the battery provider API
  (current versions; older ones need `bluetoothd --experimental`). Everything else works without it.

**Build**

- **Podman** — the build runs inside a container, nothing is installed on the host

The container is based on Arch Linux so the binaries match an Arch-based host (Arch, CachyOS,
EndeavourOS, …). On other distributions, build natively instead (see
[Building from source](#building-from-source)).

On Arch-based systems, the runtime packages are:

```bash
sudo pacman -S --needed bluez pipewire pipewire-pulse libpulse openssl \
    qt6-base qt6-declarative qt6-websockets qt6-svg libmysofa swh-plugins layer-shell-qt
```

---

## Installation

### AppImage (any distribution)

Download `MyPods-x86_64.AppImage` from the [latest release](https://github.com/HanreichC/MyPods/releases/latest),
then run `chmod +x MyPods-x86_64.AppImage && ./MyPods-x86_64.AppImage`. Qt is bundled; BlueZ, PipeWire
and libmysofa still come from the host. Every push to `main` is a release
(`vX.Y.N`, the version the app shows).
To build it yourself: `podman run --rm -v "$PWD:/workspace:Z" -w /workspace docker.io/library/ubuntu:22.04 ./appimage.sh`

### Arch package

[packaging/arch/PKGBUILD](packaging/arch/PKGBUILD) builds a `mypods-git` package from the latest
commit on GitHub (AUR style), not from your local checkout. It installs to `/usr/lib/mypods`, with
`magicpods` and `magicpodscore` in `/usr/bin`, a start menu entry and the daemon's systemd user unit.

```bash
cd packaging/arch && makepkg -si
systemctl --user enable --now mypods-core.service   # once per user

# uninstall
systemctl --user disable --now mypods-core.service
sudo pacman -R mypods-git
```

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
| Daemon as systemd user service | `~/.config/systemd/user/mypods-core.service` |
| Icon | `~/.local/share/icons/magicpods.png` |

The daemon runs as a systemd user service, independent of the tray app: ear detection, automatic
switching and the effects keep working after you quit the tray app, and systemd restarts the daemon
if it crashes. Its log is in `journalctl --user -u mypods-core`. Without systemd (and in the AppImage)
the tray app starts the daemon itself, as before. Without a Bluetooth adapter the daemon exits, and
systemd (or the tray app) starts it again until one shows up; it uses BlueZ's first adapter, which need
not be `hci0`. The same happens when the adapter goes away or bluetoothd restarts or crashes.

Run the script again to update. It stops the running instance, reinstalls and starts it again.

**Uninstall**

```bash
systemctl --user disable --now mypods-core.service
pkill -x magicpods; pkill -x magicpodscore
rm -rf ~/.local/opt/mypods \
       ~/.local/share/applications/app.magicpods.desktop \
       ~/.config/autostart/app.magicpods.desktop \
       ~/.config/systemd/user/mypods-core.service \
       ~/.local/share/icons/magicpods.png
systemctl --user daemon-reload
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
| Noise control, Conversation Awareness, press/swipe settings, microphone, hearing aid | No, needs AAP |
| Loud Sound Reduction, customized transparency | No, needs an ATT channel over L2CAP |
| Battery level in the system's Bluetooth settings | No, Windows takes no levels from applications |
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

### Keyboard shortcuts

`magicpods --action <name>` changes the connected headphones through the daemon and exits, without
opening a window. Bind it to a key in your desktop's shortcut settings (KDE: *System Settings →
Keyboard → Shortcuts → Add New → Command*, GNOME: *Settings → Keyboard → Custom Shortcuts*); that
works on X11 and Wayland alike.

| Action | Does |
|--------|------|
| `noise-next` | Next noise control mode the headphones offer |
| `noise-off`, `noise-anc`, `noise-transparency`, `noise-adaptive` | That noise control mode |
| `conversation-awareness` | Conversation Awareness on/off |
| `move-here` | Take the AirPods over from the iPhone ("Move here") |
| `eq-next` | Next equalizer preset (Linux) |
| `effects-bypass` | Compare with and without effects (A/B) on/off (Linux) |

It exits with status 1 and a message when the daemon isn't running or the headphones can't do it.

---

## Configuration

Settings are stored by the daemon in TOML:

```
${XDG_CONFIG_HOME:-~/.config}/mypods/config.toml
```

Most options are set through the UI. Global options live in the `[magicpods]` table:

| Key | Default | Meaning |
|-----|---------|---------|
| `animation` | `true` | Shows the lid-open popup. The BLE scan behind it runs only while AirPods are paired and none is connected (on Windows also while connected, the advertisements are all it has). Automatic switching uses the same scan to tell whether you wear the AirPods, so with "Automatically" selected it keeps running when the popup is off — see [Troubleshooting](#troubleshooting). |
| `logLevel` | Info | Daemon log verbosity (debug builds always log at debug level). |

Per-device settings (for example the stored `irk`/`enc` keys, switching mode, spatial audio and
equalizer choice) are saved in a table named after the device. Deleting the keys forces MyPods to
request them again on the next connection. These are only set by hand:

| Key | Default | Meaning |
|-----|---------|---------|
| `sofa` | libmysofa's default KEMAR | Path to a SOFA file (for example a personal HRTF) for spatial audio. The bass bypass and the tonal correction are tuned for the default KEMAR and are left out for other files. |
| `eqFile` | none | Path to a `ParametricEQ.txt` (AutoEQ, Equalizer APO) that replaces the built-in headphone correction; "Headphone correction" then switches it. Its preamp is ignored (the pre-gain is computed). Files with per-channel sections are refused. |
| `loudnessReference` | `90` | Loudness compensation: phon at 100 % volume. Measured with a sound level meter it calibrates the compensation for your headphones; higher means more bass at the same volume. |

### Bluetooth sound quality

AirPods and Beats speak SBC and AAC only, so AAC is the best there is (LDAC and aptX aren't
supported by the headphones). PipeWire's AAC encoder runs at a constant bitrate by default; its
highest variable-bitrate quality is one WirePlumber setting away. It takes effect only for
headphones that offer VBR, the rest stay on constant bitrate. MyPods leaves the system's PipeWire
configuration alone, so this one is up to you:

```
# ~/.config/wireplumber/wireplumber.conf.d/51-aac-vbr.conf
monitor.bluez.rules = [
  {
    matches = [ { device.name = "~bluez_card.*" } ]
    actions = { update-props = { bluez5.a2dp.aac.bitratemode = 5 } }
  }
]
```

Restart with `systemctl --user restart wireplumber` and reconnect the headphones.

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
framework required. All but two run without hardware.

```bash
# Everything below that needs no hardware, in one go (CI runs it before building the AppImage)
sh tools/run_checks.sh build

# Daemon: AAP battery/ANC parsing, BLE advertisement decoding, smart routing packets, control
# commands, ATT settings, audio effect chain, settings file, Galaxy Buds and Parrot Zik protocol
./build/modules/magicpodscore --selftest

# Emulated AirPods Max through the real audio path (spatial audio, head tracking, EQ, limiter,
# loudness, hearing profile, A/B, 7.1, routing). Needs PipeWire, no Bluetooth; briefly switches the
# default sink to a fake headphones sink.
./build/modules/magicpodscore --emulate-airpods
# The same in the dev container, against its own PipeWire (leaves the host's audio alone)
podman run --rm -v "$PWD:/workspace" -w /workspace <dev image> sh tools/emulate_in_container.sh build

# BLE advertisement decoder in the sniffing tool, against known captures
python3 tools/sniff.py --selftest

# Every UI string translated in every language
python3 tools/check_translations.py
```

`run_checks.sh` also compiles and runs the standalone checks for the ANC and battery wire paths,
the low battery warning, the keyboard shortcut actions and the picker
([core/src/tests/AncSelfCheck.cpp](core/src/tests/AncSelfCheck.cpp),
[core/src/tests/BatterySelfCheck.cpp](core/src/tests/BatterySelfCheck.cpp),
[ui/tests/LowBatteryCheck.cpp](ui/tests/LowBatteryCheck.cpp),
[ui/tests/ActionsCheck.cpp](ui/tests/ActionsCheck.cpp),
[ui/tests/tst_picker.qml](ui/tests/tst_picker.qml)); each file also names its own compile command.

Two checks do need hardware, because the behavior they guard only exists on a real adapter:

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
echo '{"method":"GetAll"}' | websocat -n1 ws://127.0.0.1:2020/
```

Available methods: `GetAll`, `GetDevices`, `ConnectDevice`, `DisconnectDevice`,
`GetActiveDeviceInfo`, `SetActiveDevice`, `SetCapabilities`, `GetDefaultBluetoothAdapter`,
`EnableDefaultBluetoothAdapter`, `DisableDefaultBluetoothAdapter`, `GetSettingsAll`,
`GetSettings`, `GetSetting`, `SetSetting`. The daemon also broadcasts changes (capabilities,
connection state, active device, adapter state, settings, popup trigger) to every connected client.

The full reference with request and response examples is in
[docs/core-api-reference.md](docs/core-api-reference.md).

---

## Known limitations

- **Generic HRTF.** Spatial audio uses the generic KEMAR HRTF shipped with libmysofa. Apple
  personalizes it from a scan of your ears. A personal SOFA file can be set per device (`sofa`, see
  [Configuration](#configuration)). Below 250 Hz the bass bypasses the HRTF, whose low end is
  unusable. Spatial audio plays 3 dB quieter with the limiter, about 10 dB without it (then turning
  your head more than 45° away from the screen can still clip very loud material); turn the
  headphones up to make up for it. The 3 dB are a judgment call: very loud masters make the limiter
  work. 7.1 gets 3 dB more, and its center, side and rear speakers use the tonal correction fitted
  for the front pair.
- **Loudness and hearing profile are uncalibrated.** Which listening level a volume setting means
  depends on the headphones (`loudnessReference`, see [Configuration](#configuration)). The hearing
  profile uses the half-gain rule, not a fitting formula such as NAL-NL2, and has no built-in hearing
  test: an uncalibrated tone test can't give dB HL. Enter an audiogram from an audiologist or a
  hearing test app.
- **Head tracking calibration.** Interpreting the head-tracking stream is a heuristic taken from
  LibrePods and not yet calibrated on real hardware.
- **No takeover for non-MPRIS audio.** Games and system sounds deliberately do not trigger
  automatic switching, same as on a Mac.
- **iPhone handoff** requires the Apple DeviceID in BlueZ (see
  [One-time system setup](#one-time-system-setup)).
- **Not implemented:** heart rate (AirPods Pro 3), Find My and the case speaker. These parts of
  Apple's protocol have not been reverse engineered publicly. Head gestures (nod or shake to answer
  a call) aren't either: the AirPods only stream head motion and the phone interprets it, and a Linux
  desktop has no call to answer.
- **AirPods Pro 2/3 settings over ATT** (Loud Sound Reduction, customized transparency) follow
  LibrePods and are not verified on hardware, including the value ranges of the transparency sliders.
  The hearing aid switch needs the hearing test from an iPhone; without it the switch stays hidden.
- **Popup while connected (Linux).** The BLE scan pauses while AirPods are connected, so opening the
  case of AirPods that are still connected to this computer shows no popup. Opening it while they are
  disconnected, or connecting them, does.
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
advertisements. The popup also needs the IRK/ENC keys, which are fetched when the AirPods connect
to MyPods. On Linux there is no popup while the AirPods are connected (see
[Known limitations](#known-limitations)).

**My Bluetooth mouse or keyboard stopped reconnecting on its own.**
This was fixed in the lid-open popup's BLE scan: it no longer sets an LE-only discovery
filter. An LE-only discovery scans without a gap and the kernel never gets to finish the
allowlist auto-connect that paired Bluetooth LE devices need, so they stay disconnected for
as long as MyPods scans. The default transport interleaves BR/EDR inquiry with the LE scan
and leaves exactly those gaps, at no measurable cost to advertisement throughput. To switch the
scan off entirely, turn off the popup (`animation = false`) and set automatic switching to
"When last connected to this computer".

**Music stutters while the AirPods are playing.**
The BLE scan's classic inquiry competes with A2DP on many adapters, so on Linux MyPods pauses the
scan while AirPods are connected. If it still stutters, check `journalctl --user -u mypods-core`
for "BLE scan started" while they are connected, and whether another program runs a Bluetooth scan.

**Noise control and settings are missing.**
These need an active classic connection (A2DP/HFP). Connect the AirPods first. If they still do not
show up, make sure no other tool (for example LibrePods) holds the L2CAP channel; the daemon log then
says "control channel unavailable". Audio keeps working, and the next connection tries again. If the
AirPods close the channel on their own, the daemon reopens it; after five tries in one connection it
logs "control channel keeps closing" and waits for the next connection.

**The battery level doesn't appear in the system's Bluetooth settings.**
The daemon log says "BlueZ takes no battery levels from MyPods" when BlueZ lacks the battery provider
API; older BlueZ versions only offer it with `bluetoothd --experimental`. A headset that already reports
its battery over HFP (through PipeWire) keeps that value.

**Automatic switching does not work.**
Verify the `DeviceID = bluetooth:004C:0000:0000` line in `/etc/bluetooth/main.conf`, restart
Bluetooth and reconnect the AirPods. Switching is also skipped while a call is active on the other
device, and only MPRIS players trigger it.

**The UI says it cannot reach the daemon.**
Something else might be using port 2020 on `127.0.0.1`, or an old daemon is still running:
`systemctl --user restart mypods-core` (or `pkill -x magicpodscore` without the service) and start
MyPods again. `journalctl --user -u mypods-core` shows the daemon's log.

**The About page shows different versions.**
Every build has one version, `MAJOR.MINOR.<commits since MAJOR.MINOR was set>` (e.g. `1.0.5`), the same
in the About page, the MSI, the Arch package and the release title. "MyPods Core" is the daemon that is actually running. If it differs from the app,
a daemon from another installation still runs, typically an old `mypods-core` user service:
`systemctl --user cat mypods-core` shows which binary it starts; restart it or remove the old copy.

**Spatial audio has no effect.**
Check that `/usr/share/libmysofa/default.sofa` exists (package `libmysofa`) and that applications
play to the `mypods_fx` sink.

---

## Security

- The daemon's WebSocket API has **no authentication**. It listens on `127.0.0.1:2020` only, so
  other machines can't reach it, but every local user and process can. It rejects connections that
  carry an `Origin` header, which is what stops web pages in your browser from changing settings
  through it. The AirPods keys (`irk`, `enc`) never leave the daemon through the API: settings
  responses and update broadcasts leave them out.
- The AirPods' IRK and ENC keys are stored in plain text in `~/.config/mypods/config.toml`. The daemon
  writes that file with mode `0600` and replaces it atomically, so a crash never leaves it half written.
  If it can't be parsed, it is moved to `config.toml.broken` and MyPods starts with defaults.

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
- [AutoEQ](https://github.com/jaakkopasanen/AutoEq) by Jaakko Pasanen (MIT), with measurements by
  oratory1990, crinacle and Rtings: the headphone correction filters

The full list, including vendored libraries and academic papers, is in [CREDITS.md](CREDITS.md).

---

## License

MyPods is licensed under the [GNU General Public License v3.0](LICENSE), because the code it is
based on is GPL-3.0. Vendored libraries keep their own licenses (MIT, Apache-2.0, LGPL-2.1 with
exception, SIL OFL 1.1), see [CREDITS.md](CREDITS.md).

Apple, AirPods, Beats and macOS are trademarks of Apple Inc. Parrot and Zik are trademarks of
Parrot. Samsung and Galaxy Buds are trademarks of Samsung Electronics Co., Ltd.
This project is not affiliated with or endorsed by any of them.
