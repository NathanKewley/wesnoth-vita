#!/usr/bin/env python3
"""
Reads a PS Vita crash dump (ux0:data/psp2core-*.psp2dmp) of the Wesnoth build: which threads
stopped, their registers, and every stack word that points into Wesnoth's code, resolved to a
function with addr2line.

    vita/core-dump.py <dump.psp2dmp> [vita/build/src/wesnoth]

The dump is a gzipped ELF core file: modules, threads and registers are in its notes, memory in its
load segments. (Format as read by xyzz/vita-parse-core.)
"""
import gzip
import io
import os
import struct
import subprocess
import sys

from elftools.elf.elffile import ELFFile

PORT_DIR = os.path.dirname(os.path.abspath(__file__))
ADDR2LINE = os.path.join(os.environ.get("VITASDK", "/opt/vitasdk"), "bin", "arm-vita-eabi-addr2line")
STOP_REASONS = {0x30002: "undefined instruction", 0x30003: "prefetch abort", 0x30004: "data abort",
                0x60080: "division by zero"}


def u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def c_str(data, off):
    return data[off:data.index(b"\0", off)].decode("latin-1")


def load(path):
    raw = open(path, "rb").read()
    try:
        raw = gzip.decompress(raw)
    except OSError:
        pass
    elf = ELFFile(io.BytesIO(raw))
    notes, memory = {}, []
    for seg in elf.iter_segments():
        if seg.header.p_type == "PT_NOTE":
            for note in seg.iter_notes():
                notes[note["n_name"]] = note["n_desc"]
        elif seg.header.p_type == "PT_LOAD":
            memory.append((seg.header.p_vaddr, seg.data()))
    return notes, memory


def modules(notes):
    data = notes["MODULE_INFO"]
    off, result = 8, []
    for _ in range(u32(data, 4)):
        name, nsegs = c_str(data, off + 0x24), u32(data, off + 0x4C)
        off += 0x50
        segs = [(u32(data, off + 0x14 * i + 8), u32(data, off + 0x14 * i + 12)) for i in range(nsegs)]
        off += 0x14 * nsegs + 0x10
        result.append((name, segs))
    return result


def threads(notes):
    info, regs = notes["THREAD_INFO"], notes["THREAD_REG_INFO"]
    result, off = {}, 8
    for _ in range(u32(info, 4)):
        size = u32(info, off)
        uid = u32(info, off + 4)
        result[uid] = {"name": c_str(info, off + 8), "stop": u32(info, off + 0x74), "pc": u32(info, off + 0x9C)}
        off += size
    off = 8
    for _ in range(u32(regs, 4)):
        size = u32(regs, off)
        tid = u32(regs, off + 4)
        if tid in result:
            result[tid]["gpr"] = [u32(regs, off + 8 + 4 * i) for i in range(16)]
        off += size
    return result


def main():
    dump = sys.argv[1]
    elf_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(PORT_DIR, "build", "src", "wesnoth")
    notes, memory = load(dump)
    mods = modules(notes)
    # The game's code segment, wherever the loader put it, maps to the ELF's link address.
    game = next(m for m in mods if m[0].startswith("wesnoth") or m[0].endswith(".elf") or m[0] == "eboot.bin")
    code_start, code_size = game[1][0]
    link_base = 0x81000000

    def in_code(addr):
        return code_start <= (addr & ~1) < code_start + code_size

    def resolve(addrs):
        if not addrs:
            return []
        args = [ADDR2LINE, "-f", "-C", "-e", elf_path] + [hex((a & ~1) - code_start + link_base) for a in addrs]
        out = subprocess.run(args, capture_output=True, text=True).stdout.splitlines()
        return [out[i] for i in range(0, len(out), 2)]

    def read_word(addr):
        for base, data in memory:
            if base <= addr < base + len(data) - 3:
                return u32(data, addr - base)
        return None

    print(f"game module {game[0]}: code at 0x{code_start:x} (+0x{code_size:x})")
    for uid, t in threads(notes).items():
        if not t["stop"]:
            continue
        gpr = t.get("gpr", [0] * 16)
        print(f"\nthread {t['name']} <0x{uid:x}> stopped: {STOP_REASONS.get(t['stop'], hex(t['stop']))}")
        pc, lr, sp = t["pc"], gpr[14], gpr[13]
        for label, addr in (("PC", pc), ("LR", lr)):
            where = resolve([addr])[0] if in_code(addr) else "(not in game code)"
            print(f"  {label} 0x{addr:08x}  {where}")
        print("  registers: " + " ".join(f"r{i}=0x{gpr[i]:x}" for i in range(13)) + f" sp=0x{sp:x}")
        stack = [w for w in (read_word(sp + 4 * i) for i in range(512)) if w and in_code(w)]
        print("  code addresses on the stack (most recent first):")
        seen = []
        for name in resolve(stack):
            if not seen or seen[-1] != name:
                seen.append(name)
        for name in seen[:40]:
            print("    " + name[:160])


if __name__ == "__main__":
    main()
