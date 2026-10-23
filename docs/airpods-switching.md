# AirPods switching between this computer and an iPhone: the rules

How AirPods Pro 3 (firmware 8A356/90.34) move between this computer and an iPhone, as read off a real pair, iPhone
and logs between 2026-10-22 and 2026-10-23. Apple documents none of it; the packets come from LibrePods
(`AACPManager.kt`), the behaviour from MyPods' own logs (`journalctl --user -u mypods-core`). Every rule below
broke at least once when a change ignored it. **A change to `AapAudioSwitchCapability` or `AppAnimationCapability`
keeps every rule, its check stays green, and a new rule gets a check that fails without it.**

Checks: the emulator (`tools/emulate_in_container.sh`, its `Check(...)` names), the selftest
(`mypodscore --selftest`), the QML tests (`tools/run_checks.sh`) and, for what the emulator can't play out, the
hardware run at the end, read with `tools/check_switch_log.py`.

## What decides everything

Automatic switching to the iPhone only works while the **iPhone itself is connected to the AirPods**: it then shows
in their connected-devices list (AAP 0x2E; logged as `AutoSwitch: N device(s) on the AirPods`). Whether it is, after
they come out of the case, depends on **who had them last**: the iPhone reconnects when it had them, not when this
computer had them. Then the first switch to the iPhone needs the AirPods picked there once by hand; from then on
it stays connected and switching runs on its own. Nothing this computer does changes that (a Mac does it over
iCloud): waiting for the iPhone before or after connecting was tried and disproven (see the end). Taps on the
AirPods reach whichever device has them.

## Switching

| | Rule | Seen | Code | Check |
|---|---|---|---|---|
| **R2** | Every device that shows up on the list gets NewDeviceInformation + AddTiPiDevice ("TiPi" = multipoint peer), whoever connected first. | Before 17eaf9f an iPhone that joined after the laptop needed a pick by hand every time. | `OnReceivedData` 0x2E | selftest `NewDeviceInformation/AddTiPiDevice matches LibrePods`, emulator `the iPhone on the list learns of this computer...` |
| **R3** | An `owns connection 0` in the 3 s grace after a takeover is real: kept, and looked at once the grace is over (claim once more while playing here, let go on the next one). | 10-22 21:28:37, 21:30:33: `owns 0` ~1 s after `owns 1`. Ignored, the laptop was sure it had them: Play here did nothing, the next switch to the iPhone dropped the laptop entirely. | `UpdateHold` | emulator `that last word of the grace isn't lost...` |
| **R4** | Streaming here is not "ours": Play here claims again unless this computer claimed on this connection and the AirPods haven't said otherwise since. | 10-22 20:06: audio routed without a claim; the next switch to the iPhone dropped the laptop instead of handing over. | `TakeOver` (`claimed`, `ownsReport`) | hardware only (no PipeWire card in the emulator) |
| **R5** | No stream while both pods charge in the case: the AirPods leave the A2DP start unanswered and drop every channel, coming back with the control channel only. Their card goes `off` until they're worn. | 10-22 19:54:09 (Connect from the popup, music playing, pods in the case). | card listener, `InCase`, `routeWhenWorn` | hardware only |
| **R6** | Audio that waited in the case comes on with a takeover (claim), not RouteAudio alone. | 10-22 20:06: routed alone, the iPhone switch then dropped the laptop. | `UpdateHold` (`routeWhenWorn`) | hardware only |
| **R7** | Connected but no ear report yet on this connection: the last advertisement (from before connecting) stands, however old. | 10-22 21:43:49: Play here refused as "nobody wears them", only "Move here" worked. | `InEar` | emulator `no ear report on this connection...` |
| **R10** | Already connected (older than 6 s), Play here waits for no device list: one that hasn't come on this connection doesn't come by waiting. Whoever talked to this computer through the AirPods (audio source, smart routing) is asked to let go along with the list. | 10-23 19:06:42: 10 s waited for a list that never came, `asking 0`, back after 10 s. | `TakeOver`, `Talking`, `Claim` | emulator `R10: no list on this connection...` |
| **R12** | The claim goes out only once the control channel is up (`aapReady`, after the AAP handshake), 5 s at most: BlueZ's Connect returns at once, the link comes 1–2 s later and the channel ~2 s after that. And "already ours" only once the AirPods confirmed the claim (`owns connection 1`). | 10-23 19:46:39: claim sent 1 s before the channel was up, never confirmed; the laptop was sure it had them, the next Play here did nothing. | `TakeOver`, `AapDevice::aapReady` | emulator `R12: the claim only once the control channel is up` (fails without the wait); the confirmation: hardware only |
| **R9** | This computer connects the AirPods only to take them over (Play here, put in while playing, Move here), never just because they're worn. | 10-22 20:33 and 21:22: connecting early, with or without a claim, and the iPhone stayed unconnected. | (absent on purpose) | hardware only |

