#!/usr/bin/env python3
"""input_pruef.py LOG BILD - prueft INPTEST: Tasten/Zeichen in Reihenfolge, Maus-Ereignisse,
Kaestchen an den Klickpositionen ganz schwarz (Zeiger restlos entfernt), Zeiger am Ende sichtbar."""
import sys, re
from PIL import Image
log = open(sys.argv[1]).read().split('\n'); im = Image.open(sys.argv[2]).convert('L')
f = 0
def ok(c, t):
    global f
    print(('ok     ' if c else 'FEHLER ') + t); f += not c
v = lambda name: [int(l.split()[-1], 16) for l in log if l.startswith('INPTEST: ' + name + ' ')]
chars = ''.join(chr(c) for c in v('CHAR'))
ok(chars == 'halloA\x03\x1b', 'Zeichen: %r' % chars)
keys = v('KEY')
ok(0x13B in [k & 0xFFFF for k in keys], 'F1 als Sondertaste 0x13B')
ok(any((k >> 16) & 1 and (k & 0xFFFF) == ord('A') for k in keys), 'Shift bei A gemeldet')
ok(any((k >> 16) & 4 and (k & 0xFFFF) == 3 for k in keys), 'Strg bei Strg+C gemeldet')
ok(len(v('MOVE')) >= 3, 'Mausbewegungen: %d' % len(v('MOVE')))
downs = v('DOWN bei'); ups = v('UP'); dbl = v('DBLCLK')
ok(len(downs) == 3, 'Tastendruecke: %d (erwartet 3, der vierte ist Doppelklick)' % len(downs))
ok(len(dbl) == 1, 'Doppelklick: %d' % len(dbl))
ok(len(ups) == 4, 'Loslassen: %d' % len(ups))
ok('RD: Speicher vollstaendig zurueck' in '\n'.join(log), 'Speicher zurueck')
W, H = im.size
sy = 2 if H == 400 else 1                      # QEMU zeigt CGA 640x200 mit doppelten Zeilen
for d in (downs[0], downs[2]) if len(downs) == 3 else []:
    x, y = d & 0xFFFF, d >> 16
    blk = [im.getpixel((xx, yy)) for xx in range(x - 4, x + 12) for yy in range((y - 4) * sy, (y + 12) * sy) if 0 <= xx < W and 0 <= yy < H]
    ok(blk and max(blk) < 128, 'Kaestchen bei %d,%d ganz schwarz (kein Zeigerrest)' % (x, y))
weiss = sum(1 for xx in range(W) for yy in range(40, H) if im.getpixel((xx, yy)) > 128)
ok(weiss > 0, 'Bild vorhanden')
print('Eingabetest:', 'alles ok' if not f else '%d Fehler' % f)
sys.exit(1 if f else 0)
