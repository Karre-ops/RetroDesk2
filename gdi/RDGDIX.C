/* RDGDIX.C - selten gebrauchte Zeichenfunktionen von RDGDI: Ellipse, runde Rechtecke,
 * Polygone, Fuellen. Verwerfbares Modul, laedt RDGDI beim ersten Aufruf nach. Kein
 * eigener Zustand ueber einen Aufruf hinaus; gezeichnet wird ueber die Tabelle GI. */
#include "GDII.H"
#include "RD2DRV.H"

static GIS s;
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
void RDEXPORT x_Ellipse(GI FAR *g, HDC h, const RECT FAR *rp, WORD fl, WORD frei)
{
    short l, t, r, b, y, x0, x1, n0, cx2;
    (void)frei;
    if (!rp || !g->begin(h, &s)) return;
    l = (short)(rp->l + s.ox); r = (short)(rp->r + s.ox); t = (short)(rp->t + s.oy); b = (short)(rp->b + s.oy);
    if (l >= r || t >= b) { g->end(); return; }
    cx2 = (short)(l + r - 1);
    for (y = t; y < b; y++) {
        x0 = ex0(y, l, r, t, b); x1 = (short)(cx2 - x0);
        if (x0 > x1) continue;
        if (fl && !s.brnone) g->span(y, x0, x1, 1);
        if (y == t || y == b - 1) { g->span(y, x0, x1, 0); continue; }
        n0 = (short)(ex0((short)(2 * y < t + b - 1 ? y + 1 : y - 1), l, r, t, b) - 1);   /* Nachbarzeile zur Mitte hin */
        if (n0 < x0) n0 = x0;
        g->span(y, x0, n0, 0);
        g->span(y, (short)(cx2 - n0), x1, 0);
    }
    g->end();
}

void RDEXPORT x_RoundRect(GI FAR *g, HDC h, const RECT FAR *rp, WORD rad, WORD fl)
{
    short l, t, r, b, y, i, in, nin;
    if (!rp || !g->begin(h, &s)) return;
    l = (short)(rp->l + s.ox); r = (short)(rp->r + s.ox); t = (short)(rp->t + s.oy); b = (short)(rp->b + s.oy);
    if (l >= r || t >= b) { g->end(); return; }
    if ((short)rad * 2 > r - l) rad = (WORD)((r - l) / 2);
    if ((short)rad * 2 > b - t) rad = (WORD)((b - t) / 2);
    for (y = t; y < b; y++) {
        i = (short)(y - t < b - 1 - y ? y - t : b - 1 - y);          /* Abstand zum naechsten Rand */
        #define INSET(k) ((k) >= (short)rad ? 0 : (short)rad - (short)((isqrt(4UL * rad * rad - (DWORD)(2L * (rad - (k)) - 1) * (2L * (rad - (k)) - 1)) + 1) >> 1))
        in = INSET(i);
        if (fl && !s.brnone) g->span(y, (short)(l + in), (short)(r - 1 - in), 1);
        if (i == 0) { g->span(y, (short)(l + in), (short)(r - 1 - in), 0); continue; }
        nin = INSET(i - 1);                                          /* Nachbar zum Rand hin */
        nin = (short)(nin > in ? nin - 1 : in);
        g->span(y, (short)(l + in), (short)(l + nin), 0);
        g->span(y, (short)(r - 1 - nin), (short)(r - 1 - in), 0);
        #undef INSET
    }
    g->end();
}

#define MAXPX 32
void RDEXPORT x_Polygon(GI FAR *g, HDC h, const POINT FAR *p, WORD n, WORD fl)
{
    WORD i, j, k; short y, ymin, ymax, xs[MAXPX];
    if (!p || n < 2 || !g->begin(h, &s)) return;
    ymin = ymax = p[0].y;
    for (i = 1; i < n; i++) { if (p[i].y < ymin) ymin = p[i].y; if (p[i].y > ymax) ymax = p[i].y; }
    if (fl && !s.brnone && n > 2)
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
                g->span((short)(y + s.oy), (short)(xs[i] + s.ox), (short)(xs[i + 1] + s.ox), 1);
        }
    for (i = 0; i < n; i++) {
        WORD c = (i + 1) % n;
        if (c == 0 && n == 2) break;
        g->line((short)(p[i].x + s.ox), (short)(p[i].y + s.oy), (short)(p[c].x + s.ox), (short)(p[c].y + s.oy));
    }
    g->end();
}

