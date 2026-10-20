/* GFX1.C - Grafiktreiber fuer 1 Bit pro Pixel: CGA 640x200, Hercules 720x348, VGA 640x480 mono.
 * Eine Quelle, drei Treiber: mit -DCGA, -DHGC oder -DVGAM bauen.
 * Treiber kennen kein Clipping: RDGDI ruft nur mit gueltigen Koordinaten.
 * Farbe 0 = schwarz, 1 = weiss. Muster (8 Bytes) geben die Bits direkt vor. */
#include <i86.h>
#include <conio.h>
#include <string.h>
#include "RD2DRV.H"

#if defined(CGA)
#  define W 640
#  define H 200
#  define BPR 80
#  define VSEG 0xB800
#  define ROW(y) ((((WORD)(y) >> 1) * 80) + (((WORD)(y) & 1) << 13))
#  define NAME "CGA 640x200"
#  define ASPY 240
#elif defined(HGC)
#  define W 720
#  define H 348
#  define BPR 90
#  define VSEG 0xB000
#  define ROW(y) ((((WORD)(y) >> 2) * 90) + (((WORD)(y) & 3) << 13))
#  define NAME "Hercules 720x348"
#  define ASPY 155
#elif defined(VGAM)
#  define W 640
#  define H 480
#  define BPR 80
#  define VSEG 0xA000
#  define ROW(y) ((WORD)(y) * 80)
#  define NAME "VGA 640x480 mono"
#  define ASPY 100
#else
#  error CGA, HGC oder VGAM angeben
#endif

#define VP(x, y) ((BYTE __far *)MK_FP(VSEG, ROW(y) + ((WORD)(x) >> 3)))

static BYTE altmodus;

/* INT 10h direkt: int86() der Watcom-Bibliothek legt REGS auf den Stack und braucht SS = DS,
   Treiber laufen aber mit dem Stack des Aufrufers (-zu) */
WORD int10(WORD ax);
#pragma aux int10 = "push bp" "int 10h" "pop bp" parm [ax] value [ax] modify [bx cx dx si di es]
static BYTE bios_mode(void) { return (BYTE)(int10(0x0F00) & 0x7F); }
static void bios_set(BYTE m) { int10(m); }

#ifdef HGC
static void hgc(int gr)
{
    static const BYTE crt[2][12] = {
        { 0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19, 0x02, 0x0D, 0x0B, 0x0C },   /* Text */
        { 0x35, 0x2D, 0x2E, 0x07, 0x5B, 0x02, 0x57, 0x57, 0x02, 0x03, 0x00, 0x00 } }; /* 720x348 */
    WORD i;
    outp(0x3BF, gr);                              /* Grafik erlauben, nur Seite 0 */
    outp(0x3B8, gr ? 0x02 : 0x20);                /* Bild aus beim Umprogrammieren */
    for (i = 0; i < 12; i++) { outp(0x3B4, i); outp(0x3B5, crt[gr][i]); }
    outp(0x3B8, gr ? 0x0A : 0x28);
    *(BYTE __far *)MK_FP(0x40, 0x49) = (BYTE)(gr ? 6 : 7);   /* Maustreiber: 6 = Hercules-Grafik */
}
#endif

static WORD RDEXPORT d_init(WORD param)
{
    (void)param;
    altmodus = bios_mode();
#if defined(HGC)
    {   /* Hercules: Monochrom-Textmodus und Bit 7 von 3BAh (Strahlruecklauf) wechselt */
        WORD i; BYTE a;
        if (altmodus != 7) return RD_ERR_NOTSUP;
        a = (BYTE)(inp(0x3BA) & 0x80);
        for (i = 0; i < 0x8000U; i++) if ((inp(0x3BA) & 0x80) != a) return 0;
        return RD_ERR_NOTSUP;
    }
#elif defined(VGAM)
    if ((BYTE)int10(0x1A00) != 0x1A) return RD_ERR_NOTSUP;
#endif
    if (altmodus > 3 && altmodus != 7) altmodus = 3;
    return 0;
}
static void RDEXPORT d_done(void) { }

static WORD RDEXPORT d_modes(GFXMODE FAR *m, WORD max)
{
    if (max) {
        m->w = W; m->h = H; m->bpp = 1; m->farben = 2;
        m->aspx = 100; m->aspy = ASPY; m->zeilenbytes = BPR; m->flags = 0;
    }
    return 1;
}

static WORD RDEXPORT d_setmode(WORD nr)
{
    (void)nr;
#if defined(HGC)
    hgc(1);
    _fmemset(MK_FP(VSEG, 0), 0, 0x8000U);
#elif defined(VGAM)
    bios_set(0x11);
#else
    bios_set(0x06);
#endif
    return 0;
}

