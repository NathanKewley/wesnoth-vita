#!/usr/bin/env python3
"""
Summarises one game of vita/ai-test.sh from its logs.

    vita/ai-test-summary.py <game log> <vita3k log> <scenario> <sides> <turns> <exit status> <seconds>
"""
import re, sys
log, emu, scenario, sides, turns, status, elapsed = sys.argv[1:]
text = open(log, errors="replace").read()
emu_text = open(emu, errors="replace").read()
fps, heap, slow, very_slow = [], [], 0, 0
costs = {}
for line in text.split("\n"):
    m = re.search(r"profile: ([\d.]+) fps \(\d+ loops\) slow (\d+)/(\d+) heap (\d+) MB", line)
    if not m:
        continue
    heap.append(int(m.group(4)))
    if "map draw" in line:
        fps.append(float(m.group(1)))
        slow += int(m.group(2))
        very_slow += int(m.group(3))
    for name, n, ms in re.findall(r"\| ([^|]+?) (\d+)x (\d+)ms", line):
        c = costs.setdefault(name, [0, 0])
        c[0] += int(n)
        c[1] += int(ms)
throws = {}
for t in re.findall(r"\[vita\] throw (\S+)", text):
    throws[t] = throws.get(t, 0) + 1
# (Vita3K logs "Unhandled SIGSEGV" for faults it recovers from itself: not a sign of anything.)
if "Ran out of memory" in text or "Aborted" in text:
    ending = "CRASHED"
elif status == "124":
    ending = "time limit"
elif "Shutting down game" in emu_text:
    ending = "finished"
else:
    ending = f"exit {status}"
ai = costs.get("ai turn", [0, 0])
print(f"== {scenario}: {sides} AI sides, {turns} turns - {ending} after {int(elapsed) // 60}m{int(elapsed) % 60:02d}s")
print(f"   side turns played {ai[0]}, AI thinking {ai[1] / 1000:.0f} s ({ai[1] / max(ai[0], 1) / 1000:.1f} s a side turn)")
if fps:
    print(f"   in game: {sum(fps) / len(fps):.1f} fps average, lowest window {min(fps):.1f}; slow frames {slow} (>20 ms), {very_slow} (>34 ms)")
if heap:
    print(f"   heap: peak {max(heap)} MB, at the end {heap[-1]} MB")
for name in ("attack", "map draw", "terrain images", "image load", "image modify", "wml events", "minimap", "autosave"):
    if name in costs:
        n, ms = costs[name]
        print(f"   {name}: {n}x, {ms / 1000:.1f} s")
print("   exceptions: " + (", ".join(f"{k} x{v}" for k, v in sorted(throws.items(), key=lambda kv: -kv[1])) or "none"))
