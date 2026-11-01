#!/usr/bin/env python3
"""mkpas.py AUS.PAS - Unit RD2 fuer Turbo Pascal 5.5-7.0 und Free Pascal (i8086-msdos) aus sdk/RD2.H und sdk/RD2API.H.

Jede RD2-Funktion ist ein direkter Sprung in die API-Tabelle (pascal, far): bei Turbo Pascal als inline-Makro
(les bx,[RD2Api]; call far es:[bx+n*4]), bei Free Pascal als assembler-Stueck mit jmp far. Zeiger, Texte und
Strukturen werden als Pointer uebergeben (@x, Z('Text')). Strukturen heissen T<Name> (TMsg, TRect ...)."""
import re, sys, os

SDK = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sdk")
RESERVIERT = set("""and array asm begin case const constructor destructor div do downto else end file for function goto
if implementation in inherited inline interface label mod nil not object of or packed procedure program record repeat
set shl shr string then to type unit until uses var while with xor absolute assembler far forward interrupt near
private public virtual external exports library operator out result self""".split())
HANDLES = "HMEM HWND HDC HFONT HBMP HMOD HTASK HINST HMENU HSOCK".split()
SKALAR = {"WORD": "Word", "short": "Integer", "BYTE": "Byte", "DWORD": "Longint", "COLOR": "Longint", "BOOL": "Integer",
          "long": "Longint", "int": "Integer", "unsigned": "Word", "char": "Char"}
for h in HANDLES: SKALAR[h] = h

def pname(n):
    return n + "_" if n.lower() in RESERVIERT else n

def ptyp(t):
    t = re.sub(r"\bFAR\b", "", t.replace("const ", "")).strip()
    if "*" in t or t in ("LPCSTR", "LPSTR", "WNDPROC", "DLGPROC", "FARPROC"): return "Pointer"
    if t in SKALAR: return SKALAR[t]
    raise ValueError("Typ " + t)

h = open(os.path.join(SDK, "RD2.H"), encoding="latin-1").read()
api = open(os.path.join(SDK, "RD2API.H"), encoding="latin-1").read()
h = re.sub(r"/\*.*?\*/", "", h, flags=re.S)
api_ohne = re.sub(r"/\*.*?\*/", "", api, flags=re.S)

# ---- Strukturen -------------------------------------------------------------------
recs, rnamen = [], {}
for m in re.finditer(r"typedef\s+struct\s*\{(.*?)\}\s*(\w+)\s*;", h, re.S):
    body, name = m[1], m[2]
    tname = "T" + name.capitalize() if name not in ("WNDCLASS", "SYSINFO", "TASKINFO", "RDTIME", "FINDDATA", "DRIVEINFO",
             "FONTINFO", "FILEDLG", "JOYSTATE", "NETINFO", "PRNJOB", "GFXINFO") else None
    tname = tname or {"WNDCLASS": "TWndClass", "SYSINFO": "TSysInfo", "TASKINFO": "TTaskInfo", "RDTIME": "TRdTime",
             "FINDDATA": "TFindData", "DRIVEINFO": "TDriveInfo", "FONTINFO": "TFontInfo", "FILEDLG": "TFileDlg",
             "JOYSTATE": "TJoyState", "NETINFO": "TNetInfo", "PRNJOB": "TPrnJob", "GFXINFO": "TGfxInfo"}[name]
    rnamen[name] = tname
    felder = []
    for decl in body.split(";"):
        decl = decl.strip()
        if not decl: continue
        mm = re.match(r"(\w+)\s+(.*)", decl)
        ct, rest = mm[1], mm[2]
        for v in rest.split(","):
            v = v.strip(); arr = re.match(r"(\w+)\s*\[(\d+)\]", v)
            if ct in rnamen: pt = rnamen[ct]
            elif ct in ("LPCSTR", "LPSTR", "WNDPROC", "DLGPROC"): pt = "Pointer"
            else: pt = SKALAR[ct]
            if arr: felder.append((pname(arr[1]), f"array[0..{int(arr[2]) - 1}] of {pt}"))
            else: felder.append((pname(v), pt))
    recs.append((tname, felder))

