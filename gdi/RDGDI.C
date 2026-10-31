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
#include "GDII.H"

#define MAXBW   1024                              /* breiteste Bitmap/Zeile in Pixeln */
#define RB      (MAXBW / 8 + 2)


typedef struct { WORD w, h, bpp, bpr; } BMPH;     /* Kopf eines Bitmap-Blocks, danach die Zeilen */

static DCX dcs[MAXDC];                             /* HDC 1..MAXDC = dcs[0..MAXDC-1] */
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
static BYTE bunt, weiss = 1;                       /* Ziel Schirm mit 16 Farben: Farbe = Index, weiss = 15 */
static WORD patf;                                  /* bunt mit Muster: Bits 0-3 Farbe, 8-11 Hintergrund */

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
static BYTE cbit(COLOR c) { return bunt ? (BYTE)((GFXDRVF FAR *)drv)->farbe(syscol(c)) : (BYTE)(lum(c) >= 128); }   /* bunt: Treiber waehlt den Index */

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
    if (bunt) {                                   /* 16 Farben: einfarbig ohne Raster, Muster mit zwei Farben */
        if (d->pat == PAT_SOLID || d->pat > PAT_HATCH) { pat[0] = cbit(d->brush); patp = 0; return; }
        for (y = 0; y < 8; y++) pat[y] = mus[d->pat][y];
        patf = (WORD)(cbit(d->brush) | ((WORD)cbit(d->bk) << 8)); patp = pat; return;
    }
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
static const BYTE cmb[64] = { 0x7F,0xFF,0x3F,0xFF,0x1F,0xFF,0x0F,0xFF,0x07,0xFF,0x03,0xFF,0x01,0xFF,0x00,0xFF,0x00,0x7F,0x00,0x3F,0x01,0xFF,0x10,0xFF,0x30,0xFF,0x78,0x7F,0xF8,0x7F,0xFC,0xFF,
                               0x00,0x00,0x00,0x00,0x40,0x00,0x60,0x00,0x70,0x00,0x78,0x00,0x7C,0x00,0x7E,0x00,0x7F,0x00,0x7C,0x00,0x6C,0x00,0x46,0x00,0x06,0x00,0x03,0x00,0x03,0x00,0x00,0x00 };
static GK gk = { 1, 0, 0, 0, cmb };               /* kclip: Ursprung/Clip/Treiber geaendert */
#define kclip gk.kclip
static SYSINFO si;
static BYTE maus, con, cvis;
static volatile BYTE gbusy, moved;
static volatile short mx, my; static volatile WORD mbt;
static short zx, zy, zw, zh;                       /* gezeichnete Lage */
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
    drv->cursorhide(1);                           /* Zeiger zeichnet der Treiber (Pflicht ab 22.10.2026) */
    con = 0;
}
static void zhin(void)                            /* Zeiger an (mx, my) zeichnen */
{
    if (con || !cvis || !drv) return;
    zx = mx - gk.hx; zy = my - gk.hy; if (zx < 0) zx = 0; if (zy < 0) zy = 0;
    zw = (short)gm.w - zx; if (zw > 16) zw = 16;
    zh = (short)gm.h - zy; if (zh > 16) zh = 16;
    if (zw <= 0 || zh <= 0) return;
    con = 1;
    drv->cursor(zx, zy, gk.zb);
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
        WORD k;                                   /* a = 3: Druecken und Loslassen in einem Aufruf (kurzer Klick, */
        for (k = 1; k <= 2; k++)                  /* langsamer Rechner/Treiber): beides melden, erst gedrueckt */
            if ((a & k) && (BYTE)(mqs - mqk) < 16) {
                volatile MEV *e = &mq[mqs & 15];
                e->art = (BYTE)(k + 1); e->taste = (BYTE)(i + 1); e->x = (short)x; e->y = (short)y;
                mqs++;
            }
    }
    if (!gbusy && con && (zx != mx - gk.hx || zy != my - gk.hy)) { zweg(); zhin(); }
}
static void mausan(void)                          /* Kern hat den Treiber schon gefunden und zurueckgesetzt */
{
    if (!si.maus) { maus = 0; return; }
    maus = (BYTE)si.maus;
    m33(7, 0, 0, gm.w - 1); m33(8, 0, 0, gm.h - 1);
    mx = (short)(gm.w / 2); my = (short)(gm.h / 2);
    m33(4, 0, (WORD)mx, (WORD)my);
    mqk = mqs = 0; moved = 0; cvis = 1; con = 0;
    { WORD v = (WORD)(7 - ((si.maus >> 8) & 7)) * 2; if (v - 4 < 9) m33(0x0F, 0, v, v * 2); }   /* Tempo 1..5 -> 12..4 Mickeys je 8 Punkte (3 = Treiberstandard 8), nach jedem Zuruecksetzen */
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
        if (si.maus & 0x8000 && (e[1] & 3) % 3) e[1] ^= 3;   /* Linkshaender: Taste 1/2 tauschen */
    } else if (moved) { moved = 0; e[0] = 1; e[1] = mbt; e[2] = (WORD)mx; e[3] = (WORD)my; }
    else r = 0;
    sti();
    return r;
}

