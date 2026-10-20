/* RDGDI.C - Grafik fuer RetroDesk 2: Geraetekontexte, Clipping mit Rechtecklisten,
 * Stifte, Pinsel mit Mustern und Dithering, Linien, Rechtecke, Ellipsen, Polygone,
 * Fuellen, Bitmaps im Speicher, BitBlt/StretchBlt mit Rasteroperationen.
 *
 * Systemmodul: Export 1 = Abschluss (zurueck in den Textmodus), Export 2.. =
 * API 0x100.. in Reihenfolge; dahinter GetDC/ReleaseDC fuer den ganzen Schirm
 * (bis RDUSER sie uebernimmt). Der Grafiktreiber wird beim ersten GetDC geladen.
 *
 * Alle Zeichenfunktionen rechnen intern in Geraetekoordinaten und schneiden jede
 * Spanne an der Clipliste ab. Treiber (und die Bitmap-Funktionen unten) bekommen
 * nur gueltige Koordinaten. */
#include <i86.h>
#include <string.h>
#include "RD2DRV.H"

#define MAXDC   24
#define MAXCLIP 16
#define MAXBW   1024                              /* breiteste Bitmap/Zeile in Pixeln */
#define RB      (MAXBW / 8 + 2)

typedef struct {
    BYTE  used, typ;                              /* typ 0 = Schirm, 1 = Bitmap */
    HBMP  bmp;
    HTASK owner;
    short ox, oy;
    WORD  nclip;
    RECT  clip[MAXCLIP];                          /* Geraeteclip (Fenster), Geraetekoordinaten */
    BYTE  uclip;
    RECT  ucl;                                    /* SetClip, logische Koordinaten */
    COLOR pen, brush, txt, bk;
    WORD  pat, rop, bkmode;
    HFONT font;
    short cx, cy;
} DCX;

typedef struct { WORD w, h, bpp, bpr; } BMPH;     /* Kopf eines Bitmap-Blocks, danach die Zeilen */

static DCX dcs[MAXDC + 1];                         /* HDC 1..MAXDC */
static GFXDRV FAR *drv;                            /* Schirmtreiber, 0 = noch keiner */
static HMOD drvmod;
static GFXMODE gm;
static WORD gfxtyp;

/* ---- aktuelles Zeichenziel ------------------------------------------------- */
static GFXDRV FAR *dv;                             /* Treiber oder Bitmap-Tabelle */
static BYTE FAR *mb; static WORD mbpr, mw, mh;     /* gesperrte Bitmap */
static RECT ec[MAXCLIP]; static WORD nec;          /* wirksame Clipliste */
static DCX *cd;
static BYTE penb, txtb, bkb, pat[8], *patp, brnone;

/* ---- Bitmap als Geraet (1 Bit pro Pixel, Bit 7 links) ----------------------- */
#define MP(x, y) (mb + (WORD)(y) * mbpr + ((WORD)(x) >> 3))
static void wr(BYTE FAR *p, BYTE m, BYTE v, WORD rop)
{
    switch (rop) {
    case ROP_COPY: *p = (BYTE)((*p & ~m) | (v & m)); break;
    case ROP_OR:   *p |= (BYTE)(v & m); break;
    case ROP_AND:  *p &= (BYTE)(v | ~m); break;
    case ROP_XOR:  *p ^= (BYTE)(v & m); break;
    case ROP_NOT:  *p ^= m; break;
    }
}
static void RDAPI m_span(short y, short x0, short x1, WORD farbe, const BYTE FAR *mu, WORD rop)
{
    BYTE FAR *p = MP(x0, y);
    WORD b0 = (WORD)x0 >> 3, b1 = (WORD)x1 >> 3, n;
    BYTE lm = (BYTE)(0xFF >> (x0 & 7)), rm = (BYTE)(0xFF << (7 - (x1 & 7)));
    BYTE v = mu ? mu[y & 7] : (BYTE)(farbe ? 0xFF : 0);
    if (b0 == b1) { wr(p, (BYTE)(lm & rm), v, rop); return; }
    wr(p++, lm, v, rop);
    for (n = b1 - b0 - 1; n; n--) wr(p++, 0xFF, v, rop);
    wr(p, rm, v, rop);
}
static void RDAPI m_pixel(short x, short y, WORD farbe, WORD rop)
{
    wr(MP(x, y), (BYTE)(0x80 >> (x & 7)), (BYTE)(farbe ? 0xFF : 0), rop);
}
static WORD RDAPI m_getpixel(short x, short y) { return (*MP(x, y) >> (7 - (x & 7))) & 1; }
static void RDAPI m_put(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes,
                        WORD srcbpp, WORD rop, WORD fg, WORD bg)
{
    WORD sh = x & 7, nb = (sh + w + 7) >> 3, sb = (w + 7) >> 3, i;
    BYTE lm = (BYTE)(0xFF >> sh), rm = (BYTE)(0xFF << (7 - ((x + w - 1) & 7)));
    BYTE vf = (BYTE)(fg ? 0xFF : 0), vb = (BYTE)(bg ? 0xFF : 0);
    (void)srcbpp;
    for (; h > 0; h--, y++, src += srcbytes) {
        BYTE FAR *p = MP(x, y);
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
static void RDAPI m_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    WORD sh = x & 7, sb = (w + 7) >> 3, i, last = (WORD)(((WORD)x + w - 1) >> 3);
    BYTE rm = (BYTE)(0xFF << ((8 - (w & 7)) & 7));
    for (; h > 0; h--, y++, dst += dstbytes) {
        BYTE FAR *p = MP(x, y);
        WORD b0 = (WORD)x >> 3;
        for (i = 0; i < sb; i++)
            dst[i] = (BYTE)((p[i] << sh) | (sh && b0 + i + 1 <= last ? p[i + 1] >> (8 - sh) : 0));
        dst[sb - 1] &= rm;
    }
}
static GFXDRV mtab = { { { 'R', 'D', 'R', 'V' }, DRV_GFX, 0x0100, sizeof(GFXDRV), "Bitmap", 0, 0 },
                       0, 0, 0, m_span, m_pixel, m_getpixel, m_put, m_get };

/* ---- Farben ------------------------------------------------------------------ */
static COLOR syscol(COLOR c)
{
    if ((c >> 24) != 0xFF) return c & 0xFFFFFFUL;
    switch ((WORD)c) {
    case 1: return C_WHITE;                         /* C_FENSTER */
    case 5: return C_GRAY;                          /* C_DESKTOP */
    default: return C_BLACK;                        /* Text, markiert, Titel */
    }
}
static WORD lum(COLOR c)
{
    c = syscol(c);
    return (WORD)(((WORD)(BYTE)c * 30U + (WORD)(BYTE)(c >> 8) * 59U + (WORD)(BYTE)(c >> 16) * 11U) / 100U);
}
static BYTE cbit(COLOR c) { return (BYTE)(lum(c) >= 128); }

static const BYTE bayer[8][8] = {
    { 0, 32,  8, 40,  2, 34, 10, 42 }, { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 }, { 60, 28, 52, 20, 62, 30, 54, 22 },
    { 3, 35, 11, 43,  1, 33,  9, 41 }, { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 }, { 63, 31, 55, 23, 61, 29, 53, 21 } };
static const BYTE mus[5][8] = {
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 },   /* 50 % */
    { 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22 },   /* 25 % */
    { 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD },   /* 75 % */
    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } }; /* schraeg */

