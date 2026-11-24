/* PCL.C - Druckertreiber HP PCL 3/5 (PCL.DRV, PLAN_API Schritt 7, 24.11.2026): LaserJet ab IIP, DeskJet ab 500 und
 * Kompatible (Kyocera, Brother, Canon LBP und andere im PCL-Modus). Rastergrafik 300 dpi, bei PJ_ENTWURF 150 dpi:
 * ESC E, A4 (ESC &l26A), ESC *t R, an den Seitenanfang, ESC *r1A, Kompression 2 (TIFF/PackBits, ESC *b2M), je Zeile
 * ESC *b n W, leere Zeilen kurz (ESC *b0W), rechts abgeschnitten. Seite: ESC *rB, FF. Ende: ESC E.
 * Druckbereich 8 x 11 Zoll wie die Nadeltreiber. RDPRN liefert die Ausgaberoutine; 1 Bit je Punkt, 1 = Punkt. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2DRV.H"

#define MAXB 300                                  /* 8 Zoll x 300 dpi / 8 */
static PRNOUT aus;
static WORD nb, dpi; static BYTE ras;             /* ras: Rastergrafik laeuft */
static BYTE zb[16 + MAXB + MAXB / 128 + 4];       /* ESC *b n W + gepackte Zeile */
static char kb[24]; static BYTE pk[MAXB + MAXB / 128 + 4];   /* im Datensegment: Stapel ist SS != DS */

static WORD RDEXPORT d_init(WORD p) { (void)p; return 0; }
static void RDEXPORT d_done(void) { }
static WORD sende(const BYTE *p, WORD n) { return aus((const BYTE FAR *)p, n); }
static WORD zahl(char *p, WORD v) { char r[6]; WORD n = 0, k = 0; do r[n++] = (char)('0' + v % 10); while (v /= 10); while (n) p[k++] = r[--n]; return k; }
static WORD befehl(const char *a, WORD v, char e)  /* ESC a <v> e, z. B. "*t" 300 'R' */
{
    WORD n = 1; kb[0] = 27; while (*a) kb[n++] = *a++;
    n += zahl(kb + n, v); kb[n++] = e;
    return sende((const BYTE *)kb, n);
}
static WORD start(void)                           /* Raster an den Seitenanfang */
{
    WORD e;
    if ((e = befehl("*p", 0, 'X')) || (e = befehl("*p", 0, 'Y')) || (e = befehl("*r", 1, 'A')) || (e = befehl("*b", 2, 'M'))) return e;
    ras = 1; return 0;
}
static WORD RDEXPORT d_open(PRNJOB FAR *j, PRNOUT a)
{
    static const BYTE ini[2] = { 27, 'E' }; WORD e;
    aus = a; dpi = (j->flags & PJ_ENTWURF) ? 150 : 300;
    j->dpi_x = j->dpi_y = dpi;
    j->breite = (short)(8 * dpi); j->hoehe = (short)(11 * dpi);
    nb = (WORD)j->breite >> 3; ras = 0;
    if ((e = sende(ini, 2)) || (e = befehl("&l", 26, 'A')) || (e = befehl("*t", dpi, 'R'))) return e;
    return 0;
}
static WORD packe(const BYTE FAR *q, WORD n, BYTE *o)   /* TIFF-PackBits; Laenge */
{
    WORD i = 0, k = 0, l;
    while (i < n) {
        for (l = 1; i + l < n && l < 128 && q[i + l] == q[i]; l++) ;
        if (l >= 3) { o[k++] = (BYTE)(257 - l); o[k++] = q[i]; i += l; continue; }
        for (l = 0; i + l < n && l < 128; l++)    /* wortweise: bis eine Folge von 3 gleichen beginnt */
            if (i + l + 2 < n && q[i + l] == q[i + l + 1] && q[i + l] == q[i + l + 2]) break;
        if (!l) l = 1;
        o[k++] = (BYTE)(l - 1); while (l--) o[k++] = q[i++];
    }
    return k;
}
static WORD RDEXPORT d_band(const BYTE FAR *bits, WORD bpr, WORD zeilen)
{
    WORD r, n, k, e;
    if (!ras && (e = start()) != 0) return e;
    for (r = 0; r < zeilen; r++) {
        const BYTE FAR *q = bits + (DWORD)r * bpr;
        for (n = nb; n && !q[n - 1]; n--) ;
        k = 1; zb[0] = 27; zb[k++] = '*'; zb[k++] = 'b';
        if (n) { WORD p = packe(q, n, pk), i; k += zahl((char *)zb + k, p); zb[k++] = 'W'; for (i = 0; i < p; i++) zb[k++] = pk[i]; }
        else { zb[k++] = '0'; zb[k++] = 'W'; }
        if ((e = sende(zb, k)) != 0) return e;
    }
    return 0;
}
static WORD RDEXPORT d_page(void)
{
    static const BYTE ff[1] = { 12 }; WORD e;
    if (ras) { ras = 0; if ((e = befehl("*r", 0, 'B')) != 0) return e; }
    return sende(ff, 1);
}
static WORD RDEXPORT d_close(BOOL abbruch)
{
    static const BYTE ende[2] = { 27, 'E' }; static const BYTE ff[1] = { 12 };
    if (ras) { ras = 0; befehl("*r", 0, 'B'); if (abbruch) sende(ff, 1); }
    return sende(ende, 2);
}

static PRNDRV tab = {
    { { 'R', 'D', 'R', 'V' }, DRV_PRN, 0x0100, sizeof(PRNDRV), "HP PCL 3/5", d_init, d_done },
    d_open, 1, d_band, d_page, d_close
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