# ---- Konstanten ---------------------------------------------------------------------
def ausdruck(v, bekannt):
    v = v.strip()
    v = re.sub(r"RGB\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)", lambda m: "$%06X" % (int(m[1]) | int(m[2]) << 8 | int(m[3]) << 16), v)
    v = re.sub(r"\((?:COLOR|WORD|DWORD|BYTE|short|long)\)", "", v)
    v = re.sub(r"\b0[xX]([0-9A-Fa-f]+)[uUlL]*\b", lambda m: "$" + m[1].upper(), v)
    v = re.sub(r"\b(\d+)[uUlL]+\b", r"\1", v)
    if re.fullmatch(r'"[^"\\]*"', v): return "'" + v[1:-1] + "'"
    if re.search(r"['\"?:]", v): return None
    for w in re.findall(r"(?<![$\w])[A-Za-z_]\w*", v):
        if w not in bekannt: return None
    v = v.replace("<<", " shl ").replace(">>", " shr ").replace("|", " or ").replace("&", " and ").replace("~", " not ")
    return re.sub(r"\s+", " ", v).strip()

konst, bekannt, ausgelassen = [], set(), []
for m in re.finditer(r"^#define\s+(\w+)[ \t]+([^\n]+)$", h, re.M):
    n, v = m[1], m[2].strip()
    if n.startswith("RD2_") and n not in ("RD2_API_MAJOR", "RD2_API_MINOR", "RD2_API_VERSION") or n in ("FAR", "RDAPI", "RDCALLBACK", "RDEXPORT", "TRUE", "FALSE"):
        continue
    p = ausdruck(v, bekannt)
    if p is None or "(" in n: ausgelassen.append(n); continue
    konst.append((n, p)); bekannt.add(n)

# ---- Funktionen ---------------------------------------------------------------------
funk = []
for m in re.finditer(r"^RDF\(\s*0x([0-9A-Fa-f]+)\s*,\s*([\w ]+?\*?)\s*,\s*(\w+)\s*,\s*\((.*?)\)\s*\)", api_ohne, re.M):
    nr, ret, name, par = int(m[1], 16), m[2].strip(), m[3], m[4].strip()
    ps = []
    if par != "void":
        for i, p in enumerate(par.split(",")):
            p = p.strip()
            mm = re.match(r"(.*?)(\w+)$", p)
            typ, nm = (mm[1].strip(), mm[2]) if mm and mm[1].strip() and mm[1].strip() not in ("const",) and not mm[1].strip().endswith("const") else (p, None)
            if nm in SKALAR or nm in ("LPCSTR", "LPSTR", "WNDPROC", "DLGPROC", "FARPROC") or typ.endswith("const"):
                typ, nm = p, None
            if nm is None: nm = f"a{i + 1}"
            ps.append((pname(nm), ptyp(typ)))
    seen = set()
    for i, (n, t) in enumerate(ps):
        while n.lower() in seen: n += "_"
        seen.add(n.lower()); ps[i] = (n, t)
    r = None if ret == "void" else ptyp(ret.replace("void FAR *", "void *") if "*" in ret else ret)
    funk.append((nr, name, ps, r))

def kopf(name, ps, r):
    gr = []                                   # gleiche Typen hintereinander zusammenfassen
    for n, t in ps:
        if gr and gr[-1][1] == t: gr[-1][0].append(n)
        else: gr.append(([n], t))
    a = "(" + "; ".join(", ".join(ns) + ": " + t for ns, t in gr) + ")" if ps else ""
    k = f"function {name}{a}: {r}" if r else f"procedure {name}{a}"
    return umbruch(k)

def umbruch(k, breite=100):                   # Turbo Pascal: hoechstens 127 Zeichen je Zeile
    z, zeilen = "", []
    for teil in re.split(r"(?<=[;,/]) ", k):
        if z and len(z) + len(teil) + 1 > breite: zeilen.append(z); z = "    " + teil
        else: z = z + " " + teil if z else teil
    zeilen.append(z)
    return "\n".join(zeilen)

