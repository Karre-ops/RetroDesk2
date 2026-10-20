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

/* ---- Mauszeiger -----------------------------------------------------------------
 * INT 33h (MOUSE.COM, CTMOUSE): Bereich = Bildschirm, eigener Ereignis-Handler. Der Zeiger
 * wird im Handler (Interrupt) bewegt, ausser RDGDI zeichnet gerade (gbusy): dann am Ende
 * des Zeichenaufrufs. Zeichenaufrufe, die das Zeigerrechteck beruehren, nehmen ihn vorher
 * weg (ex). Ereignisse sammelt ein Ring, der Kern holt sie mit g_MausHole ab. */
static const BYTE cand[32] = { 0x7F,0xFF,0x3F,0xFF,0x1F,0xFF,0x0F,0xFF,0x07,0xFF,0x03,0xFF,0x01,0xFF,0x00,0xFF,0x00,0x7F,0x00,0x3F,0x01,0xFF,0x10,0xFF,0x30,0xFF,0x78,0x7F,0xF8,0x7F,0xFC,0xFF };
static const BYTE cxor[32] = { 0x00,0x00,0x00,0x00,0x40,0x00,0x60,0x00,0x70,0x00,0x78,0x00,0x7C,0x00,0x7E,0x00,0x7F,0x00,0x7C,0x00,0x6C,0x00,0x46,0x00,0x06,0x00,0x03,0x00,0x03,0x00,0x00,0x00 };
static SYSINFO si;
static BYTE maus, con, cvis;
static volatile BYTE gbusy, moved;
static volatile short mx, my; static volatile WORD mbt;
static short zx, zy, zw, zh;                       /* gezeichnete Lage */
static BYTE csave[32];
typedef struct { BYTE art, taste; short x, y; } MEV;
static volatile MEV mq[16]; static volatile BYTE mqk, mqs;

WORD m33(WORD ax, WORD bx, WORD cx, WORD dx);
#pragma aux m33 = "int 33h" parm [ax] [bx] [cx] [dx] value [ax] modify [bx cx dx si di es]
WORD m33bx(WORD ax);
#pragma aux m33bx = "xor bx,bx" "int 33h" "cmp ax,0FFFFh" "je m1" "xor bx,bx" "m1:" parm [ax] value [bx] modify [ax cx dx si di es]
void m33h(WORD ax, WORD cx, void (__far *f)(void));
#pragma aux m33h = "int 33h" parm [ax] [cx] [es dx] modify [ax bx cx dx si di es]
void cli(void);
#pragma aux cli = "cli"
void sti(void);
#pragma aux sti = "sti"

