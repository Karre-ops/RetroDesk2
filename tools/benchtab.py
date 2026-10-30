#!/usr/bin/env python3
"""benchtab.py - Tabelle aus obj/bench_modi/*.txt (BENCH2 je Grafikart): Mikrosekunden je Aufgabe."""
import re, os, sys
D = 'obj/bench_modi'
INFO = {'CGA': ('CGA 640x200 mono', 640, 200), 'CGA4': ('CGA 320x200 4', 320, 200), 'HGC': ('Hercules 720x348', 720, 348),
        'VGA': ('VGA 640x480 mono', 640, 480), 'VGA4': ('VGA 640x480 4', 640, 480), 'VGA16': ('VGA 640x480 16', 640, 480),
        'MCGA': ('MCGA 320x200 256', 320, 200), 'EGAM': ('EGA 640x350 mono', 640, 350), 'EGA4': ('EGA 640x350 4', 640, 350),
        'EGA': ('EGA 640x350 16', 640, 350), 'EGA200': ('EGA 640x200 16', 640, 200), 'EGA320': ('EGA 320x200 16', 320, 200),
        'VESA': ('VESA 640x480 256 (386)', 640, 480), 'VESA800': ('VESA 800x600 256 (386)', 800, 600),
        'VESA1024': ('VESA 1024x768 256 (386)', 1024, 768), 'SVGA16': ('VESA 800x600 16', 800, 600)}
AUF = [('Text 20 Zeichen, x=13', 'Text durchs.'), ('Text deckend, x=13', 'Text deckend'), ('Schirm fuellen', 'Schirm fuellen'),
       ('Muster 50% voll', 'Muster'), ('Rechteck 200x100 invertieren', 'Invert 200x100'), ('Rahmen 200x100', 'Rahmen'),
       ('Block 208x150 sichern+zurueck', 'Block 1 Bit'), ('Block 208x150 Geraeteformat', 'Block Geraet'), ('Block 208x150 roh', 'Block roh')]
def lies(g):
    p = os.path.join(D, g + '.txt'); r = {}; last = None
    if not os.path.exists(p): return r
    for l in open(p, errors='replace'):
        l = l.strip(); m = re.match(r'BENCH2: us (0x[0-9A-Fa-f]+)', l)
        if m and last: r[last] = int(m.group(1), 16); last = None; continue
        m = re.match(r'BENCH2: (.*)$', l)
        if m and not m.group(1).startswith(('us', 'fertig', 'kein')): last = m.group(1).strip()
    return r
z = ['| Grafikart | ' + ' | '.join(k for _, k in AUF) + ' | Füllen je 1000 Punkte |', '|' + '---|' * (len(AUF) + 2)]
for g in sys.argv[1:] or INFO:
    r = lies(g)
    if not r: z.append(f'| {INFO[g][0]} | ' + ' | '.join('–' for _ in AUF) + ' | – |'); continue
    n, w, h = INFO[g]; f = r.get('Schirm fuellen')
    z.append(f'| {n} | ' + ' | '.join(f"{r[a]:,}".replace(',', '.') if a in r else '–' for a, _ in AUF) + f" | {f * 1000 // (w * h) if f else '–'} |")
print('\n'.join(z))