## Lid popup

| | Rule | Seen | Code | Check |
|---|---|---|---|---|
| **P1** | Put away (they disconnect): no popup for 10 s. Waiting for the lid to close instead never ended. | 10-22 21:28–21:31: no popup on any opening. | `AppAnimationCapability` connection listener | hardware only |
| **P2** | A closed case advertises nothing: 5 s of silence is the lid closing (the scan's own pause is 1.5 s). | 10-22 19:35:14: popup held in the UI after a dismiss, never released. | `lidWatch` | emulator `Lid: ...` |
| **P3** | Connected, the popup closes ("lid closed" for the UI's hold). The UI also drops its hold when it connects to a (re)started daemon. | 10-22 20:22. | connection listener, `PopupAnimation.qml` | hardware only |
| **P4** | Shown at least 3 s. | 10-22 21:22: gone "after a millisecond" when the pods left the case at once. | `PopupAnimation.qml` | `tst_popup.qml` |
| **P5** | No Connect button while automatic switching is on. | 10-22 20:16. | `PopupAnimation.qml` | `tst_popup.qml` |
| **P6** | Popup as soon as both pods lie in the open case and are awake: the "both active" status bit, **or** both report a battery level. The bit alone comes ~6 s after the lid opens; pods reporting no battery (0xF) are asleep. | 10-23 18:43 (taken straight out: no popup), 18:45:50 vs :56 (6 s late). | `ParseBle` | selftest `Lid just opened...`, `sniff.py --selftest` |

## Measured, so nobody guesses again

- A takeover with the AirPods already connected: Play → sound ~2 s (A2DP comes back from `off`). Connecting first:
  the page alone takes 0.1–3.7 s; pausing the discovery does not speed it up (10-22 21:42, 2.9 s with it paused).
- Routing before the claim once routed nothing at all, with no log line (10-22 21:42, unexplained): claim first,
  then the card and RouteAudio.
- `asking N other device(s)` at each takeover says whether the iPhone was asked (N ≥ 1).
- The BLE scan pauses 1.5 s every 15 s so paired LE mice reconnect (see `DBusBasedBleAdvertisingService`,
  `tools/check_no_idle_discovery.sh`).

## Hardware run before a commit that touches switching

Case closed 30 s before each part; `python3 tools/check_switch_log.py --since "HH:MM"` afterwards.

1. **A** Put in, Play here, then Play on the iPhone, then here again.
2. **B** Put in, Play on the iPhone first, then here, then the iPhone again.
3. **C** Put in and Play here at once, then the iPhone.
4. **D** Music already playing here, put in: they stay on the iPhone; pause and Play here, then the iPhone.
5. **E** Connect from the open case with music playing (R5), then put in.
6. **F** Put them away into the open case: no popup (P1); close, open: popup (P2).

Each part passes when nothing had to be picked by hand on the iPhone, no "Move here" was needed, and the checker
reports no failure.

## Tried and disproven: don't do these again

- **R1, wait for the iPhone on the list after connecting** (up to 4, then 6 s): it never joined in that time.
  10-23 19:22: 6 s waited, `asking 0`, the iPhone only by hand. It only added delay (up to 14 s with R11).
- **R11, wait for the AirPods to connect to the iPhone before connecting** (advertisement byte 10, up to 8 s after
  putting them in): 10-23 19:34, the AirPods stayed connected to nothing for the whole 8 s; 14 s until the audio came.
- **R8, take them over when they're put in while this computer already plays** (no "playback started" comes then):
  10-23 19:46–20:20, every takeover that came from putting them in had to be picked by hand on the iPhone afterwards,
  the ones started by Play here didn't. Removed: with music already playing, pause and Play once.
- **Connecting when they're worn, before anything plays** (with or without a claim): 10-22 20:33 and 21:22, the
  iPhone stayed off all the same.

The cases that looked like "this computer was too early" (10-22 20:33, 10-23 18:25, 18:52, 19:22) all had the laptop
as the last owner; the ones that worked (asking 1) had the iPhone on the AirPods already.
