#!/usr/bin/env python3
"""rd32link.py BILD.B32 STUB.EXE NAME [--res X.RES]   RD2/32-Programm fertigstellen (PLAN_32BIT Schritt 4)
Bild (wlink format raw bin, RD32C0 zuerst; den Speicher bis Ende BSS traegt der Linker in den Kopf ein) als
Ressource 11 Nr. 1 zusammen mit der .RES (BEDARF) an den 16-Bit-Vorspann haengen. DOS-Fassung: RDRC ... /32:BILD.B32"""
import sys, struct, re, subprocess, os, argparse
ap = argparse.ArgumentParser(); ap.add_argument("bild"); ap.add_argument("stub"); ap.add_argument("name"); ap.add_argument("--res")
a = ap.parse_args()
b = bytearray(open(a.bild, "rb").read())
if b[:4] != b"R32!": sys.exit("rd32link: Kopf R32! fehlt (RD32C0 zuerst linken)")
ende = struct.unpack_from("<I", b, 16)[0]
eintr = []
if a.res:
    r = open(a.res, "rb").read(); assert r[:4] == b"RDR1"; n = struct.unpack_from("<H", r, 4)[0]; p = 6
    for _ in range(n):
        typ, nr, sp, fl, ln = struct.unpack_from("<HHHHI", r, p); p += 12; eintr.append((typ, nr, sp, fl, r[p:p + ln])); p += ln
eintr.append((11, 1, 0, 2, bytes(b)))   # Flags 2 wie rdrc.py/RDRC
out = bytearray(b"RDR1" + struct.pack("<H", len(eintr)))
for typ, nr, sp, fl, d in eintr: out += struct.pack("<HHHHI", typ, nr, sp, fl, len(d)) + d
rf = a.stub + ".r32"; open(rf, "wb").write(out)
subprocess.check_call([sys.executable, os.path.join(os.path.dirname(__file__), "rd2link.py"), a.stub, a.name, "--res", rf], stdout=subprocess.DEVNULL)
os.remove(rf)
print(f"rd32link: {a.stub} 32-Bit-Bild {len(b)} Bytes, Speicher {ende}")