static void zweg(void)                            /* Zeiger entfernen */
{
    if (!con) return;
    drv->put(zx, zy, zw, zh, (const BYTE FAR *)csave, 2, 1, ROP_COPY, 1, 0);
    con = 0;
}
static void zhin(void)                            /* Zeiger an (mx, my) zeichnen */
{
    if (con || !cvis || !drv) return;
    zx = mx; zy = my;
    zw = (short)gm.w - zx; if (zw > 16) zw = 16;
    zh = (short)gm.h - zy; if (zh > 16) zh = 16;
    if (zw <= 0 || zh <= 0) return;
    drv->get(zx, zy, zw, zh, (BYTE FAR *)csave, 2);
    drv->put(zx, zy, zw, zh, (const BYTE FAR *)cand, 2, 1, ROP_AND, 1, 0);
    drv->put(zx, zy, zw, zh, (const BYTE FAR *)cxor, 2, 1, ROP_XOR, 1, 0);
    con = 1;
}
/* Zeichenaufruf beruehrt l..r-1, t..b-1 (Geraetekoordinaten) */
static void ex(short l, short t, short r, short b)
{
    if (con && dv == drv && l < zx + zw && r > zx && t < zy + zh && b > zy) zweg();
}
#pragma aux mhandler parm [ax] [bx] [cx] [dx]
static void __far __loadds mhandler(WORD ev, WORD bt, WORD x, WORD y)
{
    WORD i;
    mx = (short)x; my = (short)y; mbt = bt;
    if (ev & 1) moved = 1;
    for (i = 0; i < 3; i++) {                     /* Bits 1/2 links, 3/4 rechts, 5/6 Mitte */
        WORD a = (ev >> (1 + 2 * i)) & 3;
        if (a && (BYTE)(mqs - mqk) < 16) {
            volatile MEV *e = &mq[mqs & 15];
            e->art = (BYTE)((a & 1) ? 2 : 3); e->taste = (BYTE)(i + 1); e->x = (short)x; e->y = (short)y;
            mqs++;
        }
    }
    if (!gbusy && con && (zx != mx || zy != my)) { zweg(); zhin(); }
}
static void mausan(void)                          /* Kern hat den Treiber schon gefunden und zurueckgesetzt */
{
    if (!si.maus) { maus = 0; return; }
    maus = (BYTE)si.maus;
    m33(7, 0, 0, gm.w - 1); m33(8, 0, 0, gm.h - 1);
    mx = (short)(gm.w / 2); my = (short)(gm.h / 2);
    m33(4, 0, (WORD)mx, (WORD)my);
    mqk = mqs = 0; moved = 0; cvis = 1; con = 0;
    m33h(0x0C, 0x7F, (void (__far *)(void))mhandler);
    cli(); zhin(); sti();
}
static void mausaus(void)
{
    if (!maus) return;
    m33bx(0);                                     /* Zuruecksetzen entfernt auch den Handler */
    cli(); zweg(); cvis = 0; sti();
    maus = 0;
}
/* fuer den Kern: naechstes Ereignis. e[0] 1 bewegt / 2 gedrueckt / 3 losgelassen,
   e[1] Taste 1..3 (bei Bewegung: Tastenbits), e[2] x, e[3] y. 0 = keins */
