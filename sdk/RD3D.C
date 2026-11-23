/* RD3D.C - 3D fuer RD2/32 (PLAN_32BIT Schritt 5a): API, Software-Treiber, Hilfen fuer Drehung und Projektion.
 * Rasterung zeilenweise, Werte je Dreieck als Ebenengleichung (16.16, Zwischenwerte 64 Bit), Textur affin.
 * Ein Bildpunkt: Z pruefen, Alpha-Raster, Texel oder Farbe, Helligkeit (Gouraud) mal Texel-Helligkeit, mal Nebel. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD3D.H"

static const FIX vsin[257] = {
    0, 402, 804, 1206, 1608, 2010, 2412, 2814, 3216, 3617, 4019, 4420,
    4821, 5222, 5623, 6023, 6424, 6824, 7224, 7623, 8022, 8421, 8820, 9218,
    9616, 10014, 10411, 10808, 11204, 11600, 11996, 12391, 12785, 13180, 13573, 13966,
    14359, 14751, 15143, 15534, 15924, 16314, 16703, 17091, 17479, 17867, 18253, 18639,
    19024, 19409, 19792, 20175, 20557, 20939, 21320, 21699, 22078, 22457, 22834, 23210,
    23586, 23961, 24335, 24708, 25080, 25451, 25821, 26190, 26558, 26925, 27291, 27656,
    28020, 28383, 28745, 29106, 29466, 29824, 30182, 30538, 30893, 31248, 31600, 31952,
    32303, 32652, 33000, 33347, 33692, 34037, 34380, 34721, 35062, 35401, 35738, 36075,
    36410, 36744, 37076, 37407, 37736, 38064, 38391, 38716, 39040, 39362, 39683, 40002,
    40320, 40636, 40951, 41264, 41576, 41886, 42194, 42501, 42806, 43110, 43412, 43713,
    44011, 44308, 44604, 44898, 45190, 45480, 45769, 46056, 46341, 46624, 46906, 47186,
    47464, 47741, 48015, 48288, 48559, 48828, 49095, 49361, 49624, 49886, 50146, 50404,
    50660, 50914, 51166, 51417, 51665, 51911, 52156, 52398, 52639, 52878, 53114, 53349,
    53581, 53812, 54040, 54267, 54491, 54714, 54934, 55152, 55368, 55582, 55794, 56004,
    56212, 56418, 56621, 56823, 57022, 57219, 57414, 57607, 57798, 57986, 58172, 58356,
    58538, 58718, 58896, 59071, 59244, 59415, 59583, 59750, 59914, 60075, 60235, 60392,
    60547, 60700, 60851, 60999, 61145, 61288, 61429, 61568, 61705, 61839, 61971, 62101,
    62228, 62353, 62476, 62596, 62714, 62830, 62943, 63054, 63162, 63268, 63372, 63473,
    63572, 63668, 63763, 63854, 63944, 64031, 64115, 64197, 64277, 64354, 64429, 64501,
    64571, 64639, 64704, 64766, 64827, 64884, 64940, 64993, 65043, 65091, 65137, 65180,
    65220, 65259, 65294, 65328, 65358, 65387, 65413, 65436, 65457, 65476, 65492, 65505,
    65516, 65525, 65531, 65535, 65536,
};
FIX R3dSin(WORD w)
{
    w &= 1023;
    if (w < 256) return vsin[w];
    if (w < 512) return vsin[512 - w];
    if (w < 768) return -vsin[w - 512];
    return -vsin[1024 - w];
}
FIX R3dCos(WORD w) { return R3dSin((WORD)(w + 256)); }
void R3dIdent(R3DMAT *m) { int i, j; for (i = 0; i < 3; i++) for (j = 0; j < 4; j++) m->m[i][j] = i == j ? FX(1) : 0; }
void R3dMal(R3DMAT *e, const R3DMAT *a, const R3DMAT *b)
{
    R3DMAT t; int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) t.m[i][j] = FXMUL(a->m[i][0], b->m[0][j]) + FXMUL(a->m[i][1], b->m[1][j]) + FXMUL(a->m[i][2], b->m[2][j]);
        t.m[i][3] = FXMUL(a->m[i][0], b->m[0][3]) + FXMUL(a->m[i][1], b->m[1][3]) + FXMUL(a->m[i][2], b->m[2][3]) + a->m[i][3];
    }
    *e = t;
}
void R3dDreh(R3DMAT *m, WORD wx, WORD wy, WORD wz)
{
    R3DMAT x, y, z; FIX s, c;
    R3dIdent(&x); s = R3dSin(wx); c = R3dCos(wx); x.m[1][1] = c; x.m[1][2] = -s; x.m[2][1] = s; x.m[2][2] = c;
    R3dIdent(&y); s = R3dSin(wy); c = R3dCos(wy); y.m[0][0] = c; y.m[0][2] = s; y.m[2][0] = -s; y.m[2][2] = c;
    R3dIdent(&z); s = R3dSin(wz); c = R3dCos(wz); z.m[0][0] = c; z.m[0][1] = -s; z.m[1][0] = s; z.m[1][1] = c;
    R3dMal(m, &y, &x); R3dMal(m, &z, m);
}
void R3dVersch(R3DMAT *m, FIX x, FIX y, FIX z) { m->m[0][3] += x; m->m[1][3] += y; m->m[2][3] += z; }
void R3dXform(const R3DMAT *m, const R3DVEC *v, R3DVEC *e)
{
    R3DVEC t;
    t.x = FXMUL(m->m[0][0], v->x) + FXMUL(m->m[0][1], v->y) + FXMUL(m->m[0][2], v->z) + m->m[0][3];
    t.y = FXMUL(m->m[1][0], v->x) + FXMUL(m->m[1][1], v->y) + FXMUL(m->m[1][2], v->z) + m->m[1][3];
    t.z = FXMUL(m->m[2][0], v->x) + FXMUL(m->m[2][1], v->y) + FXMUL(m->m[2][2], v->z) + m->m[2][3];
    *e = t;
}
#define ZNAH FX(1)
int R3dProj(R3D *r, const R3DVEC *v, R3DVERT *e, FIX zmax)
{
    FIX f = (FIX)(r->w / 2) << 16; __int64 t;
    if (v->z < ZNAH) return 0;
    e->x = ((FIX)(r->w / 2) << 16) + (FIX)(((__int64)v->x * f / v->z));
    t = (__int64)v->y * f / v->z;
    if (r->art == R3D_VOLL13) t = t * 5 / 6;      /* 320x200 auf 4:3: Bildpunkte hoeher als breit */
    e->y = ((FIX)(r->h / 2) << 16) - (FIX)t;
    t = (__int64)(v->z - ZNAH) * 65535 / (zmax > ZNAH ? zmax - ZNAH : 1);
    if (t > 65535) t = 65535;
    e->z = (FIX)t;                                /* 0..65535 = 0..1 in 16.16 */
    return 1;
}

