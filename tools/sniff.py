#!/usr/bin/env python3
"""Captures and decodes Apple's proximity pairing advertisements (0x004C, type 0x07).

Purpose: check on real hardware what the decoder assumes: model IDs, how the lid-open counter
behaves, the battery nibbles, the status bits. Captures (--log) become test vectors for the parser.

    python3 tools/sniff.py --selftest          # the decoder against known captures
    python3 tools/sniff.py                     # capture live (no root)
    python3 tools/sniff.py --log captures/pro3.txt --seconds 120

No root needed: BlueZ hands the advertisement data over D-Bus, and a bluetoothctl subprocess
keeps the discovery running.
"""

import argparse
import json
import subprocess
import sys
import time

APPLE_COMPANY_ID = 76  # 0x004C
PROXIMITY_PAIRING = 0x07

MODELS = {
    0x2002: "AirPods 1", 0x200F: "AirPods 2", 0x2013: "AirPods 3",
    0x2019: "AirPods 4", 0x201B: "AirPods 4 (ANC)", 0x200E: "AirPods Pro",
    0x2014: "AirPods Pro 2", 0x2024: "AirPods Pro 2 (USB-C)",
    0x2027: "AirPods Pro 3", 0x200A: "AirPods Max", 0x201F: "AirPods Max (USB-C)",
    0x202D: "AirPods Max 2",
}

# Headphones without a case: the popup trigger is the high nibble of byte 8 (== 8), not the case state
MAX_MODELS = {0x200A, 0x201F, 0x202D}

COLORS = {
    0x00: "white", 0x01: "black", 0x02: "red", 0x03: "blue", 0x04: "pink",
    0x05: "gray", 0x06: "silver", 0x07: "gold", 0x08: "rose gold",
    0x09: "space gray", 0x0A: "dark blue", 0x0B: "light blue", 0x0C: "yellow",
}

CONN_STATES = {
    0x00: "disconnected", 0x04: "idle", 0x05: "music", 0x06: "call",
    0x07: "ringing", 0x09: "hanging up",
}


def battery(nibble):
    """Coarse battery level from a nibble. 0x0F = unknown, >10 = disconnected."""
    if nibble == 0x0F:
        return None
    return nibble * 10 if nibble <= 10 else None


def decode(data):
    """Decodes a proximity pairing advertisement (27 bytes) into a dict."""
    if len(data) < 27 or data[0] != PROXIMITY_PAIRING or data[1] != 0x19:
        return None
    if data[1] + 2 != len(data):
        return None

    status = data[5]
    primary_left = bool(status & 0x20)
    flipped = not primary_left
    pods, flags_case = data[6], data[7]
    flags = (flags_case >> 4) & 0x0F
    lid = data[8]

    left_nib = (pods >> 4) if flipped else (pods & 0x0F)
    right_nib = (pods & 0x0F) if flipped else (pods >> 4)

    both_in_case = bool(status & 0x04)
    two_pods_active = bool(status & 0x01)
    this_pod_in_case = bool(status & 0x40)
    xor = flipped ^ this_pod_in_case
    model_id = (data[4] << 8) | data[3]

    return {
        "paired": data[2] == 0x01,
        "model_id": model_id,
        "model": MODELS.get(model_id, "unbekannt"),
        "color": COLORS.get(data[9], f"0x{data[9]:02x}"),
        "primary": "links" if primary_left else "rechts",
        "left": battery(left_nib),
        "right": battery(right_nib),
        "case": battery(flags_case & 0x0F),
        "left_charging": bool(flags & (0x02 if flipped else 0x01)),
        "right_charging": bool(flags & (0x01 if flipped else 0x02)),
        "case_charging": bool(flags & 0x04),
        "left_in_ear": bool(status & (0x08 if xor else 0x02)),
        "right_in_ear": bool(status & (0x02 if xor else 0x08)),
        "one_in_case": bool(status & 0x10),
        "both_in_case": both_in_case,
        "lid_counter": lid & 0x07,
        "lid_open": not bool((lid >> 3) & 0x01),
        # MagicPodsCore's popup heuristic (AppAnimationCapability::ParseBle)
        "popup": (lid >> 4) == 8 if model_id in MAX_MODELS
                 else both_in_case and (two_pods_active or (left_nib <= 10 and right_nib <= 10)) and (lid & 0x0F) <= 8,
        "conn": CONN_STATES.get(data[10], f"0x{data[10]:02x}"),
        "status_byte": status,
        "lid_byte": lid,
    }


def describe(d):
    def pct(v, charging):
        return "--" if v is None else f"{v}%{'+' if charging else ''}"

    if d["model_id"] in MAX_MODELS:
        # one battery, in the nibble of the primary "pod"; the other is 0
        level = max(d["left"] or 0, d["right"] or 0)
        akku = f"battery {pct(level, d['left_charging'] or d['right_charging'])}"
    else:
        akku = (f"L {pct(d['left'], d['left_charging'])} R {pct(d['right'], d['right_charging'])} "
                f"Case {pct(d['case'], d['case_charging'])}")
    return (
        f"{d['model']} (0x{d['model_id']:04x}, {d['color']}) {akku} | "
        f"lid {'open' if d['lid_open'] else 'closed'} #{d['lid_counter']} "
        f"(Byte8 0x{d['lid_byte']:02x}) | "
        f"both in case: {'yes' if d['both_in_case'] else 'no'} | "
        f"in ear L/R: {int(d['left_in_ear'])}/{int(d['right_in_ear'])} | "
        f"{d['conn']} | POPUP: {'YES' if d['popup'] else 'no'}"
    )


