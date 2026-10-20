#!/usr/bin/env python3
"""mkfont.py OUTDIR - erzeugt die Systemschriften SYS8.FNT, SYS14.FNT, SYS16.FNT (Codepage 437).

Quellen: 8x8 und 8x16 aus dem VGA-BIOS von SeaBIOS (LGPL, IBM-Zeichenformen),
8x14 aus RDFONT.F14 von RetroDesk 1. Zeichen 32..255.
Format .FNT (SPEC 7): 'RDF1', WORD hoehe, oberlaenge, stil, BYTE fest, erstes, letztes, 0,
char name[13], BYTE breite[n], WORD offset[n], Zeichendaten zeilenweise ((breite+7)/8 Bytes je Zeile)."""
import sys, os, struct
D = os.path.dirname(os.path.abspath(__file__))
out = sys.argv[1]
ERSTES, LETZTES = 32, 255

def bios():
    for f in ("/usr/share/seabios/vgabios-stdvga.bin", "/usr/share/seabios/vgabios-ati.bin", "/usr/share/qemu/vgabios-stdvga.bin"):
        if os.path.exists(f): return open(f, "rb").read()
    sys.exit("mkfont: kein VGA-BIOS gefunden")

def suche(d, muster, h):
    i = d.find(muster)
    if i < 0: sys.exit("mkfont: Schrift %d nicht gefunden" % h)
    return i - 0x41 * h

def fnt(name, h, ober, glyph):
    n = LETZTES - ERSTES + 1
    kopf = b"RDF1" + struct.pack("<HHHBBBB", h, ober, 0, 1, ERSTES, LETZTES, 0) + name.encode().ljust(13, b"\0")
    breite = bytes([8] * n)
    offs = b"".join(struct.pack("<H", i * h) for i in range(n))
    daten = b"".join(glyph(c) for c in range(ERSTES, LETZTES + 1))
    p = os.path.join(out, name)
    open(p, "wb").write(kopf + breite + offs + daten)
    print(name, os.path.getsize(p))

d = bios()
o8 = suche(d, bytes([0x30, 0x78, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0]), 8)
o16 = suche(d, bytes([0, 0, 0x10, 0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0xC6, 0, 0, 0, 0]), 16)
f14 = open(os.path.join(D, "..", "..", "RDFONT.F14"), "rb").read()
fnt("SYS8.FNT", 8, 7, lambda c: d[o8 + c * 8:o8 + c * 8 + 8])
fnt("SYS14.FNT", 14, 11, lambda c: f14[c * 14:c * 14 + 14])
fnt("SYS16.FNT", 16, 12, lambda c: d[o16 + c * 16:o16 + c * 16 + 16])