/* Pinsel als 8x8-Muster (1 = weiss). patp = 0: einfarbig (Farbe in pat[0] & 1) */
static void mkbrush(DCX *d)
{
    WORD y, x, lv;
    brnone = (BYTE)(d->pat == PAT_NONE);
    if (d->pat == PAT_SOLID || d->pat > PAT_HATCH) {
        lv = (WORD)((lum(d->brush) * 65U + 128U) >> 8);   /* 0..64 */
        if (lv == 0 || lv >= 64) { pat[0] = (BYTE)(lv ? 1 : 0); patp = 0; return; }
        for (y = 0; y < 8; y++) {
            BYTE b = 0;
            for (x = 0; x < 8; x++) if (bayer[y][x] < lv) b |= (BYTE)(0x80 >> x);
            pat[y] = b;
        }
    } else {
        BYTE ink = (BYTE)(cbit(d->brush) ? 0xFF : 0), bg = (BYTE)(cbit(d->bk) ? 0xFF : 0);
        for (y = 0; y < 8; y++) pat[y] = (BYTE)((mus[d->pat][y] & ink) | (~mus[d->pat][y] & bg));
    }
    patp = pat;
}

/* ---- Auswahl des Ziels -------------------------------------------------------- */
static DCX *getdc(HDC h)
{
    if (h < 1 || h > MAXDC || !dcs[h].used) { SetLastError(RD_ERR_HANDLE); return 0; }
    return &dcs[h];
}
static void isect(RECT *a, const RECT *b)
{
    if (b->l > a->l) a->l = b->l;
    if (b->t > a->t) a->t = b->t;
    if (b->r < a->r) a->r = b->r;
    if (b->b < a->b) a->b = b->b;
}
/* Zeichenziel waehlen: Treiber/Bitmap, Clipliste berechnen. 0 = nichts zu zeichnen */
static int sel(DCX *d)
{
    static RECT dev, u, r; WORD i;
    cd = d; nec = 0;
    if (d->typ == 0) {
        if (!drv) return 0;
        dv = drv; dev.l = 0; dev.t = 0; dev.r = (short)gm.w; dev.b = (short)gm.h;
    } else {
        BMPH FAR *bh = (BMPH FAR *)MemLock(d->bmp);
        if (!bh) return 0;
        dv = (GFXDRV FAR *)&mtab; mb = (BYTE FAR *)(bh + 1); mbpr = bh->bpr; mw = bh->w; mh = bh->h;
        dev.l = 0; dev.t = 0; dev.r = (short)mw; dev.b = (short)mh;
    }
    if (d->uclip) { u = d->ucl; u.l += d->ox; u.r += d->ox; u.t += d->oy; u.b += d->oy; isect(&dev, &u); }
    if (!d->nclip) { if (dev.l < dev.r && dev.t < dev.b) ec[nec++] = dev; }
    else for (i = 0; i < d->nclip; i++) {
        r = d->clip[i];
        isect(&r, &dev);
        if (r.l < r.r && r.t < r.b) ec[nec++] = r;
    }
    penb = cbit(d->pen); txtb = cbit(d->txt); bkb = cbit(d->bk);
    mkbrush(d);
    return 1;
}
static void unsel(void) { if (cd && cd->typ == 1) MemUnlock(cd->bmp); cd = 0; }

