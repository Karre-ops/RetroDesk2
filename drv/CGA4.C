/* CGA4.C - Grafiktreiber CGA 320x200 mit 4 Farben (Modus 4, 2 Bit je Punkt, Baenke zu 2000h).
 * Palette 1 hell: 0 schwarz, 1 helltuerkis, 2 hellrosa, 3 weiss. Laeuft auf CGA, EGA und VGA.
 * Treiber kennen kein Clipping. Farbe = Index 0..3. Mit Muster (span/box): Bit 1 = Farbe (Bits 0-1),
 * Bit 0 = Hintergrund (Bits 8-9). Bitmaps im Geraeteformat wie VGA16: 4 Bit je Punkt gepackt,
 * linker Punkt im oberen Halbbyte (Bit 8 von srcbpp: erster Punkt im unteren Halbbyte der Quelle).
 * Schlicht in C (Karsten, 30.10.2026): selten gebraucht, Hauptsache richtig. */
#include <i86.h>
#include <conio.h>
#include "RD2DRV.H"

#define W 320
#define H 200
#define BPR 80
#define VP(x, y) ((BYTE __far *)MK_FP(0xB800, (((WORD)(y) >> 1) * BPR) + (((WORD)(y) & 1) << 13) + ((WORD)(x) >> 2)))
#define SH(x) (6 - (((WORD)(x) & 3) << 1))
#pragma intrinsic(inp, outp)

static BYTE altmodus;
WORD int10(WORD ax);
#pragma aux int10 = "push bp" "int 10h" "pop bp" parm [ax] value [ax] modify [bx cx dx si di es]
WORD int10b(WORD ax, WORD bx);
#pragma aux int10b = "push bp" "int 10h" "pop bp" parm [ax] [bx] value [ax] modify [bx cx dx si di es]
WORD equip(void);
#pragma aux equip = "int 11h" value [ax]

static void pset(WORD x, WORD y, BYTE c, WORD rop)
{
    BYTE __far *p = VP(x, y); BYTE s = (BYTE)SH(x), m = (BYTE)(3 << s), v = (BYTE)((c & 3) << s);
    switch (rop) {
    case ROP_OR:  *p |= v; break;
    case ROP_AND: *p &= (BYTE)(~m | v); break;
    case ROP_XOR: *p ^= v; break;
    case ROP_NOT: *p ^= m; break;
    default:      *p = (BYTE)((*p & ~m) | v);
    }
}
static BYTE pget(WORD x, WORD y) { return (BYTE)((*VP(x, y) >> SH(x)) & 3); }

