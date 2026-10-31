/* CGA4.C - Grafiktreiber CGA 320x200 mit 4 Farben (Modus 4, 2 Bit je Punkt, Baenke zu 2000h).
 * Palette 1 hell: 0 schwarz, 1 helltuerkis, 2 hellrosa, 3 weiss. Laeuft auf CGA, EGA und VGA.
 * Treiber kennen kein Clipping. Farbe = Index 0..3. Mit Muster (span/box): Bit 1 = Farbe (Bits 0-1),
 * Bit 0 = Hintergrund (Bits 8-9). Bitmaps im Geraeteformat wie VGA16: 4 Bit je Punkt gepackt,
 * linker Punkt im oberen Halbbyte (Bit 8 von srcbpp: erster Punkt im unteren Halbbyte der Quelle).
 *
 * Wie die anderen Treiber byteweise (Karsten, 30.10.2026: keine Kompromisse): Zeilenanfaenge aus
 * einer Tabelle, 1 Bit -> 2 Bit ueber eine Tabelle (ein Quellbyte = zwei Schirmbytes), Zeilen
 * zuerst ausgerichtet in einen Puffer und dann mit Rand- und Bitmaske in Assembler geschrieben,
 * Fuellen der Mitte mit rep stosb. Text (schmale Schrift) laeuft ueber put mit 1 Bit. */
#include <i86.h>
#include <conio.h>
#include <string.h>
#include "RD2DRV.H"

#define W 320
#define H 200
#define BPR 80
#define NB (BPR + 2)                              /* Schirmbytes je Zeile plus Rand */
#pragma intrinsic(inp, outp, _fmemset, _fmemcpy, memset)

static BYTE altmodus;
/* 30.10.2026: ohne grosse Tabellen (1,9 KB weniger), nur e4 mit 16 Bytes; der Rest wird gerechnet */
static BYTE e4[16];                               /* 4 Punkte 1 Bit -> 8 Bit (je Punkt 11) */
#define E1(b)  ((WORD)e4[(BYTE)(b) >> 4] << 8 | e4[(b) & 15])        /* 8 Punkte 1 Bit -> 16 Bit, links oben */
#define N2(i)  ((BYTE)((((i) >> 2) & 0x0C) | ((i) & 3)))           /* 2 Punkte 4 Bit (Halbbytes) -> 4 Bit (2 x 2 Bit) */
#define N2H(i) ((BYTE)(N2(i) << 4))
#define G1(n)  ((BYTE)((((n) & 0x0C) << 2) | ((n) & 3)))           /* 2 Punkte 2 Bit -> 2 Halbbytes */
#define G2(a)  ((WORD)G1((BYTE)(a) >> 4) << 8 | G1((a) & 15))       /* 4 Punkte 2 Bit -> 4 Halbbytes (2 Bytes, links zuerst) */
static WORD zo(WORD y) { WORD h = y >> 1; h = ((h << 2) + h) << 4; return h + ((y & 1) << 13); }   /* *80 ohne MUL */
#define ZO(y)  zo((WORD)(y))
#define VP(x, y) ((BYTE __far *)MK_FP(0xB800, ZO(y) + ((WORD)(x) >> 2)))
#define FP4(c) ((BYTE)(((c) & 3) * 0x55))         /* Farbe in alle 4 Punkte eines Bytes */

WORD int10(WORD ax);
#pragma aux int10 = "push bp" "int 10h" "pop bp" parm [ax] value [ax] modify [bx cx dx si di es]
WORD int10b(WORD ax, WORD bx);
#pragma aux int10b = "push bp" "int 10h" "pop bp" parm [ax] [bx] value [ax] modify [bx cx dx si di es]
WORD equip(void);
#pragma aux equip = "int 11h" value [ax]

