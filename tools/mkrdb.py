#!/usr/bin/env python3
"""mkrdb.py AUS.RDB BILD.TXT [BILD2.TXT]   .RDB aus Textbildern (SPEC.md 7)
   Zeichen: '#' dunkel, '.' hell, Leerzeichen durchsichtig (dann mit Maske), 'g' grau (nur --bpp 4).
   --bpp 1|4 (4 = Palette mit 16 Farben, Standard 1), --cga: das letzte Bild ist die CGA-Fassung.
   Mehrere Bilder kommen hintereinander in eine Datei (Symbol normal + CGA)."""
import sys, struct
a = [x for x in sys.argv[1:] if not x.startswith("--")]
bpp = 4 if "--bpp" in " ".join(sys.argv) and "4" in sys.argv[sys.argv.index("--bpp") + 1] else 1
a = [x for x in a if x != "4" and x != "1"]
if len(a) < 2: sys.exit(__doc__)
PAL = [(0, 0, 0)] + [(0, 0, 0)] * 7 + [(128, 128, 128)] + [(0, 0, 0)] * 6 + [(255, 255, 255)]
out = b""
for n, f in enumerate(a[1:]):
    z = [l.rstrip("\n") for l in open(f) if l.strip("\n") != "" or True]
    while z and not z[-1].strip(): z.pop()
    w = max(len(l) for l in z); h = len(z); z = [l.ljust(w) for l in z]
    mask = any(" " in l for l in z); cga = "--cga" in sys.argv and n == len(a) - 2
    out += b"RDB1" + struct.pack("<HHHH", w, h, bpp, (1 if mask else 0) | (4 if cga else 0))
    if bpp == 4: out += b"".join(bytes(c) for c in PAL)
    for l in z:
        if bpp == 1:
            r = bytearray((w + 7) // 8)
            for i, c in enumerate(l):
                if c in ". ": r[i >> 3] |= 0x80 >> (i & 7)
        else:
            r = bytearray((w + 1) // 2)
            for i, c in enumerate(l):
                v = {"#": 0, ".": 15, " ": 15, "g": 8}[c]
                r[i >> 1] |= v << (0 if i & 1 else 4)
        out += bytes(r)
    if mask:
        for l in z:
            r = bytearray((w + 7) // 8)
            for i, c in enumerate(l):
                if c != " ": r[i >> 3] |= 0x80 >> (i & 7)
            out += bytes(r)
open(a[0], "wb").write(out)
print(f"{a[0]}: {len(a) - 1} Bild(er), {len(out)} Bytes")
