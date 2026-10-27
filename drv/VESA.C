/* VESA.C - Grafiktreiber 256 Farben ueber VBE (VESA BIOS Extension), 640x480 (Modus 101h).
 * Bildspeicher in 64-KB-Fenstern bei A000h, Bankumschaltung ueber INT 10h 4F05h.
 * Treiber kennen kein Clipping. Farbe = Index 0..255:
 *   0..15    Standardfarben wie VGA16 (0 schwarz, 7 hellgrau, 8 dunkelgrau, 15 weiss)
 *   16..231  Farbwuerfel 6x6x6 (Stufen 0, 51, 102, 153, 204, 255), Index 16 + 36 r + 6 g + b
 *   232..255 24 Graustufen
 * Mit Muster (span/box): Bit 1 = Farbe (Bits 0-7), Bit 0 = Hintergrund (Bits 8-15).
 * Bitmaps im Geraeteformat: 8 Bit je Punkt (put/get mit srcbpp 8), 1-Bit-Quellen mit fg/bg.
 * Umkehren (ROP_NOT, Text/Zeiger XOR) = Index XOR 0Fh wie bei 16 Farben (weiss <-> schwarz).
 * Profil Komfort (386+): mit -3 uebersetzt. */
#include <i86.h>
#include <conio.h>
#include <string.h>
#include "RD2DRV.H"

#define W 640
#define H 480
#define MODUS 0x101
#pragma intrinsic(inp, outp)

static BYTE altmodus, bank = 0xFF, schritt = 1;    /* schritt: Fensterschritte je 64 KB */
static WORD bpl = W, seg = 0xA000;
static BYTE vb[512];                               /* VBE-Infoblock / Modusinfo */

WORD int10(WORD ax);
#pragma aux int10 = "push bp" "int 10h" "pop bp" parm [ax] value [ax] modify [bx cx dx si di es]
WORD vbe(WORD ax, WORD cx, void *p);              /* ES:DI = ds:p */
#pragma aux vbe = "push ds" "pop es" "push bp" "int 10h" "pop bp" parm [ax] [cx] [di] value [ax] modify [bx dx si es]
WORD vmode(WORD bx);
#pragma aux vmode = "mov ax,4F02h" "push bp" "int 10h" "pop bp" parm [bx] value [ax] modify [cx dx si di es]
void vbank(WORD dx);
#pragma aux vbank = "mov ax,4F05h" "xor bx,bx" "push bp" "int 10h" "pop bp" parm [dx] modify [ax bx cx si di es]

/* Adresse x,y: Bank schalten, Zeiger im Fenster */
static BYTE __far *va(WORD x, WORD y)
{
    DWORD a = (DWORD)y * bpl + x; BYTE b = (BYTE)(a >> 16);
    if (b != bank) { bank = b; vbank((WORD)b * schritt); }
    return (BYTE __far *)MK_FP(seg, (WORD)a);
}
/* Rest bis Bankende ab dem Zeiger (1..65536 als DWORD) */
#define REST(p) (0x10000UL - FP_OFF(p))

static void setz(BYTE __far *p, BYTE c, WORD rop)
{
    switch (rop) {
    case ROP_OR:  *p |= c; break;
    case ROP_AND: *p &= c; break;
    case ROP_XOR: *p ^= c; break;
    case ROP_NOT: *p ^= 0x0F; break;
    default:      *p = c;
    }
}