/* Schreiben mit Maske: d = (d & ~m) | (v & m) fuer n Bytes; v, m aus den Puffern (DS:SI, DS:BX) */
void mschr(BYTE __far *d, const BYTE *v, const BYTE *m, WORD n);
#pragma aux mschr = "jcxz e" "l: mov al,[si]" "mov ah,[bx]" "and al,ah" "not ah" "and ah,es:[di]" "or al,ah" "stosb" "inc si" "inc bx" "loop l" "e:" \
    parm [es di] [si] [bx] [cx] modify [ax cx si di bx]
/* Verknuepfen: OR/AND/XOR von v (schon mit der Maske verrechnet) */
void mor(BYTE __far *d, const BYTE *v, WORD n);
#pragma aux mor = "jcxz e" "l: lodsb" "or es:[di],al" "inc di" "loop l" "e:" parm [es di] [si] [cx] modify [ax cx si di]
void mand(BYTE __far *d, const BYTE *v, WORD n);
#pragma aux mand = "jcxz e" "l: lodsb" "and es:[di],al" "inc di" "loop l" "e:" parm [es di] [si] [cx] modify [ax cx si di]
void mxor(BYTE __far *d, const BYTE *v, WORD n);
#pragma aux mxor = "jcxz e" "l: lodsb" "xor es:[di],al" "inc di" "loop l" "e:" parm [es di] [si] [cx] modify [ax cx si di]

/* Fuellen mit zwei Bytewerten im Wechsel (Muster): AX = erster | zweiter << 8, rep stosw */
void f2(BYTE __far *d, WORD n, WORD v);
#pragma aux f2 = "shr cx,1" "rep stosw" "jnc e" "stosb" "e:" parm [es di] [cx] [ax] modify [cx di]
/* Verknuepfen n Bytes mit einem Wert (XOR/OR/AND) */
void bxor(BYTE __far *d, WORD n, BYTE v);
#pragma aux bxor = "jcxz e" "l: xor es:[di],al" "inc di" "loop l" "e:" parm [es di] [cx] [al] modify [cx di]
void bor(BYTE __far *d, WORD n, BYTE v);
#pragma aux bor = "jcxz e" "l: or es:[di],al" "inc di" "loop l" "e:" parm [es di] [cx] [al] modify [cx di]
void band(BYTE __far *d, WORD n, BYTE v);
#pragma aux band = "jcxz e" "l: and es:[di],al" "inc di" "loop l" "e:" parm [es di] [cx] [al] modify [cx di]
static BYTE vb[NB], mb[NB], ab[NB + 2];            /* Werte, Masken, ausgerichteter Quellstrom */

/* Zeilenpuffer schreiben: vb/mb fuer nb Bytes ab Schirmbyte p, rop COPY/MASK/OR/AND/XOR */
static void schreib(BYTE __far *p, WORD nb, WORD rop)
{
    WORD i;
    switch (rop) {
    case ROP_OR:  for (i = 0; i < nb; i++) vb[i] &= mb[i]; mor(p, vb, nb); break;
    case ROP_XOR: for (i = 0; i < nb; i++) vb[i] &= mb[i]; mxor(p, vb, nb); break;
    case ROP_AND: for (i = 0; i < nb; i++) vb[i] |= (BYTE)~mb[i]; mand(p, vb, nb); break;
    default:      mschr(p, vb, mb, nb);
    }
}
/* Strom ab (2-Bit-Bytes, Punkt 0 im obersten Bitpaar) um sh Punkte nach rechts auf Schirmbytes legen */
static void lege(const BYTE *a, WORD n, WORD sh, BYTE *z)
{
    WORD i, s = sh << 1;
    if (!s) { memcpy(z, a, n); return; }
    z[0] = (BYTE)(a[0] >> s);
    for (i = 1; i < n; i++) z[i] = (BYTE)((a[i - 1] << (8 - s)) | (a[i] >> s));
}
static WORD randmaske(WORD x, WORD w)             /* mb = Randmaske fuer Punkte x..x+w-1, liefert Bytezahl */
{
    WORD nb = ((x & 3) + w + 3) >> 2;
    memset(mb, 0xFF, nb);
    mb[0] &= (BYTE)(0xFF >> ((x & 3) << 1));
    mb[nb - 1] &= (BYTE)(0xFF << ((3 - ((x + w - 1) & 3)) << 1));
    return nb;
}