/* ---- geclippte Grundfunktionen (Geraetekoordinaten) ---------------------------- */
static void hs(short y, short x0, short x1, int brush, WORD rop)   /* x0..x1 inklusive */
{
    WORD i;
    if (x0 > x1) { short t = x0; x0 = x1; x1 = t; }
    for (i = 0; i < nec; i++) {
        RECT *r = &ec[i]; short a = x0, b = x1;
        if (y < r->t || y >= r->b) continue;
        if (a < r->l) a = r->l;
        if (b >= r->r) b = (short)(r->r - 1);
        if (a > b) continue;
        /* nahe Null wird beim Umwandeln zu DS:0000, deshalb ausdruecklich ferne Null */
        if (brush && patp) dv->span(y, a, b, 0, (const BYTE FAR *)patp, rop);
        else dv->span(y, a, b, brush ? pat[0] : penb, (const BYTE FAR *)0, rop);
    }
}
static void px(short x, short y, BYTE farbe, WORD rop)
{
    WORD i;
    for (i = 0; i < nec; i++)
        if (x >= ec[i].l && x < ec[i].r && y >= ec[i].t && y < ec[i].b) { dv->pixel(x, y, farbe, rop); return; }
}
static void vs(short x, short y0, short y1, WORD rop)
{
    WORD i;
    if (y0 > y1) { short t = y0; y0 = y1; y1 = t; }
    for (i = 0; i < nec; i++) {
        RECT *r = &ec[i]; short a = y0, b = y1;
        if (x < r->l || x >= r->r) continue;
        if (a < r->t) a = r->t;
        if (b >= r->b) b = (short)(r->b - 1);
        if (a > b) continue;
        if (dv->vline) dv->vline(x, a, b, penb, rop);
        else for (; a <= b; a++) dv->pixel(x, a, penb, rop);
    }
}
/* eine Zeile 1-Bit-Daten (Bit 0 = x) geclippt ausgeben */
static BYTE sbuf[RB];
static void crow(short y, short x, short w, const BYTE *buf, WORD rop, BYTE fg, BYTE bg)
{
    WORD i, k, o, n;
    for (i = 0; i < nec; i++) {
        RECT *r = &ec[i]; short a = x, b = (short)(x + w);
        const BYTE *s = buf;
        if (y < r->t || y >= r->b) continue;
        if (a < r->l) a = r->l;
        if (b > r->r) b = r->r;
        if (a >= b) continue;
        o = (WORD)(a - x);
        if (o) {                                  /* ab Bit o nach vorn schieben */
            WORD sh = o & 7, q = o >> 3;
            n = ((WORD)(b - a) + 7) >> 3;
            for (k = 0; k < n; k++) sbuf[k] = (BYTE)((buf[q + k] << sh) | (sh ? buf[q + k + 1] >> (8 - sh) : 0));
            s = sbuf;
        }
        dv->put(a, y, (short)(b - a), 1, (const BYTE FAR *)s, RB, 1, rop, fg, bg);
    }
}

static void line(short x0, short y0, short x1, short y1)
{
    short dx, dy, sx, sy, e, e2;
    WORD rop = cd->rop;
    if (y0 == y1) { hs(y0, x0, x1, 0, rop); return; }
    if (x0 == x1) { vs(x0, y0, y1, rop); return; }
    dx = (short)(x1 > x0 ? x1 - x0 : x0 - x1); sx = (short)(x0 < x1 ? 1 : -1);
    dy = (short)(y1 > y0 ? y0 - y1 : y1 - y0); sy = (short)(y0 < y1 ? 1 : -1);
    e = (short)(dx + dy);
    for (;;) {
        px(x0, y0, penb, rop);
        if (x0 == x1 && y0 == y1) break;
        e2 = (short)(2 * e);
        if (e2 >= dy) { e = (short)(e + dy); x0 = (short)(x0 + sx); }
        if (e2 <= dx) { e = (short)(e + dx); y0 = (short)(y0 + sy); }
    }
}

static WORD isqrt(DWORD v)
{
    DWORD r = 0, b = 1UL << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return (WORD)r;
}

