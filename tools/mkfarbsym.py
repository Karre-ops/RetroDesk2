#!/usr/bin/env python3
"""mkfarbsym.py AUS.DAT PROGRAMM:NR:BILD.TXT ...   Farbiger Symbolsatz (FARBSYM.DAT, Diskette 2).
   Je Eintrag: char name[8] (Programmdatei ohne Endung, gross, mit 0 aufgefuellt), WORD nr (RT_ICON),
   WORD laenge, dann das Bild als RDB1 (4 Bit, Maske). Der Kern (RDKX IconDraw) nimmt es statt des
   Symbols aus dem Programm, wenn der Schirm bunt ist. Fehlt die Datei, bleibt es schwarz-weiss."""
import sys, struct, subprocess, os, tempfile
out = b""
for a in sys.argv[2:]:
    name, nr, bild = a.split(":", 2)
    t = tempfile.mktemp(suffix=".rdb")
    subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "mkrdb.py"), t, bild, "--bpp", "4"], check=True, stdout=subprocess.DEVNULL)
    d = open(t, "rb").read(); os.remove(t)
    out += name.upper().encode().ljust(8, b"\0")[:8] + struct.pack("<HH", int(nr), len(d)) + d
open(sys.argv[1], "wb").write(out)
print(f"{sys.argv[1]}: {len(sys.argv) - 2} Symbole, {len(out)} Bytes")