static WORD RDEXPORT d_init(WORD param)
{
    (void)param;
    altmodus = (BYTE)(int10(0x0F00) & 0x7F);
    if (altmodus > 3 && altmodus != 7) altmodus = 3;
    if (vbe(0x4F00, 0, vb) != 0x004F || memcmp(vb, "VESA", 4)) return RD_ERR_NOTSUP;
    if (vbe(0x4F01, MODUS, vb) != 0x004F || !(vb[0] & 1)) return RD_ERR_NOTSUP;   /* Modus unterstuetzt */
    {   WORD g = *(WORD *)(vb + 4);               /* Fensterschritt in KB */
        if (!g || g > 64) return RD_ERR_NOTSUP;
        schritt = (BYTE)(64 / g);
        if (*(WORD *)(vb + 8)) seg = *(WORD *)(vb + 8);   /* Segment Fenster A */
        if (*(WORD *)(vb + 16)) bpl = *(WORD *)(vb + 16); /* Bytes je Zeile */
    }
    return 0;
}
static void RDEXPORT d_done(void) { }
static WORD RDEXPORT d_modes(GFXMODE FAR *m, WORD max)
{
    if (max) { m->w = W; m->h = H; m->bpp = 8; m->farben = 256; m->aspx = 100; m->aspy = 100; m->zeilenbytes = W; m->flags = GD_PALETTE; }
    return 1;
}
static void RDEXPORT d_palette(WORD first, WORD n, const COLOR FAR *c);
static COLOR RDEXPORT d_rgb(WORD i);
static WORD RDEXPORT d_setmode(WORD nr)
{
    WORD i;
    (void)nr;
    if (vmode(MODUS) != 0x004F) return RD_ERR_NOTSUP;
    bank = 0xFF;
    outp(0x3C8, 0);
    for (i = 0; i < 256; i++) { COLOR v = d_rgb(i); outp(0x3C9, (BYTE)v >> 2); outp(0x3C9, (BYTE)(v >> 8) >> 2); outp(0x3C9, (BYTE)(v >> 16) >> 2); }
    return 0;
}
static void RDEXPORT d_textmode(void) { int10(altmodus); bank = 0xFF; }

/* ---- Fuellen ---------------------------------------------------------------- */
#define NURFG 0x80
static void zeile(WORD y, WORD x0, WORD x1, BYTE fg, BYTE bg, BYTE mu, WORD rop)   /* mu: Musterzeile, 0xFF = voll */
{
    BYTE __far *p = va(x0, y); WORD x = x0; int beide = rop == ROP_COPY || rop == ROP_MASK;
    if (rop == NURFG) beide = 0;                  /* Text deckungslos: nur gesetzte Punkte */
    if (rop == ROP_MASK || rop == NURFG) rop = ROP_COPY;
    if (mu == 0xFF && rop == ROP_COPY) {           /* voll: Stuecke bis zum Bankende */
        while (x <= x1) {
            DWORD r = REST(p); WORD n = x1 - x + 1;
            if (n > r) n = (WORD)r;
            _fmemset(p, fg, n); x += n;
            if (x <= x1) p = va(x, y);
        }
        return;
    }
    for (; x <= x1; x++) {
        if (mu & (0x80 >> (x & 7))) setz(p, fg, rop); else if (beide) setz(p, bg, ROP_COPY);
        if (!FP_OFF(++p) && x < x1) p = va(x + 1, y);
    }
}
static void RDEXPORT d_box(short x0, short y0, short x1, short y1, WORD farbe, const BYTE FAR *mu, WORD rop)
{
    short y;
    for (y = y0; y <= y1; y++) zeile(y, x0, x1, (BYTE)farbe, (BYTE)(farbe >> 8), mu ? mu[y & 7] : (BYTE)0xFF, rop);
}
static void RDEXPORT d_span(short y, short x0, short x1, WORD farbe, const BYTE FAR *mu, WORD rop) { d_box(x0, y, x1, y, farbe, mu, rop); }
static void RDEXPORT d_pixel(short x, short y, WORD farbe, WORD rop) { setz(va(x, y), (BYTE)farbe, rop == ROP_MASK ? ROP_COPY : rop); }
static WORD RDEXPORT d_getpixel(short x, short y) { return *va(x, y); }