/* ---- API: Einstellungen ------------------------------------------------------- */
short RDEXPORT g_GetCaps(HDC h, WORD i)
{
    DCX *d = getdc(h);
    if (!d) return 0;
    if (d->typ == 1) {
        BMPH FAR *bh = (BMPH FAR *)MemLock(d->bmp); short v = 0;
        if (!bh) return 0;
        if (i == CAPS_W) v = (short)bh->w; else if (i == CAPS_H) v = (short)bh->h;
        else if (i == CAPS_BPP) v = 1; else if (i == CAPS_FARBEN) v = 2;
        else if (i == CAPS_ASPX || i == CAPS_ASPY) v = 100; else if (i == CAPS_TECH) v = 1;
        MemUnlock(d->bmp);
        return v;
    }
    switch (i) {
    case CAPS_W: return (short)gm.w;
    case CAPS_H: return (short)gm.h;
    case CAPS_BPP: return (short)gm.bpp;
    case CAPS_FARBEN: return (short)gm.farben;
    case CAPS_ASPX: return (short)gm.aspx;
    case CAPS_ASPY: return (short)gm.aspy;
    case CAPS_DPI: return 72;
    case CAPS_TEXTH: return (short)(gm.h >= 400 ? 16 : gm.h >= 340 ? 14 : 8);
    default: return 0;
    }
}
COLOR RDEXPORT g_SetPen(HDC h, COLOR c)        { DCX *d = getdc(h); COLOR o; if (!d) return 0; o = d->pen; d->pen = c; return o; }
COLOR RDEXPORT g_SetBrush(HDC h, COLOR c, WORD m) { DCX *d = getdc(h); COLOR o; if (!d) return 0; o = d->brush; d->brush = c; d->pat = m; return o; }
WORD  RDEXPORT g_SetRop(HDC h, WORD r)         { DCX *d = getdc(h); WORD o; if (!d) return 0; o = d->rop; d->rop = r > ROP_NOT ? ROP_COPY : r; return o; }
HFONT RDEXPORT g_SetFont(HDC h, HFONT f)       { DCX *d = getdc(h); HFONT o; if (!d) return 0; o = d->font; d->font = f; return o; }
COLOR RDEXPORT g_SetTextColor(HDC h, COLOR c)  { DCX *d = getdc(h); COLOR o; if (!d) return 0; o = d->txt; d->txt = c; return o; }
COLOR RDEXPORT g_SetBkColor(HDC h, COLOR c)    { DCX *d = getdc(h); COLOR o; if (!d) return 0; o = d->bk; d->bk = c; return o; }
WORD  RDEXPORT g_SetBkMode(HDC h, WORD m)      { DCX *d = getdc(h); WORD o; if (!d) return 0; o = d->bkmode; d->bkmode = m; return o; }
void  RDEXPORT g_SetOrigin(HDC h, short x, short y) { DCX *d = getdc(h); if (d) { d->ox = x; d->oy = y; } }
void  RDEXPORT g_SetClip(HDC h, const RECT FAR *r)
{
    DCX *d = getdc(h);
    if (!d) return;
    if (r) { d->ucl = *r; d->uclip = 1; } else d->uclip = 0;
}

/* ---- API: Zeichnen -------------------------------------------------------------- */
void RDEXPORT g_Pixel(HDC h, short x, short y, COLOR c)
{
    DCX *d = getdc(h);
    if (!d || !sel(d)) return;
    px((short)(x + d->ox), (short)(y + d->oy), cbit(c), d->rop);
    unsel();
}
COLOR RDEXPORT g_GetPixel(HDC h, short x, short y)
{
    DCX *d = getdc(h); COLOR c = 0xFFFFFFFFUL; WORD i;
    if (!d || !sel(d)) return c;
    x = (short)(x + d->ox); y = (short)(y + d->oy);
    for (i = 0; i < nec; i++)
        if (x >= ec[i].l && x < ec[i].r && y >= ec[i].t && y < ec[i].b) { c = dv->getpixel(x, y) ? C_WHITE : C_BLACK; break; }
    unsel();
    return c;
}
void RDEXPORT g_MoveTo(HDC h, short x, short y) { DCX *d = getdc(h); if (d) { d->cx = x; d->cy = y; } }
void RDEXPORT g_Line(HDC h, short x0, short y0, short x1, short y1)
{
    DCX *d = getdc(h);
    if (!d) return;
    d->cx = x1; d->cy = y1;
    if (!sel(d)) return;
    line((short)(x0 + d->ox), (short)(y0 + d->oy), (short)(x1 + d->ox), (short)(y1 + d->oy));
    unsel();
}
void RDEXPORT g_LineTo(HDC h, short x, short y)
{
    DCX *d = getdc(h);
    if (d) g_Line(h, d->cx, d->cy, x, y);
}
static void frame(short l, short t, short r, short b)   /* Geraetekoordinaten, r/b exklusiv */
{
    WORD rop = cd->rop;
    if (l >= r || t >= b) return;
    hs(t, l, (short)(r - 1), 0, rop);
    if (b - 1 > t) hs((short)(b - 1), l, (short)(r - 1), 0, rop);
    if (b - t > 2) { vs(l, (short)(t + 1), (short)(b - 2), rop); if (r - 1 > l) vs((short)(r - 1), (short)(t + 1), (short)(b - 2), rop); }
}
void RDEXPORT g_Frame(HDC h, const RECT FAR *r)
{
    DCX *d = getdc(h);
    if (!d || !r || !sel(d)) return;
    frame((short)(r->l + d->ox), (short)(r->t + d->oy), (short)(r->r + d->ox), (short)(r->b + d->oy));
    unsel();
}
static void fill(const RECT FAR *r, int inv)
{
    short y, l = (short)(r->l + cd->ox), rr = (short)(r->r + cd->ox - 1);
    if (!inv && brnone) return;
    for (y = (short)(r->t + cd->oy); y < r->b + cd->oy; y++) hs(y, l, rr, !inv, inv ? ROP_NOT : cd->rop);
}
void RDEXPORT g_FillRect(HDC h, const RECT FAR *r)
{
    DCX *d = getdc(h);
    if (!d || !r || r->l >= r->r || !sel(d)) return;
    fill(r, 0);
    unsel();
}
void RDEXPORT g_InvertRect(HDC h, const RECT FAR *r)
{
    DCX *d = getdc(h);
    if (!d || !r || r->l >= r->r || !sel(d)) return;
    fill(r, 1);
    unsel();
}

