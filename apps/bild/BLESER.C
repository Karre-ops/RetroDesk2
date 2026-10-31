/* BLESER.C - Lesepuffer und Decoder fuer BMP (1/4/8/16/24/32 Bit, Bitfelder, OS/2), PCX (1, 4 Ebenen, 8 Bit,
 * 24 Bit) und GIF (87a/89a, erstes Bild, verschachtelt). JPEG steht in JPEG.C. */
#include "BILD.H"

BYTE FAR *bl_puf; WORD bl_n, bl_p; BYTE bl_ende; static DWORD bl_basis;
BYTE bl_fuell(void)
{
    bl_basis += bl_n; bl_p = 0;
    bl_n = bl_ende ? 0 : BRead(bl_puf, 4096);
    if (!bl_n) { bl_ende = 1; return 0; }
    bl_p = 1; return bl_puf[0];
}
static void bl_seek(DWORD pos) { BSeek(pos); bl_basis = pos; bl_n = bl_p = 0; bl_ende = 0; }
void bl_skip(DWORD n)
{
    if (n <= (DWORD)(bl_n - bl_p)) bl_p += (WORD)n;
    else bl_seek(bl_basis + bl_p + n);
}
WORD bl_w(void) { WORD a = RB(); return (WORD)(a | (RB() << 8)); }
DWORD bl_l(void) { WORD a = bl_w(); return a | ((DWORD)bl_w() << 16); }

static BYTE FAR *zr;                              /* RGB-Zeile (BMAXW * 3) */
static BYTE FAR *zq;                              /* Quellzeile */
static BYTE pal[768];