static WORD RDEXPORT d_init(WORD param)
{
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
static void zeile(WORD y, WORD x0, WORD x1, BYTE c, WORD rop)
{
    WORD x = x0;
    if (rop == ROP_COPY || rop == ROP_MASK) {     /* volle Bytes am Stueck */
        BYTE v = (BYTE)((c & 3) * 0x55); BYTE __far *p;
        while (x <= x1 && (x & 3)) pset(x++, y, c, ROP_COPY);
        p = VP(x, y);
        while (x + 3 <= x1) { *p++ = v; x += 4; }
    }
    for (; x <= x1; x++) pset(x, y, c, rop);
}
static void RDEXPORT d_box(short x0, short y0, short x1, short y1, WORD farbe, const BYTE FAR *mu, WORD rop)
{
    short y, x;
    if (!mu) { for (y = y0; y <= y1; y++) zeile(y, x0, x1, (BYTE)farbe, rop); return; }
    for (y = y0; y <= y1; y++) {
        BYTE m = mu[y & 7];
        for (x = x0; x <= x1; x++) pset(x, y, (BYTE)((m & (0x80 >> (x & 7))) ? farbe : farbe >> 8), rop == ROP_MASK ? ROP_COPY : rop);
    }
}
static void RDEXPORT d_span(short y, short x0, short x1, WORD farbe, const BYTE FAR *mu, WORD rop) { d_box(x0, y, x1, y, farbe, mu, rop); }
static void RDEXPORT d_pixel(short x, short y, WORD farbe, WORD rop) { pset(x, y, (BYTE)farbe, rop == ROP_MASK ? ROP_COPY : rop); }
static WORD RDEXPORT d_getpixel(short x, short y) { return pget(x, y); }

/* ---- Bitmaps ------------------------------------------------------------------ */
static void RDEXPORT d_put(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes,
                           WORD srcbpp, WORD rop, WORD fg, WORD bg)
{
    short r, i;
    if (w <= 0 || h <= 0) return;
    if (rop == ROP_NOT && (srcbpp & 0xFF) != 4) { d_box(x, y, x + w - 1, y + h - 1, 3, 0, ROP_XOR); return; }
    for (r = 0; r < h; r++, src += srcbytes)
        if ((srcbpp & 0xFF) == 4) {
            WORD u = srcbpp >> 8 & 1;
            for (i = 0; i < w; i++) { WORD q = i + u; BYTE c = (BYTE)((q & 1) ? src[q >> 1] & 0x0F : src[q >> 1] >> 4); pset(x + i, y + r, c, rop == ROP_MASK ? ROP_COPY : rop); }
        } else
            for (i = 0; i < w; i++) {
                if ((src[i >> 3] >> (7 - (i & 7))) & 1) pset(x + i, y + r, (BYTE)fg, rop == ROP_MASK ? ROP_COPY : rop);
                else if (rop != ROP_MASK) pset(x + i, y + r, (BYTE)bg, rop);
            }
}
static void RDEXPORT d_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    short r, i;
    for (r = 0; r < h; r++, dst += dstbytes)
        for (i = 0; i < w; i++) {
            BYTE c = pget(x + i, y + r);
            if (i & 1) dst[i >> 1] = (BYTE)((dst[i >> 1] & 0xF0) | c); else dst[i >> 1] = (BYTE)(c << 4);
        }
}
static BYTE kz[W / 2];
static void RDEXPORT d_copy(short sx, short sy, short w, short h, short dx, short dy)
{
    short r, st = 1, y0 = 0;
    if (dy > sy) { st = -1; y0 = h - 1; }
    for (r = y0; r >= 0 && r < h; r += st) { d_get(sx, sy + r, w, 1, kz, sizeof kz); d_put(dx, dy + r, w, 1, kz, sizeof kz, 4, ROP_COPY, 0, 0); }
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
static void RDEXPORT d_cursor(short x, short y, const BYTE FAR *mb)
{
    WORD r, k, n;
    crest();
    c_x = (WORD)x & ~3; c_y = (WORD)y;
    c_h = (BYTE)(H - y < 16 ? H - y : 16);
    c_nb = (BYTE)(BPR - ((WORD)x >> 2) < 5 ? BPR - ((WORD)x >> 2) : 5);
    for (r = 0; r < c_h; r++) { BYTE __far *p = VP(c_x, y + r); for (k = 0; k < c_nb; k++) csv[r][k] = p[k]; }
    n = (WORD)(W - x < 16 ? W - x : 16);
    for (r = 0; r < c_h; r++) {
        WORD a = (WORD)mb[2 * r] << 8 | mb[2 * r + 1], o = (WORD)mb[32 + 2 * r] << 8 | mb[33 + 2 * r];
        for (k = 0; k < n; k++) {
            WORD m = 0x8000 >> k;
            if (!(a & m)) pset(x + k, y + r, 0, ROP_COPY);   /* UND 0 -> schwarz */
            if (o & m) pset(x + k, y + r, 3, ROP_XOR);       /* XOR 1 -> umkehren */
        }
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

static GFXDRVF tab = { {
    { { 'R', 'D', 'R', 'V' }, DRV_GFX, 0x0100, sizeof(GFXDRVF), "CGA 320x200 4", d_init, d_done },
    d_modes, d_setmode, d_textmode,
    d_span, d_pixel, d_getpixel, d_put, d_get,
    0, 0, d_copy, 0, d_palette, d_cursor, d_cursorhide, 0, d_box }, d_farbe, d_rgb
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
