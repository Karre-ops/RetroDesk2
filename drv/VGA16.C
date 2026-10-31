/* VGA16.C - Grafiktreiber 16 Farben, Ebenen (planar): VGA 640x480 (Modus 12h, -DVGA16) und
 * EGA 640x350 (Modus 10h, -DEGA, 128 KB Bildspeicher). Treiber kennen kein Clipping.
 * Farbe = Index 0..15 (Standardpalette wie das BIOS: 0 schwarz .. 7 hellgrau, 8 dunkelgrau .. 15 weiss).
 * Mit Muster (span/box): Bit 1 = Farbe (Bits 0-3), Bit 0 = Hintergrund (Bits 8-11).
 * Bitmaps im Geraeteformat: 4 Bit je Punkt, gepackt, linker Punkt im oberen Halbbyte (put/get mit
 * srcbpp 4; Bit 8 von srcbpp: erster Punkt im unteren Halbbyte der Quelle).
 * Fuellen und Text ueber Schreibmodus 3 (Farbe aus Set/Reset, Maske aus den Daten), die Ebenen
 * bleiben dabei unberuehrt; Verknuepfung (UND/ODER/XOR) macht die Karte (Register 3). */
#include <i86.h>
#include <conio.h>
#include <string.h>
#include "RD2DRV.H"

#if defined(VGA16)
#  define H 480
#  define MODUS 0x12
#  define NAME "VGA 640x480 16"
#  define ASPY 100
#elif defined(VGA4)                               /* 4 Farben: nur Ebenen 0 und 1 (halb so viel beim Kopieren/Retten) */
#  define H 480
#  define MODUS 0x12
#  define NAME "VGA 640x480 4"
#  define ASPY 100
#  define NPL 2
#elif defined(EGA4)
#  define H 350
#  define MODUS 0x10
#  define NAME "EGA 640x350 4"
#  define ASPY 137
#  define NPL 2
#elif defined(EGA200)                             /* 640x200 16 Farben (Modus 0Eh, geht mit 64 KB) */
#  define H 200
#  define MODUS 0x0E
#  define NAME "EGA 640x200 16"
#  define ASPY 240
#elif defined(SVGA16)                             /* VESA 800x600 16 Farben (Modus 102h, 60000 Bytes je Ebene) */
#  define H 600
#  define MODUS 0x102
#  define NAME "VESA 800x600 16"
#  define ASPY 100
#  define W 800
#  define BPR 100
#elif defined(EGA320)
#  define H 200
#  define MODUS 0x0D
#  define NAME "EGA 320x200 16"
#  define ASPY 120
#  define W 320
#  define BPR 40
#elif defined(EGA)
#  define H 350
#  define MODUS 0x10
#  define NAME "EGA 640x350 16"
#  define ASPY 137
#else
#  error VGA16, EGA oder EGA320 angeben
#endif
#ifndef W
#  define W 640
#  define BPR 80
#endif
#ifndef NPL
#  define NPL 4
#endif
#define PLM ((1 << NPL) - 1)                      /* Ebenenmaske, zugleich "weiss" fuer Umkehren */
/* Zeile * BPR mit Schieben statt MUL (8088: MUL 16 Bit ~120 Takte, so ~40) */
static WORD zbpr(WORD y)
{
#if BPR == 80
    return ((y << 2) + y) << 4;
#elif BPR == 40
    return ((y << 2) + y) << 3;
#elif BPR == 100
    WORD t = (y << 3) + (y << 4) + y; return t << 2;
#else
    return y * BPR;
#endif
}
#define VP(x, y) ((BYTE __far *)MK_FP(0xA000, zbpr((WORD)(y)) + ((WORD)(x) >> 3)))
#pragma intrinsic(inp, outp, outpw, _fmemcpy, _fmemset)

static BYTE altmodus;
WORD int10(WORD ax);
#pragma aux int10 = "push bp" "int 10h" "pop bp" parm [ax] value [ax] modify [bx cx dx si di es]
WORD int10b(WORD ax, WORD bx);
#pragma aux int10b = "push bp" "int 10h" "pop bp" parm [ax] [bx] value [ax] modify [bx cx dx si di es]
WORD egamem(void);
#pragma aux egamem = "mov ah,12h" "mov bl,10h" "push bp" "int 10h" "pop bp" value [bx] modify [ax cx dx si di es]

