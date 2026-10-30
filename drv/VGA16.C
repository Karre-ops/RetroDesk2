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
#pragma intrinsic(inp, outp, outpw)

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
static void m3(BYTE farbe, WORD rop)              /* Schreibmodus 3 mit Farbe und Verknuepfung */
{
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
    w3n(p, b1 - b0 - 1, v); p += b1 - b0 - 1;
    w3(p, (BYTE)(v & rm));
}
static void RDEXPORT d_box(short x0, short y0, short x1, short y1, WORD farbe, const BYTE FAR *mu, WORD rop)
{
    short y;
    if (!mu) { m3((BYTE)farbe, rop); for (y = y0; y <= y1; y++) zeile(y, x0, x1, 0xFF); m0(); return; }
    m3((BYTE)farbe, rop);                         /* gesetzte Musterbits in Farbe */
    for (y = y0; y <= y1; y++) zeile(y, x0, x1, mu[y & 7]);
    if (rop == ROP_COPY || rop == ROP_MASK) {     /* die anderen in Hintergrundfarbe */
        m3((BYTE)(farbe >> 8), rop);
        for (y = y0; y <= y1; y++) zeile(y, x0, x1, (BYTE)~mu[y & 7]);
    }
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
/* 4 Bit gepackt: je Zeile 8 Punkte -> 4 Ebenenbytes, Schreibmodus 0 mit Bitmaske und Ebenenmaske */
static BYTE eb[4][W / 8 + 2];
static void put4(WORD x, WORD y, WORD w, WORD h, const BYTE FAR *s, WORD st, WORD rop, WORD unten)
{
    WORD r, i, e, nb = ((x & 7) + w + 7) >> 3;
    BYTE lm = (BYTE)(0xFF >> (x & 7)), rm = (BYTE)(0xFF << (7 - ((x + w - 1) & 7)));
    GC(5, 0); GC(1, 0); GC(3, fn[rop < 4 ? rop : 0]);
    for (r = 0; r < h; r++, s += st) {
        BYTE __far *p = VP(x, y + r);
        for (i = 0; i < nb; i++) eb[0][i] = eb[1][i] = eb[2][i] = eb[3][i] = 0;
        for (i = 0; i < w; i++) {                 /* Punkt i -> Bit an Schirmspalte x + i */
            WORD q = i + unten; BYTE c = (BYTE)((q & 1) ? s[q >> 1] & 0x0F : s[q >> 1] >> 4);
            WORD k = ((x & 7) + i); BYTE m = (BYTE)(0x80 >> (k & 7));
            if (c & 1) eb[0][k >> 3] |= m;
            if (c & 2) eb[1][k >> 3] |= m;
            if (c & 4) eb[2][k >> 3] |= m;
            if (c & 8) eb[3][k >> 3] |= m;
        }
        for (i = 0; i < nb; i++) {
            BYTE m = 0xFF; volatile BYTE l;
            if (!i) m &= lm;
            if (i == nb - 1) m &= rm;
            GC(8, m); l = p[i];                   /* Latch fuer die Bits ausserhalb der Maske */
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
/* get: Geraeteformat (4 Bit gepackt), w Punkte ab x */
static void RDEXPORT d_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    WORD r, i, e, nb = ((x & 7) + w + 7) >> 3;
    for (r = 0; r < (WORD)h; r++, dst += dstbytes) {
        BYTE __far *p = VP(x, y + r);
        for (e = 0; e < NPL; e++) { GC(4, e); for (i = 0; i < nb; i++) eb[e][i] = p[i]; }
        for (i = 0; i < (WORD)w; i++) {
            WORD k = (x & 7) + i; BYTE m = (BYTE)(0x80 >> (k & 7)), c = 0; k >>= 3;
            if (eb[0][k] & m) c |= 1;
            if (eb[1][k] & m) c |= 2;
            if (eb[2][k] & m) c |= 4;
            if (eb[3][k] & m) c |= 8;
            if (i & 1) dst[i >> 1] = (BYTE)((dst[i >> 1] & 0xF0) | c); else dst[i >> 1] = (BYTE)(c << 4);
        }
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
