#!/usr/bin/env bash
# Puts Wesnoth's game data where the Vita build looks for it, ux0:data/wesnoth/.
#
#   vita/install-data.sh vita3k          # into Vita3K's ux0, as links to this checkout
#   vita/install-data.sh copy <dir>      # a copy in <dir>, to put on a Vita as ux0:data/wesnoth
set -euo pipefail

PORT="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "$PORT/.." && pwd)"
DIRS=(data fonts images sounds)

case "${1:-}" in
	vita3k)
		DEST="${VITA3K_UX0:-$HOME/.local/share/Vita3K/Vita3K/ux0}/data/wesnoth"
		mkdir -p "$DEST/userdata"
		for d in "${DIRS[@]}"; do
			ln -sfn "$SRC/$d" "$DEST/$d"
		done
		;;
	copy)
		DEST="${2:?usage: install-data.sh copy <dir>}"
		mkdir -p "$DEST/userdata"
		for d in "${DIRS[@]}"; do
			rsync -a --delete --exclude '.git' "$SRC/$d/" "$DEST/$d/"
		done
		;;
	*)
		sed -n '2,6p' "$0"
		exit 1
		;;
esac

cp "$PORT/fonts.conf" "$DEST/fonts.conf"
"$PORT/make-index.py" "$DEST/vita-index.txt"
echo "installed to $DEST"
