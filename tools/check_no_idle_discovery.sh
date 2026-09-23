#!/bin/sh
# Self-check for DevicesInfoFetcher::UpdateBleState.
#
# A BlueZ discovery session makes the kernel skip hci_update_passive_scan, so its
# allowlist background scan never runs and paired LE devices (mice, keyboards)
# stop auto-reconnecting. Only AapDevice consumes BLE advertisements, so MyPods
# must not hold a discovery session when no Apple device is paired.
#
# Run with MyPods running:  sh tools/check_no_idle_discovery.sh

pgrep -x magicpodscore >/dev/null || { echo "FAIL: magicpodscore is not running"; exit 1; }

# Apple devices report vendor 0x004c (76) via Modalias usb:v004Cp....
for addr in $(bluetoothctl devices Paired | cut -d' ' -f2); do
    bluetoothctl info "$addr" | grep -qi "Modalias.*v004C" && {
        echo "SKIP: Apple device $addr is paired, discovery is expected"; exit 0; }
done

if bluetoothctl show | grep -q "Discovering: yes"; then
    echo "FAIL: discovery held with no Apple device paired - LE auto-reconnect is blocked"
    exit 1
fi

echo "no-idle-discovery self-check OK"
