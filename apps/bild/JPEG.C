/* JPEG.C - JPEG Baseline (SOF0/SOF1, Huffman, 8 Bit) fuer RDBILD: Graustufen und YCbCr mit Unterabtastung
 * 1/2/4 (4:4:4, 4:2:2, 4:2:0, 4:1:1), Neustartmarken, Exif-Lage. Verkleinert beim Entpacken: skal 8 nimmt nur
 * den Gleichanteil (keine IDCT), 2/4 mittelt die 8x8-IDCT. IDCT nach Arai/Agui/Nakajima in 16 Bit (8088). */
#include "BILD.H"

#ifdef __WATCOMC__
short mul8(short a, short b);
#pragma aux mul8 = "imul dx" "mov al,ah" "mov ah,dl" parm [ax] [dx] value [ax] modify exact [ax dx];
#else
#define mul8(a, b) ((short)(((long)(a) * (b)) >> 8))
#endif

typedef struct { WORD look[256]; WORD maxc[18]; WORD minc[17]; short vp[17]; BYTE val[256]; BYTE da; } HT;
static HT ht[8];                                  /* 0..3 DC, 4..7 AC */
static WORD qt[4][64]; static short aq[4][64];
typedef struct { BYTE id, h, v, tq, td, ta, hs, vs; short pred; BYTE FAR *pl; WORD pw; } KOMP;
static KOMP km[3]; static BYTE nk, hmax, vmax, adobe = 0xFF, jmark;
static WORD ri, mx, my;
static WORD jbb; static BYTE jbc;
static short crr[256], cbb[256], crg[256], cbg[256];
static const BYTE zz[64] = { 0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63 };
static const WORD aan[64] = { 16384,22725,21407,19266,16384,12873,8867,4520, 22725,31521,29692,26722,22725,17855,12299,6270,
    21407,29692,27969,25172,21407,16819,11585,5906, 19266,26722,25172,22654,19266,15137,10426,5315,
    16384,22725,21407,19266,16384,12873,8867,4520, 12873,17855,16819,15137,12873,10114,6967,3552,
    8867,12299,11585,10426,8867,6967,4799,2446, 4520,6270,5906,5315,4520,3552,2446,1247 };