/* ---- Auswahl des Ziels -------------------------------------------------------- */
static DCX *getdc(HDC h)
{
    if (h < 1 || h > MAXDC || !dcs[h - 1].used) { SetLastError(RD_ERR_HANDLE); return 0; }
    return &dcs[h - 1];
}
static void isect(RECT *a, const RECT *b)
{
    if (b->l > a->l) a->l = b->l;
    if (b->t > a->t) a->t = b->t;
    if (b->r < a->r) a->r = b->r;
    if (b->b < a->b) a->b = b->b;
}
/* Zeichenziel waehlen: Treiber/Bitmap, Clipliste berechnen. 0 = nichts zu zeichnen */
static DCX *kd, *kc; static BYTE kdirty = 1;
static int sel(DCX *d)
{
    static RECT dev, u, r; WORD i;
    cd = d;
    if (d->typ == 0 && d == kc && !kclip && drv) { dv = drv; goto farben; }   /* Clipliste unveraendert */
    nec = 0; kc = d; kclip = 0;
    if (d->typ == 0) {
        if (!drv) return 0;
        dv = drv; dev.l = 0; dev.t = 0; dev.r = (short)gm.w; dev.b = (short)gm.h;
    } else {
        BMPH FAR *bh = (BMPH FAR *)MemLock(d->bmp);
        if (!bh) return 0;
        if (bh->bpp != 1) { MemUnlock(d->bmp); return 0; }   /* 4-Bit-Bitmap: nur BitBlt (b4blt) */
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
    if (d->typ) kc = 0;                            /* Bitmap: jedes Mal neu */
farben:
    bunt = (BYTE)(d->typ == 0 && gm.bpp > 1); weiss = (BYTE)(bunt ? 15 : 1);
    if (d != kd || kdirty) {                       /* Farben/Pinsel nur nach Aenderung neu umrechnen */
        penb = cbit(d->pen); txtb = cbit(d->txt); bkb = cbit(d->bk);
        mkbrush(d); kd = d; kdirty = 0;
    }
    gbusy = 1;
    return 1;
}
static void unsel(void)
{
    if (cd && cd->typ == 1) MemUnlock(cd->bmp);
    cd = 0;
    if (!gbusy) return;
    cli();                                        /* Zeiger nachziehen/wieder zeigen */
    if (con && (zx != mx - gk.hx || zy != my - gk.hy)) zweg();
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
        if (brush && patp) dv->span(y, a, b, patf, (const BYTE FAR *)patp, rop);
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
        if (dv->box) dv->box(x, a, x, b, penb, (const BYTE FAR *)0, rop);   /* Schirm: Spalte in Assembler, Bitmap: Punkte */
        else for (; a <= b; a++) dv->pixel(x, a, penb, rop);
    }
}
/* eine Zeile 1-Bit-Daten (Bit 0 = x) geclippt ausgeben */
static BYTE sbuf[RB];
/* h Zeilen 1-Bit-Daten (Abstand st Bytes) geclippt ausgeben; ohne Schnitt links in einem Treiberaufruf */
static void crows(short y, short x, short w, short h, const BYTE FAR *buf, WORD st, WORD rop, BYTE fg, BYTE bg)
{
    WORD i, k, o, n;
    for (i = 0; i < nec; i++) {
        RECT *r = &ec[i]; short a = x, b = (short)(x + w), ya = y, yb = (short)(y + h);
        const BYTE FAR *s;
        if (ya < r->t) ya = r->t;
        if (yb > r->b) yb = r->b;
        if (a < r->l) a = r->l;
        if (b > r->r) b = r->r;
        if (a >= b || ya >= yb) continue;
        o = (WORD)(a - x);
        ex(a, ya, b, yb);
        s = buf + (WORD)(ya - y) * st;
        if (!o) { dv->put(a, ya, (short)(b - a), (short)(yb - ya), s, st, 1, rop, fg, bg); continue; }
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
    case CAPS_DPI: return 72; case CAPS_GFX: return (short)gfxtyp;
    case CAPS_TEXTH: return (short)syshoehe();
    default: return 0;
    }
}
COLOR RDEXPORT g_SetPen(HDC h, COLOR c)        { DCX *d = getdc(h); COLOR o; if (!d) return 0; o = d->pen; if (o != c) { d->pen = c; kdirty = 1; } return o; }
COLOR RDEXPORT g_SetBrush(HDC h, COLOR c, WORD m) { DCX *d = getdc(h); COLOR o; if (!d) return 0; o = d->brush; if (o != c || d->pat != m) { d->brush = c; d->pat = m; kdirty = 1; } return o; }
WORD  RDEXPORT g_SetRop(HDC h, WORD r)         { DCX *d = getdc(h); WORD o; if (!d) return 0; o = d->rop; d->rop = r > ROP_NOT ? ROP_COPY : r; return o; }
HFONT RDEXPORT g_SetFont(HDC h, HFONT f)       { DCX *d = getdc(h); HFONT o; if (!d) return 0; o = d->font; d->font = f; return o; }
COLOR RDEXPORT g_SetTextColor(HDC h, COLOR c)  { DCX *d = getdc(h); COLOR o; if (!d) return 0; o = d->txt; if (o != c) { d->txt = c; kdirty = 1; } return o; }
COLOR RDEXPORT g_SetBkColor(HDC h, COLOR c)    { DCX *d = getdc(h); COLOR o; if (!d) return 0; o = d->bk; if (o != c) { d->bk = c; kdirty = 1; } return o; }
WORD  RDEXPORT g_SetBkMode(HDC h, WORD m)      { DCX *d = getdc(h); WORD o; if (!d) return 0; o = d->bkmode; d->bkmode = m; return o; }
void  RDEXPORT g_SetOrigin(HDC h, short x, short y) { DCX *d = getdc(h); if (d) { d->ox = x; d->oy = y; kclip = 1; } }
void  RDEXPORT g_SetClip(HDC h, const RECT FAR *r)
{
    DCX *d = getdc(h);
    if (!d) return;
    if (r) { d->ucl = *r; d->uclip = 1; } else d->uclip = 0;
    kclip = 1;
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
        if (x >= ec[i].l && x < ec[i].r && y >= ec[i].t && y < ec[i].b) { ex(x, y, (short)(x + 1), (short)(y + 1)); c = bunt ? ((GFXDRVF FAR *)drv)->rgb(dv->getpixel(x, y)) : dv->getpixel(x, y) ? C_WHITE : C_BLACK; break; }
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
    if (dv->box) {   /* Treiber kann ganze Rechtecke: je Clip-Rechteck ein Aufruf */
        static RECT q; WORD i;
        for (i = 0; i < nec; i++) {
            q.l = l; q.r = (short)(rr + 1); q.t = (short)(r->t + cd->oy); q.b = (short)(r->b + cd->oy);
            isect(&q, &ec[i]);
            if (q.l >= q.r || q.t >= q.b) continue;
            ex(q.l, q.t, q.r, q.b);
            if (inv) dv->box(q.l, q.t, (short)(q.r - 1), (short)(q.b - 1), 0, (const BYTE FAR *)0, ROP_NOT);
            else dv->box(q.l, q.t, (short)(q.r - 1), (short)(q.b - 1), patp ? patf : pat[0], patp ? (const BYTE FAR *)patp : (const BYTE FAR *)0, cd->rop);
        }
        return;
    }
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

/* Ellipse, runde Rechtecke, Polygone, Fuellen: in RDGDIX.MOD (verwerfbar, bei Bedarf) */
static HMOD xm; static FARPROC xf[8];
static WORD RDEXPORT i_begin(HDC h, GIS FAR *s)
{
    DCX *d = getdc(h); WORD i;
    if (!d || !sel(d)) return 0;
    s->ox = d->ox; s->oy = d->oy; s->brnone = brnone; s->nclip = (BYTE)nec;
    if (nec) s->bb = ec[0];
    for (i = 1; i < nec; i++) {
        if (ec[i].l < s->bb.l) s->bb.l = ec[i].l; if (ec[i].t < s->bb.t) s->bb.t = ec[i].t;
        if (ec[i].r > s->bb.r) s->bb.r = ec[i].r; if (ec[i].b > s->bb.b) s->bb.b = ec[i].b;
    }
    return 1;
}
static void RDEXPORT i_end(void) { unsel(); }
static void RDEXPORT i_span(short y, short a, short b, WORD br) { hs(y, a, b, br, cd->rop); }
static void RDEXPORT i_line(short x0, short y0, short x1, short y1) { line(x0, y0, x1, y1); }
static WORD RDEXPORT i_get(short x, short y) { ex(x, y, (short)(x + 1), (short)(y + 1)); return dv->getpixel(x, y); }
static WORD RDEXPORT i_gfx(WORD i, WORD was, void FAR *m);
static GI gi = { i_begin, i_end, i_span, i_line, i_get, i_gfx };
static FARPROC xp(WORD nr)
{
    if (!xm && !(xm = ModLoad("RDGDIX.MOD"))) return 0;
    if (!xf[nr]) xf[nr] = ModProc(xm, nr);
    return xf[nr];
}
void RDEXPORT g_Ellipse(HDC h, const RECT FAR *r, WORD fl)            { GX_RECT f = (GX_RECT)xp(1); if (f) f(&gi, h, r, fl, 0); }
void RDEXPORT g_RoundRect(HDC h, const RECT FAR *r, WORD rad, WORD fl) { GX_RECT f = (GX_RECT)xp(2); if (f) f(&gi, h, r, rad, fl); }
void RDEXPORT g_Polygon(HDC h, const POINT FAR *p, WORD n, WORD fl)    { GX_POLY f = (GX_POLY)xp(3); if (f) f(&gi, h, p, n, fl); }
void RDEXPORT g_FloodFill(HDC h, short x, short y)                    { GX_FILL f = (GX_FILL)xp(4); if (f) f(&gi, h, x, y); }
WORD RDEXPORT g_GfxList(GFXINFO FAR *l, WORD max)                     { GX_LIST f = (GX_LIST)xp(6); return f ? f(&gi, l, max) : 0; }
WORD RDEXPORT g_GfxSelect(WORD typ)                                   { GX_SEL f = (GX_SEL)xp(7); return f ? f(&gi, typ) : RD_ERR_NOTFOUND; }

/* ---- Bitmaps ------------------------------------------------------------------ */
HBMP RDEXPORT g_BmpCreate(WORD w, WORD h, WORD bpp)
{
    WORD bpr; HMEM m; BMPH FAR *bh;
    if (bpp == BMP_ROH && !(gm.bpp > 1 && drv && ((GFXDRV FAR *)drv)->h.groesse >= sizeof(GFXDRVR))) bpp = 0;   /* Treiber ohne roh: Geraeteformat */
    if ((bpp & BMP_MASKE) && !(gm.bpp > 1 && drv && ((GFXDRV FAR *)drv)->h.groesse >= sizeof(GFXDRVR) && ((GFXDRVR FAR *)drv)->symput)) bpp &= ~BMP_MASKE;
    if (!bpp) bpp = gm.bpp;                       /* 0 = Geraeteformat des Schirms */
    if (bpp == BMP_ROH) bpr = ((GFXDRVR FAR *)drv)->rohgr(7, (short)w);   /* roh: Bildspeicher unveraendert (Menue-Hintergrund) */
    else {
        if ((bpp & 0xFF) != 4 && (bpp & 0xFF) != 8) bpp = 1;   /* 4/8 Bit: nur BitBlt mit dem Schirm, kein Zeichnen hinein */
        bpr = (w * (bpp & 0xFF) + 7) >> 3;        /* w <= MAXBW, kein Ueberlauf */
    }
    if (!w || !h || w > MAXBW || (DWORD)bpr * h > 65000UL) { SetLastError(RD_ERR_FULL); return 0; }
    m = MemAlloc((DWORD)bpr * h + ((bpp & BMP_MASKE) ? (DWORD)((w + 7) >> 3) * h : 0) + sizeof(BMPH), MEM_MOVABLE | MEM_ZEROINIT);
    if (!m) return 0;
    bh = (BMPH FAR *)MemLock(m);
    bh->w = w; bh->h = h; bh->bpp = bpp; bh->bpr = bpr;
    MemUnlock(m);
    return m;
}
static HDC newdc(BYTE typ, HBMP b)
{
    WORD i; DCX *d;
    for (i = 1; i <= MAXDC && dcs[i - 1].used; i++) ;
    if (i > MAXDC) { SetLastError(RD_ERR_FULL); return 0; }
    d = &dcs[i - 1];
    memset(d, 0, sizeof *d);
    d->used = 1; d->typ = typ; d->bmp = b; d->owner = TaskSelf();
    kdirty = 1; kclip = 1; d->pen = C_BLACK; d->brush = C_WHITE; d->txt = C_BLACK; d->bk = C_WHITE; d->bkmode = BK_OPAQUE;
    return i;
}
void RDEXPORT g_BmpFree(HBMP b)
{
    WORD i;
    for (i = 1; i <= MAXDC; i++) if (dcs[i - 1].used && dcs[i - 1].typ == 1 && dcs[i - 1].bmp == b) dcs[i - 1].used = 0;
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
/* 4-Bit-Bitmap (Geraeteformat) <-> Schirm, 1:1, fuer "Hintergrund retten": Schirm -> Bitmap ab 0,0,
   Bitmap -> Schirm geclippt; der Aufrufer bleibt in Schirm und Bitmap. 1 = erledigt */
static int b4blt(DCX *dd, short x, short y, short w, short h, DCX *sd, short sx, short sy)
{
    static RECT q; static BMPH FAR *bh; static BYTE FAR *z; static DCX *b; WORD i, o, bpr;
    b = sd->typ ? sd : dd;
    if (!b->typ || !(bh = (BMPH FAR *)MemLock(b->bmp))) return 0;
    if (bh->bpp == 1) { MemUnlock(b->bmp); return 0; }
    z = (BYTE FAR *)(bh + 1); bpr = bh->bpr;
    if (!dd->typ) {                               /* Bitmap -> Schirm: je Clip-Rechteck ein put */
        if (sel(dd)) {
            for (i = 0; i < nec; i++) {
                q.l = x; q.t = y; q.r = (short)(x + w); q.b = (short)(y + h); isect(&q, &ec[i]);
                if (q.l >= q.r || q.t >= q.b) continue;
                o = (WORD)(q.l - x + sx); ex(q.l, q.t, q.r, q.b);
                if (bh->bpp == BMP_ROH) { ((GFXDRVR FAR *)drv)->rohput(q.l, q.t, (short)(q.r - q.l), (short)(q.b - q.t), z + (WORD)(q.t - y + sy) * bpr, bpr, (short)(x - sx), (short)bh->w); continue; }
                if (bh->bpp & BMP_MASKE) {            /* Symbol: nur deckende Punkte (Maske hinter den Punkten) */
                    WORD mbp = (bh->w + 7) >> 3, r0 = (WORD)(q.t - y + sy), v = (bh->bpp & 0xFF) == 4;
                    ((GFXDRVR FAR *)drv)->symput(q.l, q.t, (short)(q.r - q.l), (short)(q.b - q.t), z + r0 * bpr + (o >> v), bpr, v & o,
                                                 z + bpr * bh->h + r0 * mbp + (o >> 3), mbp, o & 7);
                    continue;
                }
                drv->put(q.l, q.t, (short)(q.r - q.l), (short)(q.b - q.t), z + (WORD)(q.t - y + sy) * bpr + (o >> (bh->bpp == 4)), bpr, bh->bpp | ((o & 1) << 8), ROP_COPY, 0, 0);
            }
            unsel();
        }
    } else if (!sd->typ && !x && !y) { cd = 0; gbusy = 1; zweg(); if (bh->bpp == BMP_ROH) ((GFXDRVR FAR *)drv)->rohget(sx, sy, w, h, z, bpr); else drv->get(sx, sy, w, h, z, bpr); unsel(); }   /* Schirm -> Bitmap */
    MemUnlock(b->bmp);
    return 1;
}
static void blitrows(DCX *dd, short x, short y, short w, short h, DCX *sd, short sx, short sy,
                     WORD rop, WORD sw, WORD shh, WORD dw, WORD dh)
{
    static ZIEL zs, zd; short i, n, ys, yd, step; WORD yq, yr;
    if ((sd->typ || dd->typ) && !sw && b4blt(dd, x, y, w, h, sd, sx, sy)) return;
    if (!sd->typ && gm.bpp > 1) return;            /* bunter Schirm -> 1-Bit-Bitmap: nicht unterstuetzt */
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
    /* schnelle Wege ohne Zeilenpuffer (Bitmap byteweise ausgerichtet, nicht ueberlappend) */
    if (!sw && sd != dd && rop != ROP_MASK) {
        if (zs.mb && !(sx & 7)) {                  /* Bitmap -> beliebig: ganzer Block, geclippt in crows */
            crows(y, x, w, n, zs.mb + (WORD)sy * zs.bpr + ((WORD)sx >> 3), zs.bpr, rop, weiss, 0);
            goto aus;
        }
        if (!zs.mb && mb && rop == ROP_COPY && !(x & 7) && nec == 1 && x >= ec[0].l && y >= ec[0].t &&
            x + w <= ec[0].r && y + n <= ec[0].b && (!(w & 7) || x + w >= (short)mw)) {
            zs.dv->get(sx, sy, w, n, MP(x, y), mbpr);   /* Schirm -> Bitmap: Treiber schreibt direkt hinein */
            goto aus;
        }
    }
    step = 1; i = 0;
    if (!sw && sd == dd && y > sy) { step = -1; i = (short)(n - 1); }   /* ueberlappend: von unten */
    yq = 0; yr = 0;                               /* StretchBlt: yq = i * shh / dh schrittweise (step 1) */
    for (; i >= 0 && i < n; i = (short)(i + step)) {
        if (!sw) { ys = (short)(sy + i); yd = (short)(y + i); }
        else {
            ys = (short)(sy + yq); yd = (short)(y + i);
            for (yr += shh; yr >= dh; yr -= dh) yq++;
            if (ys >= (short)zs.h || ys < 0) continue;
        }
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
        else crow(yd, x, w, rbuf, rop, weiss, 0);
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
COLOR RDEXPORT g_ColorNearest(HDC h, COLOR c) { DCX *d = getdc(h); bunt = (BYTE)(d && !d->typ && gm.bpp > 1); return bunt ? ((GFXDRVF FAR *)drv)->rgb(cbit(c)) : cbit(c) ? C_WHITE : C_BLACK; }
WORD  RDEXPORT g_PaletteSet(HDC h, WORD f, WORD n, const COLOR FAR *c) { (void)h; if (gm.bpp < 4) { SetLastError(RD_ERR_NOTSUP); return 0; } drv->palette(f, n, c); return n; }   /* bunte Treiber haben palette */

/* ---- Schirm und Wahl der Grafikart ------------------------------------------
 * Kandidaten in der Reihenfolge der automatischen Wahl. Profil "Komfort" (386 und
 * schneller, oder Profil=KOMFORT in RD2.INI) nimmt Farbe zuerst, "Schnell" (8088/286)
 * nur 1-Bit-Treiber. Von Hand (Menue Grafik, RD2.INI Grafik=, RD /CGA ...) geht jede
 * Art, deren Treiber die Hardware erkennt; sonst gilt die automatische Wahl. */
typedef struct { const char *drv, *ini; WORD typ; } KAND;   /* bunt: typ ab GFX_EGA; Namen fuer die Liste in RDGDIX */
static const KAND kand[] = {
    { "VGA16.DRV", "VGA16", GFX_VGA16 },
    { "EGA.DRV",   "EGA", GFX_EGA },
    { "VESA.DRV",  "VESA", GFX_VESA },
    { "HGC.DRV",   "HGC", GFX_HGC },
    { "VGAM.DRV",  "VGA", GFX_VGAMONO },
    { "CGA.DRV",   "CGA", GFX_CGA },
    { "EGA320.DRV", "EGA320", GFX_EGA320 },     /* nur von Hand (30.10.2026) */
    { "EGAM.DRV",  "EGAM", GFX_EGAMONO },       /* ab hier ebenfalls nur von Hand */
    { "EGA4.DRV",  "EGA4", GFX_EGA4 },
    { "VGA4.DRV",  "VGA4", GFX_VGA4 },
    { "EGA200.DRV", "EGA200", GFX_EGA200 },
    { "SVGA16.DRV", "SVGA16", GFX_SVGA16 },
    { "MCGA.DRV",  "MCGA", GFX_MCGA },
    { "VESA800.DRV", "VESA800", GFX_VESA800 },
    { "VESA1024.DRV", "VESA1024", GFX_VESA1024 },
    { "CGA4.DRV",  "CGA4", GFX_CGA4 },
};
#define NK (sizeof kand / sizeof kand[0])
static GFXMODE pm;
static char wahl[12], prof[10];                  /* wahl: laengster Name VESA1024 */

/* Treiber i laden und Hardware pruefen. setzen = 1: Modus einschalten und behalten. */
static WORD cgamod(void);
static WORD probe(WORD i, GFXMODE *m, int setzen)
{
    GFXDRV FAR *t; FARPROC f; HMOD h;
    if (!(h = ModLoad(kand[i].drv))) return RD_ERR_NOTFOUND;
    f = ModProc(h, 1);
    t = f ? (GFXDRV FAR *)((void FAR * (RDAPI *)(void))f)() : 0;
    if (!t || t->h.typ != DRV_GFX || t->h.groesse < sizeof(GFXDRV) || !t->cursor || !t->box || t->h.init(0)) { ModFree(h); return RD_ERR_NOTSUP; }
    t->modes((GFXMODE FAR *)m, 1);
    if (!setzen) { t->h.done(); ModFree(h); return 0; }
    t->setmode(kand[i].typ == GFX_CGA ? cgamod() : 0);
    drv = t; drvmod = h; gfxtyp = kand[i].typ; kclip = 1;
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
    for (i = 0; i < NK; i++)
        if ((kand[i].typ < GFX_EGA || komfort()) && !probe(i, &gm, 1)) { mausan(); return 1; }
    return 0;
}
/* Grafikwahl fuer das Menue Grafik: Liste und Auswahl liegen in RDGDIX (selten gebraucht).
 * i_gfx(i, was, m): was 0 = Typ von Kandidat i (0xFFFF = Ende), 1 = Hardware pruefen und Modus nach
 * m (0xFFFF = geht nicht, sonst Bit 0 aktiv, 1 fest gewaehlt, 2 waere Standard), 2 = in RD2.INI eintragen
 * (i = 0xFFFF: AUTO). */
static WORD RDEXPORT i_gfx(WORD i, WORD was, void FAR *m)
{
    WORD r = 0; int ak;
    if (was == 2) return ProfileSetStr("RD2.INI", "System", "Grafik", i < NK ? kand[i].ini : "AUTO");
    if (i >= NK) return 0xFFFF;
    if (!was) return kand[i].typ;
    if (!i) lesen();
    ak = drv && kand[i].typ == gfxtyp;
    if (ak) pm = gm; else if (probe(i, &pm, 0)) return 0xFFFF;
    *(GFXMODE FAR *)m = pm;
    if (ak) r |= 1;
    if (fest() == (int)i) r |= 2;
    if (kand[i].typ < GFX_EGA || komfort()) r |= 4;
    return r;
}
/* ---- Schriften und Text --------------------------------------------------------
 * .FNT-Dateien (SPEC 7) liegen einmal im Speicher (MEM_SHARE, gehoeren RDGDI),
 * logische Schriften = Datei + Stil (fett, kursiv, unterstrichen rechnet RDGDI).
 * Text wird zeilenweise in einen 1-Bit-Puffer gesetzt und wie BitBlt ausgegeben.
 * Zeichensatz: Codepage 437. */
#define MAXPF 4
#define MAXLF 12
typedef struct { BYTE used, refs; HMEM m; BYTE FAR *p; char datei[13]; } PFONT;   /* p: dauerhaft gesperrt */
typedef struct { BYTE used, refs, pf; WORD stil; } LFONT;
static PFONT pfs[MAXPF];
static LFONT lfs[MAXLF + 1];                       /* HFONT 1..MAXLF */
static HFONT sysf;
static BYTE FAR *fp, FAR *fbr, FAR *fdat; static WORD FAR *fof;
static WORD fober, ferst, fletzt, fstil; static HFONT flast;   /* flast: zuletzt eingestellte Schrift */
#define TBZ 7                                     /* Zeilen je Treiberaufruf (durchsichtiger Text braucht tb nicht) */
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
    pfs[i].used = 1; pfs[i].refs = 1; pfs[i].m = m; pfs[i].p = p;   /* bleibt gesperrt: kein Sperren je Textaufruf */
    for (q = datei, k = 0; *q && k < 12; ) pfs[i].datei[k++] = *q++;
    pfs[i].datei[k] = 0;
    return i + 1;
}
static void pffree(WORD i)
{
    if (i && pfs[i - 1].used && !--pfs[i - 1].refs) { MemFree(pfs[i - 1].m); pfs[i - 1].used = 0; flast = 0; }
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
    if (!--lfs[f].refs) { pffree(lfs[f].pf); lfs[f].used = 0; flast = 0; }
}
static HFONT sysfont(void)
{
    if (!sysf && gm.w <= 320) sysf = g_FontLoad("SYSK.FNT", 0, 0);   /* 320 Punkte breit: schmale Schrift */
    if (!sysf && !(sysf = g_FontLoad("SYS", syshoehe(), 0)) && !(sysf = g_FontLoad("SYS", 14, 0))) sysf = g_FontLoad("SYS", 8, 0);
    return sysf;
}
static int flock(HFONT f)
{
    WORD n;
    if (f < 1 || f > MAXLF || !lfs[f].used) f = sysfont();
    if (!f) return 0;
    if (f == flast) return 1;                     /* Normalfall: dieselbe Schrift wie zuletzt */
    fp = pfs[lfs[f].pf - 1].p;
    fh = *(WORD FAR *)(fp + 4); fober = *(WORD FAR *)(fp + 6);
    fstil = lfs[f].stil; ferst = fp[11]; fletzt = fp[12];
    n = fletzt - ferst + 1;
    fbr = fp + 27; fof = (WORD FAR *)(fp + 27 + n); fdat = fp + 27 + 3 * n;
    flast = f;
    return 1;
}
#define funlock()
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
/* Spaltenweise: fuer jedes Zeichen 2 * s_cnt Zeilen (paarweise, eine Zeile mehr ist harmlos) seiner Glyphe (Versatz s_o[i] + s_z0 im
   Segment s_seg) untereinander nach s_t (Abstand s_nb), danach eine Fuellspalte. Mit s_inv
   invertiert, dann kann der Treiber schlicht kopieren (rep movsw). Ein Aufruf je Treiberblock. */
static WORD *s_o, s_n, s_seg, s_z0, s_nb, s_cnt, s_inv; static BYTE *s_t;
static void spalten(void)
{
    _asm {
        push si
        push di
        push ds
        push es
        push bp
        cld
        mov  ax, ds
        mov  es, ax
        mov  bx, s_o
        mov  di, s_t
        mov  cx, s_n
        mov  dx, s_nb
        dec  dx
        mov  bp, s_z0
        mov  ah, byte ptr s_cnt
        cmp  s_inv, 0
        mov  si, s_seg
        mov  ds, si
        jne  vi
    v1: mov  si, es:[bx]
        add  bx, 2
        add  si, bp
        push di
        mov  ch, ah
    v2: movsb
        add  di, dx
        movsb
        add  di, dx
        dec  ch
        jnz  v2
        pop  di
        inc  di
        dec  cl
        jnz  v1
        xor  al, al
        jmp  vz
    vi: mov  si, es:[bx]
        add  bx, 2
        add  si, bp
        push di
        mov  ch, ah
    v3: lodsb
        not  al
        stosb
        add  di, dx
        lodsb
        not  al
        stosb
        add  di, dx
        dec  ch
        jnz  v3
        pop  di
        inc  di
        dec  cl
        jnz  vi
        mov  al, 0FFh
    vz: mov  ch, ah
    v4: stosb
        add  di, dx
        stosb
        add  di, dx
        dec  ch
        jnz  v4
        pop  bp
        pop  es
        pop  ds
        pop  di
        pop  si
    }
}
/* Schmale Schrift (bis 8 Punkte je Zeichen): ein Zeichen in alle Zeilen von t (Abstand nb) einodern, um sh verschoben */
void gl1(BYTE *t, const BYTE FAR *z, WORD h, BYTE sh, WORD nb);
#pragma aux gl1 = "l: mov ah,es:[si]" "inc si" "or ah,ah" "jz n" "xor al,al" "shr ax,cl" "or [di],ah" "or [di+1],al" "n: add di,bx" "dec dx" "jnz l" \
    parm [di] [es si] [dx] [cl] [bx] modify [ax dx si di]
static void tzeile(short x, short y, LPCSTR s, WORD n)
{
    WORD tw = 0, i, r, k, nb, rop = cd->rop, fg = txtb, bg = bkb, fett = (fstil & FS_BOLD) ? 1 : 0, schnell, breit = 0;
    const BYTE FAR *fd = fdat;
    if (n > MAXBW / 8) n = MAXBW / 8;
    schnell = !(fstil & (FS_BOLD | FS_ITALIC)) && fp[10] && fbr[0] == 8;
    if (schnell) {                                /* feste 8er-Schrift: Breite steht fest, nur Versatz holen */
        WORD e = ferst, sp = fletzt - e, q = gidx('?'), fo = FP_OFF(fd);
        if (n > (MAXBW - 16) / 8) n = (MAXBW - 16) / 8;
        for (i = 0; i < n; i++) { WORD c = (BYTE)s[i] - e; go[i] = fof[c <= sp ? c : q] + fo; }
        tw = n << 3;
    } else {
        for (i = 0; i < n; i++) {
            WORD g = gidx((BYTE)s[i]), w = fbr[g];
            if (tw + w + fett > MAXBW - 16) break;
            go[i] = fof[g]; gw[i] = (BYTE)w; tw += w + fett;
            if (w > 8) breit = 1;                 /* breite Zeichen: nicht der schmale Weg */
        }
        n = i;
        if (fstil & FS_ITALIC) tw += (fh - 1) / 4;
    }
    if (!tw) return;
    nb = ((tw + 7) >> 3) + 1;
    if (cd->bkmode == BK_TRANSPARENT) {
        if (rop == ROP_COPY) rop = ROP_MASK;
        else { fg = (rop == ROP_XOR || rop == ROP_NOT) ? weiss : txtb; bg = rop == ROP_AND ? weiss : 0; }
    }
    if (schnell && !(fstil & FS_UNDER)) {          /* Normalfall: ganze Bloecke in Assembler */
        WORD zb;
        if (dv->glyphs && nec == 1 && x >= ec[0].l && (short)(x + tw) <= ec[0].r && y >= ec[0].t && (short)(y + fh) <= ec[0].b
            && (cd->bkmode == BK_TRANSPARENT ? rop == ROP_MASK || rop == ROP_XOR : rop == ROP_COPY && fg != bg)) {
            ex(x, y, (short)(x + tw), (short)(y + fh));   /* ganz sichtbar: Treiber liest die Schrift selbst */
            dv->glyphs(x, y, (const WORD FAR *)go, FP_SEG(fd), fh, n, fg, 0, rop);
            return;
        }
        zb = ((RB * TBZ) / nb) & ~1;         /* so viele Zeilen passen in tb (kurze Texte: alle), gerade */
        s_inv = !bunt && rop == ROP_COPY && !fg && bg && !(x & 7);   /* schwarz auf weiss, ausgerichtet: hier invertieren, Treiber kopiert */
        if (s_inv) { fg = 1; bg = 0; }
        s_o = go; s_n = n; s_seg = FP_SEG(fd); s_nb = nb; s_t = tb;
        for (r = 0; r < fh; r += zb) {
            k = fh - r < zb ? fh - r : zb; s_cnt = (k + 1) >> 1; s_z0 = r;
            spalten();
            crows((short)(y + r), x, (short)tw, (short)k, tb, nb, rop, (BYTE)fg, (BYTE)bg);
        }
        return;
    }
    if (!schnell && !breit && !fett && !(fstil & FS_ITALIC) && (WORD)fh * nb <= sizeof tb) {   /* schmale Schrift: Zeichen fuer Zeichen ueber alle Zeilen (30.10.2026) */
        WORD px = 0;
        memset(tb, 0, fh * nb);
        for (i = 0; i < n; i++) { gl1(tb + (px >> 3), fd + go[i], fh, (BYTE)(px & 7), nb); px += gw[i]; }
        if (fstil & FS_UNDER) {
            BYTE *t = tb + (fober + 1 < fh ? fober + 1 : fh - 1) * nb;
            for (k = 0; k < (tw >> 3); k++) t[k] = 0xFF;
            if (tw & 7) t[k] |= (BYTE)(0xFF << (8 - (tw & 7)));
        }
        crows(y, x, (short)tw, (short)fh, tb, nb, rop, (BYTE)fg, (BYTE)bg);
        return;
    }
    for (r = 0; r < fh; r++) {
        BYTE *t = tb + (r % TBZ) * nb;
        if (schnell) {
            sammeln(t, go, MK_FP(FP_SEG(fd), r), n);
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
    d->uclip = 1; kclip = 1;
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
    d->uclip = au; d->ucl = ac; kclip = 1;
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

/* RunDos: 0 = Textmodus und Maus aus, 1 = Grafik und Maus wieder an (Bild zeichnen die Programme neu) */
static WORD cgamod(void) { return (WORD)((0x10 | (ProfileGetInt("RD2.INI", "Farben", "CGA", 15) & 15)) << 8); }   /* CGA: Farbe an setmode */
static BYTE pause_;
void RDEXPORT g_Pause(WORD an)
{
    if (!an && drv && !pause_) { mausaus(); drv->textmode(); pause_ = 1; }
    else if (an && pause_) { drv->setmode(gfxtyp == GFX_CGA ? cgamod() : 0); mausan(); pause_ = 0; }
}
/* Abschluss: Kern ruft das beim Beenden von RD */
void RDEXPORT g_Ende(void)
{
    WORD i;
    mausaus();
    if (xm) { ModFree(xm); xm = 0; for (i = 0; i < 5; i++) xf[i] = 0; }
    if (drv) { drv->textmode(); drv = 0; }
    for (i = 0; i < MAXPF; i++) if (pfs[i].used) { MemFree(pfs[i].m); pfs[i].used = 0; }
    flast = 0;
    for (i = 1; i <= MAXLF; i++) lfs[i].used = 0;
    sysf = 0;
    if (drvmod) { ModFree(drvmod); drvmod = 0; }
}

/* Exporte: 1 = Ende, 2 + (API - 0x100), danach GetDC, ReleaseDC */
#define N 0x3B
static struct { WORD n; FARPROC f[N + 9]; } ext = { N + 9, {
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
    (FARPROC)g_MausHole, (FARPROC)g_Pause,
    (FARPROC)(void FAR *)dcs, (FARPROC)(void FAR *)&gk,   /* Daten fuer RDUSER (GDII.H): DC-Tabelle, Clip neu */
} };
void FAR * RDAPI RdModInit(void) { return &ext; }
