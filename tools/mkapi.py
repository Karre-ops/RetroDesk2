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
    f.write(hdr + "DGROUP group _BSS, _DATA\n_DATA segment word public 'DATA'\nextrn _k_lasterr:word\n_DATA ends\n_BSS segment word public 'BSS'\n_BSS ends\nNOTSUP_TEXT segment word public 'CODE'\nassume cs:NOTSUP_TEXT, ds:DGROUP\n")
    sizes = sorted(set(ab for _, _, ab in fns) | {0})
    for ab in sizes:
        f.write(f"ns{ab}:\n  call nsset\n" + (f"  retf {ab}\n" if ab else "  retf\n"))
    # LastError = RD_ERR_NOTSUP, Rueckgabe 0 (AX, DX)
    f.write("nsset:\n  push ds\n  mov ax, DGROUP\n  mov ds, ax\n  mov _k_lasterr, 8\n  pop ds\n  xor ax, ax\n  xor dx, dx\n  ret\n")
    # Tabelle in _BSS (nicht in der Datei), rd_nsinit fuellt sie beim Start
    f.write("nstab label word\n")
    for n, _, ab in fns: f.write(f"  dw {n*4}, ns{ab}\n")
    f.write(f"nsend label word\npublic _rd_nsinit\n_rd_nsinit proc near\n  push di\n  push si\n  push es\n  push ds\n  pop es\n")
    f.write("  mov di, offset DGROUP:_rd_api\n  mov cx, 1024\n  cld\nns_l0:\n  mov ax, offset ns0\n  stosw\n  mov ax, cs\n  stosw\n  loop ns_l0\n")
    f.write("  mov si, offset nstab\nns_l1:\n  cmp si, offset nsend\n  jae ns_l2\n  mov bx, cs:[si]\n  mov ax, cs:[si+2]\n")
    f.write("  mov word ptr _rd_api[bx], ax\n  mov word ptr _rd_api[bx+2], cs\n  add si, 4\n  jmp ns_l1\nns_l2:\n  pop es\n  pop si\n  pop di\n  ret\n_rd_nsinit endp\n")
    f.write("NOTSUP_TEXT ends\n_BSS segment word public 'BSS'\npublic _rd_api\n_rd_api dd 1024 dup(?)\n_BSS ends\nend\n")
print(f"{len(fns)} Funktionen -> sdk/lib/*.ASM, kern/NOTSUP.ASM")