/* Register: Grafik-Controller 3CEh, Sequencer 3C4h */
#define GC(i, v) outpw(0x3CE, (WORD)((i) | ((WORD)(v) << 8)))
#define MAPMASK(m) outpw(0x3C4, (WORD)(2 | ((WORD)(m) << 8)))
static const BYTE fn[6] = { 0x00, 0x10, 0x08, 0x18, 0x18, 0x00 };   /* ROP_COPY, OR, AND, XOR, NOT, MASK -> Register 3 */

/* Schreibmodus 3: Latch laden (lesen), dann Maske schreiben */
void w3(BYTE __far *p, BYTE v);
#pragma aux w3 = "mov ah,es:[di]" "mov es:[di],al" parm [es di] [al] modify [ah]
void w3n(BYTE __far *p, WORD n, BYTE v);
#pragma aux w3n = "jcxz e" "l: mov ah,es:[di]" "stosb" "loop l" "e:" parm [es di] [cx] [al] modify [ah cx di]
static BYTE kopie;                                /* m3 mit ROP_COPY: volle Bytes brauchen keinen Latch */
static void m3(BYTE farbe, WORD rop)              /* Schreibmodus 3 mit Farbe und Verknuepfung */
{
    kopie = (BYTE)(rop == ROP_COPY || rop == ROP_MASK);
    if (rop == ROP_NOT) farbe = PLM;
    GC(5, 3); GC(0, farbe); GC(1, 0x0F); GC(3, fn[rop < 6 ? rop : 0]); GC(8, 0xFF); MAPMASK(PLM);
}
static void m0(void) { GC(5, 0); GC(1, 0); GC(3, 0); GC(8, 0xFF); MAPMASK(PLM); }   /* Grundzustand */

#if defined(SVGA16)
WORD vbe(WORD ax, WORD cx, void *p);              /* ES:DI = ds:p */
#pragma aux vbe = "push ds" "pop es" "push bp" "int 10h" "pop bp" parm [ax] [cx] [di] value [ax] modify [bx dx si es]
WORD vmode(WORD bx);
#pragma aux vmode = "mov ax,4F02h" "push bp" "int 10h" "pop bp" parm [bx] value [ax] modify [cx dx si di es]
#if defined(SVGA16)
static BYTE vb[256];                              /* VBE-Infoblock */
#endif
#endif
static WORD RDEXPORT d_init(WORD param)
{
    (void)param;
    altmodus = (BYTE)(int10(0x0F00) & 0x7F);
#if defined(VGA16) || defined(VGA4)
    if ((BYTE)int10(0x1A00) != 0x1A) return RD_ERR_NOTSUP;
#elif defined(SVGA16)
    if (vbe(0x4F00, 0, vb) != 0x004F || vb[0] != 'V' || vb[1] != 'E') return RD_ERR_NOTSUP;
    if (vbe(0x4F01, MODUS, vb) != 0x004F || !(vb[0] & 1)) return RD_ERR_NOTSUP;   /* Modus 102h unterstuetzt */
#else
    {   /* EGA oder VGA: INT 10h 12h/BL=10h liefert in BL die Speichergroesse (0..3 = 64..256 KB), ohne EGA bleibt BL 10h */
        WORD m = egamem() & 0xFF;
#if defined(EGA320) || defined(EGA200)
        if (m == 0x10) return RD_ERR_NOTSUP;      /* Modus 0Dh/0Eh geht schon mit 64 KB */
#else
        if (m == 0x10 || m == 0) return RD_ERR_NOTSUP;   /* keine EGA oder nur 64 KB (Modus 10h braucht 128 KB) */
#endif
    }
#endif
    if (altmodus > 3 && altmodus != 7) altmodus = 3;
    return 0;
}
static void RDEXPORT d_done(void) { }
static WORD RDEXPORT d_modes(GFXMODE FAR *m, WORD max)
{
    if (max) { m->w = W; m->h = H; m->bpp = 4; m->farben = 1 << NPL; m->aspx = 100; m->aspy = ASPY; m->zeilenbytes = W / 2; m->flags = GD_PALETTE; }
    return 1;
}
#if NPL == 2
/* 4 Farben: 0 schwarz, 1 tuerkis (Schreibtisch, Titel, Auswahl), 2 hellgrau, 3 weiss; Palettenregister im EGA-Format */
static const COLOR f4[4] = { RGB(0, 0, 0), RGB(0, 170, 170), RGB(170, 170, 170), RGB(255, 255, 255) };
#endif
static WORD RDEXPORT d_setmode(WORD nr)
{
#if defined(VGA16) || defined(SVGA16) || NPL == 2
    WORD i;
#endif
    (void)nr;
#if defined(SVGA16)
    if (vmode(MODUS) != 0x004F) return RD_ERR_NOTSUP;
#else
    int10(MODUS);
#endif
#if defined(SVGA16)
    int10b(0x1000, 0x0F12);                       /* Farbebenen 0-3 einschalten (manches VBE-BIOS laesst sie aus) */
#endif
#if defined(VGA16) || defined(SVGA16)
    for (i = 0; i < 16; i++) int10b(0x1000, (WORD)(i | (i << 8)));   /* Attribut i -> DAC i (Standardfarben in DAC 0..15 setzt palette) */
    {   static const BYTE dac[16][3] = { {0,0,0},{0,0,42},{0,42,0},{0,42,42},{42,0,0},{42,0,42},{42,21,0},{42,42,42},
                                         {21,21,21},{21,21,63},{21,63,21},{21,63,63},{63,21,21},{63,21,63},{63,63,21},{63,63,63} };
        outp(0x3C8, 0); for (i = 0; i < 16; i++) { outp(0x3C9, dac[i][0]); outp(0x3C9, dac[i][1]); outp(0x3C9, dac[i][2]); } }
#endif
#if NPL == 2
    {   static const BYTE ega[4] = { 0x00, 0x03, 0x07, 0x3F };   /* EGA-Farbwerte; VGA: Standard-DAC enthaelt dieselben 64 Farben */
        for (i = 0; i < 4; i++) int10b(0x1000, (WORD)(i | (ega[i] << 8))); }
#endif
    m0();
    return 0;
}
static void RDEXPORT d_textmode(void) { m0(); int10(altmodus); }

