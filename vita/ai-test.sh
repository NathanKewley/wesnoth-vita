#!/usr/bin/env bash
# Plays AI-vs-AI games in Vita3K, one after another, and summarises each: how it ended, side turns
# played, frame rate, slow frames, heap, AI time and exceptions. For soak-testing the port without
# a Vita. The profiler must be on (ux0:data/wesnoth/vita-profile.txt).
#
#   vita/ai-test.sh "<scenario id> <sides> <turns> <minutes>" ...
#   vita/ai-test.sh "multiplayer_Den_of_Onis 2 15 15" "multiplayer_Hexcake 6 10 25"
#
# Logs go to vita/build/ai-test/<scenario>.log (the game's output) and .vita3k.log.
set -uo pipefail
source "$(cd "$(dirname "$0")" && pwd)/env.sh"

UX0="${VITA3K_UX0:-$HOME/.local/share/Vita3K/Vita3K/ux0}"
DATA="$UX0/data/wesnoth"
OUT="$VITA_PORT/build/ai-test"
mkdir -p "$OUT"

if ! cmake --build "$VITA_PORT/build" -j"$(nproc)" > "$VITA_PORT/build/build.log" 2>&1; then
	echo "build failed, see vita/build/build.log"
	exit 1
fi

# Leave the usual arguments and any input script as they were.
cp "$DATA/vita-args.txt" "$OUT/vita-args.saved" 2>/dev/null
[ -e "$DATA/vita-input-test.txt" ] && mv "$DATA/vita-input-test.txt" "$OUT/vita-input-test.saved"
touch "$DATA/vita-profile.txt"

for game in "$@"; do
	read -r scenario sides turns minutes <<< "$game"
	{
		echo "--no-log-to-file"
		echo "--multiplayer"
		echo "--scenario=$scenario"
		echo "--turns=$turns"
		echo "--exit-at-end"
		for side in $(seq 1 "$sides"); do
			echo "--controller=$side:ai"
		done
	} > "$DATA/vita-args.txt"
	# Every 10 s, Enter or End Scenario (ctrl+space): at the end the game waits for both, with
	# --exit-at-end too - the message, then the map kept on screen ("linger"). An AI's turn takes
	# no notice of either.
	for t in $(seq 10000 20000 $((minutes * 60000))); do
		echo "$t key - Return"
		echo "$((t + 10000)) key ctrl space"
	done > "$DATA/vita-input-test.txt"

	start=$(date +%s)
	timeout -k 10 "$((minutes * 60))" vita3k --frontend sdl "$VITA_PORT/build/src/wesnoth.vpk" > "$OUT/$scenario.vita3k.log" 2>&1
	status=$?
	elapsed=$(( $(date +%s) - start ))
	sed -n 's/.*\*\*\* TTY: //p' "$OUT/$scenario.vita3k.log" | tr -d '\n' | sed 's/\[vita\]/\n[vita]/g' > "$OUT/$scenario.log"

	"$VITA_PORT/ai-test-summary.py" "$OUT/$scenario.log" "$OUT/$scenario.vita3k.log" "$scenario" "$sides" "$turns" "$status" "$elapsed"
done

cp "$OUT/vita-args.saved" "$DATA/vita-args.txt" 2>/dev/null
rm -f "$DATA/vita-input-test.txt"
[ -e "$OUT/vita-input-test.saved" ] && mv "$OUT/vita-input-test.saved" "$DATA/vita-input-test.txt"
exit 0
