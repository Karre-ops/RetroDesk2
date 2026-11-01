#!/usr/bin/env python3
"""Erzeugt src/drv/GLYPHS.H: d_glyphs fuer GFX1.C (CGA/HGC/VGAM), 25.10.2026.
Text spaltenweise direkt aus der Schrift. Durchsichtig verschoben: je Glyphe ein Wort (ODER/UND/XOR).
Deckend verschoben (x & 7): jedes Bildschirmbyte genau einmal,
Inhalt = (vorige Glyphe:diese Glyphe) >> sh. Erste und letzte Spalte mit Maske (deckend).
Arten: 1 ODER, 2 UND-NICHT, 3 XOR, 4 deckend (Bits = Schrift), 5 deckend invertiert."""
import sys
L = []
def e(s): L.append(s)
n = [0]
def lab(p):
    n[0] += 1; return f"{p}{n[0]}"

def adv(vga):
    if vga:
        e("        add  di, ZADD")
    else:
        w = lab("w")
        e("        add  di, ZADD"); e("        cmp  di, ZLIM"); e(f"        jb   {w}"); e("        sub  di, ZSUB"); e(f"    {w}:")

def op_mid(k):
    s = lab("s")
    if k == 1: e("        or   al, al"); e(f"        jz   {s}"); e("        or   es:[di], al")
    if k == 2: e("        or   al, al"); e(f"        jz   {s}"); e("        not  al"); e("        and  es:[di], al")
    if k == 3: e("        or   al, al"); e(f"        jz   {s}"); e("        xor  es:[di], al")
    if k == 4: e("        mov  es:[di], al")
    if k == 5: e("        not  al"); e("        mov  es:[di], al")
    e(f"    {s}:")

def op_edge(k):                     # ah = Maske: Art 4 ~m, Art 5 m
    if k <= 3: op_mid(k); return
    if k == 4: e("        and  es:[di], ah"); e("        or   es:[di], al")
    if k == 5: e("        or   es:[di], ah"); e("        xor  es:[di], al")

def holen():                         # si = Glyphe der naechsten Spalte, di = Spaltenanfang
    e("        mov  ds, [bp+8]"); e("        mov  si, [bp+10]"); e("        mov  si, [si]")
    e("        add  word ptr [bp+10], 2"); e("        mov  ds, [bp+2]"); e("        mov  di, dx"); e("        mov  ch, [bp+4]")

# Rahmen: [bp+0] vorige Glyphe, [bp+2] fseg, [bp+4] h, [bp+6] n, [bp+8] ts, [bp+10] tof, [bp+12] op
def block(vga):
    n[0] = 0
    e("    _asm {")
    for r in ("push si", "push di", "push es", "push ds", "mov  dx, row", "mov  cl, sh", "mov  ax, VSEG", "mov  es, ax", "cld",
              "push bp", "mov  al, op", "xor  ah, ah", "push ax", "push tof", "push ts", "push n", "push h", "push fseg", "push ax", "mov  bp, sp"):
        e("        " + r)
    for k in range(1, 6):          # Verteiler
        nx = f"v{k}"
        e(f"        cmp  byte ptr [bp+12], {k}"); e(f"        jne  {nx}")
        e("        or   cl, cl"); e(f"        jnz  u{k}x"); e(f"        jmp  a{k}"); e(f"    u{k}x: jmp  u{k}"); e(f"    {nx}:")
    e("        jmp  ende")
    for k in range(1, 6):
        # ausgerichtet: ein Byte je Zeile
        e(f"    a{k}:"); holen()
        e(f"    a{k}r: lodsb"); op_mid(k); adv(vga); e("        dec  ch"); e(f"        jnz  a{k}r")
        e("        inc  dx"); e("        dec  word ptr [bp+6]"); e(f"        jnz  a{k}"); e("        jmp  ende")
        if k <= 3:                 # durchsichtig verschoben: je Glyphe ein Wort (leere Zeilen schnell uebersprungen)
            e(f"    u{k}:"); holen()
            z = lab("z")
            e(f"    u{k}r: lodsb"); e("        or   al, al"); e(f"        jz   {z}"); e("        xor  ah, ah"); e("        ror  ax, cl")
            if k == 1: e("        or   es:[di], ax")
            if k == 2: e("        not  ax"); e("        and  es:[di], ax")
            if k == 3: e("        xor  es:[di], ax")
            e(f"    {z}:"); adv(vga); e("        dec  ch"); e(f"        jnz  u{k}r")
            e("        inc  dx"); e("        dec  word ptr [bp+6]"); e(f"        jnz  u{k}"); e("        jmp  ende")
            continue
        # deckend verschoben: erste Spalte
        e(f"    u{k}:"); holen(); e("        mov  [bp+0], si")
        if k >= 4:
            e("        mov  ah, 0FFh"); e("        shr  ah, cl")
            if k == 4: e("        not  ah")
        e(f"    f{k}r: lodsb"); e("        shr  al, cl"); op_edge(k); adv(vga); e("        dec  ch"); e(f"        jnz  f{k}r")
        e("        inc  dx"); e("        dec  word ptr [bp+6]"); e(f"        jz   l{k}")
        # mittlere Spalten: (vorige:diese) >> sh, ganzes Byte
        e(f"    m{k}:"); holen(); e("        mov  bx, [bp+0]"); e("        mov  [bp+0], si")
        e(f"    m{k}r: mov  ah, [bx]"); e("        inc  bx"); e("        lodsb"); e("        shr  ax, cl"); op_mid(k); adv(vga)
        e("        dec  ch"); e(f"        jnz  m{k}r")
        e("        inc  dx"); e("        dec  word ptr [bp+6]"); e(f"        jnz  m{k}")
        # letzte Spalte: Rest der letzten Glyphe
        e(f"    l{k}: mov  bx, [bp+0]"); e("        mov  di, dx"); e("        mov  ch, [bp+4]"); e("        neg  cl"); e("        add  cl, 8")
        if k >= 4:
            e("        mov  ah, 0FFh"); e("        shl  ah, cl")
            if k == 4: e("        not  ah")
        e(f"    l{k}r: mov  al, [bx]"); e("        inc  bx"); e("        shl  al, cl"); op_edge(k); adv(vga)
        e("        dec  ch"); e(f"        jnz  l{k}r"); e("        jmp  ende")
    e("    ende: add  sp, 14"); e("        pop  bp"); e("        pop  ds"); e("        pop  es"); e("        pop  di"); e("        pop  si")
    e("    }")

e("/* ERZEUGT von tools/mkglyphs.py - nicht von Hand aendern */")
e("static void RDEXPORT d_glyphs(short x, short y, const WORD FAR *offs, WORD fseg, WORD h, WORD n, WORD fg, WORD bg, WORD rop)")
e("{")
e("    WORD row = ROW(y) + ((WORD)x >> 3), tof = FP_OFF(offs), ts = FP_SEG(offs); BYTE sh = (BYTE)(x & 7), op;")
e("    (void)bg;")
e("    if (!n || !h) return;")
e("    op = (BYTE)(rop == ROP_COPY ? (fg ? 4 : 5) : rop == ROP_XOR ? 3 : fg ? 1 : 2);")
e("#if defined(VGAM)")
block(True)
e("#else")
block(False)
e("#endif")
e("}")
open(sys.argv[1], "w").write("\n".join(L) + "\n")
