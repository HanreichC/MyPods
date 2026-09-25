# MagicPodsCore WebSocket JSON API

This document describes the public WebSocket API used by MagicPodsCore to communicate with the frontend.

All messages are sent as text WebSocket frames in JSON format.

## Connection

Connect to the WebSocket at the following address:

```
ws://127.0.0.1:2020/
```

The daemon listens on loopback only and rejects connections that send an `Origin` header (browsers).

After connecting to the WebSocket, MagicPodsCore sends an `init` message and then starts sending notifications about changes in `info`, `headphone`, and `defaultbluetooth` states.

```json
{
  "init": {
    "api": 0,
    "version": "2.0.7"
  }
}
```

Clients should request the initial state using the `GetAll` method after connection.

## Errors

MagicPodsCore returns an empty string response if invalid data is received, or ignores the `SetCapabilities` method when invalid data is provided.


# Methods

## Bluetooth adapter

### GetDefaultBluetoothAdapter

Returns the status of the active Bluetooth adapter.

Request:

```json
{ "method": "GetDefaultBluetoothAdapter" }
```

Response:
```json
{
  "defaultbluetooth": { "enabled": true }
}
```

### EnableDefaultBluetoothAdapter

Enables the active Bluetooth adapter.

Request:

```json
{ "method": "EnableDefaultBluetoothAdapter" }
```

Response:

```json
{
  "defaultbluetooth": { "enabled": true }
}
```

### DisableDefaultBluetoothAdapter

Disables the active Bluetooth adapter.

Request:

```json
{ "method": "DisableDefaultBluetoothAdapter" }
```

Response:

```json
{
  "defaultbluetooth": { "enabled": false }
}
```

### Broadcast (Notification)

If the connection status of the active Bluetooth adapter changes:

```json
{
  "defaultbluetooth": { "enabled": true }
}
```

## Headphones

### GetDevices

Returns a list of supported headphones on the system.

Request:

```json
{ "method": "GetDevices" }
```

Response:
```json
{
  "headphones": [
    {
      "name": "AirPods",
      "address": "AA:BB:CC:DD:EE:FF",
      "vendor": 76,
      "model": 8207,
      "color": 0,
      "connected": false
    },
    {
      "name": "Galaxy Buds 3",
      "address": "11:22:33:44:55:66",
      "vendor": 117,
      "model": 9,
      "color": 0,
      "connected": false
    }
  ]
}
```

Returns empty `headphones` if no supported headphones are found or if no headphones are paired with the system:

```json
{
  "headphones": []
}
```

### ConnectDevice

Connects to the headphones at the specified address.

Request:

```json
{
  "method": "ConnectDevice",
  "arguments": { "address": "AA:BB:CC:DD:EE:FF" }
}
```

Response:

```json
{
  "headphones": [
    {
      "name": "AirPods",
      "address": "AA:BB:CC:DD:EE:FF",
      "vendor": 76,
      "model": 8207,
      "color": 0,
      "connected": true
    },
    {
      "name": "Galaxy Buds 3",
      "address": "11:22:33:44:55:66",
      "vendor": 117,
      "model": 9,
      "color": 0,
      "connected": false
    }
  ]
}
```

### DisconnectDevice

Disconnects from the headphones at the specified address.

Request:

```json
{
  "method": "DisconnectDevice",
  "arguments": { "address": "AA:BB:CC:DD:EE:FF" }
}
```

Response:

```json
{
  "headphones": [
    {
      "name": "AirPods",
      "address": "AA:BB:CC:DD:EE:FF",
      "vendor": 76,
      "model": 8207,
      "color": 0,
      "connected": false
    },
    {
      "name": "Galaxy Buds 3",
      "address": "11:22:33:44:55:66",
      "vendor": 117,
      "model": 9,
      "color": 0,
      "connected": false
    }
  ]
}
```

### Broadcast (Notification)

When the connection status of any headphone changes:

