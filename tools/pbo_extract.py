#!/usr/bin/env python3
"""Minimal PBO extractor (uncompressed entries only), for reading vanilla DayZ scripts.

    python3 -I tools/pbo_extract.py ARCHIVE.pbo OUT_DIR [FILTER_SUBSTRING]
"""
import os
import struct
import sys


def read_cstr(f):
    out = bytearray()
    while (c := f.read(1)) not in (b"\0", b""):
        out += c
    return out.decode("utf-8", "replace")


def main():
    archive, out_dir = sys.argv[1], sys.argv[2]
    flt = sys.argv[3].lower() if len(sys.argv) > 3 else ""
    with open(archive, "rb") as f:
        entries = []
        while True:
            name = read_cstr(f)
            method, orig, _res, _ts, size = struct.unpack("<5I", f.read(20))
            if name == "" and method == 0x56657273:  # 'Vers' header with properties
                while read_cstr(f):
                    read_cstr(f)
                continue
            if name == "":
                break
            entries.append((name, method, orig, size))
        for name, method, orig, size in entries:
            data = f.read(size)
            if flt and flt not in name.lower():
                continue
            if method != 0 and orig != size:
                print(f"skip compressed: {name}", file=sys.stderr)
                continue
            rel = name.replace("\\", "/")
            if rel.startswith("/") or ".." in rel.split("/"):
                continue
            path = os.path.join(out_dir, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as o:
                o.write(data)
    print(f"{len(entries)} entries")


main()
