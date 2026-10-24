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
disk = {}                                         # Datei -> Diskette (Spalte 4)
for line in open(os.path.join(D, "budget.txt"), encoding="latin-1"):
    t = line.split("#")[0].split()
    if len(t) >= 4 and not t[0].startswith("@"): disk[t[0]] = t[3]
def clu(n): return (n + 1023) // 1024 * 1024   # 360 KB: Cluster 1 KB
for dk, name in (("S", "Systemdiskette"), ("P", "Programmdiskette"), ("N", "Netzdiskette"), ("D", "SDK-Diskette")):
    fs = [f for f, d in disk.items() if d == dk and os.path.exists(os.path.join(out, f))]
    if not fs: continue
    summe = sum(clu(os.path.getsize(os.path.join(out, f))) for f in fs) + (lim.get("@DOS", 0) if dk == "S" else 0)
    mx = lim["@SYSTEM_DISK"] if dk == "S" else lim["@DISK"]
    ok = summe <= mx; err += not ok
    print(f"{'ok    ' if ok else 'FEHLER'} {name} (360 KB) {summe} / {mx} Bytes" + (" inkl. DOS 5" if dk == "S" else "") + f", {len(fs)} Dateien, frei {lim['@DISK'] - summe}")
if img:                                           # Testdiskette: tatsaechlich belegt
    b = open(img, "rb").read(32)
    bps, spc, res, nf, rde, tot, _, spf = struct.unpack("<HBHBHHBH", b[11:24])
    nutz = (tot - res - nf * spf - rde * 32 // bps) * bps
    free = [l for l in subprocess.run(["mdir", "-i", img, "::"], capture_output=True, text=True).stdout.split("\n") if "bytes free" in l][0]
    freeb = int("".join(c for c in free.split("bytes")[0] if c.isdigit()))
    print(f"ok     Testdiskette ({tot * bps // 1024} KB, FreeDOS + Tests) belegt {nutz - freeb} / {nutz} Bytes, frei {freeb}")
print("Budget:", "ok" if not err else f"{err} Fehler")
sys.exit(1 if err else 0)
