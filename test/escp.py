#!/usr/bin/env python3
"""escp.py DATEI AUS.png -- Epson ESC/P (9 Nadeln) als Bild: ESC @, ESC K (60 dpi), ESC L (120 dpi),
ESC J n (n/216 Zoll), CR, LF (1/6 Zoll), FF (neue Seite). Raster 120 x 72 dpi; ESC K setzt jede Spalte doppelt.
Seiten untereinander, 4 Zeilen Abstand. Ausgabe: Seiten, Durchgaenge, Groesse, unbekannte Befehle."""
import sys
from PIL import Image
d = open(sys.argv[1], 'rb').read()
seiten, punkte, y, i, passes, unbek = [], set(), 0, 0, 0, []
maxx = 0
def neue():
    global punkte, y
    seiten.append(punkte); punkte = set(); y = 0
while i < len(d):
    c = d[i]
    if c == 27:
        k = d[i + 1]
        if k in (ord('K'), ord('L')):
            n = d[i + 2] | d[i + 3] << 8; b = d[i + 4:i + 4 + n]; f = 2 if k == ord('K') else 1
            for x, v in enumerate(b):
                for p in range(8):
                    if v & (0x80 >> p):
                        for q in range(f): punkte.add((x * f + q, y // 3 + p))
            maxx = max(maxx, n * f); passes += 1; i += 4 + n; continue
        if k == ord('J'): y += d[i + 2]; i += 3; continue
        if k == ord('@'): i += 2; continue
        unbek.append('ESC %c' % k); i += 2; continue
    if c == 13: pass
    elif c == 10: y += 36
    elif c == 12: neue()
    else: unbek.append(hex(c))
    i += 1
if punkte: neue()
H = 11 * 72; W = max(maxx, 960)
im = Image.new('L', (W, max(1, len(seiten)) * (H + 4)), 255)
for s, pk in enumerate(seiten):
    for (x, yy) in pk:
        if x < W and yy < H: im.putpixel((x, s * (H + 4) + yy), 0)
im.save(sys.argv[2])
print('Seiten', len(seiten), 'Durchgaenge', passes, 'Breite', maxx, 'unbekannt', sorted(set(unbek)))