/* ---------------- BMP ---------------- */
static DWORD bmoff, msk[3]; static WORD bbpp, bkomp, stride; static BYTE unten, bsh[3], bbits[3];
static void maske(WORD i)
{
    DWORD m = msk[i]; BYTE s = 0, n = 0;
    if (!m) { bsh[i] = 0; bbits[i] = 0; return; }
    while (!(m & 1)) { m >>= 1; s++; }
    while (m & 1) { m >>= 1; n++; }
    bsh[i] = s; bbits[i] = n;
}
static BYTE kanal(DWORD p, WORD i)
{
    DWORD v = (p & msk[i]) >> bsh[i]; BYTE n = bbits[i];
    if (!n) return 0;
    if (n >= 8) return (BYTE)(v >> (n - 8));
    return (BYTE)((v * 255) / ((1UL << n) - 1));
}
static WORD bmp_start(BILD *b)
{
    DWORD hs, ncol, i; long hh;
    bl_w(); bl_l(); bl_l(); bmoff = bl_l(); hs = bl_l();
    if (hs == 12) { b->qw = bl_w(); hh = (short)bl_w(); bl_w(); bbpp = bl_w(); bkomp = 0; ncol = 0; }
    else if (hs >= 40) {
        long ww = (long)bl_l(); hh = (long)bl_l(); bl_w(); bbpp = bl_w(); bkomp = (WORD)bl_l(); bl_l(); bl_l(); bl_l(); ncol = bl_l(); bl_l();
        if (ww <= 0 || ww > 65535L) return BE_KAPUTT;
        b->qw = (WORD)ww;
        if (hs == 40) { if (bkomp == 3) { msk[0] = bl_l(); msk[1] = bl_l(); msk[2] = bl_l(); } }
        else if (hs >= 52) { msk[0] = bl_l(); msk[1] = bl_l(); msk[2] = bl_l(); bl_skip(hs - 52); }
        else bl_skip(hs - 40);
    } else return BE_FORMAT;
    if (bkomp != 0 && bkomp != 3) return BE_NICHT;  /* RLE, JPEG im BMP */
    unten = 1; if (hh < 0) { unten = 0; hh = -hh; }
    if (!hh || hh > 65535L || !b->qw) return BE_KAPUTT;
    b->qh = (WORD)hh;
    if (bbpp != 1 && bbpp != 4 && bbpp != 8 && bbpp != 16 && bbpp != 24 && bbpp != 32) return BE_NICHT;
    if (bbpp <= 8) {
        if (!ncol || ncol > (1UL << bbpp)) ncol = 1UL << bbpp;
        for (i = 0; i < 768; i++) pal[i] = 0;
        for (i = 0; i < ncol; i++) { pal[3 * i + 2] = RB(); pal[3 * i + 1] = RB(); pal[3 * i] = RB(); if (hs != 12) RB(); }
    } else if (bkomp == 0) {
        if (bbpp == 16) { msk[0] = 0x7C00; msk[1] = 0x03E0; msk[2] = 0x001F; }
        else { msk[0] = 0xFF0000UL; msk[1] = 0xFF00; msk[2] = 0xFF; }
    }
    maske(0); maske(1); maske(2);
    stride = (WORD)((((DWORD)b->qw * bbpp + 31) / 32) * 4);
    b->farben = (BYTE)bbpp;
    return 0;
}
static WORD bmp_lesen(BILD *b)
{
    WORD r, x, i; BYTE FAR *q;
    bl_seek(bmoff);
    for (r = 0; r < b->qh; r++) {
        for (i = 0; i < stride; i++) zq[i] = RB();
        if (bl_ende) return BE_KAPUTT;
        q = zq;
        for (x = 0; x < b->qw; x++) {
            BYTE FAR *z = zr + 3 * x; WORD c;
            switch (bbpp) {
            case 1: c = (q[x >> 3] >> (7 - (x & 7))) & 1; goto palt;
            case 4: c = (x & 1) ? q[x >> 1] & 15 : q[x >> 1] >> 4; goto palt;
            case 8: c = q[x];
            palt:   z[0] = pal[3 * c]; z[1] = pal[3 * c + 1]; z[2] = pal[3 * c + 2]; break;
            case 24: z[0] = q[3 * x + 2]; z[1] = q[3 * x + 1]; z[2] = q[3 * x]; break;
            default: {
                DWORD p = bbpp == 16 ? (DWORD)(q[2 * x] | (q[2 * x + 1] << 8)) : (q[4 * x] | ((DWORD)q[4 * x + 1] << 8) | ((DWORD)q[4 * x + 2] << 16) | ((DWORD)q[4 * x + 3] << 24));
                if (bbpp == 32 && bkomp == 0) { z[0] = q[4 * x + 2]; z[1] = q[4 * x + 1]; z[2] = q[4 * x]; }
                else { z[0] = kanal(p, 0); z[1] = kanal(p, 1); z[2] = kanal(p, 2); }
            } }
        }
        if (!BZeile((WORD)(unten ? b->qh - 1 - r : r), zr)) return BE_ABBRUCH;
    }
    return 0;
}
/* ---------------- PCX ---------------- */
static WORD pbpp, pebn, pbpl; static BYTE ppal16[48];
static WORD pcx_start(BILD *b)
{
    WORD x0, y0, x1, y1, i;
    RB(); RB(); if (RB() != 1) return BE_NICHT;
    pbpp = RB(); x0 = bl_w(); y0 = bl_w(); x1 = bl_w(); y1 = bl_w(); bl_w(); bl_w();
    for (i = 0; i < 48; i++) ppal16[i] = RB();
    RB(); pebn = RB(); pbpl = bl_w();
    if (x1 < x0 || y1 < y0 || !pbpl || bl_ende) return BE_KAPUTT;
    b->qw = x1 - x0 + 1; b->qh = y1 - y0 + 1;
    if (!((pbpp == 8 && (pebn == 1 || pebn == 3)) || (pbpp == 1 && (pebn == 1 || pebn == 4)))) return BE_NICHT;
    if ((DWORD)pbpl * pebn > 3UL * BMAXW) return BE_GROSS;
    b->farben = (BYTE)(pbpp * pebn);
    if (pbpp == 8 && pebn == 1) {                 /* 256er-Palette am Dateiende */
        DWORD g = BSize();
        if (g < 769 + 128) return BE_KAPUTT;
        bl_seek(g - 769);
        if (RB() != 12) return BE_KAPUTT;
        for (i = 0; i < 768; i++) pal[i] = RB();
    }
    return 0;
}
static WORD pcx_lesen(BILD *b)
{
    WORD y, i, n = pebn * pbpl, x; BYTE c = 0, k = 0;
    bl_seek(128);
    for (y = 0; y < b->qh; y++) {
        for (i = 0; i < n; ) {
            if (!k) { c = RB(); if ((c & 0xC0) == 0xC0) { k = (BYTE)(c & 0x3F); c = RB(); } else k = 1; }
            while (k && i < n) { zq[i++] = c; k--; }
        }
        if (bl_ende) return BE_KAPUTT;
        for (x = 0; x < b->qw; x++) {
            BYTE FAR *z = zr + 3 * x;
            if (pbpp == 8 && pebn == 1) { WORD v = zq[x]; z[0] = pal[3 * v]; z[1] = pal[3 * v + 1]; z[2] = pal[3 * v + 2]; }
            else if (pbpp == 8) { z[0] = zq[x]; z[1] = zq[pbpl + x]; z[2] = zq[2 * pbpl + x]; }
            else if (pebn == 1) { BYTE v = (BYTE)((zq[x >> 3] >> (7 - (x & 7))) & 1 ? 255 : 0); z[0] = z[1] = z[2] = v; }
            else {
                WORD v = 0, e;
                for (e = 0; e < 4; e++) v |= ((zq[e * pbpl + (x >> 3)] >> (7 - (x & 7))) & 1) << e;
                z[0] = ppal16[3 * v]; z[1] = ppal16[3 * v + 1]; z[2] = ppal16[3 * v + 2];
            }
        }
        if (!BZeile(y, zr)) return BE_ABBRUCH;
    }
    return 0;
}
/* ---------------- GIF ---------------- */
static WORD glace; static BYTE gmcs, gblk;
static WORD gif_start(BILD *b)
{
    BYTE f, c; WORD i, n;
    bl_skip(6); bl_w(); bl_w(); f = RB(); RB(); RB();
    if (f & 0x80) { n = 3 << ((f & 7) + 1); for (i = 0; i < n; i++) pal[i] = RB(); }
    for (;;) {
        c = RB();
        if (bl_ende) return BE_KAPUTT;
        if (c == 0x21) { RB(); while ((n = RB()) != 0 && !bl_ende) bl_skip(n); continue; }
        if (c == 0x2C) break;
        if (c == 0x3B) return BE_KAPUTT;
    }
    bl_w(); bl_w(); b->qw = bl_w(); b->qh = bl_w(); f = RB();
    if (f & 0x80) { n = 3 << ((f & 7) + 1); for (i = 0; i < n; i++) pal[i] = RB(); }
    glace = f & 0x40;
    gmcs = RB();
    if (!b->qw || !b->qh || gmcs < 1 || gmcs > 11 || bl_ende) return BE_KAPUTT;
    b->farben = 8;
    return 0;
}
static BYTE gb(void)                              /* Byte aus den Datenbloecken */
{
    if (!gblk) { gblk = RB(); if (!gblk) { bl_ende = 1; return 0; } }
    gblk--; return RB();
}
static WORD gif_lesen(BILD *b)
{
    WORD FAR *pre; BYTE FAR *suf, FAR *stk;
    WORD clear = 1U << gmcs, eoi = clear + 1, next = clear + 2, csz = gmcs + 1, cmask = (1U << (gmcs + 1)) - 1;
    WORD old = 0xFFFF, code, in, sp, x = 0, y = 0, pass = 0; DWORD bits = 0; BYTE nb = 0, first = 0;
    static const BYTE ps[4] = { 0, 4, 2, 1 }, st[4] = { 8, 8, 4, 2 };
    if (!(pre = (WORD FAR *)BMem(8192)) || !(suf = (BYTE FAR *)BMem(4096)) || !(stk = (BYTE FAR *)BMem(4097))) return BE_SPEICHER;
    gblk = 0;
    for (;;) {
        while (nb < csz) { bits |= (DWORD)gb() << nb; nb += 8; if (bl_ende) break; }
        if (bl_ende && nb < csz) break;
        code = (WORD)(bits & cmask); bits >>= csz; nb = (BYTE)(nb - csz);
        if (code == clear) { next = clear + 2; csz = gmcs + 1; cmask = (1U << csz) - 1; old = 0xFFFF; continue; }
        if (code == eoi) break;
        sp = 0; in = code;
        if (old == 0xFFFF) { if (code >= clear) return BE_KAPUTT; stk[sp++] = (BYTE)code; first = (BYTE)code; old = code; }
        else {
            if (code > next) return BE_KAPUTT;
            if (code == next) { stk[sp++] = first; code = old; }
            while (code >= clear) { if (sp >= 4096) return BE_KAPUTT; stk[sp++] = suf[code]; code = pre[code]; }
            first = (BYTE)code; stk[sp++] = first;
            if (next < 4096) { pre[next] = old; suf[next] = first; next++; if (next == cmask + 1 && csz < 12) { csz++; cmask = (1U << csz) - 1; } }
            old = in;
        }
        while (sp) {
            BYTE v = stk[--sp];
            if (y < b->qh) { zr[3 * x] = pal[3 * v]; zr[3 * x + 1] = pal[3 * v + 1]; zr[3 * x + 2] = pal[3 * v + 2]; }
            if (++x == b->qw) {
                x = 0;
                if (y < b->qh && !BZeile(y, zr)) return BE_ABBRUCH;
                if (glace) { y += st[pass]; while (y >= b->qh && pass < 3) { pass++; y = ps[pass]; } if (y >= b->qh) y = b->qh; }
                else y++;
                if (y >= b->qh) return 0;
            }
        }
    }
    return y >= b->qh ? 0 : (y > 0 ? 0 : BE_KAPUTT);   /* kurze Bilder: was da ist, zeigen */
}
/* ---------------- Verteiler ---------------- */
WORD BildStart(BILD *b)
{
    BYTE a, c; WORD r;
    b->dreh = 1; b->skal = 1; b->farben = 24;
    if (!bl_puf && !(bl_puf = (BYTE FAR *)BMem(4096))) return BE_SPEICHER;
    bl_seek(0);
    a = RB(); c = RB(); bl_seek(0);
    if (a == 'B' && c == 'M') { b->art = BA_BMP; r = bmp_start(b); }
    else if (a == 10 && c <= 5) { b->art = BA_PCX; r = pcx_start(b); }
    else if (a == 'G' && c == 'I') { b->art = BA_GIF; r = gif_start(b); }
    else if (a == 0xFF && c == 0xD8) { b->art = BA_JPG; return JpgStart(b); }
    else return BE_FORMAT;
    if (!r && b->qw > BMAXW) r = BE_GROSS;
    b->w = b->qw; b->h = b->qh;
    return r;
}
WORD BildLesen(BILD *b)
{
    if (b->art == BA_JPG) return JpgLesen(b);
    if (!(zr = (BYTE FAR *)BMem(3UL * b->qw)) || !(zq = (BYTE FAR *)BMem(4UL * b->qw + 16))) return BE_SPEICHER;
    if (b->art == BA_BMP) return bmp_lesen(b);
    if (b->art == BA_PCX) return pcx_lesen(b);
    return gif_lesen(b);
}