o = []
w = o.append
w("{ RD2.PAS - Unit RD2 fuer RetroDesk-2-Programme in Pascal (API %d.%d)" % (1, 0))
w("  Erzeugt von tools/mkpas.py aus sdk/RD2.H und sdk/RD2API.H, nicht von Hand aendern.")
w("  SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING")
w("  Turbo Pascal 5.5, 6.0, 7.0:  TPC PROG.PAS, dann RDRC PROG.RC PROG.EXE \"Name\" /PAS")
w("  Free Pascal (i8086-msdos):   ppcross8086 -Tmsdos -WmLarge PROG.PAS, dann ebenso RDRC ... /PAS")
w("  RD2 muss die erste Unit in uses sein. Keine Crt, Graph oder Write auf den Bildschirm.")
w("  Zeiger, Texte, Strukturen: als Pointer (@Rechteck, Z('Text')). Fensterfunktionen: far, pascal,")
w("  per CbProc(@Funktion) uebergeben (setzt DS auf die Daten des Programms). }")
w("unit RD2;")
w("{$IFDEF FPC} {$MODE TP} {$ASMMODE INTEL} {$PACKRECORDS 1} {$ENDIF}")
w("{$F+}")
w("interface")
w("")
w("type")
for hn in HANDLES: w(f"  {hn} = Word;")
for tname, felder in recs:
    w(f"  {tname} = record")
    for n, t in felder: w(f"    {n}: {t};")
    w("  end;")
w("")
w("const")
for n, v in konst: w(f"  {n} = {v};")
w("")
w("var")
w("  RD2Api: Pointer;                     { API-Tabelle des Kerns }")
w("  RD2Inst, RD2Show: Word;              { HINST und show aus dem Start }")
w("  RD2Cmd: Pointer;                     { Befehlszeile (0-terminiert) }")
w("  Egal: Longint;                       { TP 5.5 kann Funktionsergebnisse nicht verwerfen: Egal := SetBkMode(...) }")
w("")
w("{ Hilfen (in der Unit) }")
w("function Z(s: String): Pointer;       { Pascal-String -> 0-terminiert (8 Puffer im Wechsel) }")
w("function CbProc(p: Pointer): Pointer;  { Rueckruf (Fenster-, Dialogfunktion) mit eigenem DS }")
w("function RGB(r, g, b: Byte): Longint;")
w("function LoWord(l: Longint): Word;")
w("function HiWord(l: Longint): Word;")
w("function MakeLong(lo, hi: Word): Longint;")
w("function PasStr(p: Pointer): String;   { 0-terminiert -> Pascal-String }")
w("")
w("{ RD2-Funktionen }")
w("{$IFDEF FPC}")
for nr, name, ps, r in funk: w(kopf(name, ps, r) + "; pascal;")
w("{$ELSE}")
for nr, name, ps, r in funk:
    w(umbruch(kopf(name, ps, r) + f"; inline($C4/$1E/RD2Api/$26/$FF/$9F/>${nr * 4:04X});"))
w("{$ENDIF}")
w("")
w("implementation")
w("")
w("type")
w("  TZBuf = array[0..255] of Char;")
w("  TFertig = procedure;")
w("  TEnde = procedure(code: Word);")
w("const")
w("  NTHUNK = 16;")
w("var")
w("  zbuf: array[0..7] of TZBuf;")
w("  zi: Word;")
w("  thunk: array[0..NTHUNK * 10 - 1] of Byte;")
w("  nthunks: Word;")
w("  altexit: Pointer;")
w("  endeproc: Pointer;")
w("")
w("{$IFDEF FPC}")
for nr, name, ps, r in funk:
    w(kopf(name, ps, r) + "; pascal; assembler; nostackframe;")
    w(f"asm les bx, [RD2Api]; jmp dword ptr es:[bx+{nr * 4}] end;")
