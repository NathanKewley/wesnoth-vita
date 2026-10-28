#!/usr/bin/env python3
"""
Writes the index of Wesnoth's game data the Vita build reads at start-up (vita-index.txt): every
file and directory under data, images, sounds and fonts, one per line, relative to the data
directory, directories ending in '/'.

With it the game answers "does this file exist?" for its own data from memory. Every image is
searched for in a dozen or more directories, and asking the memory card each time made the first
sight of every unit and terrain stall.

    vita/make-index.py <out-file>
"""
import os
import sys

PORT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(PORT_DIR)
DIRS = ["data", "fonts", "images", "sounds"]
SKIP = {os.path.join("data", "test"), os.path.join("data", "tools")}


def entries():
    for top in DIRS:
        yield top + "/"
        for root, dirs, names in os.walk(os.path.join(SRC, top)):
            rel_root = os.path.relpath(root, SRC)
            dirs[:] = sorted(d for d in dirs if d != ".git" and os.path.join(rel_root, d) not in SKIP)
            for d in dirs:
                yield os.path.join(rel_root, d) + "/"
            for name in sorted(names):
                yield os.path.join(rel_root, name)


def main():
    with open(sys.argv[1], "w") as out:
        for entry in entries():
            out.write(entry + "\n")


if __name__ == "__main__":
    main()