/* Ellipse im Rechteck l..r-1, t..b-1: halbe Breite je Zeile in doppelten Einheiten */
static short ex0(short y, short l, short r, short t, short b)
{
    long RX = r - l - 1, RY = b - t - 1, DY = 2L * y - (t + b - 1);
    long hx;
    if (RY <= 0) return l;
    if (DY < 0) DY = -DY;
    if (DY > RY) return 0x7FFF;
    hx = RX * (long)isqrt((DWORD)(RY * RY - DY * DY)) / RY;
    return (short)((l + r - 1 - hx + 1) >> 1);
}
void RDEXPORT g_Ellipse(HDC h, const RECT FAR *rp, WORD fl)
{
    DCX *d = getdc(h); short l, t, r, b, y, x0, x1, n0, cx2;
    if (!d || !rp || !sel(d)) return;
    l = (short)(rp->l + d->ox); r = (short)(rp->r + d->ox); t = (short)(rp->t + d->oy); b = (short)(rp->b + d->oy);
    if (l >= r || t >= b) { unsel(); return; }
    cx2 = (short)(l + r - 1);
    for (y = t; y < b; y++) {
        x0 = ex0(y, l, r, t, b); x1 = (short)(cx2 - x0);
        if (x0 > x1) continue;
        if (fl && !brnone) hs(y, x0, x1, 1, d->rop);
        if (y == t || y == b - 1) { hs(y, x0, x1, 0, d->rop); continue; }
        n0 = (short)(ex0((short)(2 * y < t + b - 1 ? y + 1 : y - 1), l, r, t, b) - 1);   /* Nachbarzeile zur Mitte hin */
        if (n0 < x0) n0 = x0;
        hs(y, x0, n0, 0, d->rop);
        hs(y, (short)(cx2 - n0), x1, 0, d->rop);
    }
    unsel();
}

void RDEXPORT g_RoundRect(HDC h, const RECT FAR *rp, WORD rad, WORD fl)
{
    DCX *d = getdc(h); short l, t, r, b, y, i, in, nin;
    if (!d || !rp || !sel(d)) return;
    l = (short)(rp->l + d->ox); r = (short)(rp->r + d->ox); t = (short)(rp->t + d->oy); b = (short)(rp->b + d->oy);
    if (l >= r || t >= b) { unsel(); return; }
    if ((short)rad * 2 > r - l) rad = (WORD)((r - l) / 2);
    if ((short)rad * 2 > b - t) rad = (WORD)((b - t) / 2);
    for (y = t; y < b; y++) {
        i = (short)(y - t < b - 1 - y ? y - t : b - 1 - y);          /* Abstand zum naechsten Rand */
        #define INSET(k) ((k) >= (short)rad ? 0 : (short)rad - (short)((isqrt(4UL * rad * rad - (DWORD)(2L * (rad - (k)) - 1) * (2L * (rad - (k)) - 1)) + 1) >> 1))
        in = INSET(i);
        if (fl && !brnone) hs(y, (short)(l + in), (short)(r - 1 - in), 1, d->rop);
        if (i == 0) { hs(y, (short)(l + in), (short)(r - 1 - in), 0, d->rop); continue; }
        nin = INSET(i - 1);                                          /* Nachbar zum Rand hin */
        nin = (short)(nin > in ? nin - 1 : in);
        hs(y, (short)(l + in), (short)(l + nin), 0, d->rop);
        hs(y, (short)(r - 1 - nin), (short)(r - 1 - in), 0, d->rop);
        #undef INSET
    }
    unsel();
}

#define MAXPX 32
void RDEXPORT g_Polygon(HDC h, const POINT FAR *p, WORD n, WORD fl)
{
    DCX *d = getdc(h); WORD i, j, k; short y, ymin, ymax, xs[MAXPX];
    if (!d || !p || n < 2 || !sel(d)) return;
    ymin = ymax = p[0].y;
    for (i = 1; i < n; i++) { if (p[i].y < ymin) ymin = p[i].y; if (p[i].y > ymax) ymax = p[i].y; }
    if (fl && !brnone && n > 2)
        for (y = ymin; y <= ymax; y++) {
            for (i = 0, k = 0; i < n && k < MAXPX; i++) {
                const POINT FAR *a = &p[i], FAR *c = &p[(i + 1) % n];
                if ((a->y <= y && c->y > y) || (c->y <= y && a->y > y)) {
                    short x = (short)(a->x + (long)(y - a->y) * (c->x - a->x) / (c->y - a->y));
                    for (j = k++; j > 0 && xs[j - 1] > x; j--) xs[j] = xs[j - 1];   /* einsortieren */
                    xs[j] = x;
                }
            }
            for (i = 0; i + 1 < k; i += 2)
                hs((short)(y + d->oy), (short)(xs[i] + d->ox), (short)(xs[i + 1] + d->ox), 1, d->rop);
        }
    for (i = 0; i < n; i++) {
        WORD c = (i + 1) % n;
        if (c == 0 && n == 2) break;
        line((short)(p[i].x + d->ox), (short)(p[i].y + d->oy), (short)(p[c].x + d->ox), (short)(p[c].y + d->oy));
    }
    unsel();
}

