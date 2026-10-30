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
#define VP(x, y) ((BYTE __far *)MK_FP(0xA000, (WORD)(y) * BPR + ((WORD)(x) >> 3)))
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
static BYTE vb[256];
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
/* 4 Bit gepackt <-> Ebenen ueber Tabellen (30.10.2026, statt Punkt fuer Punkt):
   pl/ph: Quellbyte (2 Punkte) -> je Ebene 2 Bits an Bit 7/6 (pl: Ebene 0 im unteren, 1 im oberen Byte; ph: 2/3);
   4 Quellbytes = 8 Punkte: pl[s0] | pl[s1] >> 2 | pl[s2] >> 4 | pl[s3] >> 6 (die Bytes laufen nicht ineinander).
   sx/sy: Ebenenbyte -> Punkte 0-3 bzw. 4-7 als Halbbytes (Bit 0 je Halbbyte), << e fuer Ebene e. */
static WORD pl[256], sx[256], sy[256];
#if NPL == 4
static WORD ph[256];
#endif
static BYTE tabok;
static void tabellen(void)
{
    WORD i, k;
    for (i = 0; i < 256; i++) {
        WORD a = i >> 4, b = i & 15, x = 0, y = 0;
        pl[i] = (WORD)((a & 1) << 7 | (b & 1) << 6 | (a & 2) << 14 | (b & 2) << 13);
#if NPL == 4
        ph[i] = (WORD)((a & 4) << 5 | (b & 4) << 4 | (a & 8) << 12 | (b & 8) << 11);
#endif
        for (k = 0; k < 4; k++) { if (i & (0x80 >> k)) x |= 1 << (12 - 4 * k); if (i & (0x08 >> k)) y |= 1 << (12 - 4 * k); }
        sx[i] = x; sy[i] = y;
    }
    tabok = 1;
}
static BYTE eb[4][W / 8 + 2], ab[4][W / 8 + 2];
static void put4(WORD x, WORD y, WORD w, WORD h, const BYTE FAR *s, WORD st, WORD rop, WORD unten)
{
    WORD r, i, e, g, ng = (w + 7) >> 3, sh = x & 7, nb = (sh + w + 7) >> 3;
    BYTE lm = (BYTE)(0xFF >> sh), rm = (BYTE)(0xFF << (7 - ((x + w - 1) & 7)));
    if (!tabok) tabellen();
    if (nb == 1) lm &= rm;
    GC(5, 0); GC(1, 0); GC(3, fn[rop < 4 ? rop : 0]);
    for (r = 0; r < h; r++, s += st) {
        BYTE __far *p = VP(x, y + r);
        for (g = 0; g < ng; g++) {                /* 8 Punkte -> ein Byte je Ebene */
            const BYTE FAR *q = s + 4 * g; BYTE c0, c1, c2, c3; WORD v;
            if (unten) { c0 = (BYTE)(q[0] << 4 | q[1] >> 4); c1 = (BYTE)(q[1] << 4 | q[2] >> 4); c2 = (BYTE)(q[2] << 4 | q[3] >> 4); c3 = (BYTE)(q[3] << 4 | q[4] >> 4); }
            else { c0 = q[0]; c1 = q[1]; c2 = q[2]; c3 = q[3]; }
            v = pl[c0] | pl[c1] >> 2 | pl[c2] >> 4 | pl[c3] >> 6; ab[0][g] = (BYTE)v; ab[1][g] = (BYTE)(v >> 8);
#if NPL == 4
            v = ph[c0] | ph[c1] >> 2 | ph[c2] >> 4 | ph[c3] >> 6; ab[2][g] = (BYTE)v; ab[3][g] = (BYTE)(v >> 8);
#endif
        }
        for (e = 0; e < NPL; e++) {               /* auf die Lage im Schirmbyte schieben */
            BYTE *a = ab[e], *z = eb[e];
            if (!sh) { for (i = 0; i < nb; i++) z[i] = a[i]; continue; }
            a[ng] = 0; z[0] = (BYTE)(a[0] >> sh);
            for (i = 1; i < nb; i++) z[i] = (BYTE)(a[i - 1] << (8 - sh) | a[i] >> sh);
        }
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
static void RDEXPORT d_put(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes,
                           WORD srcbpp, WORD rop, WORD fg, WORD bg)
{
    if (w <= 0 || h <= 0) return;
    if ((srcbpp & 0xFF) == 4) put4(x, y, w, h, src, srcbytes, rop, srcbpp >> 8 & 1);
    else if (rop == ROP_NOT) d_box(x, y, x + w - 1, y + h - 1, PLM, 0, ROP_XOR);
    else put1(x, y, w, h, src, srcbytes, rop, (BYTE)fg, (BYTE)bg);
}
/* get: Geraeteformat (4 Bit gepackt), w Punkte ab x: je Ebene die Zeile am Stueck lesen, dann Tabellen */
static void RDEXPORT d_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    WORD r, i, e, sh = x & 7, nb = (sh + w + 7) >> 3, ng = (w + 7) >> 3, n = (w + 1) >> 1;
    if (!tabok) tabellen();
    for (r = 0; r < (WORD)h; r++, dst += dstbytes) {
        BYTE __far *p = VP(x, y + r);
        for (e = 0; e < NPL; e++) {
            BYTE *z = eb[e];
            GC(4, e); _fmemcpy(z, p, nb); z[nb] = 0;
            if (sh) for (i = 0; i < ng; i++) z[i] = (BYTE)(z[i] << sh | z[i + 1] >> (8 - sh));
        }
        for (i = 0; i < ng; i++) {
            WORD a = sx[eb[0][i]] | sx[eb[1][i]] << 1, b = sy[eb[0][i]] | sy[eb[1][i]] << 1, k = 4 * i;
#if NPL == 4
            a |= sx[eb[2][i]] << 2 | sx[eb[3][i]] << 3; b |= sy[eb[2][i]] << 2 | sy[eb[3][i]] << 3;
#endif
            dst[k] = (BYTE)(a >> 8);
            if (k + 1 < n) dst[k + 1] = (BYTE)a;
            if (k + 2 < n) dst[k + 2] = (BYTE)(b >> 8);
            if (k + 3 < n) dst[k + 3] = (BYTE)b;
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
            for (r = 0; r < h; r++) { BYTE __far *q = p + r * BPR; w3(q, (BYTE)(0xFF >> sh)); if (sh) w3(q + 1, (BYTE)(0xFF << (8 - sh))); }
        }
        m3((BYTE)(rop == ROP_XOR ? PLM : fg), rop == ROP_XOR ? ROP_XOR : ROP_COPY);
        for (r = 0; r < h; r++) {
            BYTE b = g[r]; BYTE __far *q = p + r * BPR;
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
static WORD RDEXPORT d_farbe(COLOR c)
{
    BYTE r = (BYTE)c, g = (BYTE)(c >> 8), b = (BYTE)(c >> 16), m;
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

static GFXDRVF tab = { {
    { { 'R', 'D', 'R', 'V' }, DRV_GFX, 0x0100, sizeof(GFXDRVF), NAME, d_init, d_done },
    d_modes, d_setmode, d_textmode,
    d_span, d_pixel, d_getpixel, d_put, d_get,
    0, 0, d_copy, d_glyphs, d_palette, d_cursor, d_cursorhide, 0, d_box }, d_farbe, d_rgb
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