static WORD RDEXPORT d_init(WORD param)
{
    WORD i, k;
    (void)param;
    altmodus = (BYTE)(int10(0x0F00) & 0x7F);
    if ((BYTE)int10(0x1A00) != 0x1A) {            /* ohne VGA: Farbkarte laut BIOS und ein 6845 auf 3D4h */
        BYTE alt;
        if ((equip() & 0x30) == 0x30) return RD_ERR_NOTSUP;
        outp(0x3D4, 0x0F); alt = (BYTE)inp(0x3D5);
        outp(0x3D5, 0x5A); if ((BYTE)inp(0x3D5) != 0x5A) { outp(0x3D5, alt); return RD_ERR_NOTSUP; }
        outp(0x3D5, alt);
    }
    if (altmodus > 3 && altmodus != 7) altmodus = 3;
    for (i = 0; i < 16; i++) { BYTE v = 0; for (k = 0; k < 4; k++) if (i & (8 >> k)) v |= (BYTE)(0xC0 >> (2 * k)); e4[i] = v; }
    return 0;
}
static void RDEXPORT d_done(void) { }
static WORD RDEXPORT d_modes(GFXMODE FAR *m, WORD max)
{
    if (max) { m->w = W; m->h = H; m->bpp = 4; m->farben = 4; m->aspx = 100; m->aspy = 120; m->zeilenbytes = W / 2; m->flags = 0; }
    return 1;
}
static WORD RDEXPORT d_setmode(WORD nr)
{
    (void)nr;
    int10(0x0004);
    int10b(0x0B00, 0x0101);                       /* Palette 1: tuerkis, rosa, weiss */
    int10b(0x0B00, 0x0010);                       /* Hintergrund schwarz, helle Farben */
    outp(0x3D9, 0x30);                            /* echte CGA: Palette 1 + hell, Hintergrund schwarz */
    return 0;
}
static void RDEXPORT d_textmode(void) { int10(altmodus); }

