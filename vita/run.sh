#!/usr/bin/env bash
# Builds the .vpk, installs it into Vita3K and runs it, writing the emulator's log (which includes
# the game's stderr as "TTY" lines) to vita/build/vita3k.log.
#
#   vita/run.sh [seconds]     # stop after this long (default 300)
set -uo pipefail
source "$(cd "$(dirname "$0")" && pwd)/env.sh"

if ! cmake --build "$VITA_PORT/build" -j"$(nproc)" > "$VITA_PORT/build/build.log" 2>&1; then
	grep -E 'error|Error [0-9]|undefined reference' "$VITA_PORT/build/build.log" | head -20
	echo "build failed, see vita/build/build.log"
	exit 1
fi

timeout -k 10 "${1:-300}" vita3k --frontend sdl "$VITA_PORT/build/src/wesnoth.vpk" > "$VITA_PORT/build/vita3k.log" 2>&1
echo "vita3k exit: $?"
grep -E 'TTY|\|E\|' "$VITA_PORT/build/vita3k.log" | grep -vE 'Missing file at kd/|preloaded module' | tail -40
