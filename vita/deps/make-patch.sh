#!/usr/bin/env bash
# Writes vita/deps/patches/<name>/0001-vita.patch from the edits made to the unpacked tree in
# vita/deps/build/<name>, against a fresh copy of the release tarball.
#
#   vita/deps/make-patch.sh glib meson.build glib/gthread-posix.c ...
#
# Every file the port changes has to be listed, not only the new one: the patch is rewritten.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
NAME="$1"
shift

PRISTINE="$(mktemp -d)"
trap 'rm -rf "$PRISTINE"' EXIT
tar -xf "$(ls "$HERE/src/$NAME"*.tar.* | head -1)" -C "$PRISTINE" --strip-components=1

mkdir -p "$HERE/patches/$NAME"
OUT="$HERE/patches/$NAME/0001-vita.patch"
: > "$OUT"
for f in "$@"; do
	(cd "$PRISTINE" && diff -u "$f" "$HERE/build/$NAME/$f" || true) \
		| sed "s|^--- $f|--- a/$f|; s|^+++ $HERE/build/$NAME/$f|+++ b/$f|" >> "$OUT"
done
echo "$(grep -c '^@@' "$OUT") hunks -> $OUT"
