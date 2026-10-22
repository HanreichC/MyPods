#!/bin/sh
# Self-check for DevicesInfoFetcher::UpdateBleState.
#
# A BlueZ discovery session makes the kernel skip hci_update_passive_scan, so its
# allowlist background scan never runs and paired LE devices (mice, keyboards)
# stop auto-reconnecting. Only AapDevice consumes BLE advertisements, so MyPods
# must not hold a discovery session when no Apple device is paired. With one paired
# it discovers, but has to pause every 15 s: a discovery scans from a random address,
# so the controller drops the directed ads a paired mouse reconnects with after a boot.
#
# Run with MyPods running:  sh tools/check_no_idle_discovery.sh

pgrep -x mypodscore >/dev/null || { echo "FAIL: mypodscore is not running"; exit 1; }

# Apple devices report vendor 0x004c (76) via Modalias usb:v004Cp....
for addr in $(bluetoothctl devices Paired | cut -d' ' -f2); do
    bluetoothctl info "$addr" | grep -qi "Modalias.*v004C" && {
        # 20 s covers one 15 s period; polling every 0.25 s can't miss the 1.5 s pause
        i=0; while [ $i -lt 80 ]; do
            bluetoothctl show | grep -q "Discovering: no" && { echo "scan-pause self-check OK"; exit 0; }
            sleep 0.25; i=$((i+1))
        done
        echo "FAIL: Apple device $addr paired, but discovery never paused in 20 s (or another app discovers)"
        exit 1; }
done

if bluetoothctl show | grep -q "Discovering: yes"; then
    echo "FAIL: discovery held with no Apple device paired - LE auto-reconnect is blocked"
    exit 1
fi

echo "no-idle-discovery self-check OK"
