"""bildvergleich.py foto.ppm referenz.png [...]: Schwarz/Weiss-Vergleich Pixel fuer Pixel.
Die Uhr oben rechts (64 x Hoehe/20) wird nicht verglichen, ausser mit MIT_UHR=1.
Fehlt die Referenz, wird das Foto nur als PNG abgelegt (obj/*.png) und als neu gemeldet."""
import sys, os
from PIL import Image
fehler = 0
a = sys.argv[1:]
for foto, ref in zip(a[0::2], a[1::2]):
    if not os.path.exists(foto):
        print(f"FEHLER kein Bild {foto}"); fehler += 1; continue
    b = Image.open(foto).convert('L').point(lambda v: 255 if v >= 128 else 0).convert('1')
    png = os.path.splitext(foto)[0] + '.png'; b.save(png)
    if not os.path.exists(ref):
        print(f"NEU    {png} ({b.size[0]}x{b.size[1]}), keine Referenz {ref}"); continue
    r = Image.open(ref).convert('1')
    if b.size == (r.size[0], 2 * r.size[1]): b = b.resize(r.size, Image.NEAREST)   # QEMU verdoppelt CGA-Zeilen
    if r.size != b.size:
        print(f"FEHLER {foto}: Groesse {b.size} statt {r.size}"); fehler += 1; continue
    if not os.environ.get('MIT_UHR'):   # Uhr rechts in der Menueleiste (HH:MM) ausblenden
        from PIL import ImageDraw
        w, h = r.size; k = (w - 64, 0, w - 1, h // 20)
        b = b.copy(); r = r.copy(); ImageDraw.Draw(b).rectangle(k, fill=1); ImageDraw.Draw(r).rectangle(k, fill=1)
    d = sum(1 for p, q in zip(b.tobytes(), r.tobytes()) if p != q)
    print(("ok    " if d == 0 else "FEHLER") + f" {foto}: {d} Pixel abweichend")
    fehler += d != 0
sys.exit(1 if fehler else 0)
