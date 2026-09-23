#!/usr/bin/env python3
"""Self-check: a freshly paired device reaches the UI without restarting MyPods.

BlueZ reports Paired before SDP delivers the UUIDs and Modalias the device type is
derived from, so the core has to wait for ServicesResolved and then push the device
list to connected clients. This listens like the UI does and passes once a pushed
list contains the address.

Run with MyPods running, then pair the headphones (remove them first if paired):
    python3 tools/check_new_pairing.py AA:BB:CC:DD:EE:FF
"""
import json, os, socket, struct, sys

address = sys.argv[1].upper() if len(sys.argv) > 1 else sys.exit(__doc__)

sock = socket.create_connection(("127.0.0.1", 2020))
sock.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n")
buffer = b""
while b"\r\n\r\n" not in buffer:
    buffer += sock.recv(4096)
buffer = buffer.split(b"\r\n\r\n", 1)[1]

def read(n):
    global buffer
    while len(buffer) < n:
        chunk = sock.recv(65536)
        if not chunk:
            sys.exit("FAIL: MyPods closed the connection")
        buffer += chunk
    data, buffer = buffer[:n], buffer[n:]
    return data

def next_message():
    header = read(2)
    length = header[1] & 0x7F
    if length == 126:
        length = struct.unpack(">H", read(2))[0]
    elif length == 127:
        length = struct.unpack(">Q", read(8))[0]
    return header[0] & 0x0F, read(length)

def send(text):
    payload, mask = text.encode(), os.urandom(4)
    sock.sendall(bytes([0x81, 0x80 | len(payload)]) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

def listed(message):
    return any(d.get("address", "").upper() == address for d in message.get("headphones", []))

sock.settimeout(120)
send('{"method":"GetDevices"}')
first = True  # the first list is the GetDevices reply, everything after it is a push
print(f"Listening for pushed device lists. Pair {address} now (120 s)...")
while True:
    try:
        opcode, payload = next_message()
    except socket.timeout:
        sys.exit(f"FAIL: {address} never appeared in a pushed device list")
    message = json.loads(payload) if opcode == 1 else {}
    if "headphones" not in message:
        continue
    if listed(message) and first:
        sys.exit(f"SKIP: {address} is already listed; remove it in bluetoothctl first")
    if listed(message):
        print("new-pairing self-check OK")
        break
    first = False