```json
{
  "headphones": [
    {
      "name": "AirPods",
      "address": "AA:BB:CC:DD:EE:FF",
      "vendor": 76,
      "model": 8207,
      "color": 0,
      "connected": true
    },
    {
      "name": "Galaxy Buds 3",
      "address": "11:22:33:44:55:66",
      "vendor": 117,
      "model": 9,
      "color": 0,
      "connected": false
    }
  ]
}
```

## Info

### GetActiveDeviceInfo

Gets details about the currently connected headphones.

After the connection is established, the device `capabilities` section is updated dynamically, as headphones do not provide their capabilities immediately.


Request:
```json
{ "method": "GetActiveDeviceInfo" }
```

Response:
```json
{
  "info": {
    "name": "AirPods",
    "address": "AA:BB:CC:DD:EE:FF",
    "vendor": 117,
    "model": 9,
    "color": 0,
    "connected": true,
    "capabilities": {
      "battery": {...},
      "anc": {...},
      "bluetoothCodec": {...}
    }
  }
}
```

Returns empty `capabilities` if no features are available:

```json
{
  "info": {
    "name": "AirPods",
    "address": "AA:BB:CC:DD:EE:FF",
    "vendor": 117,
    "model": 9,
    "color": 0,
    "connected": true,
    "capabilities": { }
  }
}
```

Returns empty `info` if no headphones are connected:

```json
{
    "info": {}
}
```

### Broadcast (Notification)

On any property change:

```json
{
  "info": {
    "name": "AirPods Pro",
    "address": "AA:BB:CC:DD:EE:FF",
    "vendor": 117,
    "model": 9,
    "color": 0,
    "connected": true,
    "capabilities": {
      "battery": {...},
      "anc": {...},
      "bluetoothCodec": {...}
    }
  }
}
```


### SetActiveDevice (MyPods)

With several headphones connected, makes one of them the active device: the one `GetActiveDeviceInfo` and the capability broadcasts are about. Ignored for a device that isn't connected. Without it, the headphones connected last are active.

Request:

```json
{ "method": "SetActiveDevice", "arguments": { "address": "AA:BB:CC:DD:EE:FF" } }
```

Response: the same as `GetActiveDeviceInfo`. Every client also gets `OnActiveDeviceChanged`.

### GetAll

Combines GetDevices, GetDefaultBluetoothAdapter, and GetActiveDeviceInfo into a single request.

Request:

```json
{ "method": "GetAll" }
```

Response:

```json
{
  "headphones": [ /* same as GetDevices */ ],
  "defaultbluetooth": { /* same as GetDefaultBluetoothAdapter */ },
  "info": { /* same as GetActiveDeviceInfo */ }
}
```

## Capabilities

### SetCapabilities

If a `capabilities` property is not marked as `readonly: true`, it can be modified.

Send one of the available values listed in `options` (if present) or the value specified in `selected` for each `capabilities` property, as described in this documentation.

Request:

```json
{
  "method": "SetCapabilities",
  "arguments": {
    "address": "AA:BB:CC:DD:EE:FF",
    "capabilities": {
      "anc": { "selected": 16 }
    }
  }
}
```

Invalid or unsupported capability values are ignored.

### Capabilities structure

Each capability is returned as an object with the following structure, except for the battery level.

See `List of capabilities` for details on available features and values.


Example of a noise control capability:

```json
{
  "anc": {
    "selected": 1,
    "options": 23,
    "readonly": false
  }
}
```

| Field      | Description                                                     |
| ---------- | --------------------------------------------------------------- |
| `anc`      | Name of the capability used to make a `SetCapabilities` request |
| `selected` | Current selected value                                          |
| `options`  | Available values to select from (optional)                      |

### List of capabilities

#### Common capabilities

Common capabilities independent of the headphone manufacturer

##### Battery (readonly)