static WORD seglen(void) { WORD a = RB(); return (WORD)((a << 8) | RB()); }
static WORD dht(WORD n)
{
    while (n > 17) {
        BYTE c = RB(), cnt[17]; WORD i, k, l, code = 0, tot = 0; HT *t;
        if ((c & 15) > 3 || (c >> 4) > 1) return BE_KAPUTT;
        t = &ht[(c >> 4) * 4 + (c & 15)];
        for (i = 1; i <= 16; i++) { cnt[i] = RB(); tot += cnt[i]; }
        if (tot > 256 || n < 17 + tot) return BE_KAPUTT;
        for (i = 0; i < tot; i++) t->val[i] = RB();
        for (i = 0; i < 256; i++) t->look[i] = 0;
        for (k = 0, l = 1; l <= 16; l++) {
            t->vp[l] = (short)k; t->minc[l] = code;
            for (i = 0; i < cnt[l]; i++, k++, code++)
                if (l <= 8) { WORD f = (WORD)(code << (8 - l)), e = f + (1U << (8 - l)); for (; f < e; f++) t->look[f] = (WORD)((l << 8) | t->val[k]); }
            t->maxc[l] = cnt[l] ? code - 1 : 0xFFFF;
            code <<= 1;
        }
        t->maxc[17] = 0xFFFF; t->da = 1;
        n -= 17 + tot;
    }
    return 0;
}
static WORD dqt(WORD n)
{
    while (n >= 65) {
        BYTE c = RB(); WORD i, t = c & 3;
        for (i = 0; i < 64; i++) qt[t][zz[i]] = (c >> 4) ? seglen() : RB();
        for (i = 0; i < 64; i++) aq[t][i] = (short)(((DWORD)qt[t][i] * aan[i] + 2048) >> 12);
        n -= (c >> 4) ? 129 : 65;
    }
    return 0;
}
static void exif(WORD n, BILD *b)                 /* APP1: Lage (Tag 0112h) aus IFD0 */
{
    static BYTE h[512]; WORD i, m = n > sizeof h ? sizeof h : n, be, ifd, ne;
    for (i = 0; i < m; i++) h[i] = RB();
    bl_skip((DWORD)(n - m));
    if (m < 14 || h[0] != 'E' || h[1] != 'x' || h[2] != 'i' || h[3] != 'f') return;
    be = h[6] == 'M';
#define R16(o) (be ? (WORD)((h[6 + (o)] << 8) | h[7 + (o)]) : (WORD)(h[6 + (o)] | (h[7 + (o)] << 8)))
    ifd = R16(4 + (be ? 2 : 0));                  /* Versatz (32 Bit, kleine Werte) */
    if (ifd + 2 > m - 6) return;
    ne = R16(ifd);
    for (i = 0; i < ne && ifd + 2 + 12 * i + 12 <= m - 6; i++) {
        WORD o = ifd + 2 + 12 * i;
        if (R16(o) == 0x0112) { WORD v = R16(o + 8); if (v >= 1 && v <= 8) b->dreh = (BYTE)v; }
    }
#undef R16
}
static BYTE marke(void)                           /* naechste Marke (FF xx) */
{
    BYTE c;
    do { c = RB(); if (bl_ende) return 0; } while (c != 0xFF);
    do { c = RB(); } while (c == 0xFF && !bl_ende);
    return c;
}
WORD JpgStart(BILD *b)
{
    BYTE m; WORD n, i;
    RB(); RB(); ri = 0; adobe = 0xFF;
    for (i = 0; i < 8; i++) ht[i].da = 0;
    for (;;) {
        m = marke(); if (!m) return BE_KAPUTT;
        if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01) continue;
        n = seglen(); if (n < 2) return BE_KAPUTT; n -= 2;
        if (m == 0xC0 || m == 0xC1) {
            if (RB() != 8) return BE_NICHT;
            b->qh = seglen(); b->qw = seglen(); nk = RB();
            if (nk != 1 && nk != 3) return BE_NICHT;
            hmax = vmax = 1;
            for (i = 0; i < nk; i++) {
                BYTE f; km[i].id = RB(); f = RB(); km[i].h = (BYTE)(f >> 4); km[i].v = (BYTE)(f & 15); km[i].tq = (BYTE)(RB() & 3);
                if (!km[i].h || !km[i].v || km[i].h > 4 || km[i].v > 4) return BE_KAPUTT;
                if (km[i].h > hmax) hmax = km[i].h;
                if (km[i].v > vmax) vmax = km[i].v;
            }
            if (nk == 1) { km[0].h = km[0].v = 1; hmax = vmax = 1; }
            for (i = 0; i < nk; i++) {
                BYTE a = (BYTE)(hmax / km[i].h), c = (BYTE)(vmax / km[i].v);
                if (hmax % km[i].h || vmax % km[i].v || a == 3 || c == 3 || a > 4 || c > 4) return BE_NICHT;
                km[i].hs = (BYTE)(a == 4 ? 2 : a == 2 ? 1 : 0); km[i].vs = (BYTE)(c == 4 ? 2 : c == 2 ? 1 : 0);
            }
            if (!b->qw || !b->qh) return BE_KAPUTT;
            b->farben = (BYTE)(nk == 1 ? 8 : 24);
            b->w = b->qw; b->h = b->qh;
            return 0;
        }
        if ((m >= 0xC2 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC)) return BE_NICHT;   /* progressiv, arithmetisch, verlustfrei */
        if (m == 0xC4) { if (dht(n)) return BE_KAPUTT; continue; }
        if (m == 0xDB) { dqt(n); continue; }
        if (m == 0xDD) { ri = seglen(); bl_skip(n - 2); continue; }
        if (m == 0xE1) { exif(n, b); continue; }
        if (m == 0xEE && n >= 12) { for (i = 0; i < 11; i++) RB(); adobe = RB(); bl_skip(n - 12); continue; }
        if (m == 0xD9 || m == 0xDA) return BE_KAPUTT;
        bl_skip(n);
    }
}
/* ---------------- Entropie ---------------- */
static BYTE jbyte(void)
{
    BYTE c;
    if (jmark) return 0;
    c = RB();
    if (c == 0xFF) { BYTE d; do d = RB(); while (d == 0xFF && !bl_ende); if (!d) return 0xFF; jmark = d ? d : 0xD9; return 0; }
    if (bl_ende) jmark = 0xD9;
    return c;
}
#define FILL() while (jbc <= 8) { jbb |= (WORD)jbyte() << (8 - jbc); jbc += 8; }
static WORD bits(BYTE n)
{
    WORD v;
    if (!n) return 0;
    if (n > 8) { v = bits((BYTE)(n - 8)) << 8; return v | bits(8); }
    FILL();
    v = jbb >> (16 - n); jbb <<= n; jbc = (BYTE)(jbc - n);
    return v;
}
static BYTE huff(HT *t)
{
    WORD e, code; BYTE l;
    FILL();
    e = t->look[jbb >> 8];
    if (e) { l = (BYTE)(e >> 8); jbb <<= l; jbc = (BYTE)(jbc - l); return (BYTE)e; }
    code = bits(8);
    for (l = 9; l <= 16; l++) {
        code = (WORD)((code << 1) | bits(1));
        if (t->maxc[l] != 0xFFFF && code <= t->maxc[l]) return t->val[t->vp[l] + code - t->minc[l]];
    }
    return 0;
}
static short ext(WORD v, BYTE s) { return (short)(v < (1U << (s - 1)) ? (short)v - (short)((1U << s) - 1) : (short)v); }

