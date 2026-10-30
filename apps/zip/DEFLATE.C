/* DEFLATE.C - Packen (Deflate, RFC 1951) fuer RDZIP, auf Tempo fuer den 8088 getrimmt:
 * Fenster 8 KB, Streutabelle 4096 Eintraege (3 Bytes), gierige Suche mit hoechstens 8 Kettengliedern,
 * Bloecke zu 8192 Symbolen mit eigenen oder festen Huffman-Codes (je nachdem, was kuerzer ist).
 * Speicher (vom Aufrufer, fern): Daten 16 KB, prev 16 KB, head 8 KB, Symbole 24 KB + Ausgabe 4 KB. */
#include "ZK.H"

#define W      DEF_FENSTER
#define WMASK  (W - 1)
#define NIL    0xFFFFU
#define MINLA  262U                               /* MAX_MATCH + MIN_MATCH + 1 */
#define MAXD   (W - MINLA)
#define KETTE  8

static ZLESEN le; static ZSCHREIB sc; static void *uu;
static BYTE FAR *d; static WORD FAR *pv; static WORD FAR *hd; static BYTE FAR *sy; static BYTE FAR *ob;
static WORD on; static DWORD ausn; static BYTE fehler;
static WORD bb; static BYTE bc;
static WORD fl[286], fd[30], fc[19];
static BYTE ll[320], lc[19];
static WORD cl[286], cd[30], cc[19];             /* Codes (Bits umgedreht) */
static WORD ns;

static void obyte(BYTE b)
{
    ob[on++] = b;
    if (on == 4096) { if (!fehler && sc(uu, ob, on) != on) fehler = 1; ausn += on; on = 0; }
}
static void put(WORD v, BYTE n)
{
    if (!n) return;
    bb |= v << bc; bc = (BYTE)(bc + n);
    if (bc >= 16) { obyte((BYTE)bb); obyte((BYTE)(bb >> 8)); bc -= 16; bb = bc ? (WORD)(v >> (n - bc)) : 0; }
}
static void putflush(void)
{
    while (bc > 0) { obyte((BYTE)bb); bb >>= 8; bc = (BYTE)(bc > 8 ? bc - 8 : 0); }
    bb = 0;
    if (on) { if (!fehler && sc(uu, ob, on) != on) fehler = 1; ausn += on; on = 0; }
}

