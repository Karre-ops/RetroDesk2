/* INFLATE.C - Entpacken (Deflate, RFC 1951) fuer RDZIP, 8088-tauglich.
 * Fenster 32 KB (fern), Bitpuffer 16 Bit, Huffman ueber Tabelle der ersten 9 Bits (schnell), laengere Codes
 * Bit fuer Bit (selten). Zustand in statischen Variablen (immer nur ein Entpacken gleichzeitig). */
#include "ZK.H"

BYTE zk_abbruch;
static ZLESEN le; static ZSCHREIB sc; static void *uu;
static BYTE FAR *fe; static WORD fp, fl; static DWORD gesamt;   /* Fenster, Schreibstelle, noch nicht geschrieben ab fl */
static BYTE FAR *eb; static WORD egr, en, ep; static BYTE leer, schreibfehler;
static WORD bb; static BYTE bc;

static BYTE nb(void)
{
    if (ep >= en) {
        en = le(uu, eb, egr); ep = 0;
        if (!en) { if (leer < 255) leer++; return 0; }
    }
    return eb[ep++];
}
#define NEED(n) while (bc < (n)) { bb |= (WORD)nb() << bc; bc += 8; }
static WORD bits(BYTE n)
{
    WORD v;
    if (n > 8) { v = bits(8); return v | (bits((BYTE)(n - 8)) << 8); }
    if (!n) return 0;
    NEED(n);
    v = bb & ((1U << n) - 1); bb >>= n; bc = (BYTE)(bc - n);
    return v;
}
static void flush(void)
{
    if (fp > fl && !schreibfehler) { if (sc(uu, fe + fl, (WORD)(fp - fl)) != (WORD)(fp - fl)) schreibfehler = 1; }
    fl = fp;
}
#define OUT(c) { fe[fp++] = (c); if (fp == 0x8000U) { flush(); fp = fl = 0; } }

typedef struct { WORD cnt[16]; WORD sym[288]; WORD fast[512]; } HUF;
static HUF hl, hd, hc;
static BYTE lens[320];