/* ---- Fuellen ---------------------------------------------------------------- */
static void zeile(WORD y, WORD x0, WORD x1, BYTE v)   /* Schreibmodus 3 ist gesetzt; v = Maske (Musterzeile) */
{
    BYTE __far *p = VP(x0, y); WORD b0 = x0 >> 3, b1 = x1 >> 3;
    BYTE lm = (BYTE)(0xFF >> (x0 & 7)), rm = (BYTE)(0xFF << (7 - (x1 & 7)));
    if (b0 == b1) { w3(p, (BYTE)(v & lm & rm)); return; }
    w3(p++, (BYTE)(v & lm));
    if (v == 0xFF && kopie) _fmemset(p, 0xFF, b1 - b0 - 1);   /* Maske voll: Latch egal, rep stosb */
    else w3n(p, b1 - b0 - 1, v);
    p += b1 - b0 - 1;
    w3(p, (BYTE)(v & rm));
}
static void RDEXPORT d_box(short x0, short y0, short x1, short y1, WORD farbe, const BYTE FAR *mu, WORD rop)
{
    short y;
    if (!mu) { m3((BYTE)farbe, rop); for (y = y0; y <= y1; y++) zeile(y, x0, x1, 0xFF); m0(); return; }
    if (rop == ROP_COPY || rop == ROP_MASK) {     /* erst alles in Hintergrundfarbe (volle Bytes, schnell) */
        m3((BYTE)(farbe >> 8), rop);
        for (y = y0; y <= y1; y++) zeile(y, x0, x1, 0xFF);
    }
    m3((BYTE)farbe, rop);                         /* dann die gesetzten Musterbits in Farbe */
    for (y = y0; y <= y1; y++) if (mu[y & 7]) zeile(y, x0, x1, mu[y & 7]);
    m0();
}
static void RDEXPORT d_span(short y, short x0, short x1, WORD farbe, const BYTE FAR *mu, WORD rop) { d_box(x0, y, x1, y, farbe, mu, rop); }
static void RDEXPORT d_pixel(short x, short y, WORD farbe, WORD rop)
{
    m3((BYTE)farbe, rop); w3(VP(x, y), (BYTE)(0x80 >> (x & 7))); m0();
}
static WORD RDEXPORT d_getpixel(short x, short y)
{
    BYTE __far *p = VP(x, y); BYTE m = (BYTE)(0x80 >> (x & 7)); WORD c = 0, e;
    for (e = 0; e < NPL; e++) { GC(4, e); if (*p & m) c |= 1 << e; }
    return c;
}