w("{$ENDIF}")
w("""
function Z(s: String): Pointer;
var n: Word;
begin
  zi := (zi + 1) and 7;
  n := Length(s);
  Move(s[1], zbuf[zi], n);
  zbuf[zi][n] := #0;
  Z := @zbuf[zi];
end;

function PasStr(p: Pointer): String;
var s: String; n: Word;
begin
  n := 0;
  if p <> nil then
    while (n < 255) and (Mem[Seg(p^):Ofs(p^) + n] <> 0) do begin
      s[n + 1] := Chr(Mem[Seg(p^):Ofs(p^) + n]); Inc(n);
    end;
  s[0] := Chr(n);
  PasStr := s;
end;

function CbProc(p: Pointer): Pointer;
var o: Word;
begin
  CbProc := nil;
  if nthunks >= NTHUNK then Exit;
  o := nthunks * 10; Inc(nthunks);
  thunk[o] := $B8;                         { mov ax, DSeg }
  thunk[o + 1] := Lo(DSeg); thunk[o + 2] := Hi(DSeg);
  thunk[o + 3] := $8E; thunk[o + 4] := $D8; { mov ds, ax }
  thunk[o + 5] := $EA;                     { jmp far p }
  thunk[o + 6] := Lo(Ofs(p^)); thunk[o + 7] := Hi(Ofs(p^));
  thunk[o + 8] := Lo(Seg(p^)); thunk[o + 9] := Hi(Seg(p^));
  CbProc := Ptr(DSeg, Ofs(thunk[o]));
end;

function RGB(r, g, b: Byte): Longint;
begin RGB := Longint(r) or (Longint(g) shl 8) or (Longint(b) shl 16); end;
function LoWord(l: Longint): Word; begin LoWord := Word(l and $FFFF); end;
function HiWord(l: Longint): Word; begin HiWord := Word(l shr 16); end;
function MakeLong(lo, hi: Word): Longint; begin MakeLong := Longint(lo) or (Longint(hi) shl 16); end;

function Hex4(w: Word): String;
const z: array[0..15] of Char = '0123456789ABCDEF';
begin Hex4 := z[w shr 12] + z[(w shr 8) and 15] + z[(w shr 4) and 15] + z[w and 15]; end;

procedure RD2Exit;
var s: String; e: TEnde;
begin
  ExitProc := altexit;
  if ErrorAddr <> nil then begin
    Str(ExitCode, s);
    s := 'Laufzeitfehler ' + s + ' bei ' + Hex4(Seg(ErrorAddr^)) + ':' + Hex4(Ofs(ErrorAddr^));
    Egal := MsgBox(0, Z(s), Z('Pascal'), MB_OK);
    ErrorAddr := nil;
  end;
  {$IFDEF FPC} e := TEnde(endeproc); {$ELSE} @e := endeproc; {$ENDIF}
  e(ExitCode);                             { kehrt nicht zurueck }
end;

var f: TFertig;
begin
  if MemW[PrefixSeg:$100 + 2] <> $4452 then Halt(1);  { 'RD': ohne Vorspann (RDRC /PAS) kein RD2-Start }
  nthunks := 0; zi := 0;
  RD2Api := Ptr(MemW[PrefixSeg:$116], MemW[PrefixSeg:$114]);
  RD2Inst := MemW[PrefixSeg:$118];
  RD2Show := MemW[PrefixSeg:$11A];
  RD2Cmd := Ptr(MemW[PrefixSeg:$11E], MemW[PrefixSeg:$11C]);
  endeproc := Ptr(PrefixSeg - 1, MemW[PrefixSeg:$122]);
  altexit := ExitProc;
  ExitProc := @RD2Exit;
  {$IFDEF FPC} f := TFertig(Ptr(PrefixSeg - 1, MemW[PrefixSeg:$120]));
  {$ELSE} @f := Ptr(PrefixSeg - 1, MemW[PrefixSeg:$120]); {$ENDIF}
  f;                                       { Start fertig: INT 21h wieder frei }
end.""")
open(sys.argv[1], "w", encoding="latin-1", newline="\r\n").write("\n".join(o) + "\n")
print(f"{sys.argv[1]}: {len(recs)} Strukturen, {len(konst)} Konstanten, {len(funk)} Funktionen; ausgelassen: {' '.join(ausgelassen)}", file=sys.stderr)