WORD RDEXPORT g_MausHole(WORD FAR *e)
{
    WORD r = 1;
    cli();
    if (mqk != mqs) {
        volatile MEV *q = &mq[mqk & 15];
        e[0] = q->art; e[1] = q->taste; e[2] = (WORD)q->x; e[3] = (WORD)q->y; mqk++;
    } else if (moved) { moved = 0; e[0] = 1; e[1] = mbt; e[2] = (WORD)mx; e[3] = (WORD)my; }
    else r = 0;
    sti();
    return r;
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
    gbusy = 1;
    return 1;
}
static void unsel(void)
{
    if (cd && cd->typ == 1) MemUnlock(cd->bmp);
    cd = 0;
    if (!gbusy) return;
    cli();                                        /* Zeiger nachziehen/wieder zeigen */
    if (con && (zx != mx || zy != my)) zweg();
    zhin();
    gbusy = 0;
    sti();
}

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
        ex(a, y, (short)(b + 1), (short)(y + 1));
        if (brush && patp) dv->span(y, a, b, 0, (const BYTE FAR *)patp, rop);
        else dv->span(y, a, b, brush ? pat[0] : penb, (const BYTE FAR *)0, rop);
    }
}
static void px(short x, short y, BYTE farbe, WORD rop)
{
    WORD i;
    for (i = 0; i < nec; i++)
        if (x >= ec[i].l && x < ec[i].r && y >= ec[i].t && y < ec[i].b) { ex(x, y, (short)(x + 1), (short)(y + 1)); dv->pixel(x, y, farbe, rop); return; }
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
        ex(x, a, (short)(x + 1), (short)(b + 1));
        if (dv->vline) dv->vline(x, a, b, penb, rop);
        else for (; a <= b; a++) dv->pixel(x, a, penb, rop);
    }
}
/* eine Zeile 1-Bit-Daten (Bit 0 = x) geclippt ausgeben */
static BYTE sbuf[RB];
/* h Zeilen 1-Bit-Daten (Abstand st Bytes) geclippt ausgeben; ohne Schnitt links in einem Treiberaufruf */
static void crows(short y, short x, short w, short h, const BYTE *buf, WORD st, WORD rop, BYTE fg, BYTE bg)
{
    WORD i, k, o, n;
    for (i = 0; i < nec; i++) {
        RECT *r = &ec[i]; short a = x, b = (short)(x + w), ya = y, yb = (short)(y + h);
        const BYTE *s;
        if (ya < r->t) ya = r->t;
        if (yb > r->b) yb = r->b;
        if (a < r->l) a = r->l;
        if (b > r->r) b = r->r;
        if (a >= b || ya >= yb) continue;
        o = (WORD)(a - x);
        ex(a, ya, b, yb);
        s = buf + (WORD)(ya - y) * st;
        if (!o) { dv->put(a, ya, (short)(b - a), (short)(yb - ya), (const BYTE FAR *)s, st, 1, rop, fg, bg); continue; }
        n = ((WORD)(b - a) + 7) >> 3;
        for (; ya < yb; ya++, s += st) {          /* ab Bit o nach vorn schieben */
            WORD sh = o & 7, q = o >> 3;
            for (k = 0; k < n; k++) sbuf[k] = (BYTE)((s[q + k] << sh) | (sh ? s[q + k + 1] >> (8 - sh) : 0));
            dv->put(a, ya, (short)(b - a), 1, (const BYTE FAR *)sbuf, RB, 1, rop, fg, bg);
        }
    }
}
static void crow(short y, short x, short w, const BYTE *buf, WORD rop, BYTE fg, BYTE bg)
{
    crows(y, x, w, 1, buf, RB, rop, fg, bg);
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
static WORD fh;
static WORD syshoehe(void) { return gm.h >= 480 ? 16 : gm.h >= 348 ? 14 : 8; }
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
    case CAPS_TEXTH: return (short)syshoehe();
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
        if (x >= ec[i].l && x < ec[i].r && y >= ec[i].t && y < ec[i].b) { ex(x, y, (short)(x + 1), (short)(y + 1)); c = dv->getpixel(x, y) ? C_WHITE : C_BLACK; break; }
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
    if (!d || !sel(d)) return;
    if (!nec) { unsel(); return; }
    if (dv == drv) zweg();
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
    if (zs.dv == drv) zweg();                      /* Quelle Schirm: Zeiger nicht mitkopieren */
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
            short sxx = sx, swd = (short)sw; WORD c, s, acc;
            if (sxx < 0) sxx = 0;
            if (sxx + swd > (short)zs.w) swd = (short)(zs.w - sxx);
            if (swd <= 0) continue;
            zs.dv->get(sxx, ys, swd, 1, (BYTE FAR *)q, RB);
            memset(rbuf, 0, RB);
            for (c = 0, s = 0, acc = 0; c < dw && c < MAXBW; c++) {   /* s = c * sw / dw schrittweise */
                if (c) { acc += sw; while (acc >= dw) { acc -= dw; s++; } }
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

/* ---- Schirm und Wahl der Grafikart ------------------------------------------
 * Kandidaten in der Reihenfolge der automatischen Wahl. Profil "Komfort" (386 und
 * schneller, oder Profil=KOMFORT in RD2.INI) nimmt Farbe zuerst, "Schnell" (8088/286)
 * nur 1-Bit-Treiber. Von Hand (Menue Grafik, RD2.INI Grafik=, RD /CGA ...) geht jede
 * Art, deren Treiber die Hardware erkennt; sonst gilt die automatische Wahl. */
typedef struct { const char *drv, *ini, *name; WORD typ; BYTE bunt; } KAND;
static const KAND kand[] = {
    { "VGA16.DRV", "VGA16", "VGA 16 Farben", GFX_VGA16,   1 },
    { "HGC.DRV",   "HGC",   "Hercules",      GFX_HGC,     0 },
    { "VGAM.DRV",  "VGA",   "VGA mono",      GFX_VGAMONO, 0 },
    { "CGA.DRV",   "CGA",   "CGA",           GFX_CGA,     0 },
};
#define NK (sizeof kand / sizeof kand[0])
static GFXMODE pm;
static char wahl[8], prof[10];

/* Treiber i laden und Hardware pruefen. setzen = 1: Modus einschalten und behalten. */
static WORD probe(WORD i, GFXMODE *m, int setzen)
{
    GFXDRV FAR *t; FARPROC f; HMOD h;
    if (!(h = ModLoad(kand[i].drv))) return RD_ERR_NOTFOUND;
    f = ModProc(h, 1);
    t = f ? (GFXDRV FAR *)((void FAR * (RDAPI *)(void))f)() : 0;
    if (!t || t->h.typ != DRV_GFX || t->h.init(0)) { ModFree(h); return RD_ERR_NOTSUP; }
    t->modes((GFXMODE FAR *)m, 1);
    if (!setzen) { t->h.done(); ModFree(h); return 0; }
    t->setmode(0);
    drv = t; drvmod = h; gfxtyp = kand[i].typ;
    return 0;
}
static int gleich(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if ((*a | 0x20) != (*b | 0x20)) return 0;
    return *a == *b;
}
static int komfort(void)
{
    if ((prof[0] | 0x20) == 'k') return 1;
    if ((prof[0] | 0x20) == 's') return 0;
    return si.cpu >= CPU_386;
}
static int fest(void)          /* fest eingestellte Art: Index oder -1 fuer automatisch */
{
    WORD i;
    for (i = 0; i < NK; i++)
        if (si.grafik ? si.grafik == kand[i].typ : gleich(wahl, kand[i].ini)) return (int)i;
    return -1;
}
static void lesen(void)
{
    KernInfo(&si);
    ProfileGetStr("RD2.INI", "System", "Grafik", "AUTO", wahl, sizeof wahl);
    ProfileGetStr("RD2.INI", "System", "Profil", "AUTO", prof, sizeof prof);
}
static int bildschirm(void)
{
    int f; WORD i;
    if (drv) return 1;
    lesen();
    f = fest();
    if (f >= 0 && !probe((WORD)f, &gm, 1)) { mausan(); return 1; }
    if (f >= 0) DbgPrint("RDGDI: Grafik geht hier nicht, automatisch");
    for (i = 0; i < NK; i++)
        if ((!kand[i].bunt || komfort()) && !probe(i, &gm, 1)) { mausan(); return 1; }
    return 0;
}
WORD RDEXPORT g_GfxList(GFXINFO FAR *l, WORD max)
{
    WORD i, n = 0, std = 0; int f;
    lesen(); f = fest();
    for (i = 0; i < NK && n < max; i++) {
        GFXINFO FAR *e = &l[n]; const char *q = kand[i].name; WORD k = 0;
        if (drv && kand[i].typ == gfxtyp) pm = gm;
        else if (probe(i, &pm, 0)) continue;
        e->typ = kand[i].typ; e->w = pm.w; e->h = pm.h; e->farben = pm.farben; e->flags = 0;
        if (drv && kand[i].typ == gfxtyp) e->flags |= GI_AKTIV;
        if (f == (int)i) e->flags |= GI_GEWAEHLT;
        if (!std && (!kand[i].bunt || komfort())) { e->flags |= GI_STANDARD; std = 1; }
        while (*q && k < sizeof e->name - 1) e->name[k++] = *q++;
        e->name[k] = 0;
        n++;
    }
    return n;
}
WORD RDEXPORT g_GfxSelect(WORD typ)
{
    WORD i;
    if (!typ) return ProfileSetStr("RD2.INI", "System", "Grafik", "AUTO");
    for (i = 0; i < NK && kand[i].typ != typ; i++) ;
    if (i == NK) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }
    if (!(drv && typ == gfxtyp) && probe(i, &pm, 0)) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }
    return ProfileSetStr("RD2.INI", "System", "Grafik", kand[i].ini);
}
/* ---- Schriften und Text --------------------------------------------------------
 * .FNT-Dateien (SPEC 7) liegen einmal im Speicher (MEM_SHARE, gehoeren RDGDI),
 * logische Schriften = Datei + Stil (fett, kursiv, unterstrichen rechnet RDGDI).
 * Text wird zeilenweise in einen 1-Bit-Puffer gesetzt und wie BitBlt ausgegeben.
 * Zeichensatz: Codepage 437. */
