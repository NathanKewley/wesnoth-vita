#!/usr/bin/env python3
"""
Packs Wesnoth's images into one file for the Vita build: vita-images.pak (the files back to back)
and vita-images.idx (one line per image: path relative to the data directory, offset, size).

Opening a file on the Vita's memory card costs ~20 ms, and the game opens an image file the first
time each unit, terrain and portrait appears. From the pack, which stays open, an image is one
read at a known offset.

Sound effects go in after those, decoded to WAV: Vorbis decoding took ~70 ms a sound, the first
time each one played (most often in the middle of a fight).

Some images are far bigger than the Vita's 960x544 screen can show, and decoding them took seconds.
Those go in the pack shrunk (REDUCE), keeping their aspect ratio, which is all the game's layouts
depend on for them. The shrunk copies are added after all the original files, which stay where
they were: a pack already on the Vita is then the start of the new one, and only the end has to
be sent (curl -C - -T).

    vita/make-pack.py <out-dir>
"""
import io
import os
import subprocess
import sys

from PIL import Image

PORT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(PORT_DIR)
SKIP = (os.path.join("data", "test"), os.path.join("data", "tools"))
IMAGE_TYPES = (".png", ".webp", ".jpg", ".jpeg")
# path: height to shrink to. The game finds the format from the data, not the name.
REDUCE = {
    # Title and loading screen background, and the backdrop of the campaigns' journey maps
    # (never their base layer, so the journey's positions don't depend on it): 4096x2160.
    "data/core/images/maps/background.webp": 544,
    # The title screen's picture, scaled to fit the screen by its aspect ratio: 1280x960.
    "data/core/images/maps/titlescreen.webp": 544,
}


SOUND_TYPES = (".ogg", ".wav")
SOUND_CACHE = os.path.join(PORT_DIR, "build", "sound-cache")


def sounds():
    """Sound effects: everything in a "sounds" directory (music is elsewhere, and streamed)."""
    for top in ("data", "sounds"):
        for root, dirs, names in os.walk(os.path.join(SRC, top)):
            rel_root = os.path.relpath(root, SRC)
            dirs[:] = sorted(d for d in dirs if d != ".git" and not os.path.join(rel_root, d).startswith(SKIP))
            if top != "sounds" and "sounds" not in rel_root.split(os.sep):
                continue
            for name in sorted(names):
                if name.lower().endswith(SOUND_TYPES):
                    yield os.path.join(rel_root, name)


def decoded(rel):
    """The sound as 16-bit WAV at the mixer's 44.1 kHz: nothing to decode or resample on the Vita."""
    src = os.path.join(SRC, rel)
    cached = os.path.join(SOUND_CACHE, rel + ".wav")
    if not os.path.exists(cached) or os.path.getmtime(cached) < os.path.getmtime(src):
        os.makedirs(os.path.dirname(cached), exist_ok=True)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", src, "-ar", "44100", "-acodec", "pcm_s16le",
            "-map_metadata", "-1", "-f", "wav", cached], check=True)
    return open(cached, "rb").read()


def reduced(rel, height):
    image = Image.open(os.path.join(SRC, rel))
    width = round(image.width * height / image.height)
    image = image.resize((width, height), Image.LANCZOS)
    out = io.BytesIO()
    if image.mode == "RGB":
        image.save(out, "JPEG", quality=92)
    else:
        image.save(out, "PNG", compress_level=6)
    return out.getvalue()


def images():
    for top in ("data", "images"):
        for root, dirs, names in os.walk(os.path.join(SRC, top)):
            rel_root = os.path.relpath(root, SRC)
            dirs[:] = sorted(d for d in dirs if d != ".git" and not os.path.join(rel_root, d).startswith(SKIP))
            if top != "images" and "images" not in rel_root.split(os.sep):
                continue
            for name in sorted(names):
                if name.lower().endswith(IMAGE_TYPES):
                    yield os.path.join(rel_root, name)


def main():
    out = sys.argv[1]
    offset = 0
    entries = {}
    with open(os.path.join(out, "vita-images.pak"), "wb") as pak:
        for rel in images():
            data = open(os.path.join(SRC, rel), "rb").read()
            pak.write(data)
            entries[rel] = (offset, len(data))
            offset += len(data)
        for rel, height in REDUCE.items():
            data = reduced(rel, height)
            pak.write(data)
            entries[rel] = (offset, len(data))
            offset += len(data)
        for rel in sounds():
            data = decoded(rel)
            pak.write(data)
            entries[rel] = (offset, len(data))
            offset += len(data)
    with open(os.path.join(out, "vita-images.idx"), "w") as idx:
        for rel, (at, size) in entries.items():
            idx.write(f"{rel}\t{at}\t{size}\n")
    print(f"{offset / 1e6:.0f} MB")


if __name__ == "__main__":
    main()
