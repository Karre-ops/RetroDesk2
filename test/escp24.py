#!/usr/bin/env python3
"""escp24.py DATEI AUS.png -- Epson ESC/P 24 Nadeln als Bild: ESC @, ESC * m n1 n2 (m 32/33/38/39/40: 60/120/90/180/360
dpi, 3 Bytes je Spalte), ESC J n (n/180 Zoll), CR, FF. Raster 180 dpi senkrecht, waagrecht wie m (Ausgabe auf 180 dpi
gestreckt/gestaucht, nur zur Ansicht). Seiten untereinander. Ausgabe: Seiten, Durchgaenge, dpi, Tintenrahmen in Zoll."""
import sys
from PIL import Image
d = open(sys.argv[1], 'rb').read()
DPI = {32: 60, 33: 120, 38: 90, 39: 180, 40: 360}
seiten, punkte, y, i, passes, unbek, dx = [], set(), 0, 0, 0, [], set()
def neue():
    global punkte, y
    seiten.append(punkte); punkte = set(); y = 0
while i < len(d):
    c = d[i]
    if c == 27:
        k = d[i + 1]
        if k == ord('*'):
            m = d[i + 2]; n = d[i + 3] | d[i + 4] << 8; b = d[i + 5:i + 5 + 3 * n]; xd = DPI.get(m, 180); dx.add(xd)
            for x in range(n):
                for j in range(3):
                    v = b[3 * x + j]
                    for p in range(8):
                        if v & (0x80 >> p): punkte.add((x * 180 // xd, y + 8 * j + p))
            passes += 1; i += 5 + 3 * n; continue
        if k == ord('J'): y += d[i + 2]; i += 3; continue
        if k == ord('@'): i += 2; continue
        unbek.append('ESC %c' % k); i += 2; continue
    if c == 13: pass
    elif c == 10: y += 30
    elif c == 12: neue()
    else: unbek.append(hex(c))
    i += 1
if punkte: neue()
W, H = 8 * 180, 11 * 180
im = Image.new('L', (W, max(1, len(seiten)) * (H + 4)), 255)
for s, pk in enumerate(seiten):
    for (x, yy) in pk:
        if x < W and yy < H: im.putpixel((x, s * (H + 4) + yy), 0)
im.save(sys.argv[2])
r = ''
if seiten and seiten[0]:
    xs = [p[0] for p in seiten[0]]; ys = [p[1] for p in seiten[0]]
    r = 'Rahmen %.2f %.2f %.2f %.2f' % (min(xs) / 180, min(ys) / 180, max(xs) / 180, max(ys) / 180)
print('Seiten', len(seiten), 'Durchgaenge', passes, 'dpi', sorted(dx), r, 'unbekannt', sorted(set(unbek)))