static WORD bauen(HUF *h, const BYTE *l, WORD n)  /* 0 = ok, 1 = ueberbelegt */
{
    WORD i, k, offs[16], code, next[16], r, c; long left;
    for (i = 0; i < 16; i++) h->cnt[i] = 0;
    for (i = 0; i < 512; i++) h->fast[i] = 0;
    for (i = 0; i < n; i++) h->cnt[l[i]]++;
    left = 1;
    for (i = 1; i < 16; i++) { left <<= 1; left -= h->cnt[i]; if (left < 0) return 1; }
    offs[1] = 0; for (i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->cnt[i];
    for (i = 0; i < n; i++) if (l[i]) h->sym[offs[l[i]]++] = i;
    code = 0; h->cnt[0] = 0;
    for (i = 1; i < 16; i++) { code = (WORD)((code + h->cnt[i - 1]) << 1); next[i] = code; }
    for (i = 0; i < n; i++) {
        BYTE L = l[i];
        if (!L) continue;
        c = next[L]++;
        if (L > 9) continue;
        for (r = 0, k = 0; k < L; k++) { r = (WORD)((r << 1) | (c & 1)); c >>= 1; }
        for (k = r; k < 512; k += (WORD)(1U << L)) h->fast[k] = (WORD)((L << 9) | i);
    }
    return 0;
}
static WORD dekod(HUF *h)                         /* Symbol oder 0xFFFF */
{
    WORD e, code, first, idx, cnt; BYTE len;
    NEED(9);
    e = h->fast[bb & 511];
    if (e) { len = (BYTE)(e >> 9); bb >>= len; bc = (BYTE)(bc - len); return e & 511; }
    code = first = idx = 0;
    for (len = 1; len < 16; len++) {
        code |= bits(1);
        cnt = h->cnt[len];
        if (code - first < cnt) return h->sym[idx + code - first];
        idx += cnt; first += cnt; first <<= 1; code <<= 1;
    }
    return 0xFFFF;
}
static const WORD lbase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
static const BYTE lext[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const WORD dbase[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const BYTE dext[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
static const BYTE ord[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };

static WORD block(void)                           /* komprimierter Block mit hl/hd */
{
    WORD s, len, d, src; WORD z = 0;
    for (;;) {
        s = dekod(&hl);
        if (s < 256) { OUT((BYTE)s); gesamt++; continue; }
        if (s == 256) return 0;
        if (s > 285 || s == 0xFFFF) return ZE_DATEN;
        s -= 257; len = lbase[s] + bits(lext[s]);
        s = dekod(&hd);
        if (s > 29) return ZE_DATEN;
        d = dbase[s] + bits(dext[s]);
        if ((DWORD)d > gesamt) return ZE_DATEN;
        gesamt += len;
        src = (WORD)((fp - d) & 0x7FFF);
        while (len--) { BYTE c = fe[src]; src = (WORD)((src + 1) & 0x7FFF); OUT(c); }
        if (leer > 8) return ZE_KURZ;
        if (!++z && zk_abbruch) return ZE_ABBRUCH;   /* alle 256 Treffer: Abbruch? (Taste setzt zk_abbruch im Rueckruf) */
        if (schreibfehler) return ZE_SCHREIB;
    }
}
WORD Inflate(ZLESEN l, ZSCHREIB s, void *u, BYTE FAR *fenster, BYTE FAR *ein, WORD g)
{
    WORD fin, typ, i, r = 0;
    le = l; sc = s; uu = u; fe = fenster; eb = ein; egr = g; en = ep = 0; leer = 0; schreibfehler = 0;
    fp = fl = 0; gesamt = 0; bb = 0; bc = 0;
    do {
        fin = bits(1); typ = bits(2);
        if (typ == 0) {                           /* gespeichert */
            WORD n, nn;
            bits((BYTE)(bc & 7));
            n = bits(16); nn = bits(16);
            if ((WORD)~nn != n) { r = ZE_DATEN; break; }
            while (n && bc) { OUT((BYTE)bits(8)); gesamt++; n--; }
            while (n--) { BYTE c = nb(); OUT(c); gesamt++; }
            if (leer) { r = ZE_KURZ; break; }
        } else if (typ == 1) {                    /* feste Codes */
            for (i = 0; i < 144; i++) lens[i] = 8;
            for (; i < 256; i++) lens[i] = 9;
            for (; i < 280; i++) lens[i] = 7;
            for (; i < 288; i++) lens[i] = 8;
            bauen(&hl, lens, 288);
            for (i = 0; i < 30; i++) lens[i] = 5;
            bauen(&hd, lens, 30);
            if ((r = block()) != 0) break;
        } else if (typ == 2) {                    /* eigene Codes */
            WORD nl = bits(5) + 257, nd = bits(5) + 1, nc = bits(4) + 4;
            BYTE cl[19];
            if (nl > 286 || nd > 30) { r = ZE_DATEN; break; }
            for (i = 0; i < 19; i++) cl[i] = 0;
            for (i = 0; i < nc; i++) cl[ord[i]] = (BYTE)bits(3);
            if (bauen(&hc, cl, 19)) { r = ZE_DATEN; break; }
            for (i = 0; i < nl + nd; ) {
                WORD sy = dekod(&hc), rep; BYTE v;
                if (sy < 16) { lens[i++] = (BYTE)sy; continue; }
                if (sy == 16) { if (!i) { r = ZE_DATEN; break; } v = lens[i - 1]; rep = 3 + bits(2); }
                else if (sy == 17) { v = 0; rep = 3 + bits(3); }
                else if (sy == 18) { v = 0; rep = 11 + bits(7); }
                else { r = ZE_DATEN; break; }
                if (i + rep > nl + nd) { r = ZE_DATEN; break; }
                while (rep--) lens[i++] = v;
            }
            if (r) break;
            if (bauen(&hl, lens, nl)) { r = ZE_DATEN; break; }
            if (bauen(&hd, lens + nl, nd)) { r = ZE_DATEN; break; }
            if ((r = block()) != 0) break;
        } else { r = ZE_DATEN; break; }
        if (leer > 8) { r = ZE_KURZ; break; }
    } while (!fin);
    flush();
    if (!r && schreibfehler) r = ZE_SCHREIB;
    return r;
}