#define MAXPF 4
#define MAXLF 12
typedef struct { BYTE used, refs; HMEM m; char datei[13]; } PFONT;
typedef struct { BYTE used, refs, pf; WORD stil; } LFONT;
static PFONT pfs[MAXPF];
static LFONT lfs[MAXLF + 1];                       /* HFONT 1..MAXLF */
static HFONT sysf;
static BYTE FAR *fp, FAR *fbr, FAR *fdat; static WORD FAR *fof;
static WORD fober, ferst, fletzt, fstil; static HMEM fm;
#define TBZ 16                                    /* Zeilen je Treiberaufruf */
static BYTE tb[RB * TBZ];
static char fpfad[80];

static WORD pfload(const char *datei)              /* Index + 1, 0 = Fehler */
{
    WORD i, k = 0, n; short f; long len; BYTE FAR *p; HMEM m; const char *q;
    for (i = 0; i < MAXPF; i++) if (pfs[i].used && gleich(pfs[i].datei, datei)) { pfs[i].refs++; return i + 1; }
    for (i = 0; i < MAXPF && pfs[i].used; i++) ;
    if (i == MAXPF) { SetLastError(RD_ERR_FULL); return 0; }
    for (q = datei; *q && *q != ':' && *q != '\\'; q++) ;
    if (!*q) { if (!si.api) KernInfo(&si); for (q = si.home; *q && k < 64; ) fpfad[k++] = *q++; }
    for (q = datei; *q && k < 79; ) fpfad[k++] = *q++;
    fpfad[k] = 0;
    if ((f = FileOpen(fpfad, FO_READ)) < 0) return 0;
    len = FileSeek(f, 0, FS_END); FileSeek(f, 0, FS_SET);
    if (len < 30 || len > 32000L) { FileClose(f); SetLastError(RD_ERR_FORMAT); return 0; }
    if (!(m = MemAlloc((DWORD)len, MEM_MOVABLE | MEM_SHARE))) { FileClose(f); return 0; }
    p = (BYTE FAR *)MemLock(m);
    n = FileRead(f, p, (WORD)len); FileClose(f);
    if (n != (WORD)len || p[0] != 'R' || p[1] != 'D' || p[2] != 'F' || p[3] != '1' || p[12] < p[11] || !*(WORD FAR *)(p + 4)) {
        MemUnlock(m); MemFree(m); SetLastError(RD_ERR_FORMAT); return 0;
    }
    MemUnlock(m);
    pfs[i].used = 1; pfs[i].refs = 1; pfs[i].m = m;
    for (q = datei, k = 0; *q && k < 12; ) pfs[i].datei[k++] = *q++;
    pfs[i].datei[k] = 0;
    return i + 1;
}
static void pffree(WORD i)
{
    if (i && pfs[i - 1].used && !--pfs[i - 1].refs) { MemFree(pfs[i - 1].m); pfs[i - 1].used = 0; }
}