/* ---- Bitmaps ------------------------------------------------------------------ */
static void RDEXPORT d_put(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes,
                           WORD srcbpp, WORD rop, WORD fg, WORD bg)
{
    WORD r, i;
    if (w <= 0 || h <= 0) return;
    if (rop == ROP_NOT) { d_box(x, y, x + w - 1, y + h - 1, 0x0F, 0, ROP_XOR); return; }
    for (r = 0; r < (WORD)h; r++, src += srcbytes) {
        BYTE __far *p = va(x, y + r);
        if ((srcbpp & 0xFF) == 8 && (rop == ROP_COPY || rop == ROP_MASK) && REST(p) >= (WORD)w) { _fmemcpy(p, src, w); continue; }
        for (i = 0; i < (WORD)w; i++) {
            if ((srcbpp & 0xFF) == 8) setz(p, src[i], rop == ROP_MASK ? ROP_COPY : rop);
            else if (src[i >> 3] & (0x80 >> (i & 7))) setz(p, (BYTE)fg, rop == ROP_MASK ? ROP_COPY : rop);
            else if (rop != ROP_MASK) setz(p, (BYTE)bg, rop);
            if (!FP_OFF(++p)) p = va(x + i + 1, y + r);
        }
    }
}
static void RDEXPORT d_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    WORD r, i;
    for (r = 0; r < (WORD)h; r++, dst += dstbytes) {
        BYTE __far *p = va(x, y + r);
        if (REST(p) >= (WORD)w) { _fmemcpy(dst, p, w); continue; }
        for (i = 0; i < (WORD)w; i++) { dst[i] = *p; if (!FP_OFF(++p)) p = va(x + i + 1, y + r); }
    }
}
/* Schirm -> Schirm (Rollen) zeilenweise ueber einen Puffer */
static BYTE kz[W];
static void RDEXPORT d_copy(short sx, short sy, short w, short h, short dx, short dy)
{
    short r, st = 1, y0 = 0;
    if (dy > sy) { st = -1; y0 = h - 1; }
    for (r = y0; r >= 0 && r < h; r += st) { d_get(sx, sy + r, w, 1, kz, sizeof kz); d_put(dx, dy + r, w, 1, kz, sizeof kz, 8, ROP_COPY, 0, 0); }
}

/* ---- Text: Zeichen 8 Punkte breit, Zeile i = Byte seg:offs[i]+r ------------------------ */
static void RDEXPORT d_glyphs(short x, short y, const WORD FAR *offs, WORD gseg, WORD h, WORD n, WORD fg, WORD bg, WORD rop)
{
    WORD i, r;
    for (i = 0; i < n; i++) {
        const BYTE FAR *g = (const BYTE FAR *)MK_FP(gseg, offs[i]);
        for (r = 0; r < h; r++) {
            BYTE b = g[r], sh = (BYTE)(x & 7);
            b = (BYTE)(b >> sh | b << (8 - sh));      /* zeile zaehlt Musterbits ab x & 7 */
            if (rop == ROP_COPY) zeile(y + r, x + 8 * i, x + 8 * i + 7, (BYTE)fg, (BYTE)bg, b, ROP_COPY);
            else if (b) zeile(y + r, x + 8 * i, x + 8 * i + 7, (BYTE)(rop == ROP_XOR ? 0x0F : fg), 0, b, rop == ROP_XOR ? ROP_XOR : NURFG);
        }
    }
}

/* ---- Farbtabelle (DAC) ------------------------------------------------------------------ */
static void RDEXPORT d_palette(WORD first, WORD n, const COLOR FAR *c)
{
    WORD i;
    outp(0x3C8, (BYTE)first);
    for (i = 0; i < n && first + i < 256; i++) { COLOR v = c[i]; outp(0x3C9, (BYTE)v >> 2); outp(0x3C9, (BYTE)(v >> 8) >> 2); outp(0x3C9, (BYTE)(v >> 16) >> 2); }
}