/* ---- Bitmaps ------------------------------------------------------------------ */
/* 1 Bit: Zeile verschoben auf Schirmbytes, je Byte Maske b; MASK: nur gesetzte in fg,
   sonst gesetzte in fg und geloeschte in bg (zwei Durchgaenge im Schreibmodus 3) */
static BYTE zb[W / 8 + 2];
static WORD zeile1(const BYTE FAR *s, WORD sh, WORD w)   /* Quelle (w Punkte) auf x & 7 = sh schieben -> zb, liefert Bytes */
{
    WORD nb = (sh + w + 7) >> 3, sb = (w + 7) >> 3, i, acc = 0;
    BYTE rm = (BYTE)(0xFF << ((8 - ((sh + w) & 7)) & 7));
    for (i = 0; i < nb; i++) { acc = (WORD)((acc << 8) | (i < sb ? s[i] : 0)); zb[i] = (BYTE)(acc >> sh); }
    zb[0] &= (BYTE)(0xFF >> sh); zb[nb - 1] &= rm;
    return nb;
}
static void put1(WORD x, WORD y, WORD w, WORD h, const BYTE FAR *s, WORD st, WORD rop, BYTE fg, BYTE bg)
{
    WORD r, i, nb; BYTE lm = (BYTE)(0xFF >> (x & 7)), rm = (BYTE)(0xFF << (7 - ((x + w - 1) & 7)));
    for (r = 0; r < h; r++, s += st) {
        BYTE __far *p = VP(x, y + r);
        nb = zeile1(s, x & 7, w);
        m3(fg, rop == ROP_MASK ? ROP_COPY : rop);
        for (i = 0; i < nb; i++) if (zb[i]) w3(p + i, zb[i]);
        if (rop != ROP_MASK) {
            m3(bg, rop);
            for (i = 0; i < nb; i++) { BYTE m = (BYTE)~zb[i]; if (!i) m &= lm; if (i == nb - 1) m &= rm; if (m) w3(p + i, m); }
        }
    }
    m0();
}
/* 4 Bit gepackt <-> Ebenen ohne Tabellen (30.10.2026): Assembler schiebt Bit fuer Bit durch (8 Befehle je Punkt).
   pk8: 4 Quellbytes (8 Punkte, linker im oberen Halbbyte) -> 4 Ebenenbytes nach o[0..3] (Ebene 0..3).
   up8: 4 Ebenenbytes i[0..3] -> 4 Bytes mit 8 Punkten (Halbbytes) nach o[0..3]. */
void pk8(const BYTE FAR *q, BYTE *o);
#pragma aux pk8 = "mov cx,4" "l: mov al,es:[si]" "inc si" \
    "shl al,1" "rcl dh,1" "shl al,1" "rcl dl,1" "shl al,1" "rcl bh,1" "shl al,1" "rcl bl,1" \
    "shl al,1" "rcl dh,1" "shl al,1" "rcl dl,1" "shl al,1" "rcl bh,1" "shl al,1" "rcl bl,1" "loop l" \
    "mov [di],bl" "mov [di+1],bh" "mov [di+2],dl" "mov [di+3],dh" parm [es si] [di] modify [ax bx cx dx si]
void up8(const BYTE *i, BYTE *o);
#pragma aux up8 = "mov bl,[si]" "mov bh,[si+1]" "mov dl,[si+2]" "mov dh,[si+3]" "mov cx,4" \
    "l: shl dh,1" "rcl al,1" "shl dl,1" "rcl al,1" "shl bh,1" "rcl al,1" "shl bl,1" "rcl al,1" \
    "shl dh,1" "rcl al,1" "shl dl,1" "rcl al,1" "shl bh,1" "rcl al,1" "shl bl,1" "rcl al,1" "mov [di],al" "inc di" "loop l" \
    parm [si] [di] modify [ax bx cx dx di]