/* Fuellen ab x,y: Bereich gleicher Farbe; besuchte Punkte merkt eine Bitkarte */
void RDEXPORT g_FloodFill(HDC h, short x, short y)
{
    DCX *d = getdc(h); RECT bb; WORD i, bw, bh, tgt, sp = 0; HMEM hm; BYTE FAR *vis; short FAR *st;
    #define STK 1024
    if (!d || !sel(d) || !nec) { if (d) unsel(); return; }
    bb = ec[0];
    for (i = 1; i < nec; i++) {
        if (ec[i].l < bb.l) bb.l = ec[i].l; if (ec[i].t < bb.t) bb.t = ec[i].t;
        if (ec[i].r > bb.r) bb.r = ec[i].r; if (ec[i].b > bb.b) bb.b = ec[i].b;
    }
    x = (short)(x + d->ox); y = (short)(y + d->oy);
    if (x < bb.l || x >= bb.r || y < bb.t || y >= bb.b || brnone) { unsel(); return; }
    bw = (WORD)(bb.r - bb.l); bh = (WORD)(bb.b - bb.t);
    hm = MemAlloc((DWORD)((bw + 7) >> 3) * bh + STK * 4, MEM_ZEROINIT);
    if (!hm) { unsel(); return; }
    vis = (BYTE FAR *)MemLock(hm); st = (short FAR *)(vis + (WORD)(((bw + 7) >> 3) * bh));
    if (cd->typ == 1) { BMPH FAR *b2 = (BMPH FAR *)MemLock(cd->bmp); mb = (BYTE FAR *)(b2 + 1); }   /* nach MemAlloc neu */
    tgt = dv->getpixel(x, y);
    #define VB(xx, yy) (vis + (WORD)((yy) - bb.t) * ((bw + 7) >> 3) + ((WORD)((xx) - bb.l) >> 3))
    #define VM(xx) ((BYTE)(0x80 >> (((xx) - bb.l) & 7)))
    #define IN(xx, yy) (!(*VB(xx, yy) & VM(xx)) && dv->getpixel(xx, yy) == tgt)
    st[sp++] = x; st[sp++] = y;
    while (sp) {
        short a, b, yy, k;
        y = st[--sp]; x = st[--sp];
        if (!IN(x, y)) continue;
        for (a = x; a > bb.l && IN(a - 1, y); a--) ;
        for (b = x; b < bb.r - 1 && IN(b + 1, y); b++) ;
        for (k = a; k <= b; k++) *VB(k, y) |= VM(k);
        hs(y, a, b, 1, d->rop);
        for (yy = (short)(y - 1); yy <= y + 1; yy += 2) {
            int run = 0;
            if (yy < bb.t || yy >= bb.b) continue;
            for (k = a; k <= b; k++) {
                if (IN(k, yy)) { if (!run && sp < STK * 2 - 2) { st[sp++] = k; st[sp++] = yy; } run = 1; }
                else run = 0;
            }
        }
    }
    #undef IN
    #undef VB
    #undef VM
    if (cd->typ == 1) MemUnlock(cd->bmp);
    MemUnlock(hm); MemFree(hm);
    unsel();
}

/* ---- Bitmaps ------------------------------------------------------------------ */
HBMP RDEXPORT g_BmpCreate(WORD w, WORD h, WORD bpp)
{
    WORD bpr = (w + 7) >> 3; HMEM m; BMPH FAR *bh;
    (void)bpp;                                    /* Phase 3: alle Geraete haben 1 Bit */
    if (!w || !h || w > MAXBW || (DWORD)bpr * h > 65000UL) { SetLastError(RD_ERR_FULL); return 0; }
    m = MemAlloc((DWORD)bpr * h + sizeof(BMPH), MEM_MOVABLE | MEM_ZEROINIT);
    if (!m) return 0;
    bh = (BMPH FAR *)MemLock(m);
    bh->w = w; bh->h = h; bh->bpp = 1; bh->bpr = bpr;
    MemUnlock(m);
    return m;
}
static HDC newdc(BYTE typ, HBMP b)
{
    WORD i; DCX *d;
    for (i = 1; i <= MAXDC && dcs[i].used; i++) ;
    if (i > MAXDC) { SetLastError(RD_ERR_FULL); return 0; }
    d = &dcs[i];
    memset(d, 0, sizeof *d);
    d->used = 1; d->typ = typ; d->bmp = b; d->owner = TaskSelf();
    d->pen = C_BLACK; d->brush = C_WHITE; d->txt = C_BLACK; d->bk = C_WHITE; d->bkmode = BK_OPAQUE;
    return i;
}
void RDEXPORT g_BmpFree(HBMP b)
{
    WORD i;
    for (i = 1; i <= MAXDC; i++) if (dcs[i].used && dcs[i].typ == 1 && dcs[i].bmp == b) dcs[i].used = 0;
    MemFree(b);
}
HDC  RDEXPORT g_BmpDC(HBMP b) { if (!MemSize(b)) { SetLastError(RD_ERR_HANDLE); return 0; } return newdc(1, b); }
void RDEXPORT g_DCFree(HDC h) { DCX *d = getdc(h); if (d) d->used = 0; }

