#!/usr/bin/env python3
"""budget.py OUTDIR [IMAGE] - prueft Dateigroessen und Speicherbedarf (MZ-Bild + Mindestzuteilung)
gegen tools/budget.txt; mit IMAGE auch die Systemdiskette."""
import sys, os, subprocess, struct
D = os.path.dirname(os.path.abspath(__file__))
out = sys.argv[1]; img = sys.argv[2] if len(sys.argv) > 2 else None
err = 0; lim = {}; ram = {}
for line in open(os.path.join(D, "budget.txt"), encoding="latin-1"):
    t = line.split("#")[0].split()
    if len(t) >= 2: lim[t[0]] = int(t[1])
    if len(t) >= 3 and t[2].replace('.', '', 1).isdigit(): ram[t[0]] = float(t[2])   # KB, auch 18.5
def speicher(p):                                  # belegte Bytes nach dem Laden, 0 = kein MZ
    h = open(p, "rb").read(28)
    if h[:2] != b"MZ": return 0
    cblp, cp, _, hdr, mina = struct.unpack("<HHHHH", h[2:12])
    return cp * 512 - (512 - cblp if cblp else 0) - hdr * 16 + mina * 16
belegt = {}
for name, mx in lim.items():
    if name.startswith("@"): continue
    p = os.path.join(out, name)
    if not os.path.exists(p): continue
    n = os.path.getsize(p)
    ok = n <= mx; err += not ok
    m = speicher(p); belegt[name] = m; r = ram.get(name, 0)
    rok = not m or not r or m <= r * 1024; err += not rok
    print(f"{'ok    ' if ok and rok else 'FEHLER'} {name:13} {n:7} / {mx:7} Bytes" + (f"   Speicher {m:6} / {int(r * 1024):6}" if m and r else ""))
for f in sorted(os.listdir(out)):
    if f.upper() == f and f not in lim and f.endswith((".EXE", ".MOD", ".DRV", ".FNT", ".LNG")):
        print(f"FEHLER {f:13} fehlt in budget.txt"); err += 1
# Profil Schnell (8088, 640 KB): was immer im Speicher liegt. Verwerfbares und Schriften zaehlen nicht.
fest = ["RD.EXE", "RDGDI.MOD", "RDUSER.MOD", "RDSND.MOD", "SPEAKER.DRV"]
drv = max([belegt.get(d, 0) for d in ("CGA.DRV", "HGC.DRV", "VGAM.DRV")] or [0])
summe = sum(belegt.get(f, 0) for f in fest) + drv
ok = summe <= lim.get("@RESIDENT_SCHNELL", 1 << 30); err += not ok
print(f"{'ok    ' if ok else 'FEHLER'} resident im Profil Schnell (vorhandene Teile) {summe} / {lim.get('@RESIDENT_SCHNELL', 0)} Bytes = {summe / 1024:.1f} KB")
if img:
    free = [l for l in subprocess.run(["mdir", "-i", img, "::"], capture_output=True, text=True).stdout.split("\n") if "bytes free" in l][0]
    freeb = int("".join(c for c in free.split("bytes")[0] if c.isdigit()))
    used = 713728 - freeb   # 720-KB-Diskette: 1426 Cluster x 512 Bytes nutzbar
    ok = used <= lim["@SYSTEM_DISK"]; err += not ok
    print(f"{'ok    ' if ok else 'FEHLER'} Systemdiskette belegt {used} / {lim['@SYSTEM_DISK']} Bytes, frei {freeb}")
print("Budget:", "ok" if not err else f"{err} Fehler")
sys.exit(1 if err else 0)
