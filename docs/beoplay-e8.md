# Beoplay E8 (1st generation)

What the Bang & Olufsen Beoplay E8 from 2017 tells and takes over Bluetooth, as found on a pair (firmware 5.6.0,
software 4.4.0, hardware HP10-MP) on 2026-10-15. No public documentation exists; everything here was read off the
earbuds. MyPods uses it in `core/src/device/BeoplayE8*` (Windows; on Linux BlueZ opens no LE link to headphones
connected over BR/EDR).

## Classic Bluetooth

Device ID: vendor 0x0A12 (Cambridge Silicon Radio, USB-IF source), product 0x0001. SDP offers A2DP, AVRCP,
headset (0x1108) and hands-free (0x111E) only: no vendor RFCOMM channel (no GAIA over RFCOMM). The battery comes
over hands-free like any headset.

## Bluetooth LE (same address)

| Service | Characteristic | Properties | Value / meaning |
|---|---|---|---|
| 0x180F Battery | 0x2A19 | read, notify | battery in %, the earbuds together |
| 0x180A Device Information | 0x2A24 model, 0x2A25 serial, 0x2A26 firmware, 0x2A27 hardware, 0x2A28 software | read | `Beoplay E8`, `29980579`, `5.6.0`, `HP10-MP`, `4.4.0` |
| 0xFE89 (B&O) | `4446cf5f-12f2-4c1e-afe1-b15797535ba8` | read, write, notify | state, see below |
| | `3ba91c2e-8b08-4c27-9d4e-4936a793fcfb` | read, write, notify | `00` + the earbuds' Bluetooth address |
| | `44fa50b2-d0a3-472e-a939-d80cf17638bb` | read, write, notify | one byte (0x41 seen), meaning unknown |
| | `95c09f26-95a4-4597-a798-b8e408f5ca66` | read, write | 24 bytes, likely sound settings (ToneTouch), unknown |
| | `773e4203-a0e7-456b-96fa-e1b4ad56eb63` | read, write | `00` |
| | `7dd2f744-16c4-4c58-88a4-0fafecc78343` | read, write | `01` |
| | `fa302d24-d775-4343-b9ed-8cc68ace3284` | read, write, notify | 8 × `00` |
| | `d5b5e4c2-d2a7-4eec-a2d0-6225033a4caf` | read, write, notify | `04 00`; subscribing is refused (needs a secured link?) |
| | `4a75c20f-13bd-44a1-b39d-a70f86f607a2` | read, write, notify | `00`; subscribing is refused (needs a secured link?) |

### State `4446cf5f…`

Read and notified as three bytes `03 PP MM`:

- `PP` playback paused: `01` paused (also when an earbud goes into the case), `00` playing
- `MM` listening mode: `02` normal, `03` transparency (a tap on the left earbud toggles it)

Written, a **single byte is a remote command**, not a mode: `03` next track, `02` previous track (three bytes are
refused with ATT error 0x0D, invalid length). How the B&O app sets transparency, its three levels and ToneTouch is
not known: that needs a capture of the app (Android HCI snoop log), likely on one of the refused characteristics.
Don't write to the others on a guess: one byte already acts.