/* ---- Mauszeiger 16x16 (UND/XOR wie GFX1); Hintergrund 16x16 Bytes ------------------------ */
static BYTE csv[16][16], c_on, c_h, c_w;
static WORD c_x, c_y;
static void crest(void)
{
    WORD r, k;
    if (!c_on) return;
    for (r = 0; r < c_h; r++) for (k = 0; k < c_w; k++) *va(c_x + k, c_y + r) = csv[r][k];
    c_on = 0;
}
static void RDEXPORT d_cursorhide(BOOL aus) { if (aus) crest(); }
static void RDEXPORT d_cursor(short x, short y, const BYTE FAR *mb)
{
    WORD r, k;
    crest();
    c_x = (WORD)x; c_y = (WORD)y;
    c_h = (BYTE)(H - y < 16 ? H - y : 16); c_w = (BYTE)(W - x < 16 ? W - x : 16);
    for (r = 0; r < c_h; r++)
        for (k = 0; k < c_w; k++) {
            BYTE __far *p = va(x + k, y + r); BYTE m = (BYTE)(0x80 >> (k & 7));
            csv[r][k] = *p;
            if (!(mb[2 * r + (k >> 3)] & m)) *p = 0;              /* UND 0 -> schwarz */
            if (mb[32 + 2 * r + (k >> 3)] & m) *p ^= 0x0F;        /* XOR 1 -> umkehren */
        }
    c_on = 1;
}

/* ---- Farben: RGB <-> Index ----------------------------------------------------------------- */
static COLOR std16(WORD i)
{
    BYTE v = (BYTE)((i & 8) ? 85 : 0), r = (BYTE)((i & 4) ? 170 + v : v), g = (BYTE)((i & 2) ? 170 + v : v), b = (BYTE)((i & 1) ? 170 + v : v);
    if ((i & 15) == 6) g = 85;
    return RGB(r, g, b);
}
static BYTE nah(BYTE a, BYTE b) { return (BYTE)((a > b ? a - b : b - a) <= 48); }
static WORD RDEXPORT d_farbe(COLOR c)
{
    BYTE r = (BYTE)c, g = (BYTE)(c >> 8), b = (BYTE)(c >> 16), m; WORD i; COLOR s;
    if (r == g && g == b) { int k = ((int)r - 3) / 10; if (!r) return 0; if (r == 255) return 15; if (k < 0) k = 0; if (k > 23) k = 23; return (WORD)(232 + k); }
    m = r > g ? r : g; if (b > m) m = b;          /* wie VGA16, wenn die Standardfarbe nahe genug ist */
    i = (r > 128 && g > 32 && g < 128 && b < 64) ? 6 : (WORD)((b > 64) | ((g > 64) << 1) | ((r > 64) << 2) | (m > 191 ? 8 : 0));
    s = std16(i);
    if (nah(r, (BYTE)s) && nah(g, (BYTE)(s >> 8)) && nah(b, (BYTE)(s >> 16))) return i;
    return (WORD)(16 + 36 * ((r + 25) / 51) + 6 * ((g + 25) / 51) + (b + 25) / 51);
}
static COLOR RDEXPORT d_rgb(WORD i)
{
    i &= 0xFF;
    if (i < 16) return std16(i);
    if (i >= 232) { BYTE v = (BYTE)(8 + (i - 232) * 10); return RGB(v, v, v); }
    i -= 16;
    return RGB((i / 36) * 51, (i / 6 % 6) * 51, (i % 6) * 51);
}

static GFXDRVF tab = { {
    { { 'R', 'D', 'R', 'V' }, DRV_GFX, 0x0100, sizeof(GFXDRVF), "VESA 640x480 256", d_init, d_done },
    d_modes, d_setmode, d_textmode,
    d_span, d_pixel, d_getpixel, d_put, d_get,
    0, 0, d_copy, d_glyphs, d_palette, d_cursor, d_cursorhide, 0, d_box }, d_farbe, d_rgb
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
