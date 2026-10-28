#!/usr/bin/env bash
# Runs the installed build in Vita3K, waits, and saves a picture of the emulator window: the game's
# own screenshots come out black under Vita3K, which does not copy frames back to emulated memory.
# Needs Hyprland (hyprctl) and grim.
#
#   vita/screenshot.sh [seconds-to-wait] [out.png]
set -uo pipefail
source "$(cd "$(dirname "$0")" && pwd)/env.sh"

WAIT="${1:-80}"
OUT="${2:-$VITA_PORT/build/screen.png}"

timeout -k 5 $((WAIT + 20)) vita3k --frontend sdl "$VITA_PORT/build/src/wesnoth.vpk" > "$VITA_PORT/build/vita3k.log" 2>&1 &
sleep "$WAIT"
GEOM=$(hyprctl clients -j | python3 -c '
import json, sys
for c in json.load(sys.stdin):
    if c.get("class") == "vita3k":
        x, y = c["at"]; w, h = c["size"]
        print(f"{x},{y} {w}x{h}")
        break')
if [ -z "$GEOM" ]; then
	echo "no Vita3K window"
	exit 1
fi
grim -g "$GEOM" "$OUT"
echo "$OUT"
wait
