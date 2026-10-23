#!/usr/bin/env python3
"""Reads the daemon's log after a hardware run and checks the switching rules (docs/airpods-switching.md).

    python3 tools/check_switch_log.py --since "18:20"     # journalctl --user -u mypods-core since then
    python3 tools/check_switch_log.py --file core.log     # or a saved log
    python3 tools/check_switch_log.py --selftest          # the checker against a real failure

One line per takeover (what started it, how many other devices were asked, how long until the audio was routed),
then notes, which are no fault of this computer (the iPhone wasn't on the AirPods at a takeover, so the first switch to
it needed a pick by hand; the AirPods dropped this computer for it), and failures: "Move here" needed (Play here
started no takeover), slow takeovers. Exit 1 on any failure.
"""
import re
import subprocess
import sys
from datetime import datetime

LINE = re.compile(r'(\d\d:\d\d:\d\d(?:\.\d+)?)\D.*?(?:INF|ERR|WRN|DBG) (.*)$')


def seconds(stamp):
    t = datetime.strptime(stamp.split('.')[0], '%H:%M:%S')
    return t.hour * 3600 + t.minute * 60 + t.second + (float('0.' + stamp.split('.')[1]) if '.' in stamp else 0)


def events(lines):
    for line in lines:
        m = LINE.search(line)
        if m:
            yield seconds(m.group(1)), m.group(1), m.group(2)


def check(lines):
    """Returns (report lines, notes, failures)."""
    report, notes, failures = [], [], []
    takeover, last_play_unanswered, put_away_at, connected = None, None, -100, False
    unasked = []  # takeovers that asked nobody: R1 once the iPhone shows up on the list within 2 minutes

    def close(t):
        nonlocal takeover
        if takeover:
            routed = f"{takeover['routed'] - takeover['t']:.1f} s" if takeover['routed'] else "never"
            report.append(f"{takeover['stamp']} takeover ({takeover['why']}): asked {takeover['asked']}, routed after {routed}")
            if takeover['asked'] == 0:
                unasked.append(takeover)
            if takeover['routed'] and takeover['routed'] - takeover['t'] > 6:
                failures.append(f"{takeover['stamp']} slow: {routed} until the audio was routed")
        takeover = None

    for t, stamp, text in events(lines):
        if text.startswith('AutoSwitch: taking the audio over'):
            close(t)
            why = re.search(r'\((.*)\)', text).group(1)
            if why == 'manual' and last_play_unanswered and t - last_play_unanswered < 60:
                failures.append(f"{stamp} \"Move here\" needed: Play here at {last_play_unanswered_stamp} took nothing")
            takeover = {'t': t, 'stamp': stamp, 'why': why, 'asked': None, 'routed': None}
            last_play_unanswered = None
        elif text.startswith('MPRIS:') and 'started playing' in text:
            # a takeover follows at once when it starts one; remember it until then
            last_play_unanswered, last_play_unanswered_stamp = t, stamp
        elif m := re.match(r'AutoSwitch: asking (\d+) other device', text):
            if takeover and takeover['asked'] is None:
                takeover['asked'] = int(m.group(1))
                last_play_unanswered = None
        elif m := re.match(r'AutoSwitch: (\d+) device\(s\) on the AirPods', text):
            if int(m.group(1)) >= 2:
                for early in [u for u in unasked if t - u['t'] < 120] + ([takeover] if takeover and takeover['asked'] == 0 else []):
                    notes.append(f"{early['stamp']} the iPhone wasn't on the AirPods (it joined at {stamp}): the first switch to it needed a pick by hand")
                unasked.clear()
                if takeover and takeover['asked'] == 0:
                    takeover['asked'] = -1  # reported
        elif re.match(r'.*: audio routed to bluez_output', text):
            if takeover and takeover['routed'] is None:
                takeover['routed'] = t
        elif 'Ear detection: primary 2, secondary 2' in text:
            put_away_at = t
        elif 'AutoSwitch: another device took the audio' in text:
            connected = False  # handed over: the AirPods stay connected, the next drop is no force
        elif text.endswith('channel closed by the headphones'):
            if connected and t - put_away_at > 5:
                notes.append(f"{stamp} the AirPods dropped this computer (the iPhone picked by hand)")
            connected = False
            close(t)
        elif text.startswith('AirPods') and '_client started' in text:
            connected = True
        if takeover is None and last_play_unanswered and t - last_play_unanswered > 3 and not text.startswith('MPRIS'):
            pass  # a Play here without takeover: only a failure once "Move here" follows (above)
    close(None)
    return report, notes, failures


SAMPLE = """\
18:25:35.555144 INF AutoSwitch: put in while this computer plays
18:25:35.555306 INF AutoSwitch: taking the audio over (playback started)
18:25:36.710588 INF BLE scan stopped
18:25:37.632634 INF AutoSwitch: 1 device(s) on the AirPods, 0 new
18:25:37.658706 INF AutoSwitch: asking 0 other device(s) to let go
18:25:37.864556 INF AirPods Pro von Christopher: audio routed to bluez_output.EC_E9_D2_7A_1C_3E.1
18:25:38.000000 INF AirPods Pro von Christopher _client started
18:26:31.679458 INF EC:E9:D2:7A:1C:3E channel closed by the headphones
18:26:40.702540 INF BLE scan stopped
18:26:41.241664 INF AutoSwitch: 2 device(s) on the AirPods, 1 new
18:26:43.908841 INF AirPods Pro von Christopher: audio routed to bluez_output.EC_E9_D2_7A_1C_3E.1
""".splitlines()

GOOD = """\
20:24:13.000000 INF MPRIS: :1.65 started playing
20:24:13.100000 INF AutoSwitch: taking the audio over (playback started)
20:24:13.400000 INF AutoSwitch: 2 device(s) on the AirPods, 0 new
20:24:13.466507 INF AutoSwitch: asking 1 other device(s) to let go
20:24:13.483497 INF AirPods Pro von Christopher: audio routed to bluez_output.EC_E9_D2_7A_1C_3E.1
20:24:30.000000 INF AutoSwitch: another device took the audio
""".splitlines()


def selftest():
    report, notes, failures = check(SAMPLE)
    ok = (not failures and any(n.startswith('18:25:35.555306 the iPhone wasn') for n in notes)
          and any('dropped this computer' in n for n in notes) and report and 'asked 0' in report[0])
    report2, notes2, failures2 = check(GOOD)
    ok = ok and not failures2 and not notes2 and 'asked 1' in report2[0]
    slow = check([l.replace('18:25:37.864556', '18:25:44.000000') for l in SAMPLE])[2]
    ok = ok and any(f.startswith('18:25:35.555306 slow') for f in slow)
    print("check_switch_log selftest", "OK" if ok else f"FAILED {failures} {failures2}")
    return 0 if ok else 1


def main():
    args = sys.argv[1:]
    if '--selftest' in args:
        return selftest()
    if '--file' in args:
        lines = open(args[args.index('--file') + 1], errors='replace').read().splitlines()
    else:
        since = args[args.index('--since') + 1] if '--since' in args else '-30min'
        lines = subprocess.run(['journalctl', '--user', '--no-pager', '-o', 'short-precise', '-u', 'mypods-core.service',
                                '--since', since], capture_output=True, text=True).stdout.splitlines()
    report, notes, failures = check(lines)
    print("\n".join(report) or "no takeover in that time")
    if notes:
        print("notes:\n" + "\n".join(notes))
    print("\n".join(failures) if failures else "no failure")
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
