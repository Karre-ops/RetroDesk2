#!/usr/bin/env python3
"""mkschmal.py OUTDIR - SYSK.FNT: schmale, verschieden breite Systemschrift fuer 320 Punkte Breite (Karsten, 30.10.2026).
Hoehe 9 (Zeile 8 frei fuer den Unterstrich der Tastenkuerzel), Grundlinie Zeile 6, Codepage 437.
Zeichen aus tools/schmal_glyphen.py; was fehlt, wird '?'. Format wie mkfont.py (SPEC 7)."""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from schmal_glyphen import F
H, OBER, ERSTES, LETZTES = 9, 7, 32, 255
cp = {0x84: 'ä', 0x94: 'ö', 0x81: 'ü', 0x8E: 'Ä', 0x99: 'Ö', 0x9A: 'Ü', 0xE1: 'ß', 0xFB: '\u221a'}
breiten, offs, daten = b"", b"", b""
for c in range(ERSTES, LETZTES + 1):
    ch = cp.get(c, chr(c) if c < 127 else '?')
    st, rows = F.get(ch, F['?'])
    w = len(rows[0]); bild = [0] * H
    for r, z in enumerate(rows):
        v = 0
        for i, p in enumerate(z):
            if p == '#': v |= 0x80 >> i
        if 0 <= st + r < H: bild[st + r] = v
    breiten += bytes([w + 1]); offs += struct.pack("<H", len(daten)); daten += bytes(bild)
kopf = b"RDF1" + struct.pack("<HHHBBBB", H, OBER, 0, 0, ERSTES, LETZTES, 0) + b"SYSK.FNT".ljust(13, b"\0")
open(os.path.join(sys.argv[1], "SYSK.FNT"), "wb").write(kopf + breiten + offs + daten)