HFONT RDEXPORT g_FontLoad(LPCSTR name, WORD hoehe, WORD stil)
{
    static char datei[80]; WORD k = 0, i, p; int punkt = 0; LPCSTR s;
    if (!name || !*name) name = "SYS";
    for (s = name; *s && k < 70; s++) { if (*s == '.') punkt = 1; datei[k++] = *s; }
    if (!punkt) {                                 /* "SYS", 14 -> SYS14.FNT */
        if (!hoehe) hoehe = syshoehe();
        if (hoehe >= 10) datei[k++] = (char)('0' + hoehe / 10 % 10);
        datei[k++] = (char)('0' + hoehe % 10);
        datei[k++] = '.'; datei[k++] = 'F'; datei[k++] = 'N'; datei[k++] = 'T';
    }
    datei[k] = 0;
    if (!(p = pfload(datei))) return 0;
    stil &= FS_BOLD | FS_ITALIC | FS_UNDER;
    for (i = 1; i <= MAXLF; i++)
        if (lfs[i].used && lfs[i].pf == p && lfs[i].stil == stil) { pffree(p); lfs[i].refs++; return i; }
    for (i = 1; i <= MAXLF && lfs[i].used; i++) ;
    if (i > MAXLF) { pffree(p); SetLastError(RD_ERR_FULL); return 0; }
    lfs[i].used = 1; lfs[i].refs = 1; lfs[i].pf = (BYTE)p; lfs[i].stil = stil;
    return i;
}
void RDEXPORT g_FontFree(HFONT f)
{
    if (f < 1 || f > MAXLF || !lfs[f].used || f == sysf) return;
    if (!--lfs[f].refs) { pffree(lfs[f].pf); lfs[f].used = 0; }
}
static HFONT sysfont(void)
{
    if (!sysf && !(sysf = g_FontLoad("SYS", syshoehe(), 0)) && !(sysf = g_FontLoad("SYS", 14, 0))) sysf = g_FontLoad("SYS", 8, 0);
    return sysf;
}
static int flock(HFONT f)
{
    WORD n;
    if (f < 1 || f > MAXLF || !lfs[f].used) f = sysfont();
    if (!f) return 0;
    fm = pfs[lfs[f].pf - 1].m;
    if (!(fp = (BYTE FAR *)MemLock(fm))) return 0;
    fh = *(WORD FAR *)(fp + 4); fober = *(WORD FAR *)(fp + 6);
    fstil = lfs[f].stil; ferst = fp[11]; fletzt = fp[12];
    n = fletzt - ferst + 1;
    fbr = fp + 27; fof = (WORD FAR *)(fp + 27 + n); fdat = fp + 27 + 3 * n;
    return 1;
}
static void funlock(void) { MemUnlock(fm); }
static WORD gidx(BYTE c)                          /* Zeichen -> Index, fehlende als '?' */
{
    if (c < ferst || c > fletzt) c = '?';
    if (c < ferst || c > fletzt) c = (BYTE)ferst;
    return c - ferst;
}
static WORD cw(BYTE c) { return fbr[gidx(c)] + ((fstil & FS_BOLD) ? 1 : 0); }
static WORD sbreite(LPCSTR s, WORD n)
{
    WORD w = 0;
    while (n--) w += cw((BYTE)*s++);
    return w + ((fstil & FS_ITALIC) ? (fh - 1) / 4 : 0);
}
static WORD flen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }

/* eine Textzeile ab (x, y) in Geraetekoordinaten, Ziel ist schon gewaehlt.
   Zeichen einmal je Aufruf nachschlagen; feste 8-Pixel-Schrift ohne Stil: je Zeile
   nur ein Byte je Zeichen kopieren (der Normalfall fuer die Oberflaeche). */
static WORD go[MAXBW / 8 + 2]; static BYTE gw[MAXBW / 8 + 2];
/* t[i] = z[go[i]] fuer i < n (z = Zeile r der Schrift) */
void sammeln(BYTE *t, const WORD *o, const BYTE __far *z, WORD n);
#pragma aux sammeln = "g1: lodsw" "mov bx,ax" "add bx,dx" "mov al,es:[bx]" "mov [di],al" "inc di" "loop g1" \
    parm [di] [si] [es dx] [cx] modify [ax bx cx si di]
static void tzeile(short x, short y, LPCSTR s, WORD n)
{
    WORD tw = 0, i, r, k, nb, rop = cd->rop, fg = txtb, bg = bkb, fett = (fstil & FS_BOLD) ? 1 : 0, schnell;
    const BYTE FAR *fd = fdat;
    if (n > MAXBW / 8) n = MAXBW / 8;
    for (i = 0; i < n; i++) {
        WORD g = gidx((BYTE)s[i]), w = fbr[g];
        if (tw + w + fett > MAXBW - 16) break;
        go[i] = fof[g]; gw[i] = (BYTE)w; tw += w + fett;
    }
    n = i;
    if (fstil & FS_ITALIC) tw += (fh - 1) / 4;
    if (!tw) return;
    schnell = !(fstil & (FS_BOLD | FS_ITALIC)) && fp[10] && fbr[0] == 8;
    nb = ((tw + 7) >> 3) + 1;
    if (cd->bkmode == BK_TRANSPARENT) {
        if (rop == ROP_COPY) rop = ROP_MASK;
        else { fg = (rop == ROP_XOR || rop == ROP_NOT) ? 1 : txtb; bg = rop == ROP_AND ? 1 : 0; }
    }
    for (r = 0; r < fh; r++) {
        BYTE *t = tb + (r % TBZ) * nb;
        if (schnell) {
            sammeln(t, go, fd + r, n);
            t[n] = 0;
        } else {
            WORD px = (fstil & FS_ITALIC) ? (fh - 1 - r) / 4 : 0;
            for (k = 0; k < nb; k++) t[k] = 0;
            for (i = 0; i < n; i++) {
                WORD w = gw[i], bw = (w + 7) >> 3, q = px >> 3, sh = px & 7;
                const BYTE FAR *z = fd + go[i] + r * bw;
                BYTE carry = 0, b;
                for (k = 0; k <= bw; k++) {
                    if (k < bw) b = z[k]; else if (carry) b = 0; else break;
                    if (fett) { BYTE b2 = (BYTE)(b | (b >> 1) | carry); carry = (BYTE)((b & 1) << 7); b = b2; }
                    t[q + k] |= (BYTE)(b >> sh);
                    if (sh) t[q + k + 1] |= (BYTE)(b << (8 - sh));
                }
                px += w + fett;
            }
        }
        if ((fstil & FS_UNDER) && r == (fober + 1 < fh ? fober + 1 : fh - 1)) {
            for (k = 0; k < (tw >> 3); k++) t[k] = 0xFF;
            if (tw & 7) t[k] |= (BYTE)(0xFF << (8 - (tw & 7)));
        }
        if (r % TBZ == TBZ - 1 || r == fh - 1) {
            WORD z0 = r - r % TBZ;
            crows((short)(y + z0), x, (short)tw, (short)(r - z0 + 1), tb, nb, rop, (BYTE)fg, (BYTE)bg);
        }
    }
}
void RDEXPORT g_TextOut(HDC h, short x, short y, LPCSTR s, short len)
{
    DCX *d = getdc(h);
    if (!d || !s || !flock(d->font)) return;
    if (len < 0) len = (short)flen(s);
    if (sel(d)) { tzeile((short)(x + d->ox), (short)(y + d->oy), s, (WORD)len); unsel(); }
    funlock();
}
short RDEXPORT g_TextWidth(HDC h, LPCSTR s, short len)
{
    DCX *d = getdc(h); WORD w;
    if (!d || !s || !flock(d->font)) return 0;
    if (len < 0) len = (short)flen(s);
    w = sbreite(s, (WORD)len);
    funlock();
    return (short)w;
}
/* Zeilenende ab s: Anzahl Zeichen der Zeile, *weiter = Zeichen bis zur naechsten Zeile */
static WORD zende(LPCSTR s, WORD maxw, int wrap, WORD *weiter)
{
    WORD i = 0, w = 0, sp = 0xFFFF;
    for (;; i++) {
        BYTE c = (BYTE)s[i];
        if (!c) { *weiter = i; return i; }
        if (c == '\n') { *weiter = i + 1; return i && s[i - 1] == '\r' ? i - 1 : i; }
        if (c == ' ') sp = i;
        w += cw(c);
        if (wrap && w > maxw && i) {
            if (sp != 0xFFFF) { *weiter = sp + 1; return sp; }
            *weiter = i; return i;
        }
    }
}
void RDEXPORT g_TextBox(HDC h, const RECT FAR *rc, LPCSTR s, WORD fl)
{
    static RECT ac, r; static WORD weiter;
    DCX *d = getdc(h); BYTE au; WORD n, z = 0, maxw; short y; LPCSTR t;
    if (!d || !rc || !s || !flock(d->font)) return;
    r = *rc; maxw = (WORD)(r.r > r.l ? r.r - r.l : 0);
    au = d->uclip; ac = d->ucl;                    /* am Rechteck abschneiden */
    if (au) isect(&d->ucl, &r); else d->ucl = r;
    d->uclip = 1;
    y = r.t;
    if (fl & TB_VCENTER) {
        for (t = s;; t += weiter) { zende(t, maxw, fl & TB_WRAP, &weiter); z++; if (!t[weiter]) break; }
        y = (short)(r.t + ((short)(r.b - r.t) - (short)(z * fh)) / 2);
    }
    if (sel(d)) {
        for (t = s; y < r.b; t += weiter, y = (short)(y + fh)) {
            short x = r.l; WORD w;
            n = zende(t, maxw, fl & TB_WRAP, &weiter);
            w = sbreite(t, n);
            if ((fl & 3) == TB_CENTER) x = (short)(r.l + ((short)maxw - (short)w) / 2);
            else if ((fl & 3) == TB_RIGHT) x = (short)(r.r - (short)w);
            tzeile((short)(x + d->ox), (short)(y + d->oy), t, n);
            if (!t[weiter]) break;
        }
        unsel();
    }
    d->uclip = au; d->ucl = ac;
    funlock();
}
WORD RDEXPORT g_FontInfo(HFONT f, FONTINFO FAR *fi)
{
    WORD i, n, mx = 0, sum = 0;
    if (!fi || !flock(f)) return 0;
    n = fletzt - ferst + 1;
    for (i = 0; i < n; i++) { if (fbr[i] > mx) mx = fbr[i]; sum += fbr[i]; }
    fi->hoehe = fh; fi->oberlaenge = fober;
    fi->breite_max = mx + ((fstil & FS_BOLD) ? 1 : 0);
    fi->breite_mittel = (WORD)(sum / n) + ((fstil & FS_BOLD) ? 1 : 0);
    fi->stil = fstil; fi->fest = fp[10]; fi->erstes = (BYTE)ferst; fi->letztes = (BYTE)fletzt;
    for (i = 0; i < 12 && fp[14 + i]; i++) fi->name[i] = (char)fp[14 + i];
    fi->name[i] = 0;
    funlock();
    return 1;
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
    WORD i;
    mausaus();
    if (drv) { drv->textmode(); drv = 0; }
    for (i = 0; i < MAXPF; i++) if (pfs[i].used) { MemFree(pfs[i].m); pfs[i].used = 0; }
    for (i = 1; i <= MAXLF; i++) lfs[i].used = 0;
    sysf = 0;
    if (drvmod) { ModFree(drvmod); drvmod = 0; }
}

