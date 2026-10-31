#!/usr/bin/env python3
"""Minimal rapified-config (config.bin) to text converter, for reading vanilla/mod configs.

    python3 -I tools/derap.py config.bin > config.cpp
"""
import struct
import sys


class Reader:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def u8(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.d, self.p)[0]
        self.p += 4
        return v

    def i32(self):
        v = struct.unpack_from("<i", self.d, self.p)[0]
        self.p += 4
        return v

    def f32(self):
        v = struct.unpack_from("<f", self.d, self.p)[0]
        self.p += 4
        return v

    def cstr(self):
        end = self.d.index(b"\0", self.p)
        s = self.d[self.p:end].decode("utf-8", "replace")
        self.p = end + 1
        return s

    def compressed_int(self):
        v, shift = 0, 0
        while True:
            b = self.u8()
            v |= (b & 0x7F) << shift
            shift += 7
            if not b & 0x80:
                return v


def fmt_scalar(r, sub):
    if sub == 0:
        return '"' + r.cstr().replace('"', '""') + '"'
    if sub == 1:
        return repr(round(r.f32(), 6))
    if sub == 2:
        return str(r.i32())
    raise ValueError(f"bad subtype {sub}")


def read_array(r):
    items = []
    for _ in range(r.compressed_int()):
        t = r.u8()
        if t == 3:
            items.append(read_array(r))
        else:
            items.append(fmt_scalar(r, t))
    return "{" + ",".join(items) + "}"


def read_class(r, offset, indent, out):
    save = r.p
    r.p = offset
    r.cstr()  # inherited name, already printed by the caller
    for _ in range(r.compressed_int()):
        kind = r.u8()
        pad = "\t" * indent
        if kind == 0:
            name = r.cstr()
            body = r.u32()
            out.append(f"{pad}class {name}")
            here = r.p
            r.p = body
            parent = r.cstr()
            r.p = here
            if parent:
                out[-1] += f": {parent}"
            out.append(pad + "{")
            read_class(r, body, indent + 1, out)
            out.append(pad + "};")
        elif kind == 1:
            sub = r.u8()
            name = r.cstr()
            out.append(f"{pad}{name} = {fmt_scalar(r, sub)};")
        elif kind == 2:
            name = r.cstr()
            out.append(f"{pad}{name}[] = {read_array(r)};")
        elif kind == 3:
            name = r.cstr()
            out.append(f"{pad}class {name};")
        elif kind == 4:
            out.append(f"{pad}delete {r.cstr()};")
        elif kind == 5:
            r.u32()  # flags
            name = r.cstr()
            out.append(f"{pad}{name}[] += {read_array(r)};")
        else:
            raise ValueError(f"bad entry type {kind} at {r.p}")
    r.p = save


def main():
    data = open(sys.argv[1], "rb").read()
    if data[:4] != b"\0raP":
        sys.exit("not a rapified config")
    r = Reader(data)
    r.p = 16
    out = []
    read_class(r, 16, 0, out)
    sys.stdout.write("\n".join(out) + "\n")


main()
