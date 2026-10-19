#!/usr/bin/env python3
"""Erzeugt aus sdk/RD2API.H:
   sdk/lib/Fnnn.ASM Aufrufstuecke fuer Programme (jmp ueber die API-Tabelle), werden zu RD2.LIB
   kern/NOTSUP.ASM  API-Tabelle des Kerns, vorbelegt mit "nicht vorhanden"-Stuecken
                    (AX=DX=0, RETF mit der richtigen Parametergroesse, weil pascal)"""
import re, os
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
rx = re.compile(r'^RDF\((0x[0-9A-Fa-f]+),\s*([^,]+),\s*(\w+),\s*\((.*)\)\)')
FOUR = ("DWORD", "long", "COLOR", "FARPROC", "WNDPROC", "DLGPROC", "LPSTR", "LPCSTR")
def argbytes(args):
    args = args.strip()
    if args in ("", "void"): return 0
    n = 0
    for a in args.split(","):
        a = a.strip()
        n += 4 if ("*" in a or a.split()[0] in FOUR or (a.startswith("const") and a.split()[1] in FOUR)) else 2
    return n
fns = []
for line in open(os.path.join(D, "sdk", "RD2API.H"), encoding="latin-1"):
    m = rx.match(line)
    if m: fns.append((int(m[1], 16), m[3], argbytes(m[4])))
hdr = "; ERZEUGT von tools/mkapi.py aus sdk/RD2API.H - nicht von Hand aendern\n.8086\n"
# Je Funktion ein eigenes Stueck (sdk/lib/*.ASM), damit RD2.LIB nur Benutztes einbindet.
L = os.path.join(D, "sdk", "lib"); os.makedirs(L, exist_ok=True)
for x in os.listdir(L): os.remove(os.path.join(L, x))
for n, name, ab in fns:
    with open(os.path.join(L, f"F{n:03X}.ASM"), "w") as f:
        f.write(hdr + "DGROUP group _DATA\n_DATA segment word public 'DATA'\nextrn __rd2api:dword\n_DATA ends\n")
        f.write(f"RD2LIB_TEXT segment word public 'CODE'\nassume cs:RD2LIB_TEXT, es:DGROUP\npublic {name.upper()}\n")
        f.write(f"{name.upper()}:\n  mov bx, DGROUP\n  mov es, bx\n  les bx, es:__rd2api\n  jmp dword ptr es:[bx+{n*4}]\n")
        f.write("RD2LIB_TEXT ends\nend\n")
with open(os.path.join(D, "kern", "NOTSUP.ASM"), "w") as f:
    f.write(hdr + "NOTSUP_TEXT segment word public 'CODE'\nassume cs:NOTSUP_TEXT\n")
    sizes = sorted(set(ab for _, _, ab in fns) | {0})
    for ab in sizes:
        f.write(f"ns{ab}:\n  xor ax, ax\n  xor dx, dx\n  retf {ab}\n" if ab else "ns0:\n  xor ax, ax\n  xor dx, dx\n  retf\n")
    f.write("NOTSUP_TEXT ends\nDGROUP group _DATA\n_DATA segment word public 'DATA'\npublic _rd_api\n_rd_api label dword\n")
    by = {n: ab for n, _, ab in fns}
    for i in range(0x400):
        f.write(f"  dd ns{by.get(i, 0)}\n")
    f.write("_DATA ends\nend\n")
print(f"{len(fns)} Funktionen -> sdk/lib/*.ASM, kern/NOTSUP.ASM")