/* ---- Fuellen ---------------------------------------------------------------- */
static void RDEXPORT d_box(short x0, short y0, short x1, short y1, WORD farbe, const BYTE FAR *mu, WORD rop)
{
    short y; WORD b0 = (WORD)x0 >> 2, b1 = (WORD)x1 >> 2, n = b1 - b0 + 1, i;
    BYTE lm = (BYTE)(0xFF >> ((x0 & 3) << 1)), rm = (BYTE)(0xFF << ((3 - (x1 & 3)) << 1));
    BYTE f = FP4(farbe), g = FP4(farbe >> 8);
    if (rop == ROP_NOT) { f = 0xFF; mu = 0; rop = ROP_XOR; }
    if (rop == ROP_MASK) rop = ROP_COPY;
    if (n == 1) lm &= rm;
    for (y = y0; y <= y1; y++) {
        BYTE __far *p = VP(x0, y); BYTE v0 = f, v1 = f;
        if (mu) {                                 /* Muster: gerade/ungerade Schirmbytes haben je einen Wert */
            WORD m = E1(mu[y & 7]); BYTE sh = (BYTE)(m >> 8), sl = (BYTE)m;
            v0 = (BYTE)((f & sh) | (g & ~sh)); v1 = (BYTE)((f & sl) | (g & ~sl));
            if (b0 & 1) { BYTE t = v0; v0 = v1; v1 = t; }   /* v0 = Wert des ersten Bytes */
        }
        if (rop == ROP_COPY) {                    /* Raender maskiert, Mitte am Stueck (rep stosw) */
            *p = (BYTE)((*p & ~lm) | (v0 & lm));
            if (n == 1) continue;
            { BYTE vl = (n & 1) ? v0 : v1; p[n - 1] = (BYTE)((p[n - 1] & ~rm) | (vl & rm)); }
            if (n > 2) f2(p + 1, n - 2, (WORD)(v1 | (WORD)v0 << 8));
            continue;
        }
        if (!mu) {                                /* XOR/OR/AND mit einer Farbe: Raender maskiert, Mitte am Stueck */
            if (rop == ROP_XOR) { *p ^= (BYTE)(f & lm); if (n > 1) { p[n - 1] ^= (BYTE)(f & rm); if (n > 2) bxor(p + 1, n - 2, f); } }
            else if (rop == ROP_OR) { *p |= (BYTE)(f & lm); if (n > 1) { p[n - 1] |= (BYTE)(f & rm); if (n > 2) bor(p + 1, n - 2, f); } }
            else { *p &= (BYTE)(f | ~lm); if (n > 1) { p[n - 1] &= (BYTE)(f | ~rm); if (n > 2) band(p + 1, n - 2, f); } }
            continue;
        }
        for (i = 0; i < n; i++) vb[i] = (BYTE)((i & 1) ? v1 : v0);
        memset(mb, 0xFF, n); mb[0] &= lm; mb[n - 1] &= rm;
        schreib(p, n, rop);
    }
}
static void RDEXPORT d_span(short y, short x0, short x1, WORD farbe, const BYTE FAR *mu, WORD rop) { d_box(x0, y, x1, y, farbe, mu, rop); }
static void RDEXPORT d_pixel(short x, short y, WORD farbe, WORD rop)
{
    BYTE __far *p = VP(x, y); BYTE s = (BYTE)(6 - ((x & 3) << 1)), m = (BYTE)(3 << s), v = (BYTE)((farbe & 3) << s);
    switch (rop) {
    case ROP_OR:  *p |= v; break;
    case ROP_AND: *p &= (BYTE)(~m | v); break;
    case ROP_XOR: *p ^= v; break;
    case ROP_NOT: *p ^= m; break;
    default:      *p = (BYTE)((*p & ~m) | v);
    }
}
static WORD RDEXPORT d_getpixel(short x, short y) { return (WORD)((*VP(x, y) >> (6 - ((x & 3) << 1))) & 3); }

