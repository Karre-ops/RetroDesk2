#!/usr/bin/env python3
"""rd32mod.py AUS.M32 EXPORTE DATEIEN [BIBLIOTHEKEN]   32-Bit-Modul fuer RD2/32 bauen (PLAN_32BIT)
EXPORTE: Komma-Liste der C-Namen (Watcom-Registeraufruf: name_, __pascal: NAME). DATEIEN/BIBLIOTHEKEN: wie wlink,
Komma-getrennt; RD32M0.OBJ muss vorn stehen. Das Modul wird zweimal gelinkt (Offset 0 und 1 MB): die DWORDs, die sich
genau um 1 MB unterscheiden, sind Verschiebungen. Format: Bild | Anzahl Exporte, je (Offset, Laenge, Name) | Anzahl
Verschiebungen, je Offset. Der Lader (rd32_modload) addiert die Ladeadresse und fuellt die Tabelle im Kopf."""
import sys, struct, subprocess, os, re
aus, exp, dateien = sys.argv[1], [e for e in sys.argv[2].split(",") if e], sys.argv[3]
libs = sys.argv[4] if len(sys.argv) > 4 else ""
D = 0x100000
def link(off, name):
    cmd = ["wlink", "option", "quiet", "format", "raw", "bin", "name", name, "option", f"offset={off}", "file", dateien]
    if libs: cmd += ["library", libs]
    cmd += ["option", f"nodefaultlibs,eliminate,start=rd32_mkopf,map={name}.map"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    fehler = [z for z in (r.stdout + r.stderr).splitlines() if z.strip() and "W1014" not in z and "W1023" not in z]
    if fehler or r.returncode: sys.exit("rd32mod: " + "\n".join(fehler))
    return bytearray(open(name, "rb").read()), open(name + ".map", encoding="latin-1").read()
b0, m0 = link(0, aus + ".0")
b1, _ = link(D, aus + ".1")
if len(b0) != len(b1) or b0[:4] != b"R32M": sys.exit("rd32mod: Bilder ungleich lang oder Kopf R32M fehlt (RD32M0 zuerst)")
rel, i = [], 0
while i + 4 <= len(b0):
    v0 = struct.unpack_from("<I", b0, i)[0]; v1 = struct.unpack_from("<I", b1, i)[0]
    if v1 - v0 == D: rel.append(i); i += 4; continue
    if v0 != v1 and b0[i] != b1[i]: sys.exit(f"rd32mod: unerwarteter Unterschied bei {i:#x}")
    i += 1
sym = {}
for z in m0.splitlines():
    t = z.split()
    if len(t) >= 2 and re.fullmatch(r"[0-9a-fA-F]{8}\*?", t[0]): sym[t[1]] = int(t[0].rstrip("*"), 16)
tab = bytearray(struct.pack("<I", len(exp)))
for e in exp:
    for k in (e + "_", e.upper(), "_" + e, e):
        if k in sym: break
    else: sys.exit(f"rd32mod: Export {e} nicht gefunden")
    n = e.encode()
    if len(n) > 255: sys.exit("rd32mod: Name zu lang")
    tab += struct.pack("<IB", sym[k], len(n)) + n
tab += struct.pack("<I", len(rel)) + b"".join(struct.pack("<I", r) for r in rel)
struct.pack_into("<I", b0, 8, len(b0))
msize = struct.unpack_from("<I", b0, 12)[0]
open(aus, "wb").write(b0 + tab)
for f in (aus + ".0", aus + ".1", aus + ".1.map"): os.remove(f)
os.replace(aus + ".0.map", aus + ".map")
print(f"rd32mod: {aus} Bild {len(b0)} Bytes, Speicher {msize}, {len(exp)} Exporte, {len(rel)} Verschiebungen")