/* Huffman-Laengen hoechstens mx Bits */
static WORD hw[2 * 286], hp[2 * 286], hs[286]; static BYTE hdp[2 * 286];
static void laengen(const WORD *f, WORD n, BYTE *len, BYTE mx)
{
    WORD m, i, j, k, a, b, t, fx[286]; BYTE ok;
    for (i = 0; i < n; i++) fx[i] = f[i];
    for (;;) {
        for (m = 0, i = 0; i < n; i++) { len[i] = 0; if (fx[i]) hs[m++] = i; }
        if (!m) return;
        if (m == 1) { len[hs[0]] = 1; return; }
        for (i = 1; i < m; i++) {                 /* nach Haeufigkeit sortieren */
            t = hs[i];
            for (j = i; j && fx[hs[j - 1]] > fx[t]; j--) hs[j] = hs[j - 1];
            hs[j] = t;
        }
        for (i = 0; i < m; i++) hw[i] = fx[hs[i]];
        i = 0; j = m;                             /* i: naechstes Blatt, j: naechster innerer Knoten */
        for (k = m; k < 2 * m - 1; k++) {
            if (i < m && (j >= k || hw[i] <= hw[j])) a = i++; else a = j++;
            if (i < m && (j >= k || hw[i] <= hw[j])) b = i++; else b = j++;
            hw[k] = hw[a] + hw[b]; hp[a] = hp[b] = k;
        }
        hdp[2 * m - 2] = 0; ok = 1;
        for (k = 2 * m - 2; k-- > 0; ) { hdp[k] = (BYTE)(hdp[hp[k]] + 1); if (k < m && hdp[k] > mx) ok = 0; }
        if (ok) { for (i = 0; i < m; i++) len[hs[i]] = hdp[i]; return; }
        for (i = 0; i < n; i++) if (fx[i]) fx[i] = (WORD)((fx[i] >> 1) | 1);
    }
}
static void codes(const BYTE *len, WORD n, WORD *c)
{
    WORD cnt[16], next[16], i, k, v, r, code = 0;
    for (i = 0; i < 16; i++) cnt[i] = 0;
    for (i = 0; i < n; i++) cnt[len[i]]++;
    cnt[0] = 0;
    for (i = 1; i < 16; i++) { code = (WORD)((code + cnt[i - 1]) << 1); next[i] = code; }
    for (i = 0; i < n; i++) {
        if (!len[i]) { c[i] = 0; continue; }
        v = next[len[i]]++;
        for (r = 0, k = 0; k < len[i]; k++) { r = (WORD)((r << 1) | (v & 1)); v >>= 1; }
        c[i] = r;
    }
}
static const WORD lbase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
static const BYTE lext[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const WORD dbase[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const BYTE dext[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
static const BYTE ord[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
static BYTE lcode[256];                           /* Laenge-3 -> Code-257 */
static BYTE dcode[256], dcode2[256];             /* Abstand-1: < 256 direkt, sonst dcode2[(d-1) >> 7] */
static void tabellen(void)
{
    WORD i, k, b, e;
    for (i = 0; i < 29; i++) for (k = lbase[i] - 3; k < 256 && k < (WORD)(lbase[i] - 3 + (1U << lext[i])); k++) lcode[k] = (BYTE)i;
    lcode[255] = 28;
    for (i = 0; i < 30; i++) {
        b = dbase[i] - 1; e = (WORD)(b + (1U << dext[i]) - 1);
        if (b < 256) for (k = b; k <= e; k++) dcode[k] = (BYTE)i;
        else for (k = b >> 7; k <= (e >> 7); k++) dcode2[k] = (BYTE)i;
    }
}
static BYTE dc(WORD dist)                         /* Abstand (1..32768) -> Code */
{
    WORD x = dist - 1;
    return x < 256 ? dcode[x] : dcode2[x >> 7];
}
/* Laengen in RLE (16/17/18): rs = Symbol, rx = Zusatz */
static BYTE rs[316], rx[316]; static WORD nr;
static void rle(const BYTE *l, WORD n)
{
    WORD i = 0, k;
    nr = 0;
    while (i < n) {
        BYTE v = l[i];
        for (k = 1; i + k < n && l[i + k] == v && k < 138; k++) ;
        if (!v && k >= 3) { if (k <= 10) { rs[nr] = 17; rx[nr++] = (BYTE)(k - 3); } else { rs[nr] = 18; rx[nr++] = (BYTE)(k - 11); } i += k; continue; }
        rs[nr] = v; rx[nr++] = 0; i++; k--;
        while (v && k >= 3) { WORD r = k > 6 ? 6 : k; rs[nr] = 16; rx[nr++] = (BYTE)(r - 3); i += r; k -= r; }
    }
}
static void bloc(BYTE fin)
{
    WORD i, nl, nd, nc; DWORD dyn, fix; BYTE fixd = 0;
    fl[256]++;
    laengen(fl, 286, ll, 15);
    laengen(fd, 30, ll + 286, 15);
    for (nl = 286; nl > 257 && !ll[nl - 1]; nl--) ;
    for (nd = 30; nd > 1 && !ll[286 + nd - 1]; nd--) ;
    for (i = nl; i < nl + nd; i++) ll[i] = ll[286 + i - nl];   /* Abstaende direkt hinter die Laengen */
    rle(ll, (WORD)(nl + nd));
    for (i = 0; i < 19; i++) fc[i] = 0;
    for (i = 0; i < nr; i++) fc[rs[i]]++;
    laengen(fc, 19, lc, 7);
    for (nc = 19; nc > 4 && !lc[ord[nc - 1]]; nc--) ;
    dyn = 14 + 3UL * nc; fix = 0;
    for (i = 0; i < nr; i++) dyn += lc[rs[i]] + (rs[i] == 16 ? 2 : rs[i] == 17 ? 3 : rs[i] == 18 ? 7 : 0);
    for (i = 0; i < 286; i++) {
        BYTE lx = (BYTE)(i >= 265 && i < 285 ? lext[i - 257] : 0);
        BYTE L = (BYTE)(i < nl ? ll[i] : 0);
        dyn += (DWORD)fl[i] * (L + lx);
        fix += (DWORD)fl[i] * ((i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8) + lx);
    }
    for (i = 0; i < 30; i++) { dyn += (DWORD)fd[i] * ((i < nd ? ll[nl + i] : 0) + dext[i]); fix += (DWORD)fd[i] * (5 + dext[i]); }
    if (fix <= dyn) {
        fixd = 1;
        for (i = 0; i < 144; i++) ll[i] = 8;
        for (; i < 256; i++) ll[i] = 9;
        for (; i < 280; i++) ll[i] = 7;
        for (; i < 288; i++) ll[i] = 8;
        nl = 288; codes(ll, 288, cl);
        for (i = 0; i < 30; i++) ll[288 + i] = 5;
        codes(ll + 288, 30, cd);
        put(fin, 1); put(1, 2);
    } else {
        codes(ll, nl, cl); codes(ll + nl, nd, cd); codes(lc, 19, cc);
        put(fin, 1); put(2, 2);
        put((WORD)(nl - 257), 5); put((WORD)(nd - 1), 5); put((WORD)(nc - 4), 4);
        for (i = 0; i < nc; i++) put(lc[ord[i]], 3);
        for (i = 0; i < nr; i++) {
            put(cc[rs[i]], lc[rs[i]]);
            if (rs[i] == 16) put(rx[i], 2); else if (rs[i] == 17) put(rx[i], 3); else if (rs[i] == 18) put(rx[i], 7);
        }
    }
    {   BYTE *ld = fixd ? ll + 288 : ll + nl;
        for (i = 0; i < ns; i++) {
            BYTE a = sy[3 * i]; WORD dist = (WORD)(sy[3 * i + 1] | (sy[3 * i + 2] << 8));
            if (!dist) { put(cl[a], ll[a]); continue; }
            {   BYTE lcd = lcode[a], dcd = dc(dist);
                put(cl[257 + lcd], ll[257 + lcd]);
                put((WORD)(a + 3 - lbase[lcd]), lext[lcd]);
                put(cd[dcd], ld[dcd]);
                put((WORD)(dist - dbase[dcd]), dext[dcd]);
            }
        }
        put(cl[256], ll[256]);
    }
    for (i = 0; i < 286; i++) fl[i] = 0;
    for (i = 0; i < 30; i++) fd[i] = 0;
    ns = 0;
}
WORD Deflate(ZLESEN l, ZSCHREIB s, void *u, BYTE FAR *daten, WORD FAR *prev, WORD FAR *head, BYTE FAR *sym, DWORD *aus)
{
    WORD st = 0, we = 0, la, h = 0, i, ml, md, cur, z = 0; BYTE eof = 0;
    le = l; sc = s; uu = u; d = daten; pv = prev; hd = head; sy = sym; ob = sym + 3U * DEF_SYM;
    on = 0; ausn = 0; fehler = 0; bb = 0; bc = 0; ns = 0;
    tabellen();
    for (i = 0; i < 4096; i++) hd[i] = NIL;
    for (i = 0; i < 286; i++) fl[i] = 0;
    for (i = 0; i < 30; i++) fd[i] = 0;
    for (;;) {
        la = we - st;
        if (la < MINLA && !eof) {
            if (st >= W + MAXD) {                  /* nach vorn schieben */
                for (i = 0; i < W; i++) d[i] = d[i + W];
                st -= W; we -= W;
                for (i = 0; i < 4096; i++) hd[i] = hd[i] == NIL || hd[i] < W ? NIL : hd[i] - W;
                for (i = 0; i < W; i++) pv[i] = pv[i] == NIL || pv[i] < W ? NIL : pv[i] - W;
            }
            while (we < 2 * W && !eof) { WORD n = l(u, d + we, (WORD)(2 * W - we)); if (!n) eof = 1; we += n; }
            if (st == 0 && we >= 2) h = (WORD)(((d[0] << 4) ^ d[1]) & 4095);
            la = we - st;
            if (zk_abbruch) return ZE_ABBRUCH;
        }
        if (!la) break;
        ml = 0; md = 0;
        if (la >= 3) {
            WORD lim = st > MAXD ? st - MAXD : 0, ketten = KETTE, mx = la < 258 ? la : 258;
            h = (WORD)(((h << 4) ^ d[st + 2]) & 4095);
            cur = hd[h]; pv[st & WMASK] = cur; hd[h] = st;
            while (cur != NIL && cur >= lim && cur < st && ketten--) {
                if (d[cur + ml] == d[st + ml] && d[cur] == d[st]) {
                    WORD k = 1;
                    while (k < mx && d[cur + k] == d[st + k]) k++;
                    if (k > ml) { ml = k; md = st - cur; if (k >= mx) break; }
                }
                cur = pv[cur & WMASK];
            }
        }
        if (ml >= 3) {
            sy[3 * ns] = (BYTE)(ml - 3); sy[3 * ns + 1] = (BYTE)md; sy[3 * ns + 2] = (BYTE)(md >> 8); ns++;
            fl[257 + lcode[ml - 3]]++; fd[dc(md)]++;
            if (ml <= 16 && la > ml + 2) {        /* kurze Treffer: alle Stellen eintragen */
                for (i = 1; i < ml; i++) { st++; h = (WORD)(((h << 4) ^ d[st + 2]) & 4095); pv[st & WMASK] = hd[h]; hd[h] = st; }
                st++;
            } else { st += ml; if (we - st >= 2) h = (WORD)(((d[st] << 4) ^ d[st + 1]) & 4095); }
        } else {
            sy[3 * ns] = d[st]; sy[3 * ns + 1] = 0; sy[3 * ns + 2] = 0; ns++;
            fl[d[st]]++; st++;
        }
        if (ns == DEF_SYM) bloc(0);
        if (!++z && zk_abbruch) return ZE_ABBRUCH;
        if (fehler) return ZE_SCHREIB;
    }
    bloc(1);
    putflush();
    *aus = ausn;
    return fehler ? ZE_SCHREIB : 0;
}