/* ---- Bitmaps ------------------------------------------------------------------ */
static void RDEXPORT d_put(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes,
                           WORD srcbpp, WORD rop, WORD fg, WORD bg)
{
    short r; WORD i, nb, na, sh = x & 3; BYTE f = FP4(fg), g = FP4(bg);
    if (w <= 0 || h <= 0) return;
    if (rop == ROP_NOT && (srcbpp & 0xFF) != 4) { d_box(x, y, x + w - 1, y + h - 1, 3, 0, ROP_XOR); return; }
    for (r = 0; r < h; r++, src += srcbytes) {
        nb = randmaske(x, w);
        if ((srcbpp & 0xFF) == 4) {               /* 4 Bit: Werte 0..3, zwei Quellbytes -> ein 2-Bit-Byte */
            WORD u = srcbpp >> 8 & 1;
            na = (w + 3) >> 2;
            if (!u) { const BYTE FAR *q = src; for (i = 0; i < na; i++, q += 2) ab[i] = (BYTE)(N2H(q[0]) | N2(q[1])); }
            else for (i = 0; i < na; i++) {       /* ab dem unteren Halbbyte: um einen Punkt versetzt */
                BYTE a = (BYTE)(src[2 * i] << 4 | src[2 * i + 1] >> 4), b = (BYTE)(src[2 * i + 1] << 4 | src[2 * i + 2] >> 4);
                ab[i] = (BYTE)(N2(a) << 4 | N2(b));
            }
            ab[na] = 0;
            if (rop == ROP_COPY || rop == ROP_MASK) {   /* Raender maskiert, Mitte direkt */
                BYTE __far *p = VP(x, y + r); const BYTE *v = vb;
                if (sh) lege(ab, nb, sh, vb); else v = ab;
                *p = (BYTE)((*p & ~mb[0]) | (v[0] & mb[0]));
                if (nb > 1) { p[nb - 1] = (BYTE)((p[nb - 1] & ~mb[nb - 1]) | (v[nb - 1] & mb[nb - 1])); if (nb > 2) _fmemcpy(p + 1, v + 1, nb - 2); }
                continue;
            }
            lege(ab, nb, sh, vb);
            schreib(VP(x, y + r), nb, rop);
        } else {                                  /* 1 Bit: gesetzt in fg; MASK nur gesetzt, sonst geloescht in bg */
            WORD k = (w + 7) >> 3;
            for (i = 0; i < k; i++) { WORD e = E1(src[i]); ab[2 * i] = (BYTE)(e >> 8); ab[2 * i + 1] = (BYTE)e; }
            ab[2 * k] = 0;
            lege(ab, nb, sh, vb);                 /* vb = Punktmaske */
            if (rop == ROP_MASK) {
                for (i = 0; i < nb; i++) { mb[i] &= vb[i]; vb[i] = f; }
                schreib(VP(x, y + r), nb, ROP_COPY);
            } else {
                for (i = 0; i < nb; i++) { BYTE s = vb[i]; vb[i] = (BYTE)((f & s) | (g & ~s)); }
                schreib(VP(x, y + r), nb, rop);
            }
        }
    }
}
/* get: Geraeteformat (4 Bit gepackt), w Punkte ab x */
static void RDEXPORT d_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    short r; WORD i, nb = ((x & 3) + w + 3) >> 2, s = (x & 3) << 1, n = (w + 1) >> 1;
    for (r = 0; r < h; r++, dst += dstbytes) {
        const BYTE __far *p = VP(x, y + r);
        _fmemcpy(vb, p, nb); vb[nb] = 0;
        for (i = 0; i < nb; i++) {                /* ausrichten: Punkt x zuerst */
            BYTE a = s ? (BYTE)(vb[i] << s | vb[i + 1] >> (8 - s)) : vb[i]; WORD gg = G2(a);
            if (2 * i < n) dst[2 * i] = (BYTE)(gg >> 8);
            if (2 * i + 1 < n) dst[2 * i + 1] = (BYTE)gg;
        }
        if (w & 1) dst[n - 1] &= 0xF0;
    }
}
/* Schirm -> Schirm (Rollen): gleiche Lage im Byte ganze Bytes am Stueck, Raender und sonst ueber get/put */
static BYTE kz[W / 2 + 2];
static void RDEXPORT d_copy(short sx, short sy, short w, short h, short dx, short dy)
{
    short r, st = 1, y0 = 0;
    if (dy > sy) { st = -1; y0 = h - 1; }
    for (r = y0; r >= 0 && r < h; r += st) {
        if (!((sx ^ dx) & 3) && w >= 8) {
            WORD a = (WORD)sx, b = (WORD)(sx + w - 1), ma = (a + 3) & ~3, me = (b + 1) & ~3, n = (me - ma) >> 2;
            if (ma > a) { d_get(a, sy + r, ma - a, 1, kz, sizeof kz); d_put(dx, dy + r, ma - a, 1, kz, sizeof kz, 4, ROP_COPY, 0, 0); }
            if (n) {
                BYTE __far *s = VP(ma, sy + r), __far *d = VP(ma + (dx - sx), dy + r);
                if (dy != sy || d < s) _fmemcpy(d, s, n);
                else { WORD i; for (i = n; i--; ) d[i] = s[i]; }
            }
            if (b >= me) { d_get(me, sy + r, b - me + 1, 1, kz, sizeof kz); d_put(me + (dx - sx), dy + r, b - me + 1, 1, kz, sizeof kz, 4, ROP_COPY, 0, 0); }
        } else { d_get(sx, sy + r, w, 1, kz, sizeof kz); d_put(dx, dy + r, w, 1, kz, sizeof kz, 4, ROP_COPY, 0, 0); }
    }
}
static void RDEXPORT d_palette(WORD first, WORD n, const COLOR FAR *c) { (void)first; (void)n; (void)c; }