Unique capability structure that indicates the battery level of headphones. Always sends `single`, `left`, `right`, and `case` entries, even if the headphones support only some of them. Unavailable battery types will have the `NotAvailable` status.

```json
{
  "battery": {
    "single": { "battery": 0, "charging": false, "status": 0 },
    "left":   { "battery": 80, "charging": true,  "status": 2 },
    "right":  { "battery": 70, "charging": false, "status": 2 },
    "case":   { "battery": 40, "charging": false, "status": 3 },
    "readonly": true
  }
}
```

| Battery type |                                                                |
| ------------ | -------------------------------------------------------------- |
| `single`     | Battery info for non-TWS devices or averaged battery reporting |
| `left`       | Left earbud battery info                                       |
| `right`      | Right earbud battery info                                      |
| `case`       | Charging case battery info                                     |

| Bettery structure |             |
| ----------------- | ----------- |
| `battery`         | int (0-100) |
| `charging`        | bool        |
| `status`          | enum        |

| status |              |
| ------ | ------------ |
| `0`    | NotAvailable |
| `1`    | Disconnected |
| `2`    | Connected    |
| `3`    | Cached       |

##### Noise control

Controls switching between noise control modes.

```json
{
  "anc": {
    "selected": 16,
    "options": 19,
    "readonly": false
  }
}
```

| Field      |               |
| ---------- | ------------- |
| `selected` | int           |
| `options`  | bitmask (int) |
| `readonly` | bool          |

| options    |                   |
| ---------- | ----------------- |
| Bit 0 (1)  | Off               |
| Bit 1 (2)  | Transparency      |
| Bit 2 (4)  | Adaptive          |
| Bit 3 (8)  | WindCancellation  |
| Bit 4 (16) | NoiseCancellation |

##### Bluetooth codec

Supported Bluetooth profiles/codecs

```json
{
  "bluetoothCodec": {
    "selected": "a2dp-sink",
    "options": [
      ["a2dp-sink", "High Fidelity Playback (A2DP Sink)"],
      ["headset-head-unit", "Headset Head Unit (HSP/HFP)"]
    ],
    "readonly": false
  }
}
```

| Field      |                         |
| ---------- | ----------------------- |
| `selected` | string                  |
| `readonly` | bool                    |
| `options`  | array<[string, string]> |


| Tuple element |                                    |
| ------------- | ---------------------------------- |
| `[0]`         | Profile identifier                 |
| `[1]`         | Human-readable profile description |


### Parrot Zik 2.0 capabilities (MyPods)

Detected by the RFCOMM service `8b6814d3-6ce7-4498-9700-9312c1711f63`; protocol notes in `core/src/sdk/zik/ZikProtocol.h`.
Besides the common `battery` (`single` only), `anc` and `bluetoothCodec`:

- `anc`: options Street mode (`2`, Transparency) and Noise cancellation (`16`), plus `level` `1` (normal) or `2` (maximum). `SetCapabilities` accepts `selected`, `level` or both. No Off: on a Zik 2 (fw 2.05) noise control off mutes the music; `selected` is still `1` if the Zik reports off.
- `equalizer`: same presets as below plus `Custom`, applied by the Zik's own DSP (the 10 bands are averaged down to its 5 and doubled, the Zik's gains are weak). `bands` are the Zik's 5 gains (−12 to 12) of what is selected, `frequencies` their centers in Hz. `SetCapabilities` accepts `selected` or `custom`, 5 numbers from −12 to 12, which also selects `Custom`; the first `Custom` starts from the preset that was on.
- `earDetection`: head detection, `selected` bool.
- Switches, `selected` bool: `concertHall`, `smartAudioTune`, `ancPhoneMode` (noise control during calls), `voicePrompts`, `autoConnection`.
- Lists, `selected` is an index into `options` (`-1` if the Zik reports a value not in the list):