/* ---- Software-Treiber ------------------------------------------------------------------------------------ */
static BYTE mul[16][16];                          /* Helligkeit a * b / 15 */
static int sw_open(R3D *r)
{
    WORD i, j;
    for (i = 0; i < 16; i++) for (j = 0; j < 16; j++) mul[i][j] = (BYTE)((i * j + 7) / 15);
    if (!(r->bild = rd32_alloc((DWORD)r->w * r->h)) || !(r->z = rd32_alloc((DWORD)r->w * r->h * 2))) return 0;
    return 1;
}
static void sw_close(R3D *r) { if (r->bild) rd32_free(r->bild); if (r->z) rd32_free(r->z); r->bild = 0; r->z = 0; }
static void sw_clear(R3D *r, BYTE farbe)
{
    DWORD n = (DWORD)r->w * r->h, i, f = farbe * 0x01010101UL, *b = (DWORD *)r->bild, *z = (DWORD *)r->z;
    for (i = 0; i < n / 4; i++) b[i] = f;
    for (i = 0; i < n / 2; i++) z[i] = 0xFFFFFFFFUL;
}
typedef struct { __int64 a0; FIX dx, dy; } EBENE;  /* a(x, y) = a0 + dx * x + dy * y (x, y in 16.16) */
static int ebene(EBENE *e, FIX a0, FIX a1, FIX a2, const R3DVERT *p0, const R3DVERT *p1, const R3DVERT *p2, __int64 d)
{
    __int64 n1 = (__int64)(a1 - a0) * (p2->y - p0->y) - (__int64)(a2 - a0) * (p1->y - p0->y);
    __int64 n2 = (__int64)(a2 - a0) * (p1->x - p0->x) - (__int64)(a1 - a0) * (p2->x - p0->x);
    e->dx = (FIX)(n1 / d); e->dy = (FIX)(n2 / d);
    e->a0 = ((__int64)a0 << 16) - (__int64)e->dx * p0->x - (__int64)e->dy * p0->y;   /* 32.32 */
    return 1;
}
static FIX wert(const EBENE *e, FIX x, FIX y) { return (FIX)((e->a0 + (__int64)e->dx * x + (__int64)e->dy * y) >> 16); }
static void sw_tri(R3D *r, const R3DVERT *a, const R3DVERT *b, const R3DVERT *c)
{
    const R3DVERT *p[3], *t; FIX y, ye, xl, xr, x0, x1, yc; long yi; __int64 d;   /* d 64 Bit: grosse Dreiecke (ab 1024x768) liefen ueber (23.11.2026) */ EBENE ez, eu = { 0, 0, 0 }, ev = { 0, 0, 0 }, es = { 0, 0, 0 }; WORD fl = r->zustand;
    __int64 dd = (__int64)(b->x - a->x) * (c->y - a->y) - (__int64)(c->x - a->x) * (b->y - a->y);
    p[0] = a; p[1] = b; p[2] = c;
    if (p[1]->y < p[0]->y) { t = p[0]; p[0] = p[1]; p[1] = t; }
    if (p[2]->y < p[1]->y) { t = p[1]; p[1] = p[2]; p[2] = t; }
    if (p[1]->y < p[0]->y) { t = p[0]; p[0] = p[1]; p[1] = t; }
    d = dd >> 16;
    if (d == 0 || p[2]->y - p[0]->y < 0x100) return;
    ebene(&ez, a->z, b->z, c->z, a, b, c, d);
    if (fl & R3S_TEXTUR) { ebene(&eu, a->u, b->u, c->u, a, b, c, d); ebene(&ev, a->v, b->v, c->v, a, b, c, d); }
    if (fl & R3S_GOURAUD) ebene(&es, a->s, b->s, c->s, a, b, c, d);
    y = (p[0]->y + 0xFFFF) & ~0xFFFFL; if (y < 0) y = 0;
    ye = p[2]->y; if (ye > ((FIX)r->h << 16)) ye = (FIX)r->h << 16;
    for (; y < ye; y += 0x10000) {
        yc = y + 0x8000;                          /* Zeilenmitte */
        xl = p[0]->x + (FIX)((__int64)(yc - p[0]->y) * (p[2]->x - p[0]->x) / (p[2]->y - p[0]->y));
        if (yc < p[1]->y) xr = p[1]->y - p[0]->y ? p[0]->x + (FIX)((__int64)(yc - p[0]->y) * (p[1]->x - p[0]->x) / (p[1]->y - p[0]->y)) : p[1]->x;
        else xr = p[2]->y - p[1]->y ? p[1]->x + (FIX)((__int64)(yc - p[1]->y) * (p[2]->x - p[1]->x) / (p[2]->y - p[1]->y)) : p[1]->x;
        if (xl > xr) { x0 = xl; xl = xr; xr = x0; }
        x0 = (xl + 0x7FFF) >> 16; x1 = (xr + 0x7FFF) >> 16;   /* Bildpunkte mit Mitte in [xl, xr) */
        if (x0 < 0) x0 = 0;
        if (x1 > r->w) x1 = r->w;
        if (x0 >= x1) continue;
        yi = y >> 16;
        {
            FIX xs = (x0 << 16) + 0x8000, z = wert(&ez, xs, yc), u = 0, v = 0, s = (FIX)(a->s);
            BYTE *pb = r->bild + (DWORD)yi * r->w + x0; WORD *pz = r->z + (DWORD)yi * r->w + x0; long n = x1 - x0, x = x0;
            BYTE rampe = (BYTE)(r->farbe & 0xF0), hell = (BYTE)(r->farbe & 15), tl = r->tl, tm = r->tm; const BYTE *tx = r->tex;
            if (fl & R3S_TEXTUR) { u = wert(&eu, xs, yc); v = wert(&ev, xs, yc); }
            if (fl & R3S_GOURAUD) s = wert(&es, xs, yc);
            for (; n > 0; n--, x++, pb++, pz++, z += ez.dx, u += eu.dx, v += ev.dx, s += es.dx) {
                WORD zz = (WORD)(z < 0 ? 0 : z > 65535 ? 65535 : z); BYTE h, sh;
                if ((fl & R3S_ZTEST) && zz >= *pz) continue;
                if ((fl & R3S_ALPHA) && ((x ^ yi) & 1)) continue;
                if (fl & R3S_TEXTUR) { BYTE te = tx[((((DWORD)v >> 16) & tm) << tl) | (((DWORD)u >> 16) & tm)]; rampe = (BYTE)(te & 0xF0); hell = (BYTE)(te & 15); }
                sh = (BYTE)(s < 0 ? 0 : s > FX(15) ? 15 : s >> 16);
                h = mul[hell][sh];
                if (fl & R3S_NEBEL) h = mul[h][r->nebel[zz >> 8]];
                *pb = (BYTE)(rampe | h);
                if (fl & R3S_ZWRITE) *pz = zz;
            }
        }
    }
}
static void sw_flip(R3D *r)
{
    WORD y, x;
    if (r->art == R3D_FENSTER) {
        HDC dc; DWORD i, n = (DWORD)r->w * r->h;
        for (i = 0; i < n; i++) r->buf.pix[i] = r->map[r->bild[i]];
        if ((dc = GetDC(r->fenster)) != 0) { BufShow(&r->buf, dc, r->fx, r->fy); ReleaseDC(r->fenster, dc); }
    } else if (r->schirm && r->bpp > 8) {          /* Echtfarben: je Bildpunkt ueber die Tabelle */
        const DWORD *t = r->tc;
        for (y = 0; y < r->h; y++) {
            const BYTE *q = r->bild + (DWORD)y * r->w; BYTE *z = r->schirm + (DWORD)y * r->zeile;
            if (r->bpp == 32) for (x = 0; x < r->w; x++) ((DWORD *)z)[x] = t[q[x]];
            else if (r->bpp == 24) for (x = 0; x < r->w; x++, z += 3) { DWORD c = t[q[x]]; z[0] = (BYTE)c; z[1] = (BYTE)(c >> 8); z[2] = (BYTE)(c >> 16); }
            else for (x = 0; x < r->w; x++) ((WORD *)z)[x] = (WORD)t[q[x]];
        }
    } else if (r->schirm) {
        for (y = 0; y < r->h; y++) {
            DWORD *q = (DWORD *)(r->bild + (DWORD)y * r->w), *z = (DWORD *)(r->schirm + (DWORD)y * r->zeile);
            for (x = 0; x < r->w / 4; x++) z[x] = q[x];
        }
    }
}
static const R3DDRV sw = { "Software", sw_open, sw_close, sw_clear, sw_tri, sw_flip };

