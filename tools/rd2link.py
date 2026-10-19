#!/usr/bin/env python3
"""rd2link.py EXE Name [--api 0x0100] [--flags 0] [--stack 4096] [--kb 0] [--ver 0x0100] [--type P|M|D]
Haengt den RD2-Block und den 16-Byte-Abschluss an eine MZ-EXE (SPEC.md Abschnitt 2).
Ein vorhandener Anhang wird ersetzt. Ressourcen kommen spaeter mit RDRC dazu."""
import sys, struct, argparse
a = argparse.ArgumentParser()
a.add_argument("exe"); a.add_argument("name")
for o, d in (("api", "0x0100"), ("flags", "0"), ("stack", "4096"), ("kb", "0"), ("ver", "0x0100")):
    a.add_argument("--" + o, default=d)
a.add_argument("--type", default="P", choices="PMD")
o = a.parse_args()
d = bytearray(open(o.exe, "rb").read())
if d[:2] != b"MZ": sys.exit("keine MZ-EXE")
if d[-16:-12] == b"RD2!":
    blk = struct.unpack_from("<I", d, len(d) - 12)[0]; d = d[:blk]
cblp, cp = struct.unpack_from("<HH", d, 2)
img_end = cp * 512 - ((512 - cblp) if cblp else 0)
if len(d) < img_end: sys.exit("Datei kuerzer als im MZ-Kopf angegeben")
d = d[:img_end]
i = lambda s: int(s, 0)
name = o.name.encode("cp437")[:15].ljust(16, b"\0")
blk = struct.pack("<4sHHHH16sHHHHH10s", ("RD2" + o.type).encode(), i(o.api), i(o.flags), i(o.stack), i(o.kb),
                  name, i(o.ver), 0, 0, 48, 48, b"\0" * 10)
assert len(blk) == 48
off = len(d)
d += blk + struct.pack("<4sIIHH", b"RD2!", off, len(blk), sum(blk) & 0xFFFF, 0)
open(o.exe, "wb").write(d)
print(f"{o.exe}: RD2-Anhang bei {off}, Datei {len(d)} Bytes")
