#!/usr/bin/env python3
"""Prueft rd2/sdk/RD2API.H: Nummern eindeutig, in der richtigen Gruppe, Namen eindeutig.
Mit --list wird die Funktionstabelle ausgegeben (fuer SPEC/Handbuch)."""
import re, sys, os
p = os.path.join(os.path.dirname(__file__), "..", "sdk", "RD2API.H")
G = [(0x000,0x036,"Kern"),(0x037,0x049,"Speicher"),(0x04A,0x063,"Datei"),(0x064,0x0A8,"Grafik"),(0x0A9,0x0F2,"Oberflaeche"),(0x0F3,0x11A,"Dialoge"),(0x11B,0x12E,"Sound"),(0x12F,0x138,"Eingabe"),(0x139,0x14B,"Netz"),(0x14C,0x157,"Drucker"),(0x158,0x15F,"Seriell")]
rx = re.compile(r'^RDF\((0x[0-9A-Fa-f]+),\s*([^,]+),\s*(\w+),\s*\((.*)\)\)')
nums, names, err, rows = {}, {}, 0, []
for ln, line in enumerate(open(p, encoding="latin-1"), 1):
    if not line.startswith("RDF("): continue
    m = rx.match(line)
    if not m: print(f"{ln}: nicht lesbar"); err += 1; continue
    n, ret, name, args = int(m[1], 16), m[2].strip(), m[3], m[4]
    grp = [g for g in G if g[0] <= n <= g[1]]
    if not grp: print(f"{ln}: {name} 0x{n:03X} in keiner Gruppe"); err += 1
    if n in nums: print(f"{ln}: Nummer 0x{n:03X} doppelt ({nums[n]}, {name})"); err += 1
    if name in names: print(f"{ln}: Name {name} doppelt"); err += 1
    nums[n] = name; names[name] = n; rows.append((n, ret, name, args, grp[0][2] if grp else "?"))
if "--list" in sys.argv:
    for n, ret, name, args, g in rows: print(f"| 0x{n:03X} | {g} | `{ret} {name}({args})` |")
print(f"{len(rows)} Funktionen, {err} Fehler")
sys.exit(1 if err else 0)
