/* PRNTEST.C - Drucken (Checkliste Punkt 7): eine Testseite ueber PrnOpen/PrnPage/PrnClose.
 * Befehlszeile: D = mit Druckdialog, Ziffer = Kopien, S = zwei Seiten, A = nach dem ersten Band abbrechen.
 * Rahmen um den Druckbereich, Text oben, Kreis, Linien, gefuelltes Feld, Raster unten (Nadeln/Baender). */
#include "RD2.H"
static void seite(HDC dc, PRNJOB *j, WORD nr)
{
    RECT r; short w = j->breite, h = j->hoehe, i, cx = w / 2, cy = h / 2;
    static char t[] = "RetroDesk 2 Testseite 1";
    t[22] = (char)('0' + nr);
    r.l = 0; r.t = 0; r.r = w; r.b = h; Frame(dc, &r);
    r.l = 2; r.t = 2; r.r = w - 2; r.b = h - 2; Frame(dc, &r);
    TextOut(dc, 16, 16, t, -1);
    TextOut(dc, 16, 34, "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789 \x84\x94\x81\x8E\x99\x9A\xE1", -1);
    r.l = cx - h / 4 * j->dpi_x / j->dpi_y; r.r = cx + h / 4 * j->dpi_x / j->dpi_y; r.t = cy - h / 4; r.b = cy + h / 4;
    Ellipse(dc, &r, 0);
    Line(dc, 0, 0, w - 1, h - 1); Line(dc, w - 1, 0, 0, h - 1);
    r.l = 16; r.t = 60; r.r = 16 + w / 4; r.b = 60 + 40; SetBrush(dc, C_BLACK, PAT_SOLID); FillRect(dc, &r);
    r.l = r.r + 8; r.r = r.l + w / 4; SetBrush(dc, C_BLACK, PAT_GRAY50); FillRect(dc, &r); SetBrush(dc, C_WHITE, PAT_SOLID);
    for (i = 0; i < 64; i++) Line(dc, (short)(16 + i * 3), h - 80, (short)(16 + i * 3), (short)(h - 80 + i));   /* Treppe: jede Zeile einmal */
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    static PRNJOB j; HDC dc; WORD r = 0, n, s, seiten = 1, ab = 0;
    (void)inst; (void)show;
    j.groesse = sizeof j; j.name = "PRNTEST"; j.kopien = 1;
    for (; cmd && *cmd; cmd++) {
        if ((*cmd | 0x20) == 'd') j.flags |= PJ_DIALOG;
        else if ((*cmd | 0x20) == 's') seiten = 2;
        else if ((*cmd | 0x20) == 'a') ab = 1;
        else if (*cmd >= '1' && *cmd <= '9') j.kopien = (WORD)(*cmd - '0');
    }
    if (!(dc = PrnOpen(0, &j))) { DbgHex("PRNTEST: PrnOpen Fehler", LastError()); return 1; }
    DbgHex("PRNTEST: dpi x", j.dpi_x); DbgHex("PRNTEST: dpi y", j.dpi_y);
    DbgHex("PRNTEST: Breite", (WORD)j.breite); DbgHex("PRNTEST: Hoehe", (WORD)j.hoehe); DbgHex("PRNTEST: Kopien", j.kopien);
    for (s = 1; s <= seiten; s++) {
        n = 0;
        do { seite(dc, &j, s); n++; if (ab) { PrnAbort(dc); DbgHex("PRNTEST: abgebrochen nach Durchgang", n); return 0; } }
        while ((r = PrnPage(dc)) == PRN_NOCHMAL);
        DbgHex("PRNTEST: Durchgaenge", n); DbgHex("PRNTEST: PrnPage", r);
        if (r) break;
    }
    DbgHex("PRNTEST: PrnClose", PrnClose(dc));
    DbgPrint("PRNTEST: fertig");
    return 0;
}