static BYTE eb[4][W / 8 + 2], mk[W / 8 + 2];
static BYTE q4[5], o4[4];
/* Zeile s (w Punkte, unten = ab dem unteren Halbbyte) in Ebenenbytes eb, auf Bitlage sh geschoben; liefert Bytezahl */
static WORD zeile4(const BYTE FAR *s, WORD w, WORD unten, WORD sh)
{
    WORD i, e, g, ng = (w + 7) >> 3, nb = (sh + w + 7) >> 3;
    for (g = 0; g < ng; g++, s += 4) {
        if (unten) { for (i = 0; i < 4; i++) q4[i] = (BYTE)(s[i] << 4 | s[i + 1] >> 4); pk8((const BYTE FAR *)q4, o4); }
        else pk8(s, o4);
        for (e = 0; e < NPL; e++) eb[e][g] = o4[e];
    }
    if (sh) for (e = 0; e < NPL; e++) {           /* an Ort und Stelle von hinten schieben */
        BYTE *z = eb[e];
        z[ng] = 0;
        for (i = nb - 1; i > 0; i--) z[i] = (BYTE)(z[i - 1] << (8 - sh) | z[i] >> sh);
        z[0] >>= sh;
    }
    return nb;
}
static void put4(WORD x, WORD y, WORD w, WORD h, const BYTE FAR *s, WORD st, WORD rop, WORD unten)
{
    WORD r, i, e, sh = x & 7, nb = (sh + w + 7) >> 3;
    BYTE lm = (BYTE)(0xFF >> sh), rm = (BYTE)(0xFF << (7 - ((x + w - 1) & 7)));
    if (nb == 1) lm &= rm;
    GC(5, 0); GC(1, 0); GC(3, fn[rop < 4 ? rop : 0]);
    for (r = 0; r < h; r++, s += st) {
        BYTE __far *p = VP(x, y + r);
        zeile4(s, w, unten, sh);
        if (rop == ROP_COPY || rop >= 4) {        /* Raender mit Bitmaske (Latch), Mitte je Ebene am Stueck */
            volatile BYTE l;
            GC(8, lm); l = p[0];
            for (e = 0; e < NPL; e++) { MAPMASK(1 << e); p[0] = eb[e][0]; }
            if (nb > 1) {
                GC(8, rm); l = p[nb - 1];
                for (e = 0; e < NPL; e++) { MAPMASK(1 << e); p[nb - 1] = eb[e][nb - 1]; }
                if (nb > 2) { GC(8, 0xFF); for (e = 0; e < NPL; e++) { MAPMASK(1 << e); _fmemcpy(p + 1, eb[e] + 1, nb - 2); } }
            }
            (void)l;
        } else for (i = 0; i < nb; i++) {         /* Verknuepfung: jedes Byte ueber den Latch */
            BYTE m = 0xFF; volatile BYTE l;
            if (!i) m &= lm;
            if (i == nb - 1) m &= rm;
            GC(8, m); l = p[i];
            for (e = 0; e < NPL; e++) { MAPMASK(1 << e); p[i] = eb[e][i]; }
            (void)l;
        }
    }
    m0();
}
/* Farbiges Symbol: Maske als Bitmaske der Karte, nur dort die Ebenen schreiben (kein Hintergrund holen) */
static void RDEXPORT d_symput(short x, short y, short w, short h, const BYTE FAR *s, WORD sbpr, WORD unten, const BYTE FAR *m, WORD mbpr, WORD mbit)
{
    WORD r, i, e, sh = x & 7, nb, nm = ((WORD)w + 7) >> 3;
    if (w <= 0 || h <= 0) return;
    GC(5, 0); GC(1, 0); GC(3, 0);
    for (r = 0; r < (WORD)h; r++, s += sbpr, m += mbpr) {
        BYTE __far *p = VP(x, y + r);
        nb = zeile4(s, w, unten, sh);
        for (i = 0; i < nm; i++) mk[i] = (BYTE)(mbit ? m[i] << mbit | m[i + 1] >> (8 - mbit) : m[i]);   /* Maske ab Bit 0 */
        if (w & 7) mk[nm - 1] &= (BYTE)(0xFF << (8 - (w & 7)));
        mk[nm] = 0;
        if (sh) { for (i = nb - 1; i > 0; i--) mk[i] = (BYTE)(mk[i - 1] << (8 - sh) | mk[i] >> sh); mk[0] >>= sh; }
        for (i = 0; i < nb; i++) {
            volatile BYTE l;
            if (!mk[i]) continue;
            GC(8, mk[i]); l = p[i];
            for (e = 0; e < NPL; e++) { MAPMASK(1 << e); p[i] = eb[e][i]; }
            (void)l;
        }
    }
    m0();
}
static void RDEXPORT d_put(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes,
                           WORD srcbpp, WORD rop, WORD fg, WORD bg)
{
    if (w <= 0 || h <= 0) return;
    if ((srcbpp & 0xFF) == 4) put4(x, y, w, h, src, srcbytes, rop, srcbpp >> 8 & 1);
    else if (rop == ROP_NOT) d_box(x, y, x + w - 1, y + h - 1, PLM, 0, ROP_XOR);
    else put1(x, y, w, h, src, srcbytes, rop, (BYTE)fg, (BYTE)bg);
}
/* get: Geraeteformat (4 Bit gepackt), w Punkte ab x: je Ebene die Zeile am Stueck lesen, dann up8 */
static void RDEXPORT d_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    WORD r, i, e, k, sh = x & 7, nb = (sh + w + 7) >> 3, ng = (w + 7) >> 3, n = (w + 1) >> 1;
    for (r = 0; r < (WORD)h; r++, dst += dstbytes) {
        BYTE __far *p = VP(x, y + r);
        for (e = 0; e < 4; e++) {
            BYTE *z = eb[e];
            if (e >= NPL) { for (i = 0; i <= ng; i++) z[i] = 0; continue; }
            GC(4, e); _fmemcpy(z, p, nb); z[nb] = 0;
            if (sh) for (i = 0; i < ng; i++) z[i] = (BYTE)(z[i] << sh | z[i + 1] >> (8 - sh));
        }
        for (i = 0; i < ng; i++) {
            q4[0] = eb[0][i]; q4[1] = eb[1][i]; q4[2] = eb[2][i]; q4[3] = eb[3][i];
            up8(q4, o4);
            for (k = 0; k < 4 && 4 * i + k < n; k++) dst[4 * i + k] = o4[k];
        }
        if (w & 1) dst[n - 1] &= 0xF0;
    }
    GC(4, 0);
}
/* Schirm -> Schirm (Rollen): gleiche Bitlage ganze Bytes ueber die Latches (Schreibmodus 1),
   Raender und sonst zeilenweise ueber get/put */