static void RDEXPORT d_textmode(void)
{
#ifdef HGC
    hgc(0);
    bios_set(7);
#else
    bios_set(altmodus);
#endif
}

/* ein Byte mit Maske m und Wert v nach Rasteroperation schreiben */
static void wr(BYTE __far *p, BYTE m, BYTE v, WORD rop)
{
    switch (rop) {
    case ROP_COPY: *p = (BYTE)((*p & ~m) | (v & m)); break;
    case ROP_OR:   *p |= (BYTE)(v & m); break;
    case ROP_AND:  *p &= (BYTE)(v | ~m); break;
    case ROP_XOR:  *p ^= (BYTE)(v & m); break;
    case ROP_NOT:  *p ^= m; break;
    }
}

static void RDEXPORT d_span(short y, short x0, short x1, WORD farbe, const BYTE FAR *muster8, WORD rop)
{
    BYTE __far *p = VP(x0, y);
    WORD b0 = (WORD)x0 >> 3, b1 = (WORD)x1 >> 3, n;
    BYTE lm = (BYTE)(0xFF >> (x0 & 7)), rm = (BYTE)(0xFF << (7 - (x1 & 7)));
    BYTE v = muster8 ? muster8[y & 7] : (BYTE)(farbe ? 0xFF : 0);
    if (b0 == b1) { wr(p, (BYTE)(lm & rm), v, rop); return; }
    wr(p++, lm, v, rop);
    n = b1 - b0 - 1;
    if (rop == ROP_COPY) { _fmemset(p, v, n); p += n; }
    else while (n--) wr(p++, 0xFF, v, rop);
    wr(p, rm, v, rop);
}

static void RDEXPORT d_pixel(short x, short y, WORD farbe, WORD rop)
{
    wr(VP(x, y), (BYTE)(0x80 >> (x & 7)), (BYTE)(farbe ? 0xFF : 0), rop);
}

static WORD RDEXPORT d_getpixel(short x, short y)
{
    return (*VP(x, y) >> (7 - (x & 7))) & 1;
}

static void RDEXPORT d_vline(short x, short y0, short y1, WORD farbe, WORD rop)
{
    BYTE m = (BYTE)(0x80 >> (x & 7)), v = (BYTE)(farbe ? 0xFF : 0);
    for (; y0 <= y1; y0++) wr(VP(x, y0), m, v, rop);
}

/* 1-Bit-Quelle (Bit 7 = links) an x,y bringen. ROP_MASK: nur gesetzte Bits in fg. */
static void RDEXPORT d_put(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes,
                        WORD srcbpp, WORD rop, WORD fg, WORD bg)
{
    WORD sh = x & 7, nb = (sh + w + 7) >> 3, sb = (w + 7) >> 3, i;
    BYTE lm = (BYTE)(0xFF >> sh), rm = (BYTE)(0xFF << (7 - ((x + w - 1) & 7)));
    BYTE vf = (BYTE)(fg ? 0xFF : 0), vb = (BYTE)(bg ? 0xFF : 0);
    (void)srcbpp;
    for (; h > 0; h--, y++, src += srcbytes) {
        BYTE __far *p = VP(x, y);
        WORD acc = 0;
        for (i = 0; i < nb; i++) {
            BYTE b, m = 0xFF;
            acc = (WORD)((acc << 8) | (i < sb ? src[i] : 0));
            b = (BYTE)(acc >> sh);
            if (i == 0) m &= lm;
            if (i == nb - 1) m &= rm;
            if (rop == ROP_MASK) wr(p + i, (BYTE)(m & b), vf, ROP_COPY);
            else wr(p + i, m, (BYTE)((vf & b) | (vb & ~b)), rop);
        }
    }
}

static void RDEXPORT d_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    WORD sh = x & 7, sb = (w + 7) >> 3, i;
    BYTE rm = (BYTE)(0xFF << ((8 - (w & 7)) & 7));
    for (; h > 0; h--, y++, dst += dstbytes) {
        BYTE __far *p = VP(x, y);
        for (i = 0; i < sb; i++) dst[i] = (BYTE)((p[i] << sh) | (sh ? p[i + 1] >> (8 - sh) : 0));
        dst[sb - 1] &= rm;
    }
}

static GFXDRV tab = {
    { { 'R', 'D', 'R', 'V' }, DRV_GFX, 0x0100, sizeof(GFXDRV), NAME, d_init, d_done },
    d_modes, d_setmode, d_textmode,
    d_span, d_pixel, d_getpixel, d_put, d_get,
    d_vline, 0, 0, 0, 0, 0, 0, 0
};

FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
