#!/usr/bin/env python3
"""Mitschnitt und Dekodierung der Apple Proximity-Pairing-Advertisements (0x004C, Typ 0x07).

Zweck: an echter Hardware verifizieren, was in docs/PLAN.md nur behauptet ist —
Model-ID der AirPods Pro 3, Verhalten des Lid-Open-Zaehlers, Akku-Nibbles, Statusbits.
Die Mitschnitte (--log) werden spaeter zu Testvektoren fuer den Parser.

    python3 tools/sniff.py --selftest          # Dekoder gegen bekannte Captures pruefen
    python3 tools/sniff.py                     # live mitschneiden (ohne root)
    python3 tools/sniff.py --log captures/pro3.txt --seconds 120

Kein root noetig: BlueZ liefert die Advertisement-Daten ueber D-Bus, die Discovery
haelt ein bluetoothctl-Subprozess offen.
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

COLORS = {
    0x00: "weiss", 0x01: "schwarz", 0x02: "rot", 0x03: "blau", 0x04: "pink",
    0x05: "grau", 0x06: "silber", 0x07: "gold", 0x08: "rosegold",
    0x09: "spacegrau", 0x0A: "dunkelblau", 0x0B: "hellblau", 0x0C: "gelb",
}

CONN_STATES = {
    0x00: "getrennt", 0x04: "idle", 0x05: "Musik", 0x06: "Anruf",
    0x07: "klingelt", 0x09: "legt auf",
}


def battery(nibble):
    """Grober Akkustand aus einem Nibble. 0x0F = unbekannt, >10 = getrennt."""
    if nibble == 0x0F:
        return None
    return nibble * 10 if nibble <= 10 else None


def decode(data):
    """Dekodiert ein Proximity-Pairing-Advertisement (27 Byte) in ein dict."""
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

    return {
        "paired": data[2] == 0x01,
        "model_id": (data[4] << 8) | data[3],
        "model": MODELS.get((data[4] << 8) | data[3], "unbekannt"),
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
        # Popup-Heuristik von MagicPodsCore (AppAnimationCapability::ParseBle)
        "popup": both_in_case and two_pods_active and (lid & 0x0F) <= 8,
        "conn": CONN_STATES.get(data[10], f"0x{data[10]:02x}"),
        "status_byte": status,
        "lid_byte": lid,
    }


def describe(d):
    def pct(v, charging):
        return "--" if v is None else f"{v}%{'+' if charging else ''}"

    return (
        f"{d['model']} (0x{d['model_id']:04x}, {d['color']}) "
        f"L {pct(d['left'], d['left_charging'])} R {pct(d['right'], d['right_charging'])} "
        f"Case {pct(d['case'], d['case_charging'])} | "
        f"Deckel {'auf' if d['lid_open'] else 'zu'} #{d['lid_counter']} "
        f"(Byte8 0x{d['lid_byte']:02x}) | "
        f"beide im Case: {'ja' if d['both_in_case'] else 'nein'} | "
        f"im Ohr L/R: {int(d['left_in_ear'])}/{int(d['right_in_ear'])} | "
        f"{d['conn']} | POPUP: {'JA' if d['popup'] else 'nein'}"
    )


def managed_objects():
    out = subprocess.run(
        ["busctl", "--system", "--json=short", "call", "org.bluez", "/",
         "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"],
        capture_output=True, text=True, check=True,
    )
    return json.loads(out.stdout)["data"][0]


def apple_adverts(objects, all_types):
    """Liefert (pfad, adresse, bytes) fuer jedes Geraet mit Apple-Manufacturer-Data."""
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
    # bluetoothctl haelt die Discovery offen, solange der Prozess laeuft:
    # beendet sich der D-Bus-Client, stoppt BlueZ den Scan wieder.
    scan = subprocess.Popen(
        ["bluetoothctl", "--timeout", str(args.seconds), "scan", "on"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    log = open(args.log, "a") if args.log else None
    seen = {}
    print(f"Scanne {args.seconds}s. Deckel mehrfach oeffnen/schliessen, "
          f"Pods rein/raus, Case laden.\n")
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
                    print(f"{line}  (Typ 0x{data[0]:02x}, {len(data)} Byte)")
                if log:
                    log.write(line + "\n")
                    log.flush()
            # ponytail: 300-ms-Polling statt HCI-Monitor. Deckel-Zaehler-Uebergaenge
            # koennen dabei verschluckt werden. Upgrade: passiver HCI-Socket
            # (CAP_NET_RAW) wie core/src/ble_ads/PassiveHciBasedBleAdvertisingService.
            time.sleep(0.3)
    except KeyboardInterrupt:
        pass
    finally:
        scan.terminate()
        if log:
            log.close()


def selftest():
    """Testvektoren aus echten Captures (core/src/tests/TestsAapBle.cpp)."""
    cases = [
        # AirPods 2: links 100%, rechts 90% ladend, Case 60%, kein Popup
        ("0719010f2053a9960200050185c65a0aff9097826bd7c542e1cc55",
         {"model_id": 0x200F, "left": 100, "right": 90, "case": 60,
          "left_charging": False, "right_charging": True, "case_charging": False,
          "popup": False, "paired": True}),
        # Dieselben AirPods, anderes Statusbyte (0x33) -> gleiche Akkuwerte
        ("0719010f20339aa60100050185c65a0aff9097826bd7c542e1cc55",
         {"model_id": 0x200F, "left": 100, "right": 90, "case": 60,
          "right_charging": True, "popup": False}),
        # AirPods Max, Popup-Zustand laut MagicPodsCore-Test
        ("0719010a20020480820f400185c65a0aff9097826bd7c542e1cc55",
         {"model_id": 0x200A, "color": "0x0f"}),
        # Beats Solo 4: Einzelgeraet, 90%
        ("07190125200009800400041ed3edebd0bc052b11618fc29f861d8a",
         {"model_id": 0x2025}),
    ]
    for hexed, expected in cases:
        got = decode(bytes.fromhex(hexed))
        assert got is not None, f"nicht dekodiert: {hexed}"
        for key, want in expected.items():
            assert got[key] == want, f"{hexed}: {key} = {got[key]!r}, erwartet {want!r}"

    assert decode(bytes.fromhex("0719010f2053")) is None, "zu kurz muss None sein"
    assert decode(bytes.fromhex("10052318" + "00" * 23)) is None, "falscher Typ muss None sein"
    print(f"selftest ok ({len(cases)} Captures dekodiert)")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--selftest", action="store_true", help="Dekoder pruefen, kein Scan")
    p.add_argument("--log", metavar="DATEI", help="Rohdaten mitschreiben")
    p.add_argument("--seconds", type=int, default=60, help="Scandauer (Standard 60)")
    p.add_argument("--all", action="store_true", help="auch andere Apple-Nachrichtentypen zeigen")
    args = p.parse_args()

    if args.selftest:
        selftest()
        return 0
    live(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