| key                | options                                  |
| ------------------ | ---------------------------------------- |
| `concertHallRoom`  | `silent`, `living`, `jazz`, `concert`    |
| `concertHallAngle` | `30`, `60`, `90`, `120`, `150`, `180`    |
| `autoPowerOff`     | minutes: `0` (never), `5`, `10`, `15`, `30`, `60` |

```json
{
  "concertHallRoom": {
    "readonly": false,
    "selected": 3,
    "options": ["silent", "living", "jazz", "concert"]
  }
}
```

### AirPods and Beats capabilities

##### Automatic switching (MyPods)

Apple's "Connect to This Mac". `owns` = this computer is the audio source; `source` names the device that
took the audio (`"iPhone"`, `"iPad"`, `"Mac"` or empty). Send `{"autoSwitch": {"takeover": true}}` to move the
audio here ("Move back").

```json
{
  "autoSwitch": {
    "readonly": false,
    "selected": 0,
    "owns": true,
    "source": ""
  }
}
```

| selected |                                            |
| -------- | ------------------------------------------ |
| `0`      | Automatically (take over when media starts) |
| `1`      | When last connected to this computer        |

##### Automatic ear detection (MyPods)

Taking a pod out pauses, putting it back resumes. `primary`/`secondary`: `0` in ear, `1` out, `2` in case, `-1` unknown.
Galaxy Buds have the same capability with `selected` only.

```json
{
  "earDetection": {
    "readonly": false,
    "selected": true,
    "primary": 0,
    "secondary": 0
  }
}
```

##### Spatial audio (MyPods)

Rendered on this computer (PipeWire filter-chain with the libmysofa HRTF), for AirPods and generic headphones on Linux; head orientation from the AirPods.

```json
{
  "spatialAudio": {
    "readonly": false,
    "selected": 0,
    "headTracking": true,
    "surround": false
  }
}
```

| selected |              |
| -------- | ------------ |
| `0`      | Off          |
| `1`      | Fixed        |
| `2`      | Head tracked (only when `headTracking` is `true`; false on AirPods 1/2, older Beats and generic headphones without motion sensors) |

`surround` (bool, settable on its own) makes the chain a 7.1 sink while spatial audio is on.

##### Equalizer (MyPods)

Apple Music presets and the other effects, applied on this computer. `selected` must be one of
`options`. Each request sets one field.

```json
{
  "equalizer": {
    "readonly": false,
    "selected": "Off",
    "options": ["Off", "Acoustic", "Bass Booster", "..."],
    "correction": false,
    "crossfeed": false,
    "loudness": false,
    "hearing": false,
    "audiogramLeft": "20 25 30 40 55 60",
    "audiogramRight": "",
    "bypass": false
  }
}
```

