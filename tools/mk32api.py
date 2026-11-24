#!/usr/bin/env python3
"""mk32api.py sdk/RD2API.H AUS.C   32-Bit-Aufrufstuecke fuer RD2/32 (PLAN_32BIT Stufe 4b)
Je RDF-Zeile eine Funktion mit der Signatur aus RD2.H (32 Bit, flach): Werte als Woerter in Push-Reihenfolge
(pascal), Zeiger auf Daten werden mit rd32_pin unter 1 MB gebracht (liegen sie schon dort: direkt), Ausgaben
danach zurueckkopiert. Rueckgabe von Zeigern (StrGet, MemLock) als 32-Bit-Zeiger auf den Speicher unter 1 MB.
Funktionen mit Rueckrufen oder Zeigern in Strukturen stehen von Hand in sdk/RD32LIB.C (HAND)."""
import re, sys
HAND = {"TaktSet", "TaktKill", "ModProc", "RegisterClass", "DialogRun", "DialogIndirect", "FileDialog", "PrnOpen", "MemFixedFree"}
EIN = {"WORD", "short", "BOOL", "BYTE", "char", "HWND", "HDC", "HMEM", "HINST", "HTASK", "HMENU", "HSOCK", "HFONT", "HBMP", "HMOD", "unsigned"}
ZWEI = {"DWORD", "long", "COLOR", "LONG"}
GROESSE = ("len", "n", "max", "groesse", "laenge", "size")
def teile(a):
    a = a.strip()
    if a in ("void", ""): return []
    return [p.strip() for p in a.split(",")]
def param(p, i):
    m = re.match(r"^(const\s+)?(\w+)\s+FAR\s*\*\s*(\w*)$", p)
    if m: return ("zeiger", bool(m.group(1)), m.group(2), m.group(3) or f"a{i}")
    t = p.split(); typ = " ".join(t[:-1]) if len(t) > 1 else t[0]; nm = t[-1] if len(t) > 1 else f"a{i}"
    if typ == "LPCSTR": return ("text", True, "char", nm)
    if typ == "LPSTR": return ("zeiger", False, "char", nm)
    if typ in EIN: return ("w1", None, typ, nm)
    if typ in ZWEI: return ("w2", None, typ, nm)
    raise SystemExit(f"mk32api: Typ unbekannt: {p}")
out = ["/* RD32API.C - erzeugt von tools/mk32api.py aus sdk/RD2API.H, nicht von Hand aendern */",
       '#include "RD2.H"', '#include "RD32INT.H"', ""]
n = 0
for z in open(sys.argv[1], encoding="latin-1"):
    m = re.match(r"^RDF\((0x[0-9A-Fa-f]+),\s*([^,]+?),\s*(\w+),\s*\((.*)\)\)", z)
    if not m: continue
    nr, ret, name, args = m.group(1), m.group(2).strip(), m.group(3), m.group(4)
    if name in HAND: continue
    ps = [param(p, i) for i, p in enumerate(teile(args))]
    namen = [p[3] for p in ps]
    sig = ", ".join(teile(args)) or "void"
    k = [f"{ret} RDAPI {name}({sig})", "{"]
    k.append("    WORD x_s = rd32_xp; DWORD x_r; PIN x_b[6]; WORD x_n = 0;" if ps else "    DWORD x_r;")
    pz = {}
    for i, (art, const, typ, nm) in enumerate(ps):
        if art == "text": k.append(f"    DWORD p{i} = rd32_pin(&x_b[x_n++], {nm}, {nm} ? rd32_slen({nm}) + 1 : 0, 0);"); pz[i] = True
        elif art == "zeiger":
            gr = None
            for j in range(i + 1, len(ps)):
                if ps[j][3] in GROESSE: gr = ps[j][3]; break
            if typ in ("void", "char"): g = gr or ("260" if typ == "char" else "0")
            elif typ in ("POINT", "COLOR") and gr: g = f"(DWORD){gr} * sizeof({typ})"
            else: g = f"sizeof({typ})"
            k.append(f"    DWORD p{i} = rd32_pin(&x_b[x_n++], {nm}, {g}, {0 if const else (2 if typ == 'char' else 1)});"); pz[i] = True
    for i, (art, const, typ, nm) in enumerate(ps):
        if i in pz: k.append(f"    rd32_w2(p{i});")
        elif art == "w1": k.append(f"    rd32_w1((WORD){nm});")
        else: k.append(f"    rd32_w2((DWORD){nm});")
    k.append(f"    x_r = rd32_ruf({nr});")
    if ps: k.append("    rd32_unpin(x_b, x_n); rd32_xp = x_s;")
    if ret == "void": k.append("    (void)x_r;")
    elif "FAR *" in ret or ret == "LPCSTR": k.append(f"    return ({ret})rd32_fp2p(x_r);")
    else: k.append(f"    return ({ret})x_r;")
    k.append("}"); out += k; n += 1
open(sys.argv[2], "w").write("\n".join(out) + "\n")
print(f"mk32api: {n} Funktionen")