/* ---------------- IDCT ---------------- */
static short blk[64], ws[64];
static void idct(const short *q, BYTE FAR *o, WORD ow, BYTE sk)   /* 8x8 -> o (Zeilenabstand ow), sk 1/2/4 */
{
    short t0, t1, t2, t3, t4, t5, t6, t7, t10, t11, t12, t13, z5, z10, z11, z12, z13, *p, *w;
    BYTE r[64]; WORD i, x, y;
    for (i = 0; i < 8; i++) {                     /* Spalten */
        p = blk + i; w = ws + i;
        if (!(p[8] | p[16] | p[24] | p[32] | p[40] | p[48] | p[56])) {
            short d = (short)(p[0] * q[i]);
            w[0] = w[8] = w[16] = w[24] = w[32] = w[40] = w[48] = w[56] = d; continue;
        }
        t0 = (short)(p[0] * q[i]); t1 = (short)(p[16] * q[16 + i]); t2 = (short)(p[32] * q[32 + i]); t3 = (short)(p[48] * q[48 + i]);
        t10 = t0 + t2; t11 = t0 - t2; t13 = t1 + t3; t12 = mul8(t1 - t3, 362) - t13;
        t0 = t10 + t13; t3 = t10 - t13; t1 = t11 + t12; t2 = t11 - t12;
        t4 = (short)(p[8] * q[8 + i]); t5 = (short)(p[24] * q[24 + i]); t6 = (short)(p[40] * q[40 + i]); t7 = (short)(p[56] * q[56 + i]);
        z13 = t6 + t5; z10 = t6 - t5; z11 = t4 + t7; z12 = t4 - t7;
        t7 = z11 + z13; t11 = mul8(z11 - z13, 362);
        z5 = mul8(z10 + z12, 473); t10 = mul8(z12, 277) - z5; t12 = mul8(z10, -669) + z5;
        t6 = t12 - t7; t5 = t11 - t6; t4 = t10 + t5;
        w[0] = t0 + t7; w[56] = t0 - t7; w[8] = t1 + t6; w[48] = t1 - t6; w[16] = t2 + t5; w[40] = t2 - t5; w[32] = t3 + t4; w[24] = t3 - t4;
    }
    for (i = 0; i < 8; i++) {                     /* Zeilen */
        BYTE *rr = r + 8 * i; short v;
        w = ws + 8 * i;
#define AUS(k, e) { v = (short)(((e) + 16) >> 5) + 128; rr[k] = (BYTE)(v < 0 ? 0 : v > 255 ? 255 : v); }
        t10 = w[0] + w[4]; t11 = w[0] - w[4]; t13 = w[2] + w[6]; t12 = mul8(w[2] - w[6], 362) - t13;
        t0 = t10 + t13; t3 = t10 - t13; t1 = t11 + t12; t2 = t11 - t12;
        z13 = w[5] + w[3]; z10 = w[5] - w[3]; z11 = w[1] + w[7]; z12 = w[1] - w[7];
        t7 = z11 + z13; t11 = mul8(z11 - z13, 362);
        z5 = mul8(z10 + z12, 473); t10 = mul8(z12, 277) - z5; t12 = mul8(z10, -669) + z5;
        t6 = t12 - t7; t5 = t11 - t6; t4 = t10 + t5;
        AUS(0, t0 + t7); AUS(7, t0 - t7); AUS(1, t1 + t6); AUS(6, t1 - t6); AUS(2, t2 + t5); AUS(5, t2 - t5); AUS(4, t3 + t4); AUS(3, t3 - t4);
#undef AUS
    }
    if (sk == 1) { for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) o[y * ow + x] = r[8 * y + x]; return; }
    for (y = 0; y < 8 / sk; y++) for (x = 0; x < 8 / sk; x++) {
        WORD s = 0, a, c;
        for (a = 0; a < sk; a++) for (c = 0; c < sk; c++) s += r[8 * (y * sk + a) + x * sk + c];
        o[y * ow + x] = (BYTE)(s / (sk * sk));
    }
}
/* ---------------- Bild ---------------- */
static BYTE FAR *zrgb;
static WORD neustart(void)
{
    WORD i;
    jbb = 0; jbc = 0;
    if (!jmark) { BYTE m = marke(); jmark = m; }
    if (jmark >= 0xD0 && jmark <= 0xD7) jmark = 0;
    for (i = 0; i < nk; i++) km[i].pred = 0;
    return 0;
}
WORD JpgLesen(BILD *b)
{
    BYTE m, sk = b->skal, bs; WORD n, i, ns, x, y, my_, mx_, k, r, rows; DWORD mcu = 0;
    if (sk != 1 && sk != 2 && sk != 4 && sk != 8) sk = 1;
    bs = (BYTE)(8 / sk);
    b->w = (WORD)((b->qw + sk - 1) / sk); b->h = (WORD)((b->qh + sk - 1) / sk);
    for (;;) {                                    /* bis zum Scan: Tabellen */
        m = marke(); if (!m) return BE_KAPUTT;
        if (m >= 0xD0 && m <= 0xD8) continue;
        n = seglen(); if (n < 2) return BE_KAPUTT; n -= 2;
        if (m == 0xDA) break;
        if (m == 0xC4) { if (dht(n)) return BE_KAPUTT; }
        else if (m == 0xDB) dqt(n);
        else if (m == 0xDD) { ri = seglen(); bl_skip(n - 2); }
        else if (m == 0xD9) return BE_KAPUTT;
        else bl_skip(n);
    }
    ns = RB();
    if (ns != nk) return BE_NICHT;                /* nur ein Scan mit allen Komponenten */
    for (i = 0; i < ns; i++) {
        BYTE id = RB(), t = RB(); WORD j;
        for (j = 0; j < nk && km[j].id != id; j++) ;
        if (j == nk) return BE_KAPUTT;
        km[j].td = (BYTE)(t >> 4 & 3); km[j].ta = (BYTE)(4 + (t & 3));
        if (!ht[km[j].td].da || !ht[km[j].ta].da) return BE_KAPUTT;
    }
    RB(); RB(); RB();
    mx = (WORD)((b->qw + 8U * hmax - 1) / (8U * hmax)); my = (WORD)((b->qh + 8U * vmax - 1) / (8U * vmax));
    for (i = 0; i < nk; i++) {
        km[i].pw = (WORD)(mx * km[i].h * bs);
        if ((DWORD)km[i].pw * km[i].v * bs > 65000UL) return BE_GROSS;
        if (!(km[i].pl = (BYTE FAR *)BMem((DWORD)km[i].pw * km[i].v * bs))) return BE_SPEICHER;
        km[i].pred = 0;
    }
    if (!(zrgb = (BYTE FAR *)BMem(3UL * b->w + 3))) return BE_SPEICHER;
    for (i = 0; i < 256; i++) {
        short c = (short)i - 128;
        crr[i] = (short)((c * 359L + 128) >> 8); cbb[i] = (short)((c * 454L + 128) >> 8);
        crg[i] = (short)((-c * 183L + 128) >> 8); cbg[i] = (short)((-c * 88L + 128) >> 8);
    }
    jbb = 0; jbc = 0; jmark = 0;
    rows = (WORD)(vmax * bs);
    for (my_ = 0; my_ < my; my_++) {
        for (mx_ = 0; mx_ < mx; mx_++) {
            if (ri && mcu && mcu % ri == 0) neustart();
            mcu++;
            for (i = 0; i < nk; i++) {
                KOMP *c = &km[i]; WORD bx, by;
                HT *hd = &ht[c->td], *ha = &ht[c->ta];
                for (by = 0; by < c->v; by++) for (bx = 0; bx < c->h; bx++) {
                    BYTE s, ac = 0; short d;
                    s = huff(hd); d = s ? ext(bits(s), s) : 0;
                    c->pred += d;
                    if (sk == 8) {
                        for (k = 1; k < 64; ) { BYTE t = huff(ha), rr = (BYTE)(t >> 4); s = (BYTE)(t & 15); if (s) { k += rr; bits(s); k++; } else if (rr == 15) k += 16; else break; }
                        {   long v = ((long)c->pred * qt[c->tq][0] + 4) / 8 + 128;
                            c->pl[(WORD)by * c->pw + (mx_ * c->h + bx)] = (BYTE)(v < 0 ? 0 : v > 255 ? 255 : v); }
                        continue;
                    }
                    for (k = 0; k < 64; k++) blk[k] = 0;
                    blk[0] = c->pred;
                    for (k = 1; k < 64; ) {
                        BYTE t = huff(ha), rr = (BYTE)(t >> 4); s = (BYTE)(t & 15);
                        if (s) { k += rr; if (k > 63) break; blk[zz[k]] = ext(bits(s), s); ac = 1; k++; }
                        else if (rr == 15) k += 16; else break;
                    }
                    {   BYTE FAR *o = c->pl + (WORD)(by * bs) * c->pw + (mx_ * c->h + bx) * bs;
                        if (!ac) {                    /* nur Gleichanteil: Flaeche */
                            short v = (short)(((long)c->pred * qt[c->tq][0] + 4) / 8 + 128); BYTE f; WORD a;
                            f = (BYTE)(v < 0 ? 0 : v > 255 ? 255 : v);
                            for (y = 0; y < bs; y++) for (a = 0; a < bs; a++) o[y * c->pw + a] = f;
                        } else idct(aq[c->tq], o, c->pw, sk);
                    }
                }
            }
        }
        for (r = 0; r < rows; r++) {              /* Zeilen dieses MCU-Streifens nach RGB */
            WORD yy = (WORD)(my_ * rows + r);
            BYTE FAR *py, FAR *pb, FAR *pr; BYTE FAR *z = zrgb;
            if (yy >= b->h) break;
            py = km[0].pl + (WORD)(r >> km[0].vs) * km[0].pw;
            if (nk == 1) { for (x = 0; x < b->w; x++) { z[0] = z[1] = z[2] = py[x]; z += 3; } }
            else {
                BYTE h1 = km[1].hs, h2 = km[2].hs;
                pb = km[1].pl + (WORD)(r >> km[1].vs) * km[1].pw; pr = km[2].pl + (WORD)(r >> km[2].vs) * km[2].pw;
                for (x = 0; x < b->w; x++) {
                    short Y = py[x >> km[0].hs], cb = pb[x >> h1], cr = pr[x >> h2], v;
                    if (adobe == 0) { z[0] = (BYTE)Y; z[1] = (BYTE)cb; z[2] = (BYTE)cr; z += 3; continue; }
                    v = Y + crr[cr]; z[0] = (BYTE)(v < 0 ? 0 : v > 255 ? 255 : v);
                    v = Y + cbg[cb] + crg[cr]; z[1] = (BYTE)(v < 0 ? 0 : v > 255 ? 255 : v);
                    v = Y + cbb[cb]; z[2] = (BYTE)(v < 0 ? 0 : v > 255 ? 255 : v);
                    z += 3;
                }
            }
            if (!BZeile(yy, zrgb)) return BE_ABBRUCH;
        }
    }
    return 0;
}