/* ---- Mauszeiger 16x16 (UND/XOR): Hintergrund 5 Bytes je Zeile ------------------------- */
static BYTE csv[16][5], c_on, c_h, c_nb;
static WORD c_x, c_y;
static void crest(void)
{
    WORD r, k;
    if (!c_on) return;
    for (r = 0; r < c_h; r++) { BYTE __far *p = VP(c_x, c_y + r); for (k = 0; k < c_nb; k++) p[k] = csv[r][k]; }
    c_on = 0;
}
static void RDEXPORT d_cursorhide(BOOL aus) { if (aus) crest(); }
static void RDEXPORT d_cursor(short x, short y, const BYTE FAR *mb2)
{
    WORD r, k, sh = x & 3; static BYTE a5[6], o5[6], t[5];   /* static: SS ist nicht DS */
    crest();
    c_x = (WORD)x; c_y = (WORD)y;
    c_h = (BYTE)(H - y < 16 ? H - y : 16);
    c_nb = (BYTE)(BPR - ((WORD)x >> 2) < 5 ? BPR - ((WORD)x >> 2) : 5);
    for (r = 0; r < c_h; r++) {
        BYTE __far *p = VP(x, y + r);
        WORD a = E1((BYTE)~mb2[2 * r]), b = E1((BYTE)~mb2[2 * r + 1]), o = E1(mb2[32 + 2 * r]), q = E1(mb2[33 + 2 * r]);
        for (k = 0; k < c_nb; k++) csv[r][k] = p[k];
        t[0] = (BYTE)(a >> 8); t[1] = (BYTE)a; t[2] = (BYTE)(b >> 8); t[3] = (BYTE)b; t[4] = 0; lege(t, 5, sh, a5);
        t[0] = (BYTE)(o >> 8); t[1] = (BYTE)o; t[2] = (BYTE)(q >> 8); t[3] = (BYTE)q; t[4] = 0; lege(t, 5, sh, o5);
        for (k = 0; k < c_nb; k++) p[k] = (BYTE)((p[k] & ~a5[k]) ^ o5[k]);   /* UND 0 -> schwarz, XOR 1 -> umkehren */
    }
    c_on = 1;
}

/* ---- Farben: 0 schwarz, 1 helltuerkis, 2 hellrosa, 3 weiss -------------------------- */
static WORD RDEXPORT d_farbe(COLOR c)
{
    BYTE r = (BYTE)c, g = (BYTE)(c >> 8), b = (BYTE)(c >> 16), mx = r > g ? r : g, mn = r < g ? r : g; WORD l = (r * 3 + g * 6 + b) / 10;
    if (b > mx) mx = b;
    if (b < mn) mn = b;
    if (mx - mn > 64 && mx > 96) return r > g && r > b / 2 ? 2 : 1;   /* rot/lila/braun -> rosa, sonst tuerkis */
    return l < 64 ? 0 : l < 150 ? 1 : 3;            /* grau: dunkel schwarz, mittel tuerkis, hell weiss */
}
static COLOR RDEXPORT d_rgb(WORD i)
{
    static const COLOR f[4] = { RGB(0, 0, 0), RGB(85, 255, 255), RGB(255, 85, 255), RGB(255, 255, 255) };
    return f[i & 3];
}

