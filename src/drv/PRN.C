/* PRN.C - Druckertreiber Epson 9-Nadel ESC/P (-DEPSON9, EPSON9.DRV).
 * Grafik in Durchgaengen zu 8 Nadeln: ESC L (120 x 72 dpi, doppelte Dichte) oder bei PJ_ENTWURF
 * ESC K (60 x 72 dpi). Die Befehle kann jeder 9-Nadel-Drucker seit dem Epson MX/FX-80
 * (auch IBM Graphics Printer, Star, Citizen, Panasonic im Epson-Modus). Vorschub nach jedem Durchgang
 * ESC J 24 (24/216 Zoll = 8 Punkte), leere Durchgaenge sammeln nur Vorschub, leere Spalten rechts
 * werden nicht gesendet. Druckbereich 8 x 11 Zoll (A4 und 12-Zoll-Endlospapier).
 * RDPRN liefert die Ausgaberoutine (Port oder Datei); 1 Bit je Punkt, 1 = Punkt. */
#include "RD2DRV.H"

static PRNOUT aus;
static WORD breite, vor;                          /* vor: ausstehender Vorschub in 1/216 Zoll */
static BYTE ent;
static BYTE zb[4 + 960 + 1];                      /* ein Durchgang: ESC L n1 n2 + Spalten + CR */

static WORD RDEXPORT d_init(WORD p) { (void)p; return 0; }
static void RDEXPORT d_done(void) { }
static WORD sende(const BYTE *p, WORD n) { return aus((const BYTE FAR *)p, n); }
static WORD vorschub(void)
{
    static BYTE b[3]; WORD r = 0, m;               /* im Datensegment: Stapel ist SS != DS */
    while (vor && !r) { m = vor > 216 ? 216 : vor; b[0] = 27; b[1] = 'J'; b[2] = (BYTE)m; r = sende(b, 3); vor -= m; }
    return r;
}
static WORD RDEXPORT d_open(PRNJOB FAR *j, PRNOUT a)
{
    static const BYTE ini[2] = { 27, '@' };
    aus = a; ent = (j->flags & PJ_ENTWURF) != 0;
    j->dpi_x = ent ? 60 : 120; j->dpi_y = 72;
    j->breite = (short)(8 * j->dpi_x); j->hoehe = 11 * 72;
    breite = (WORD)j->breite; vor = 0;
    return sende(ini, 2);
}
/* zeilen Zeilen ab bits; je 8 Zeilen ein Durchgang (der letzte unten mit Leerzeilen aufgefuellt) */
static WORD RDEXPORT d_band(const BYTE FAR *bits, WORD bpr, WORD zeilen)
{
    WORD r, k, xb, n, nb = (breite + 7) >> 3, z;
    for (r = 0; r < zeilen; r += 8) {
        BYTE *o = zb + 4; const BYTE FAR *q = bits + (DWORD)r * bpr;
        z = zeilen - r > 8 ? 8 : zeilen - r;
        n = 0;
        for (xb = 0; xb < nb; xb++, q++) {
            BYTE s[8], m, c; WORD i, a = 0;
            for (k = 0; k < 8; k++) { s[k] = k < z ? q[(DWORD)k * bpr] : 0; a |= s[k]; }
            if (!a) { for (i = 0; i < 8; i++) *o++ = 0; continue; }
            for (m = 0x80; m; m >>= 1) {          /* Spalte: oberste Zeile = oberste Nadel (Bit 7) */
                c = 0;
                for (k = 0; k < 8; k++) if (s[k] & m) c |= (BYTE)(0x80 >> k);
                *o++ = c;
            }
            n = (WORD)(o - zb - 4);
        }
        while (n && !zb[4 + n - 1]) n--;
        if (n) {
            WORD e;
            if ((e = vorschub()) != 0) return e;
            zb[0] = 27; zb[1] = ent ? 'K' : 'L'; zb[2] = (BYTE)n; zb[3] = (BYTE)(n >> 8);
            zb[4 + n] = 13;                       /* CR: zurueck an den Zeilenanfang */
            if ((e = sende(zb, 4 + n + 1)) != 0) return e;
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
    { { 'R', 'D', 'R', 'V' }, DRV_PRN, 0x0100, sizeof(PRNDRV), "Epson 9-Nadel", d_init, d_done },
    d_open, 8, d_band, d_page, d_close
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