| Field | |
| ----- | - |
| `correction` | bool, headphone correction (the `eqFile` setting or the model's AutoEQ one); missing when there is neither |
| `crossfeed` | bool, stereo without spatial audio only |
| `loudness` | bool, ISO 226 loudness compensation that follows the volume |
| `hearing` | bool, hearing profile from the audiograms |
| `audiogramLeft`, `audiogramRight` | string, six thresholds in dB HL at 250, 500, 1000, 2000, 4000 and 8000 Hz (−10 to 120, separated by spaces, commas or semicolons); `""` clears the ear |
| `bypass` | bool, A/B comparison: every effect off, pre-gain kept; not saved |

##### Volume swipe

Adjust the volume by swiping up or down on the sensor located on the AirPods Pro stem.

```json
{
  "volumeSwipe": {
    "readonly": false,
    "selected": true
  }
}
```

| Field      |      |
| ---------- | ---- |
| `selected` | bool |
| `readonly` | bool |

| selected |         |
| -------- | ------- |
| `true`   | Enable  |
| `false`  | Disable |

##### Volume swipe length

Adjust the preferred waiting time between swipes.

```json
{
  "volumeSwipeLength": {
    "readonly": false,
    "selected": 0
  }
}
```

| Field      |      |
| ---------- | ---- |
| `selected` | enum |
| `readonly` | bool |

| selected |         |
| -------- | ------- |
| `0`      | Default |
| `1`      | Longer  |
| `2`      | Longest |

##### Press speed

Adjust the speed required to press the Left AirPod and the right AirPod twice or three times.

```json
{
  "pressSpeed": {
    "readonly": false,
    "selected": 0
  }
}
```

| Field      |      |
| ---------- | ---- |
| `selected` | enum |
| `readonly` | bool |


| selected |         |
| -------- | ------- |
| `0`      | Default |
| `1`      | Slower  |
| `2`      | Slowest |

##### Press and hold duration

Adjust the duration required to press and hold on your AirPods.

```json
{
  "pressAndHoldDuration": {
    "readonly": false,
    "selected": 0
  }
}
```

| Field      |      |
| ---------- | ---- |
| `selected` | enum |
| `readonly` | bool |


| selected |          |
| -------- | -------- |
| `0`      | Default  |
| `1`      | Shorter  |
| `2`      | Shortest |

##### Tone volume

Adjusts the volume of sound effects played by AirPods when no audio is playing.

```json
{
  "toneVolume": {
    "readonly": false,
    "selected": 29
  }
}
```

| selected |                     |
| -------- | ------------------- |
| `15-125` | Volume from 15-125% |

##### Adaptive audio noise

Adaptive audio dynamically responds to your environment and cancels or allows external noise.

```json
{
  "adaptiveAudioNoise": {
    "readonly": false,
    "selected": 50
  }
}
```

| selected |                                  |
| -------- | -------------------------------- |
| `0-100`  | 0 - more noise, 100 - less noise |

##### Personalized volume

Adjust the volume of media in response to your environment.

```json
{
  "personalizedVolume": {
    "readonly": false,
    "selected": false
  }
}
```

| Field      |      |
| ---------- | ---- |
| `selected` | enum |
| `readonly` | bool |

| selected |         |
| -------- | ------- |
| `true`   | Enable  |
| `false`  | Disable |

##### Conversation awareness

Lowers media volume and reduces background noise when you start speaking to other people.

```json
{
  "conversationAwareness": {
    "readonly": false,
    "selected": true,
    "duckVolume": 30
  }
}
```

| selected |         |
| -------- | ------- |
| `true`   | Enable  |
| `false`  | Disable |

`duckVolume` (MyPods): how loud media stays on this computer while you speak, in percent of its volume (0–100, default 30). Stored per device; can be sent on its own.

##### Conversation awareness speaking (read-only)

Notifies when a conversation starts or ends.

```json
{
  "conversationAwarenessSpeaking": {
    "readonly": true,
    "selected": false
  }
}
```

| Field      | Type |
| ---------- | ---- |
| `selected` | bool |
| `readonly` | bool |

| selected |                    |
| -------- | ------------------ |
| `true`   | Start conversation |
| `false`  | Stop conversation  |

While the wearer speaks, the daemon also lowers the AirPods' volume on this computer to 30 % and restores it afterwards (MyPods).

##### Press and hold modes (MyPods)

The noise control modes a press-and-hold of the stem cycles through, as a bitmask. At least two bits; `0x01` needs `allowOff`.

```json
{
  "listeningModes": {
    "readonly": false,
    "selected": 6
  }
}
```

| bit    | Mode               |
| ------ | ------------------ |
| `0x01` | Off                |
| `0x02` | Noise cancellation |
| `0x04` | Transparency       |
| `0x08` | Adaptive           |

##### Allow Off (MyPods)

Whether "Off" can be one of the press-and-hold modes. `selected`: bool.

```json
{ "allowOff": { "readonly": false, "selected": true } }
```

##### Digital Crown, sleep detection, automatic connection (MyPods)

Switches the AirPods report; each appears only on models that have it. `selected`: bool.

| Name             | `true` means                                   |
| ---------------- | ---------------------------------------------- |
| `crownReversed`  | AirPods Max: Digital Crown turned the other way |
| `sleepDetection` | pause playback when you fall asleep            |
| `autoConnect`    | connect to this computer automatically         |

```json
{ "sleepDetection": { "readonly": false, "selected": true } }
```

##### Microphone (MyPods)

```json
{ "micMode": { "readonly": false, "selected": 0 } }
```

| selected |              |
| -------- | ------------ |
| `0`      | Automatic    |
| `1`      | Always right |
| `2`      | Always left  |

##### Hearing aid (MyPods)

Present only when a hearing test was set up on an iPhone. `selected`: bool.

```json
{ "hearingAid": { "readonly": false, "selected": false } }
```

##### Loud Sound Reduction (MyPods, AirPods Pro 2/3)

Read and written over the ATT channel. `selected`: bool.

```json
{ "loudSoundReduction": { "readonly": false, "selected": true } }
```

##### Customized transparency (MyPods, AirPods Pro 2/3)

Read and written over the ATT channel. `SetCapabilities` accepts any subset of the fields; the others keep their value. Present once the AirPods' current values have been read.

```json
{
  "transparencyTuning": {
    "readonly": false,
    "enabled": true,
    "amplification": 0.2,
    "balance": 0.0,
    "tone": 0.0,
    "ambientNoiseReduction": 0.5,
    "conversationBoost": false
  }
}
```

| Field                   | Range         |
| ----------------------- | ------------- |
| `enabled`               | bool          |
| `amplification`         | -1 … 1        |
| `balance`               | -1 (left) … 1 (right) |
| `tone`                  | -1 … 1        |
| `ambientNoiseReduction` | 0 … 1         |
| `conversationBoost`     | bool          |

##### Noise cancellation with one AirPod

Allow AirPods to be put in noise cancellation mode when only one AirPod is in your ear.

```json
{
  "ancOneAirPod": {
    "readonly": false,
    "selected": false
  }
}
```

| selected |         |
| -------- | ------- |
| `true`   | Enable  |
| `false`  | Disable |

##### End call

Specifies how a call is ended. If ending a call is assigned to a single press, muting/unmuting the microphone is assigned to a double press, and vice versa.

```json
{
  "endCall": {
    "readonly": false,
    "selected": 2
  }
}
```

| Field      |      |
| ---------- | ---- |
| `selected` | enum |
| `readonly` | bool |

| selected |             |
| -------- | ----------- |
| `2`      | DoublePress |
| `3`      | SinglePress |


## Settings

A general settings storage. Settings are stored in containers in TOML format. Container names must use only Latin characters; the names must not include magicpods or MAC addresses. Allowed separators are _, -, or camelCase. Supported types are bool, string, int, and float.

Location:

```
~/.config/mypods/config.toml
```

The AirPods keys (`irk`, `enc` in a device's container) are never returned: `GetSettingsAll` and `GetSettings` leave them out, `GetSetting` answers `null` for them, and changes to them are not broadcast.

### GetSetting

Retrieves a setting value by container name and setting name.

Request:

```json
{
  "method": "GetSetting",
  "arguments": {
    "container": "myappname",
    "setting": "settingname"
  }
}
```

Response:

```json
{
  "settings": {
    "myappname": {
      "settingname": "value"
    }
  }
}

Returns an empty `settings` object if no setting is found:

```json
{
  "settings": {}
}
```

### SetSetting

Saves a setting value to the specified container.

Request:

```json
{
  "method": "SetSetting",
  "arguments": {
    "container": "myappname",
    "setting": "settingname",
    "value": "value"
  }
}
```

Response:

```json
{
  "settings": {
    "myappname": {
      "settingname": "value"
    }
  }
}

Returns an empty `settings` object if no setting is found:

```json
{
  "settings": {}
}

### Broadcast (Notification)

If any setting changes:

```json
{
  "settings":{
    "myappname":{
      "settingname": "value"
    }
  }
}
```