static BYTE kz[W / 2];
static void RDEXPORT d_copy(short sx, short sy, short w, short h, short dx, short dy)
{
    short r, st = 1, y0 = 0;
    if (dy > sy) { st = -1; y0 = h - 1; }
    for (r = y0; r >= 0 && r < h; r += st) {
        if (!((sx ^ dx) & 7) && w >= 16) {
            WORD a = (WORD)sx, b = (WORD)(sx + w - 1), ma = (a + 7) & ~7, mbb = (b + 1) & ~7, n = (mbb - ma) >> 3;
            BYTE __far *s = VP(ma, sy + r); BYTE __far *d = VP(ma + (dx - sx), dy + r);
            if (ma > a) { d_get(a, sy + r, ma - a, 1, kz, sizeof kz); d_put(dx, dy + r, ma - a, 1, kz, sizeof kz, 4, ROP_COPY, 0, 0); }
            GC(5, 1);
            if (dx < sx || dy != sy) { WORD i; for (i = 0; i < n; i++) d[i] = s[i]; }
            else { WORD i; for (i = n; i--; ) d[i] = s[i]; }
            GC(5, 0);
            if (b >= mbb) { d_get(mbb, sy + r, b - mbb + 1, 1, kz, sizeof kz); d_put(mbb + (dx - sx), dy + r, b - mbb + 1, 1, kz, sizeof kz, 4, ROP_COPY, 0, 0); }
        } else { d_get(sx, sy + r, w, 1, kz, sizeof kz); d_put(dx, dy + r, w, 1, kz, sizeof kz, 4, ROP_COPY, 0, 0); }
    }
}