/* Fuellen ab x,y: Bereich gleicher Farbe; besuchte Punkte merkt eine Bitkarte */
void RDEXPORT x_FloodFill(GI FAR *g, HDC h, short x, short y)
{
    RECT bb; WORD bw, bh, tgt, sp = 0; HMEM hm; BYTE FAR *vis; short FAR *st;
    #define STK 1024
    if (!g->begin(h, &s)) return;                  /* Huelle bestimmen, dann Speicher holen */
    bb = s.bb; g->end();
    x = (short)(x + s.ox); y = (short)(y + s.oy);
    if (!s.nclip || x < bb.l || x >= bb.r || y < bb.t || y >= bb.b || s.brnone) return;
    bw = (WORD)(bb.r - bb.l); bh = (WORD)(bb.b - bb.t);
    hm = MemAlloc((DWORD)((bw + 7) >> 3) * bh + STK * 4, MEM_ZEROINIT);
    if (!hm) return;
    vis = (BYTE FAR *)MemLock(hm); st = (short FAR *)(vis + (WORD)(((bw + 7) >> 3) * bh));
    if (!g->begin(h, &s)) { MemUnlock(hm); MemFree(hm); return; }
    tgt = g->get(x, y);
    #define VB(xx, yy) (vis + (WORD)((yy) - bb.t) * ((bw + 7) >> 3) + ((WORD)((xx) - bb.l) >> 3))
    #define VM(xx) ((BYTE)(0x80 >> (((xx) - bb.l) & 7)))
    #define IN(xx, yy) (!(*VB(xx, yy) & VM(xx)) && g->get(xx, yy) == tgt)
    st[sp++] = x; st[sp++] = y;
    while (sp) {
        short a, b, yy, k;
        y = st[--sp]; x = st[--sp];
        if (!IN(x, y)) continue;
        for (a = x; a > bb.l && IN(a - 1, y); a--) ;
        for (b = x; b < bb.r - 1 && IN(b + 1, y); b++) ;
        for (k = a; k <= b; k++) *VB(k, y) |= VM(k);
        g->span(y, a, b, 1);
        for (yy = (short)(y - 1); yy <= y + 1; yy += 2) {
            int run = 0;
            if (yy < bb.t || yy >= bb.b) continue;
            for (k = a; k <= b; k++) {
                if (IN(k, yy)) { if (!run && sp < STK * 2 - 2) { st[sp++] = k; st[sp++] = yy; } run = 1; }
                else run = 0;
            }
        }
    }
    g->end();
    MemUnlock(hm); MemFree(hm);
}

/* Farbiges Symbol (RDKX IconDraw): b im Geraeteformat (4/8 Bit), m Maske (1 = deckend). Hintergrund in
   eine Bitmap holen, deckende Punkte einsetzen, zurueck auf den Schirm. */
static void RDEXPORT x_IconColor(HDC dc, short x, short y, HBMP b, HBMP m)
{
    WORD FAR *bh = (WORD FAR *)MemLock(b), w = bh[0], h = bh[1], fb = bh[2], zb = bh[3], mb = (w + 7) >> 3, yy, xx;
    HBMP t = BmpCreate(w, h, 0); HDC s; BYTE FAR *p, FAR *c = (BYTE FAR *)bh + 8, FAR *k = (BYTE FAR *)MemLock(m) + 8;
    if (t && (s = BmpDC(t)) != 0) {
        BitBlt(s, 0, 0, w, h, dc, x, y, ROP_COPY);
        p = (BYTE FAR *)MemLock(t) + 8;
        for (yy = 0; yy < h; yy++, p += zb, c += zb, k += mb)
            for (xx = 0; xx < w; xx++)
                if (k[xx >> 3] & (0x80 >> (xx & 7))) {
                    if (fb == 8) p[xx] = c[xx];
                    else { BYTE n = (BYTE)(xx & 1 ? 0x0F : 0xF0); p[xx >> 1] = (BYTE)((p[xx >> 1] & ~n) | (c[xx >> 1] & n)); }
                }
        MemUnlock(t);
        BitBlt(dc, x, y, w, h, s, 0, 0, ROP_COPY); DCFree(s);
    }
    if (t) BmpFree(t);
    MemUnlock(b); MemUnlock(m);
}
/* Grafikwahl (Menue Grafik): Namen in der Reihenfolge der Kandidaten von RDGDI (kand[]) */
static const char *const gnam[] = { "VGA", "EGA", "VESA", "Hercules", "VGA", "CGA", "EGA/VGA", "EGA", "EGA", "VGA", "EGA", "VESA", "MCGA/VGA", "VESA", "VESA", "CGA" };   /* Optionen zeigt dazu Aufloesung und Farben */
static WORD RDEXPORT x_GfxList(GI FAR *g, GFXINFO FAR *l, WORD max)
{
    WORD i, n = 0, std = 0, t, f, k; GFXMODE m; const char *q; GFXINFO FAR *e;
    for (i = 0; n < max && (t = g->gfx(i, 0, 0)) != 0xFFFF; i++) {
        if ((f = g->gfx(i, 1, (void FAR *)&m)) == 0xFFFF) continue;
        e = &l[n++];
        e->typ = t; e->w = m.w; e->h = m.h; e->farben = m.farben; e->flags = 0;
        if (f & 1) e->flags |= GI_AKTIV;
        if (f & 2) e->flags |= GI_GEWAEHLT;
        if (!std && (f & 4)) { e->flags |= GI_STANDARD; std = 1; }
        for (q = i < sizeof gnam / sizeof gnam[0] ? gnam[i] : "?", k = 0; *q && k < sizeof e->name - 1; ) e->name[k++] = *q++;
        e->name[k] = 0;
    }
    return n;
}
static WORD RDEXPORT x_GfxSelect(GI FAR *g, WORD typ)
{
    WORD i, t; GFXMODE m;
    if (!typ) return g->gfx(0xFFFF, 2, 0);
    for (i = 0; (t = g->gfx(i, 0, 0)) != 0xFFFF && t != typ; i++) ;
    if (t == 0xFFFF || g->gfx(i, 1, (void FAR *)&m) == 0xFFFF) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }
    return g->gfx(i, 2, 0);
}
static struct { WORD n; FARPROC f[7]; } ext = { 7, { (FARPROC)x_Ellipse, (FARPROC)x_RoundRect, (FARPROC)x_Polygon, (FARPROC)x_FloodFill, (FARPROC)x_IconColor,
                                               (FARPROC)x_GfxList, (FARPROC)x_GfxSelect } };
void FAR * RDAPI RdModInit(void) { return &ext; }