/* Zeilenweise kopieren: Quelle lesen (ihr Geraet), Ziel geclippt schreiben */
static BYTE rbuf[RB];
typedef struct { GFXDRV FAR *dv; BYTE FAR *mb; WORD bpr, w, h; } ZIEL;
static void zset(const ZIEL *z) { dv = z->dv; mb = z->mb; mbpr = z->bpr; }
static int zget(DCX *d, ZIEL *z)
{
    if (d->typ == 0) { if (!drv) return 0; z->dv = drv; z->w = gm.w; z->h = gm.h; z->mb = 0; z->bpr = 0; return 1; }
    { BMPH FAR *bh = (BMPH FAR *)MemLock(d->bmp);
      if (!bh) return 0;
      z->dv = (GFXDRV FAR *)&mtab; z->mb = (BYTE FAR *)(bh + 1); z->bpr = bh->bpr; z->w = bh->w; z->h = bh->h; return 1; }
}
static void blitrows(DCX *dd, short x, short y, short w, short h, DCX *sd, short sx, short sy,
                     WORD rop, WORD sw, WORD shh, WORD dw, WORD dh)
{
    static ZIEL zs, zd; short i, n, ys, yd, step;
    if (!zget(sd, &zs)) return;
    if (!sel(dd)) { if (sd->typ == 1) MemUnlock(sd->bmp); return; }
    zd.dv = dv; zd.mb = mb; zd.bpr = mbpr;
    if (!sw) {                                     /* BitBlt: Quelle an ihren Grenzen abschneiden */
        if (sx < 0) { x = (short)(x - sx); w = (short)(w + sx); sx = 0; }
        if (sy < 0) { y = (short)(y - sy); h = (short)(h + sy); sy = 0; }
        if (sx + w > (short)zs.w) w = (short)(zs.w - sx);
        if (sy + h > (short)zs.h) h = (short)(zs.h - sy);
        if (w > MAXBW) w = MAXBW;
        n = h;
    } else { n = (short)dh; w = (short)dw; }
    if (w <= 0 || n <= 0) goto aus;
    step = 1; i = 0;
    if (!sw && sd == dd && y > sy) { step = -1; i = (short)(n - 1); }   /* ueberlappend: von unten */
    for (; i >= 0 && i < n; i = (short)(i + step)) {
        if (!sw) { ys = (short)(sy + i); yd = (short)(y + i); }
        else { ys = (short)(sy + (long)i * shh / dh); yd = (short)(y + i); if (ys >= (short)zs.h || ys < 0) continue; }
        zset(&zs);
        if (!sw) zs.dv->get(sx, ys, w, 1, (BYTE FAR *)rbuf, RB);
        else {                                     /* StretchBlt: Spalten waehlen */
            static BYTE q[RB];
            short sxx = sx, swd = (short)sw; WORD c;
            if (sxx < 0) sxx = 0;
            if (sxx + swd > (short)zs.w) swd = (short)(zs.w - sxx);
            if (swd <= 0) continue;
            zs.dv->get(sxx, ys, swd, 1, (BYTE FAR *)q, RB);
            memset(rbuf, 0, RB);
            for (c = 0; c < dw && c < MAXBW; c++) {
                WORD s = (WORD)((DWORD)c * sw / dw);
                if (s < (WORD)swd && (q[s >> 3] & (0x80 >> (s & 7)))) rbuf[c >> 3] |= (BYTE)(0x80 >> (c & 7));
            }
            w = (short)(dw > MAXBW ? MAXBW : dw);
        }
        zset(&zd);
        if (rop == ROP_MASK) crow(yd, x, w, rbuf, ROP_MASK, txtb, 0);
        else crow(yd, x, w, rbuf, rop, 1, 0);
    }
aus:
    if (sd->typ == 1) MemUnlock(sd->bmp);
    unsel();
}
void RDEXPORT g_BitBlt(HDC d, short x, short y, short w, short h, HDC s, short sx, short sy, WORD rop)
{
    DCX *dd = getdc(d), *sd = getdc(s);
    if (!dd || !sd || w <= 0 || h <= 0) return;
    blitrows(dd, (short)(x + dd->ox), (short)(y + dd->oy), w, h, sd, (short)(sx + sd->ox), (short)(sy + sd->oy), rop, 0, 0, 0, 0);
}
void RDEXPORT g_StretchBlt(HDC d, const RECT FAR *dr, HDC s, const RECT FAR *sr, WORD rop)
{
    DCX *dd = getdc(d), *sd = getdc(s);
    if (!dd || !sd || !dr || !sr || dr->r <= dr->l || dr->b <= dr->t || sr->r <= sr->l || sr->b <= sr->t) return;
    blitrows(dd, (short)(dr->l + dd->ox), (short)(dr->t + dd->oy), 0, 0, sd, (short)(sr->l + sd->ox), (short)(sr->t + sd->oy),
             rop, (WORD)(sr->r - sr->l), (WORD)(sr->b - sr->t), (WORD)(dr->r - dr->l), (WORD)(dr->b - dr->t));
}
COLOR RDEXPORT g_ColorNearest(HDC h, COLOR c) { (void)h; return cbit(c) ? C_WHITE : C_BLACK; }
WORD  RDEXPORT g_PaletteSet(HDC h, WORD f, WORD n, const COLOR FAR *c) { (void)h; (void)f; (void)n; (void)c; SetLastError(RD_ERR_NOTSUP); return 0; }