/* ---- Text: Zeichen 8 Punkte breit, Zeile i = Byte seg:offs[i]+r ------------------------ */
static void RDEXPORT d_glyphs(short x, short y, const WORD FAR *offs, WORD seg, WORD h, WORD n, WORD fg, WORD bg, WORD rop)
{
    WORD i, r, sh = x & 7;
    for (i = 0; i < n; i++) {
        const BYTE FAR *g = (const BYTE FAR *)MK_FP(seg, offs[i]);
        WORD gx = (WORD)x + 8 * i;
        BYTE __far *p = VP(gx, y);
        if (rop == ROP_COPY) {                    /* deckend: Hintergrund der Zelle, dann Zeichen */
            m3((BYTE)bg, ROP_COPY);
            { BYTE __far *q = p; for (r = 0; r < h; r++, q += BPR) { w3(q, (BYTE)(0xFF >> sh)); if (sh) w3(q + 1, (BYTE)(0xFF << (8 - sh))); } }
        }
        m3((BYTE)(rop == ROP_XOR ? PLM : fg), rop == ROP_XOR ? ROP_XOR : ROP_COPY);
        for (r = 0; r < h; r++, p += BPR) {
            BYTE b = g[r]; BYTE __far *q = p;
            if (!b) continue;
            w3(q, (BYTE)(b >> sh));
            if (sh) { BYTE c = (BYTE)(b << (8 - sh)); if (c) w3(q + 1, c); }
        }
    }
    m0();
}

/* ---- Farbtabelle (DAC; EGA: nur die 16 Standardfarben) ------------------------------ */
static void RDEXPORT d_palette(WORD first, WORD n, const COLOR FAR *c)
{
#if defined(VGA16)
    WORD i;
    outp(0x3C8, (BYTE)first);
    for (i = 0; i < n && first + i < 16; i++) { COLOR v = c[i]; outp(0x3C9, (BYTE)v >> 2); outp(0x3C9, (BYTE)(v >> 8) >> 2); outp(0x3C9, (BYTE)(v >> 16) >> 2); }
#else
    (void)first; (void)n; (void)c;
#endif
}

/* ---- Mauszeiger 16x16 (UND/XOR wie GFX1); Hintergrund 3 Bytes je Zeile und Ebene ---------- */
static BYTE csv[4][16][3], c_on, c_h, c_nb;
static WORD c_x, c_y;
static void crest(void)
{
    WORD r, k, e;
    if (!c_on) return;
    GC(5, 0); GC(1, 0); GC(3, 0); GC(8, 0xFF);
    for (e = 0; e < NPL; e++) {
        MAPMASK(1 << e);
        for (r = 0; r < c_h; r++) { BYTE __far *p = VP(c_x, c_y + r); for (k = 0; k < c_nb; k++) p[k] = csv[e][r][k]; }
    }
    m0(); c_on = 0;
}
static void RDEXPORT d_cursorhide(BOOL aus) { if (aus) crest(); }
static void RDEXPORT d_cursor(short x, short y, const BYTE FAR *mb)
{
    WORD r, k, e, sh = x & 7;
    crest();
    c_x = (WORD)x; c_y = (WORD)y;
    c_h = (BYTE)(H - y < 16 ? H - y : 16);
    c_nb = (BYTE)(BPR - ((WORD)x >> 3) < 3 ? BPR - ((WORD)x >> 3) : 3);
    for (e = 0; e < NPL; e++) { GC(4, e); for (r = 0; r < c_h; r++) { BYTE __far *p = VP(x, y + r); for (k = 0; k < c_nb; k++) csv[e][r][k] = p[k]; } }
    GC(4, 0);
    for (r = 0; r < c_h; r++) {
        BYTE __far *p = VP(x, y + r);
        DWORD a = ((DWORD)(BYTE)~mb[2 * r] << 16 | (DWORD)(BYTE)~mb[2 * r + 1] << 8) >> sh;   /* UND 0 -> schwarz */
        DWORD o = ((DWORD)mb[32 + 2 * r] << 16 | (DWORD)mb[33 + 2 * r] << 8) >> sh;          /* XOR 1 -> umkehren */
        m3(0, ROP_COPY);
        for (k = 0; k < c_nb; k++) { BYTE m = (BYTE)(a >> (16 - 8 * k)); if (m) w3(p + k, m); }
        m3(PLM, ROP_XOR);
        for (k = 0; k < c_nb; k++) { BYTE m = (BYTE)(o >> (16 - 8 * k)); if (m) w3(p + k, m); }
    }
    m0(); c_on = 1;
}

