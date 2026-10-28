#!/usr/bin/env python3
"""Totals a Wesnoth log's [vita] profile lines: frames, slow frames, and each named cost.

    vita/profile-summary.py <log> [--map]    # --map: only windows where the map was drawn
"""
import re
import sys
from collections import defaultdict

only_map = "--map" in sys.argv
windows = frames = slow = very_slow = 0
seconds = 0.0
totals = defaultdict(lambda: [0, 0.0])
stages = defaultdict(float)
for line in open(sys.argv[1], errors="replace"):
    if "[vita] profile:" not in line or (only_map and "map draw" not in line):
        continue
    m = re.search(r"profile: ([\d.]+) fps \((\d+) loops\)(?: slow (\d+)/(\d+))?", line)
    if not m:
        continue
    windows += 1
    seconds += 2
    frames += float(m.group(1)) * 2
    if m.group(3):
        slow += int(m.group(3))
        very_slow += int(m.group(4))
    for name, pct in re.findall(r" (update|layout|render|expose|present|idle|between) (\d+)%", line):
        stages[name] += int(pct)
    for name, n, ms in re.findall(r"\| ([^|]+?) (\d+)x (\d+)ms", line):
        totals[name][0] += int(n)
        totals[name][1] += int(ms)
print(f"{windows} windows, {seconds:.0f} s, {frames / seconds:.1f} fps average, slow frames {slow} (>20ms) {very_slow} (>34ms)")
print("stages: " + " ".join(f"{k} {v / windows:.0f}%" for k, v in stages.items()))
for name, (n, ms) in sorted(totals.items(), key=lambda kv: -kv[1][1])[:40]:
    print(f"{ms / seconds:8.1f} ms/s {n:8d}x {ms / max(n, 1):7.2f} ms each  {name}")