/* Exporte: 1 = Ende, 2 + (API - 0x100), danach GetDC, ReleaseDC */
#define N 0x3B
static struct { WORD n; FARPROC f[N + 6]; } ext = { N + 6, {
    (FARPROC)g_Ende, (FARPROC)g_GetCaps, (FARPROC)g_SetPen, (FARPROC)g_SetBrush,
    (FARPROC)g_SetRop, (FARPROC)g_SetFont, (FARPROC)g_SetTextColor, (FARPROC)g_SetBkColor,
    (FARPROC)g_SetBkMode, (FARPROC)g_SetOrigin, (FARPROC)g_SetClip, 0,
    0, 0, 0, 0,
    0, (FARPROC)g_Pixel, (FARPROC)g_GetPixel, (FARPROC)g_MoveTo,
    (FARPROC)g_LineTo, (FARPROC)g_Line, (FARPROC)g_Frame, (FARPROC)g_FillRect,
    (FARPROC)g_InvertRect, (FARPROC)g_Ellipse, (FARPROC)g_Polygon, (FARPROC)g_FloodFill,
    (FARPROC)g_RoundRect, 0, 0, 0,
    0, (FARPROC)g_TextOut, (FARPROC)g_TextWidth, (FARPROC)g_TextBox,
    0, 0, 0, 0,
    0, (FARPROC)g_FontLoad, (FARPROC)g_FontFree, (FARPROC)g_FontInfo,
    0, 0, 0, 0,
    0, (FARPROC)g_BmpCreate, (FARPROC)g_BmpFree, (FARPROC)g_BmpDC,
    (FARPROC)g_DCFree, (FARPROC)g_BitBlt, (FARPROC)g_StretchBlt, 0,
    0, 0, (FARPROC)g_ColorNearest, (FARPROC)g_PaletteSet,
    (FARPROC)g_GetDC, (FARPROC)g_ReleaseDC, (FARPROC)g_GfxList, (FARPROC)g_GfxSelect,
    (FARPROC)g_MausHole,
} };
void FAR * RDAPI RdModInit(void) { return &ext; }
