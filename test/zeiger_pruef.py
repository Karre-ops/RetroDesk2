#!/usr/bin/env python3
"""zeiger_pruef.py COM1-LOG: prueft in obj/zeig{1,2,3}_{cga,vga}.ppm, ob an der gemeldeten Position
die gemeldete Form steht ('#' schwarz, 'o' weiss; Formen wie in user/RDUSER.C, Quelle tools/zeiger.py)."""
import sys, re, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
from PIL import Image
import zeiger
FORM = {1: "WAIT", 2: "TEXT", 3: "CROSS", 4: "HAND"}
log = open(sys.argv[1], encoding="latin-1").read().replace("\r", "")
pos = re.findall(r"ZEIGTEST: (Z[123]) 0x([0-9A-F]{8})\nZEIGTEST: Form 0x([0-9A-F]{8})", log)
fehler = 0
if len(pos) != 6: print(f"FEHLER: {len(pos)} statt 6 Meldungen"); sys.exit(1)
for k, (z, v, f) in enumerate(pos):
    karte = "cga" if k < 3 else "vga"
    v = int(v, 16); x, y = v & 0xFFFF, v >> 16; form = FORM[int(f, 16)]
    hx, hy, art = zeiger.Z[form]
    im = Image.open(f"obj/zeig{z[1]}_{karte}.ppm").convert("L")
    sx = im.size[0] / (640 if karte == "cga" else 640); sy = im.size[1] / (200 if karte == "cga" else 480)
    rows = art.strip("\n").split("\n"); falsch = 0
    for r, zeile in enumerate(rows):
        for c, ch in enumerate(zeile):
            if ch == ".": continue
            px = im.getpixel((int((x - hx + c + 0.5) * sx), int((y - hy + r + 0.5) * sy)))
            if (px > 127) != (ch == "o"): falsch += 1
    print(("ok    " if not falsch else "FEHLER"), f"{karte} {z} {form} bei ({x},{y}): {falsch} Pixel falsch")
    fehler += falsch > 0
sys.exit(1 if fehler else 0)