/* ---- API ------------------------------------------------------------------------------------------------- */
static const BYTE std16[16][3] = { {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
    {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255} };
static R3D *oeffne(WORD art, HWND fenster, WORD w, WORD h, WORD bpp)
{
    R3D *r = rd32_alloc(sizeof(R3D)); SCREENINFO si; BYTE *z; DWORD i;
    if (!r) return 0;
    for (z = (BYTE *)r, i = 0; i < sizeof(R3D); i++) z[i] = 0;
    r->art = art; r->fenster = fenster; r->drv = &sw; r->bpp = 8;   /* RD2.INI [System] 3D= (Voodoo, ATI, nVidia): Treiber folgen */
    if (art == R3D_VOLL13 || art == R3D_VOLLLFB || art == R3D_VESA) {
        if (art == R3D_VESA ? !ScreenModeVesa(w, h, bpp, &si) : !ScreenMode(art == R3D_VOLL13 ? SM_13H : SM_LFB, &si)) { rd32_free(r); return 0; }
        r->w = (WORD)(si.w & ~3); r->h = si.h; r->zeile = si.zeilenbytes; r->schirm = ScreenPtr(); r->bpp = si.bpp;
    } else {
        r->w = (WORD)((w + 3) & ~3); r->h = h;
        if (!BufCreate(&r->buf, r->w, r->h)) { rd32_free(r); return 0; }
        { WORD *bh = MemLock(r->buf.bmp); r->dbpp = bh[2] & 0xFF; MemUnlock(r->buf.bmp); }
    }
    if (!r->drv->open(r)) { R3dClose(r); return 0; }
    for (i = 0; i < 16; i++) { r->rampe[i][0] = std16[i][0]; r->rampe[i][1] = std16[i][1]; r->rampe[i][2] = std16[i][2]; }
    r->nah = 0; r->fern = FX(1); r->farbe = 0xFF;
    R3dZustand(r, R3S_ZTEST | R3S_ZWRITE);
    R3dPalette(r);
    return r;
}
R3D *R3dOpen(WORD art, HWND fenster, WORD w, WORD h) { return art == R3D_VESA ? 0 : oeffne(art, fenster, w, h, 8); }
R3D *R3dOpenVesa(HWND fenster, WORD w, WORD h, WORD bpp) { return oeffne(R3D_VESA, fenster, w, h, bpp); }
void R3dClose(R3D *r)
{
    if (!r) return;
    if (r->drv) r->drv->close(r);
    if (r->art == R3D_FENSTER) BufFree(&r->buf);
    rd32_free(r);
}
const char *R3dTreiber(R3D *r) { return r->drv->name; }
void R3dRampe(R3D *r, WORD k, BYTE rot, BYTE gruen, BYTE blau) { if (k < 16) { r->rampe[k][0] = rot; r->rampe[k][1] = gruen; r->rampe[k][2] = blau; } }
void R3dNebel(R3D *r, BYTE rot, BYTE gruen, BYTE blau, FIX nah, FIX fern) { r->nf[0] = rot; r->nf[1] = gruen; r->nf[2] = blau; r->nah = nah; r->fern = fern; }
void R3dPalette(R3D *r)
{
    WORD k, s, i, c; long f;
    for (k = 0; k < 16; k++) for (s = 0; s < 16; s++) for (c = 0; c < 3; c++)
        r->pal[(k * 16 + s) * 3 + c] = (BYTE)((r->nf[c] + ((int)r->rampe[k][c] - r->nf[c]) * (int)s / 15) >> 2);
    for (i = 0; i < 256; i++) {                   /* Nebel: Faktor 15 vor "nah", 0 hinter "fern" */
        f = ((long)i << 8);
        r->nebel[i] = (BYTE)(f <= r->nah ? 15 : f >= r->fern ? 0 : 15 - (15L * (f - r->nah) / (r->fern - r->nah)));
    }
    if (r->art != R3D_FENSTER && r->bpp > 8) {      /* Echtfarben: Tabelle Farbnummer -> Bildpunkt (6 Bit je Farbe -> 8 Bit) */
        for (i = 0; i < 256; i++) {
            DWORD R = r->pal[i * 3] * 255UL / 63, G = r->pal[i * 3 + 1] * 255UL / 63, B = r->pal[i * 3 + 2] * 255UL / 63;
            r->tc[i] = r->bpp == 15 ? (R >> 3) << 10 | (G >> 3) << 5 | B >> 3 : r->bpp == 16 ? (R >> 3) << 11 | (G >> 2) << 5 | B >> 3 : R << 16 | G << 8 | B;
        }
        return;
    }
    if (r->art != R3D_FENSTER) { ScreenPalette(0, 256, r->pal); return; }
    for (i = 0; i < 256; i++) {                   /* Fenster: feste Systempalette (VESA 6x6x6 + 16, VGA16, mono) */
        WORD R = r->pal[i * 3] << 2, G = r->pal[i * 3 + 1] << 2, B = r->pal[i * 3 + 2] << 2;
        if (r->dbpp == 8) r->map[i] = (BYTE)(16 + 36 * ((R + 25) / 51) + 6 * ((G + 25) / 51) + (B + 25) / 51);
        else if (r->dbpp == 4) { DWORD best = 0xFFFFFFFFUL, dd; WORD j;
            for (j = 0; j < 16; j++) { long dr = (long)R - std16[j][0], dg = (long)G - std16[j][1], db = (long)B - std16[j][2];
                dd = dr * dr + dg * dg + db * db; if (dd < best) { best = dd; r->map[i] = (BYTE)j; } } }
        else r->map[i] = (BYTE)((R * 3 + G * 6 + B) / 10 >= 100 ? 15 : 0);
    }
}
void R3dZustand(R3D *r, WORD z) { r->zustand = z; }
void R3dFarbe(R3D *r, BYTE farbe) { r->farbe = farbe; }
void R3dTextur(R3D *r, const BYTE *t, WORD lg) { r->tex = t; r->tl = (BYTE)lg; r->tm = (BYTE)((1 << lg) - 1); }
void R3dClear(R3D *r, BYTE farbe) { r->drv->clear(r, farbe); }
void R3dTri(R3D *r, const R3DVERT *a, const R3DVERT *b, const R3DVERT *c) { r->drv->tri(r, a, b, c); }
void R3dFlip(R3D *r) { r->drv->flip(r); r->bilder++; }
BYTE R3dPunkt(R3D *r, WORD x, WORD y) { return x < r->w && y < r->h ? r->bild[(DWORD)y * r->w + x] : 0; }
