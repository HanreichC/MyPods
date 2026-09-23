#!/usr/bin/env python3
"""Self-check: the Bluetooth codec the user picked last is the one that ends up active.

PipeWire acks a card profile change at once but renegotiates A2DP for ~3 s and silently
drops a change that arrives meanwhile. This picks codecs a second apart like an impatient
user, then checks that the card and the last pushed capability both show the last pick.

Run with MyPods running and the headphones connected:
    python3 tools/check_codec_switch.py AA:BB:CC:DD:EE:FF
"""
import json, os, socket, struct, subprocess, sys, time

address = sys.argv[1].upper() if len(sys.argv) > 1 else sys.exit(__doc__)
card = "bluez_card." + address.replace(":", "_")

sock = socket.create_connection(("127.0.0.1", 2020))
sock.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n")
buffer = b""
while b"\r\n\r\n" not in buffer:
    buffer += sock.recv(4096)
buffer = buffer.split(b"\r\n\r\n", 1)[1]

def send(text):
    payload, mask = text.encode(), os.urandom(4)
    header = bytes([0x81, 0x80 | len(payload)]) if len(payload) < 126 else bytes([0x81, 0xFE]) + struct.pack(">H", len(payload))
    sock.sendall(header + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

def pushed_codecs(seconds):
    """The bluetoothCodec capability of every message that arrives within `seconds`."""
    global buffer
    codecs, end = [], time.time() + seconds
    sock.settimeout(0.1)
    while time.time() < end:
        try:
            buffer += sock.recv(65536)
        except socket.timeout:
            pass
        while len(buffer) >= 2:
            length, offset = buffer[1] & 0x7F, 2
            if length == 126:
                length, offset = struct.unpack(">H", buffer[2:4])[0], 4
            elif length == 127:
                length, offset = struct.unpack(">Q", buffer[2:10])[0], 10
            if len(buffer) < offset + length:
                break
            opcode, payload, buffer = buffer[0] & 0x0F, buffer[offset:offset + length], buffer[offset + length:]
            info = json.loads(payload).get("info", {}) if opcode == 1 else {}
            if info.get("address", "").upper() == address and "bluetoothCodec" in info.get("capabilities", {}):
                codecs.append(info["capabilities"]["bluetoothCodec"])
    return codecs

def active():
    cards = subprocess.run(["pactl", "list", "cards"], capture_output=True, text=True).stdout
    return cards.split("Name: " + card, 1)[1].split("Active Profile: ", 1)[1].split("\n", 1)[0]

send('{"method":"GetActiveDeviceInfo"}')
codecs = pushed_codecs(2)
if not codecs:
    sys.exit(f"FAIL: {address} is not the active device or reports no codec")
a2dp = [name for name, _ in codecs[-1]["options"] if name.startswith("a2dp")]
if len(a2dp) < 2:
    sys.exit(f"SKIP: {address} offers fewer than two A2DP codecs: {a2dp}")

picks = [a2dp[1], a2dp[0], a2dp[1]] if active() == a2dp[0] else [a2dp[0], a2dp[1], a2dp[0]]
for pick in picks:
    print(f"picking {pick}")
    send(json.dumps({"method": "SetCapabilities", "arguments": {"address": address, "capabilities": {"bluetoothCodec": {"selected": pick}}}}))
    codecs += pushed_codecs(1)
codecs += pushed_codecs(15)

print(f"card: {active()}, last pushed: {codecs[-1]['selected']}")
assert active() == picks[-1], f"FAIL: the card runs {active()}, the last pick was {picks[-1]}"
assert codecs[-1]["selected"] == picks[-1], f"FAIL: the UI was last told {codecs[-1]['selected']}"
print("PASS")
