/* ESCP24.C - Druckertreiber Epson ESC/P 24 Nadeln und ESC/P2 (ESCP24.DRV, PLAN_API Schritt 7, 24.11.2026).
 * Grafik in Durchgaengen zu 24 Nadeln mit ESC * m n1 n2, je Spalte 3 Bytes (oberste Nadel = Bit 7 des ersten Bytes):
 * m = 39 (180 x 180 dpi, Schoenschrift) oder bei PJ_ENTWURF m = 33 (120 x 180 dpi). Das koennen alle 24-Nadel-Drucker
 * seit dem Epson LQ-800 und alle ESC/P2-Geraete: Epson LQ und Stylus, NEC P-Serie, Star LC-24, OKI und Panasonic im
 * Epson-Modus, Canon BJ in der LQ-Emulation. Vorschub ESC J n (n/180 Zoll), leere Durchgaenge sammeln nur Vorschub,
 * leere Spalten rechts werden nicht gesendet. Druckbereich 8 x 11 Zoll (A4 und 12-Zoll-Endlospapier).
 * RDPRN liefert die Ausgaberoutine (Port oder Datei); 1 Bit je Punkt, 1 = Punkt. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2DRV.H"

#define MAXP 1440                                 /* 8 Zoll x 180 dpi */
static PRNOUT aus;
static WORD breite, vor;                          /* vor: ausstehender Vorschub in 1/180 Zoll */
static BYTE ent;
static BYTE zb[5 + MAXP * 3 + 1];                 /* ein Durchgang: ESC * m n1 n2 + Spalten + CR */

static WORD RDEXPORT d_init(WORD p) { (void)p; return 0; }
static void RDEXPORT d_done(void) { }
static WORD sende(const BYTE *p, WORD n) { return aus((const BYTE FAR *)p, n); }
static WORD vorschub(void)
{
    static BYTE b[3]; WORD r = 0, m;               /* im Datensegment: Stapel ist SS != DS */
    while (vor && !r) { m = vor > 180 ? 180 : vor; b[0] = 27; b[1] = 'J'; b[2] = (BYTE)m; r = sende(b, 3); vor -= m; }
    return r;
}
static WORD RDEXPORT d_open(PRNJOB FAR *j, PRNOUT a)
{
    static const BYTE ini[2] = { 27, '@' };
    aus = a; ent = (j->flags & PJ_ENTWURF) != 0;
    j->dpi_x = ent ? 120 : 180; j->dpi_y = 180;
    j->breite = (short)(8 * j->dpi_x); j->hoehe = 11 * 180;
    breite = (WORD)j->breite; vor = 0;
    return sende(ini, 2);
}
/* zeilen Zeilen ab bits; je 24 Zeilen ein Durchgang (der letzte unten mit Leerzeilen aufgefuellt) */
static WORD RDEXPORT d_band(const BYTE FAR *bits, WORD bpr, WORD zeilen)
{
    WORD r, k, xb, n, nb = (breite + 7) >> 3, z, e;
    for (r = 0; r < zeilen; r += 24) {
        BYTE *o = zb + 5; const BYTE FAR *q = bits + (DWORD)r * bpr;
        z = zeilen - r > 24 ? 24 : zeilen - r;
        n = 0;
        for (xb = 0; xb < nb; xb++, q++) {
            BYTE s[24], m; WORD i, a = 0;
            for (k = 0; k < 24; k++) { s[k] = k < z ? q[(DWORD)k * bpr] : 0; a |= s[k]; }
            if (!a) { for (i = 0; i < 24; i++) *o++ = 0; continue; }
            for (m = 0x80; m; m >>= 1) {          /* Spalte: 3 Bytes, je 8 Nadeln von oben */
                for (i = 0; i < 24; i += 8) {
                    BYTE c = 0;
                    for (k = 0; k < 8; k++) if (s[i + k] & m) c |= (BYTE)(0x80 >> k);
                    *o++ = c;
                }
            }
            n = (WORD)(o - zb - 5) / 3;
        }
        while (n && !(zb[5 + 3 * n - 1] | zb[5 + 3 * n - 2] | zb[5 + 3 * n - 3])) n--;
        if (n) {
            if ((e = vorschub()) != 0) return e;
            zb[0] = 27; zb[1] = '*'; zb[2] = (BYTE)(ent ? 33 : 39); zb[3] = (BYTE)n; zb[4] = (BYTE)(n >> 8);
            zb[5 + 3 * n] = 13;                   /* CR: zurueck an den Zeilenanfang */
            if ((e = sende(zb, 5 + 3 * n + 1)) != 0) return e;
        }
        vor += 24;
    }
    return 0;
}
static WORD RDEXPORT d_page(void) { static const BYTE ff[2] = { 13, 12 }; vor = 0; return sende(ff, 2); }
static WORD RDEXPORT d_close(BOOL abbruch)
{
    static const BYTE ab[4] = { 13, 12, 27, '@' };
    return abbruch ? sende(ab, 4) : sende(ab + 2, 2);
}

static PRNDRV tab = {
    { { 'R', 'D', 'R', 'V' }, DRV_PRN, 0x0100, sizeof(PRNDRV), "ESC/P 24-Nadel", d_init, d_done },
    d_open, 24, d_band, d_page, d_close
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