def managed_objects():
    out = subprocess.run(
        ["busctl", "--system", "--json=short", "call", "org.bluez", "/",
         "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"],
        capture_output=True, text=True, check=True,
    )
    return json.loads(out.stdout)["data"][0]


def apple_adverts(objects, all_types):
    """Yields (path, address, bytes) for every device with Apple manufacturer data."""
    for path, ifaces in objects.items():
        dev = ifaces.get("org.bluez.Device1")
        if not dev:
            continue
        md = dev.get("ManufacturerData")
        if not md:
            continue
        payload = md["data"].get(str(APPLE_COMPANY_ID))
        if not payload:
            continue
        data = bytes(payload["data"])
        if not data:
            continue
        if data[0] == PROXIMITY_PAIRING or all_types:
            yield path, dev.get("Address", {}).get("data", "?"), data


def live(args):
    # bluetoothctl keeps the discovery open while it runs: when the D-Bus client ends,
    # BlueZ stops the scan again.
    scan = subprocess.Popen(
        ["bluetoothctl", "--timeout", str(args.seconds), "scan", "on"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    log = open(args.log, "a") if args.log else None
    seen = {}
    print(f"Scanning {args.seconds}s. Open and close the lid a few times, "
          f"take the pods out and back, charge the case.\n")
    try:
        deadline = time.time() + args.seconds
        while time.time() < deadline and scan.poll() is None:
            for path, addr, data in apple_adverts(managed_objects(), args.all):
                hexed = data.hex()
                if seen.get(path) == hexed:
                    continue
                seen[path] = hexed
                stamp = time.strftime("%H:%M:%S")
                d = decode(data)
                line = f"{stamp} {addr} {hexed}"
                if d:
                    print(f"{line}\n         {describe(d)}")
                elif args.all:
                    print(f"{line}  (type 0x{data[0]:02x}, {len(data)} bytes)")
                if log:
                    log.write(line + "\n")
                    log.flush()
            # ponytail: polling every 300 ms instead of an HCI monitor, so lid counter steps can
            # be missed. Upgrade: a passive HCI socket (CAP_NET_RAW) like
            # core/src/ble_ads/PassiveHciBasedBleAdvertisingService.
            time.sleep(0.3)
    except KeyboardInterrupt:
        pass
    finally:
        scan.terminate()
    if not seen:
        print("No AirPods advertisements (type 0x07) seen. Are they awake, out of the "
              "Smart Case and in range? Pairing mode makes them send.")
        if log:
            log.close()


def selftest():
    """Test vectors from real captures (core/src/tests/TestsAapBle.cpp)."""
    cases = [
        # AirPods 2: left 100%, right 90% charging, case 60%, no popup
        ("0719010f2053a9960200050185c65a0aff9097826bd7c542e1cc55",
         {"model_id": 0x200F, "left": 100, "right": 90, "case": 60,
          "left_charging": False, "right_charging": True, "case_charging": False,
          "popup": False, "paired": True}),
        # The same AirPods, another status byte (0x33) -> the same battery levels
        ("0719010f20339aa60100050185c65a0aff9097826bd7c542e1cc55",
         {"model_id": 0x200F, "left": 100, "right": 90, "case": 60,
          "right_charging": True, "popup": False}),
        # AirPods Max in the popup state, from MagicPodsCore's test
        ("0719010a20020480820f400185c65a0aff9097826bd7c542e1cc55",
         {"model_id": 0x200A, "color": "0x0f", "popup": True}),
        # AirPods Max on the head -> no popup
        ("0719010a20220580000f45e80d9bb8e51897326a99455bb1cff367",
         {"model_id": 0x200A, "popup": False}),
        # Synthetic: the Max popup capture with the Max 2 ID (no real capture yet)
        ("0719012d20020480820f400185c65a0aff9097826bd7c542e1cc55",
         {"model_id": 0x202D, "model": "AirPods Max 2", "popup": True}),
        # Beats Solo 4: single unit, 90%
        ("07190125200009800400041ed3edebd0bc052b11618fc29f861d8a",
         {"model_id": 0x2025}),
        # AirPods Pro 3, the lid just opened: "both active" not set yet, but both pods report a battery -> popup
        ("0719012720349ab51100002accced00dc72463635b396a65570371",
         {"model_id": 0x2027, "both_in_case": True, "lid_open": True, "popup": True}),
    ]
    for hexed, expected in cases:
        got = decode(bytes.fromhex(hexed))
        assert got is not None, f"not decoded: {hexed}"
        for key, want in expected.items():
            assert got[key] == want, f"{hexed}: {key} = {got[key]!r}, expected {want!r}"

    assert decode(bytes.fromhex("0719010f2053")) is None, "too short must be None"
    assert decode(bytes.fromhex("10052318" + "00" * 23)) is None, "wrong type must be None"
    print(f"selftest ok ({len(cases)} captures decoded)")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--selftest", action="store_true", help="check the decoder, no scan")
    p.add_argument("--log", metavar="FILE", help="write the raw data along")
    p.add_argument("--seconds", type=int, default=60, help="scan duration (default 60)")
    p.add_argument("--all", action="store_true", help="also show other Apple message types")
    args = p.parse_args()

    if args.selftest:
        selftest()
        return 0
    live(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
