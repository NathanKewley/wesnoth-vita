#!/usr/bin/env python3
"""
Copies Wesnoth's game data to a Vita over FTP (VitaShell's FTP server), as ux0:data/wesnoth, with
the image and sound pack (vita/make-pack.py) and the data index (vita/make-index.py).

    vita/ftp-upload.py 192.168.1.10:1337

Files go up in batches of a few hundred per curl run, each batch over one connection, since
VitaShell's FTP is slow to set up a connection per file. Finished batches are recorded in
vita/build/ftp-upload.done, so running it again after an interruption carries on where it stopped.
"""
import os
import subprocess
import sys
import urllib.parse

PORT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(PORT_DIR)
DIRS = ["data", "fonts", "images", "sounds"]
# Wesnoth's unit-test scenarios and developer tools: not needed to play.
SKIP = {os.path.join("data", "test"), os.path.join("data", "tools")}
DEST = "ux0:/data/wesnoth"
BATCH = 300
DONE_FILE = os.path.join(PORT_DIR, "build", "ftp-upload.done")


def files():
    yield os.path.join(PORT_DIR, "fonts.conf"), "fonts.conf"
    index = os.path.join(PORT_DIR, "build", "vita-index.txt")
    subprocess.run([sys.executable, os.path.join(PORT_DIR, "make-index.py"), index], check=True)
    yield index, "vita-index.txt"
    for top in DIRS:
        for root, dirs, names in os.walk(os.path.join(SRC, top)):
            rel_root = os.path.relpath(root, SRC)
            dirs[:] = sorted(d for d in dirs if d != ".git" and os.path.join(rel_root, d) not in SKIP)
            for name in sorted(names):
                yield os.path.join(root, name), os.path.join(rel_root, name)


def main():
    host = sys.argv[1]
    base = f"ftp://{host}/{DEST}/"
    all_files = list(files())
    batches = [all_files[i:i + BATCH] for i in range(0, len(all_files), BATCH)]
    total_bytes = sum(os.path.getsize(path) for path, _ in all_files)
    print(f"{len(all_files)} files, {total_bytes / 1e6:.0f} MB, {len(batches)} batches", flush=True)

    os.makedirs(os.path.dirname(DONE_FILE), exist_ok=True)
    done = set()
    if os.path.exists(DONE_FILE):
        done = {int(line) for line in open(DONE_FILE) if line.strip()}

    for n, batch in enumerate(batches):
        if n in done:
            continue
        config = []
        for path, rel in batch:
            url = base + urllib.parse.quote(rel)
            config.append(f'upload-file = "{path}"')
            config.append(f'url = "{url}"')
        result = subprocess.run(
            ["curl", "--silent", "--show-error", "--ftp-create-dirs", "--retry", "3", "-K", "-"],
            input="\n".join(config), text=True)
        if result.returncode != 0:
            print(f"batch {n} failed (curl exit {result.returncode}); run again to resume", flush=True)
            sys.exit(1)
        with open(DONE_FILE, "a") as f:
            f.write(f"{n}\n")
        print(f"batch {n + 1}/{len(batches)} done", flush=True)

    # The image and sound pack (vita/make-pack.py): one big file, sent resumably. make-pack.py adds
    # to its end only, so after a data update just the new end goes up.
    pack_dir = os.path.join(PORT_DIR, "build")
    subprocess.run([sys.executable, os.path.join(PORT_DIR, "make-pack.py"), pack_dir], check=True)
    for name, resume in (("vita-images.pak", True), ("vita-images.idx", False)):
        result = subprocess.run(["curl", "--silent", "--show-error", "--retry", "3"]
            + (["-C", "-"] if resume else [])
            + ["-T", os.path.join(pack_dir, name), base + name])
        if result.returncode != 0:
            print(f"{name} failed (curl exit {result.returncode}); run again to resume", flush=True)
            sys.exit(1)
        print(f"{name} done", flush=True)

    print("all uploaded", flush=True)


if __name__ == "__main__":
    main()