/* ---- Farben: RGB <-> Index der Standardpalette (CGA/EGA/VGA: 6 = braun) -------------------- */
#if NPL == 2
static WORD RDEXPORT d_farbe(COLOR c)              /* 4 Farben: Grau nach Helligkeit, Buntes -> tuerkis (dunkel -> schwarz) */
{
    BYTE r = (BYTE)c, g = (BYTE)(c >> 8), b = (BYTE)(c >> 16), mx = r > g ? r : g, mn = r < g ? r : g; WORD l = (r * 3 + g * 6 + b) / 10;
    if (b > mx) mx = b;
    if (b < mn) mn = b;
    if (mx - mn > 64 && mx > 96) return 1;
    return l < 64 ? 0 : l < 200 ? 2 : 3;
}
static COLOR RDEXPORT d_rgb(WORD i) { return f4[i & 3]; }
#else
static COLOR RDEXPORT d_rgb(WORD i);
static WORD RDEXPORT d_farbe(COLOR c)
{
    BYTE r = (BYTE)c, g = (BYTE)(c >> 8), b = (BYTE)(c >> 16), m;
    WORD i;
    for (i = 0; i < 16; i++) if (d_rgb(i) == (c & 0xFFFFFFUL)) return i;   /* eigene Farben (ColorNearest) wieder genau treffen */
    if (r == g && g == b) return r < 43 ? 0 : r < 128 ? 8 : r < 213 ? 7 : 15;   /* Grau */
    m = r > g ? r : g; if (b > m) m = b;
    if (r > 128 && g > 32 && g < 128 && b < 64) return 6;                        /* braun/orange */
    return (WORD)((b > 64) | ((g > 64) << 1) | ((r > 64) << 2) | (m > 191 ? 8 : 0));
}
static COLOR RDEXPORT d_rgb(WORD i)
{
    BYTE v = (BYTE)((i & 8) ? 85 : 0), r = (BYTE)((i & 4) ? 170 + v : v), g = (BYTE)((i & 2) ? 170 + v : v), b = (BYTE)((i & 1) ? 170 + v : v);
    if ((i & 15) == 6) g = 85;
    return RGB(r, g, b);
}

#endif

/* ---- Hintergrund roh: je Zeile die Ebenen hintereinander (je ps Bytes), Raender mit Bitmaske ---- */
static WORD RDEXPORT d_rohgr(short x, short w) { return (WORD)((((x & 7) + w + 7) >> 3) * NPL); }
static void RDEXPORT d_rohget(short x, short y, short w, short h, BYTE FAR *z, WORD bpr)
{
    WORD r, e, ps = ((x & 7) + w + 7) >> 3;
    for (r = 0; r < (WORD)h; r++, z += bpr) { BYTE __far *p = VP(x, y + r); for (e = 0; e < NPL; e++) { GC(4, e); _fmemcpy(z + e * ps, p, ps); } }
    GC(4, 0);
}
static void RDEXPORT d_rohput(short qx, short qy, short qw, short qh, const BYTE FAR *z, WORD bpr, short x0, short w0)
{
    WORD r, e, ps = ((x0 & 7) + w0 + 7) >> 3, c = ((WORD)qx >> 3) - ((WORD)x0 >> 3), nb = ((qx & 7) + qw + 7) >> 3;
    BYTE lm = (BYTE)(0xFF >> (qx & 7)), rm = (BYTE)(0xFF << (7 - ((qx + qw - 1) & 7)));
    if (nb == 1) lm &= rm;
    GC(5, 0); GC(1, 0); GC(3, 0);
    for (r = 0; r < (WORD)qh; r++, z += bpr) {
        BYTE __far *p = VP(qx, qy + r); const BYTE FAR *q = z + c; volatile BYTE l;
        GC(8, lm); l = p[0]; for (e = 0; e < NPL; e++) { MAPMASK(1 << e); p[0] = q[e * ps]; }
        if (nb > 1) {
            GC(8, rm); l = p[nb - 1]; for (e = 0; e < NPL; e++) { MAPMASK(1 << e); p[nb - 1] = q[e * ps + nb - 1]; }
            if (nb > 2) { GC(8, 0xFF); for (e = 0; e < NPL; e++) { MAPMASK(1 << e); _fmemcpy(p + 1, q + e * ps + 1, nb - 2); } }
        }
        (void)l;
    }
    m0();
}

static GFXDRVR tab = { { {
    { { 'R', 'D', 'R', 'V' }, DRV_GFX, 0x0100, sizeof(GFXDRVR), NAME, d_init, d_done },
    d_modes, d_setmode, d_textmode,
    d_span, d_pixel, d_getpixel, d_put, d_get,
    0, 0, d_copy, d_glyphs, d_palette, d_cursor, d_cursorhide, 0, d_box }, d_farbe, d_rgb },
    d_rohgr, d_rohget, d_rohput, d_symput
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
