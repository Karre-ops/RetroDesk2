#!/usr/bin/env python3
"""pcl.py DATEI AUS.png -- HP PCL Rastergrafik als Bild: ESC E, ESC &l..A, ESC *t n R (dpi), ESC *p n x/Y, ESC *r n A,
ESC *b n M (Kompression 0 oder 2), ESC *b n W (Zeile), ESC *b n Y (Zeilen ueberspringen), ESC *r B/C, FF.
Ausgabe: Seiten, Zeilen, dpi, Kompression, Tintenrahmen in Zoll, unbekannte Befehle. Bild in der Druckaufloesung."""
import sys, re
from PIL import Image
d = open(sys.argv[1], 'rb').read()
seiten, punkte, y, i, dpi, mode, zeilen, unbek, ras = [], set(), 0, 0, 75, 0, 0, [], False
def neue():
    global punkte, y
    seiten.append(punkte); punkte = set(); y = 0
def entpacke(b):
    o, k = bytearray(), 0
    while k < len(b):
        n = b[k]; k += 1
        if n < 128: o += b[k:k + n + 1]; k += n + 1
        elif n > 128: o += bytes([b[k]]) * (257 - n); k += 1
    return bytes(o)
pat = re.compile(rb'\x1b([*&])([a-z])((?:[-+]?[0-9.]+[a-z])*[-+]?[0-9.]*[A-Z@])')
while i < len(d):
    c = d[i]
    if c == 27:
        if d[i + 1] == ord('E'): i += 2; continue
        m = pat.match(d, i)
        if not m: unbek.append('ESC ' + chr(d[i + 1])); i += 2; continue
        g, gr = m.group(1) + m.group(2), m.group(3); i = m.end()
        for wert, buch in re.findall(rb'([-+]?[0-9.]*)([a-zA-Z@])', gr):
            v = int(float(wert)) if wert else 0; b = buch.upper()
            if g == b'*t' and b == b'R': dpi = v
            elif g == b'*b' and b == b'M': mode = v
            elif g == b'*b' and b == b'W':
                z = d[i:i + v]; i += v
                if mode == 2: z = entpacke(z)
                for x, w in enumerate(z):
                    for p in range(8):
                        if w & (0x80 >> p): punkte.add((x * 8 + p, y))
                y += 1; zeilen += 1
            elif g == b'*b' and b == b'Y': y += v
            elif g == b'*r' and b == b'A': ras = True
            elif g == b'*r' and b in (b'B', b'C'): ras = False
            elif g == b'*p' and b == b'Y': y = v
            elif g in (b'&l', b'*p', b'*r'): pass
            else: unbek.append((g + b).decode())
        continue
    if c == 12: neue()
    else: unbek.append(hex(c))
    i += 1
if punkte: neue()
W, H = 8 * dpi, 11 * dpi
im = Image.new('L', (W, max(1, len(seiten)) * (H + 4)), 255)
for s, pk in enumerate(seiten):
    for (x, yy) in pk:
        if x < W and yy < H: im.putpixel((x, s * (H + 4) + yy), 0)
im.save(sys.argv[2])
r = ''
if seiten and seiten[0]:
    xs = [p[0] for p in seiten[0]]; ys = [p[1] for p in seiten[0]]
    r = 'Rahmen %.2f %.2f %.2f %.2f' % (min(xs) / dpi, min(ys) / dpi, max(xs) / dpi, max(ys) / dpi)
print('Seiten', len(seiten), 'Zeilen', zeilen, 'dpi', dpi, 'Kompression', mode, r, 'unbekannt', sorted(set(unbek)))