/* ---- Hintergrund roh: Schirmbytes unveraendert, Raender maskiert ---- */
static WORD RDEXPORT d_rohgr(short x, short w) { return (WORD)(((x & 3) + w + 3) >> 2); }
static void RDEXPORT d_rohget(short x, short y, short w, short h, BYTE FAR *z, WORD bpr)
{
    WORD r, nb = ((x & 3) + w + 3) >> 2;
    for (r = 0; r < (WORD)h; r++, z += bpr) _fmemcpy(z, VP(x, y + r), nb);
}
static void RDEXPORT d_rohput(short qx, short qy, short qw, short qh, const BYTE FAR *z, WORD bpr, short x0, short w0)
{
    WORD r, c = ((WORD)qx >> 2) - ((WORD)x0 >> 2), nb = ((qx & 3) + qw + 3) >> 2;
    BYTE lm = (BYTE)(0xFF >> ((qx & 3) << 1)), rm = (BYTE)(0xFF << ((3 - ((qx + qw - 1) & 3)) << 1));
    (void)w0;
    if (nb == 1) lm &= rm;
    for (r = 0; r < (WORD)qh; r++, z += bpr) {
        BYTE __far *p = VP(qx, qy + r); const BYTE FAR *q = z + c;
        *p = (BYTE)((*p & ~lm) | (q[0] & lm));
        if (nb > 1) { p[nb - 1] = (BYTE)((p[nb - 1] & ~rm) | (q[nb - 1] & rm)); if (nb > 2) _fmemcpy(p + 1, q + 1, nb - 2); }
    }
}

/* Farbiges Symbol: Werte wie put (4 Bit), Maske ueber e1 auf 2 Bit je Punkt, nur dort schreiben */
static BYTE mq[NB + 2];
static void RDEXPORT d_symput(short x, short y, short w, short h, const BYTE FAR *src, WORD sbpr, WORD unten, const BYTE FAR *m, WORD mbpr, WORD mbit)
{
    short r; WORD i, nb, na, sh = x & 3, k = ((WORD)w + 7) >> 3;
    if (w <= 0 || h <= 0) return;
    for (r = 0; r < h; r++, src += sbpr, m += mbpr) {
        nb = randmaske(x, w); na = (w + 3) >> 2;
        if (!unten) { const BYTE FAR *q = src; for (i = 0; i < na; i++, q += 2) ab[i] = (BYTE)(N2H(q[0]) | N2(q[1])); }
        else for (i = 0; i < na; i++) {
            BYTE a = (BYTE)(src[2 * i] << 4 | src[2 * i + 1] >> 4), b = (BYTE)(src[2 * i + 1] << 4 | src[2 * i + 2] >> 4);
            ab[i] = (BYTE)(N2H(a) << 0 | N2(b));
        }
        ab[na] = 0; lege(ab, nb, sh, vb);
        for (i = 0; i < k; i++) {                 /* Maskenbits ab mbit -> je Punkt 2 Bit */
            BYTE b = (BYTE)(mbit ? m[i] << mbit | m[i + 1] >> (8 - mbit) : m[i]); WORD e = E1(b);
            ab[2 * i] = (BYTE)(e >> 8); ab[2 * i + 1] = (BYTE)e;
        }
        ab[2 * k] = 0; lege(ab, nb, sh, mq);
        for (i = 0; i < nb; i++) mb[i] &= mq[i];
        mschr(VP(x, y + r), vb, mb, nb);
    }
}

static GFXDRVR tab = { { {
    { { 'R', 'D', 'R', 'V' }, DRV_GFX, 0x0100, sizeof(GFXDRVR), "CGA 320x200 4", d_init, d_done },
    d_modes, d_setmode, d_textmode,
    d_span, d_pixel, d_getpixel, d_put, d_get,
    0, 0, d_copy, 0, d_palette, d_cursor, d_cursorhide, 0, d_box }, d_farbe, d_rgb },
    d_rohgr, d_rohget, d_rohput, d_symput
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
