#!/usr/bin/env python3
"""budget.py OUTDIR [IMAGE] - prueft Dateigroessen gegen tools/budget.txt; mit IMAGE auch die Systemdiskette."""
import sys, os, subprocess
D = os.path.dirname(os.path.abspath(__file__))
out = sys.argv[1]; img = sys.argv[2] if len(sys.argv) > 2 else None
err = 0; lim = {}
for line in open(os.path.join(D, "budget.txt"), encoding="latin-1"):
    t = line.split("#")[0].split()
    if len(t) >= 2: lim[t[0]] = int(t[1])
for name, mx in lim.items():
    if name.startswith("@"): continue
    p = os.path.join(out, name)
    if not os.path.exists(p): continue
    n = os.path.getsize(p)
    ok = n <= mx; err += not ok
    print(f"{'ok    ' if ok else 'FEHLER'} {name:13} {n:7} / {mx:7} Bytes")
for f in sorted(os.listdir(out)):
    if f.upper() == f and f not in lim and f.endswith((".EXE", ".MOD", ".DRV", ".FNT", ".LNG")):
        print(f"FEHLER {f:13} fehlt in budget.txt"); err += 1
if img:
    free = [l for l in subprocess.run(["mdir", "-i", img, "::"], capture_output=True, text=True).stdout.split("\n") if "bytes free" in l][0]
    freeb = int("".join(c for c in free.split("bytes")[0] if c.isdigit()))
    used = 713728 - freeb   # 720-KB-Diskette: 1426 Cluster x 512 Bytes nutzbar
    ok = used <= lim["@SYSTEM_DISK"]; err += not ok
    print(f"{'ok    ' if ok else 'FEHLER'} Systemdiskette belegt {used} / {lim['@SYSTEM_DISK']} Bytes, frei {freeb}")
print("Budget:", "ok" if not err else f"{err} Fehler")
sys.exit(1 if err else 0)