/* ---- Schirm -------------------------------------------------------------------- */
static WORD trydrv(const char *name)
{
    GFXDRV FAR *t;
    FARPROC f;
    if (!(drvmod = ModLoad(name))) { DbgHex("RDGDI: Treiber fehlt", LastError()); return RD_ERR_NOTFOUND; }
    f = ModProc(drvmod, 1);
    t = f ? (GFXDRV FAR *)((void FAR * (RDAPI *)(void))f)() : 0;
    if (!t || t->h.typ != DRV_GFX || t->h.init(0)) { DbgHex("RDGDI: Treiber abgelehnt", (DWORD)(t != 0)); ModFree(drvmod); drvmod = 0; return RD_ERR_NOTSUP; }
    t->modes(&gm, 1);
    t->setmode(0);
    drv = t;
    return 0;
}
static int bildschirm(void)
{
    static char wahl[8]; static SYSINFO si;
    if (drv) return 1;
    KernInfo(&si);
    if (si.grafik == GFX_CGA) wahl[0] = 'C'; else if (si.grafik == GFX_HGC) wahl[0] = 'H';
    else if (si.grafik == GFX_VGAMONO) wahl[0] = 'V';
    else ProfileGetStr("RD2.INI", "System", "Grafik", "AUTO", wahl, sizeof wahl);
    if ((wahl[0] | 0x20) == 'h' && !trydrv("HGC.DRV")) gfxtyp = GFX_HGC;
    else if ((wahl[0] | 0x20) == 'v' && !trydrv("VGAM.DRV")) gfxtyp = GFX_VGAMONO;
    else if ((wahl[0] | 0x20) == 'c' && !trydrv("CGA.DRV")) gfxtyp = GFX_CGA;
    else if (!trydrv("HGC.DRV")) gfxtyp = GFX_HGC;      /* AUTO: Hercules, VGA mono, CGA */
    else if (!trydrv("VGAM.DRV")) gfxtyp = GFX_VGAMONO;
    else if (!trydrv("CGA.DRV")) gfxtyp = GFX_CGA;
    return drv != 0;
}
HDC RDEXPORT g_GetDC(HWND w)
{
    (void)w;
    if (!bildschirm()) { SetLastError(RD_ERR_NOTSUP); return 0; }
    return newdc(0, 0);
}
void RDEXPORT g_ReleaseDC(HWND w, HDC h) { (void)w; g_DCFree(h); }

/* Abschluss: Kern ruft das beim Beenden von RD */
void RDEXPORT g_Ende(void)
{
    if (drv) { drv->textmode(); drv = 0; }
    if (drvmod) { ModFree(drvmod); drvmod = 0; }
}

/* Exporte: 1 = Ende, 2 + (API - 0x100), danach GetDC, ReleaseDC */
#define N 0x3B
static struct { WORD n; FARPROC f[N + 3]; } ex;
void FAR * RDAPI RdModInit(void)
{
    #define E(nr, fn) ex.f[(nr) - 0x100 + 1] = (FARPROC)fn
    ex.n = N + 3;
    ex.f[0] = (FARPROC)g_Ende;
    E(0x100, g_GetCaps); E(0x101, g_SetPen); E(0x102, g_SetBrush); E(0x103, g_SetRop); E(0x104, g_SetFont);
    E(0x105, g_SetTextColor); E(0x106, g_SetBkColor); E(0x107, g_SetBkMode); E(0x108, g_SetOrigin); E(0x109, g_SetClip);
    E(0x110, g_Pixel); E(0x111, g_GetPixel); E(0x112, g_MoveTo); E(0x113, g_LineTo); E(0x114, g_Line);
    E(0x115, g_Frame); E(0x116, g_FillRect); E(0x117, g_InvertRect); E(0x118, g_Ellipse); E(0x119, g_Polygon);
    E(0x11A, g_FloodFill); E(0x11B, g_RoundRect);
    E(0x130, g_BmpCreate); E(0x131, g_BmpFree); E(0x132, g_BmpDC); E(0x133, g_DCFree); E(0x134, g_BitBlt);
    E(0x135, g_StretchBlt); E(0x139, g_ColorNearest); E(0x13A, g_PaletteSet);
    ex.f[N + 1] = (FARPROC)g_GetDC; ex.f[N + 2] = (FARPROC)g_ReleaseDC;
    return &ex;
}
