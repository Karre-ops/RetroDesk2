/* RDEDIT.C - Texteditor fuer RD2 (Checkliste Punkt 6, wie RD1 RDEDIT 1.0).
 * Bis 8000 Zeilen zu 250 Zeichen, Tabulator 8. Auswahl mit Umschalt+Pfeil oder Maus, Ablage (Strg+X/C/V,
 * Umschalt+Entf/Strg+Einfg/Umschalt+Einfg), Suchen (Strg+F, F3), Ersetzen (Strg+H), Gehe zu (Strg+G),
 * Einfuegen/Ueberschreiben (Einfg). Sicheres Speichern: erst NAME.$$$ ganz schreiben, dann umbenennen.
 * Startdateien (CONFIG.SYS, AUTOEXEC.BAT, FDCONFIG.SYS, FDAUTO.BAT) beim ersten Speichern als .BAK behalten.
 * Text in Bloecken zu hoechstens 4 KB (MemAlloc beweglich, also auslagerbar), je Block ganze Zeilen mit \n.
 * Datei = Zeilen mit \r\n getrennt (ohne Zeilenende nach der letzten), Laden und Speichern verlustfrei.
 * Zu grosse Dateien oder zu lange Zeilen: nur lesen (nichts geht verloren).
 * Befehlszeile: [T] [Datei]. "T": Meldungen fuer test/edit.sh. */
#include "RD2.H"
#include <string.h>

#define MAXZ  8000
#define LMAX  250
#define TABW  8
#define BLK   4096
#define FUELL 3072                                /* Laden: Bloecke so weit, Rest fuers Tippen */
#define MAXB  250
#define RAND  2
#define CM_NEU 101
#define CM_OEFFNEN 102
#define CM_SPEICHERN 103
#define CM_UNTER 104
#define CM_ENDE 109
#define CM_AUS 201
#define CM_KOPIE 202
#define CM_EIN 203
#define CM_LOESCH 204
#define CM_ALLES 205
#define CM_UEBER 206
#define CM_SUCHEN 301
#define CM_WEITER 302
#define CM_ERSETZEN 303
#define CM_GEHE 304

typedef struct { HMEM h; WORD len, nz; } BLOCK;
static BLOCK bl[MAXB];
static WORD nb, nz, cb, cbz;                      /* Bloecke, Zeilen; Merker: Block cb beginnt mit Zeile cbz */
static WORD kz = 0xFFFF, kb, ka;                  /* Lesemerker: Zeile kz kopiert, naechste ab Block kb Stelle ka */
static HINST hi; static HWND fe; static HMENU mn;
static BYTE test, geaendert, nurlesen, ueber, bak, ziehen, sel;
static char pfad[80], titel[100], meld[48], such_s[41], ers_s[41];
static char lb[LMAX + 1], tb[LMAX + 1], io[2048];
static WORD ll, lz = 0xFFFF;                      /* aktuelle Zeile lz in lb, Laenge ll */
static WORD cy, cx, wx, top, left, rows, cols, ay, ax, iop;
static short zh, cw, cbreit;

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static void szahl(char *d, WORD v, WORD n) { char t[6]; WORD i = 0; do t[i++] = (char)('0' + v % 10); while ((v /= 10) != 0); while (i) { char c[2]; c[0] = t[--i]; c[1] = 0; scat(d, c, n); } }
static BYTE klein(BYTE c) { if (c >= 'A' && c <= 'Z') return (BYTE)(c + 32); if (c == 0x8E) return 0x84; if (c == 0x99) return 0x94; if (c == 0x9A) return 0x81; return c; }
static void piep(void) { SndBeep(880, 30); }
static void dbg(LPCSTR s, WORD v) { if (test) DbgHex(s, v); }

/* ---------------- Bloecke ---------------- */
static char FAR *blk(WORD b) { return (char FAR *)MemLock(bl[b].h); }
static void ublk(WORD b) { MemUnlock(bl[b].h); }
static WORD finden(WORD z, WORD *r)
{
    if (cb >= nb || z < cbz) cb = cbz = 0;
    while (cb + 1 < nb && z >= cbz + bl[cb].nz) cbz += bl[cb++].nz;
    *r = z - cbz; return cb;
}
static WORD ofs(const char FAR *p, WORD r) { WORD i = 0; while (r) if (p[i++] == '\n') r--; return i; }
static BYTE bneu(WORD nach)                       /* leeren Block an Stelle nach */
{
    HMEM h; WORD i;
    if (nb >= MAXB || !(h = MemAlloc(BLK, MEM_MOVABLE))) return 0;
    for (i = nb; i > nach; i--) bl[i] = bl[i - 1];
    bl[nach].h = h; bl[nach].len = bl[nach].nz = 0; nb++;
    if (nach <= cb) cb = cbz = 0;
    return 1;
}
static void alle_frei(void) { while (nb) MemFree(bl[--nb].h); nz = cb = cbz = 0; kz = lz = 0xFFFF; }
static BYTE teilen(WORD b)                        /* Block b in der Mitte (an einer Zeilengrenze) teilen */
{
    char FAR *p, FAR *q; WORD i = 0, z = 0, n;
    if (!bneu(b + 1)) return 0;
    if (!(p = blk(b))) return 0;
    if (!(q = blk(b + 1))) { ublk(b); return 0; }
    while (i < bl[b].len / 2) { while (p[i++] != '\n') ; z++; }
    n = bl[b].len - i; _fmemcpy(q, p + i, n);
    bl[b + 1].len = n; bl[b + 1].nz = bl[b].nz - z; bl[b].len = i; bl[b].nz = z;
    ublk(b + 1); ublk(b); return 1;
}
static WORD zkopie(WORD z, char *d)               /* Zeile z nach d, liefert Laenge */
{
    WORD r, b, a, n = 0; char FAR *p;
    if (z >= nz) return 0;
    if (kz != 0xFFFF && z == kz + 1 && kb < nb && ka < bl[kb].len) { b = kb; if (!(p = blk(b))) return 0; a = ka; }   /* folgt auf die letzte: weiterlesen */
    else { b = finden(z, &r); if (!(p = blk(b))) return 0; a = ofs(p, r); }
    while (p[a] != '\n' && n < LMAX) d[n++] = p[a++];
    while (p[a] != '\n') a++;
    ublk(b);
    kz = z; kb = b; ka = a + 1;
    if (ka >= bl[kb].len) { kb++; ka = 0; }
    return n;
}
static void zholen(WORD z) { if (z != lz) { ll = zkopie(z, lb); lz = z; } }
static BYTE zsetzen(WORD z, const char *s, WORD n, BYTE neu)   /* Zeile z ersetzen oder (neu) davor einfuegen */
{
    WORD r, b, a, ol = 0, i; char FAR *p = 0;
    if (neu && nz >= MAXZ) return 0;
    kz = lz = 0xFFFF;
    for (i = 0; ; i++) {
        if (!nz) { b = 0; r = 0; }
        else if (z >= nz) { b = finden(nz - 1, &r); r++; }
        else b = finden(z, &r);
        if (!(p = blk(b))) return 0;
        a = r >= bl[b].nz ? bl[b].len : ofs(p, r);
        if (!neu) { ol = 0; while (p[a + ol] != '\n') ol++; ol++; }
        if (bl[b].len - ol + n + 1 <= BLK) break;
        ublk(b);
        if (i || !teilen(b)) return 0;
    }
    _fmemmove(p + a + n + 1, p + a + ol, bl[b].len - a - ol);
    _fmemcpy(p + a, s, n); p[a + n] = '\n';
    bl[b].len += n + 1 - ol;
    if (neu) { bl[b].nz++; nz++; }
    ublk(b); return 1;
}
static void zloeschen(WORD z, WORD k)             /* k Zeilen ab z entfernen, blockweise am Stueck */
{
    WORD r, b, a, e, m, i; char FAR *p;
    kz = lz = 0xFFFF;
    while (k && z < nz) {
        b = finden(z, &r); if (!(p = blk(b))) return;
        a = e = ofs(p, r);
        for (m = 0; m < k && r + m < bl[b].nz; m++) { while (p[e] != '\n') e++; e++; }
        _fmemmove(p + a, p + e, bl[b].len - e);
        bl[b].len -= e - a; bl[b].nz -= m; nz -= m; k -= m; ublk(b);
        if (!bl[b].nz && nb > 1) { MemFree(bl[b].h); nb--; for (i = b; i < nb; i++) bl[i] = bl[i + 1]; cb = cbz = 0; }
    }
    if (!nz) zsetzen(0, "", 0, 1);                /* es gibt immer eine Zeile */
}
static BYTE schreib(void) { if (!zsetzen(cy, lb, ll, 0)) return 0; lz = cy; return 1; }

/* ---------------- Spalten (Tabulator) ---------------- */
static WORD spalte(const char *s, WORD n, WORD x) { WORD c = 0, i; for (i = 0; i < x && i < n; i++) c = s[i] == '\t' ? (WORD)((c / TABW + 1) * TABW) : c + 1; return c; }
static WORD stelle(const char *s, WORD n, WORD c) { WORD i; for (i = 0; i < n && spalte(s, n, i + 1) <= c; i++) ; return i; }

/* ---------------- Anzeige ---------------- */
static void kopf(void)
{
    scpy(titel, StrGet(hi, 1), sizeof titel); scat(titel, " - ", sizeof titel);
    scat(titel, pfad[0] ? pfad : StrGet(hi, 8), sizeof titel);
    if (geaendert) scat(titel, " *", sizeof titel);
    if (nurlesen) { scat(titel, " ", sizeof titel); scat(titel, StrGet(hi, 2), sizeof titel); }
    SetWindowText(fe, titel);
}
static void neu_z(WORD a, WORD b)                 /* Zeilen a..b neu zeichnen */
{
    RECT r; WORD t;
    if (a > b) { t = a; a = b; b = t; }
    if (b < top || a >= top + rows) return;
    if (a < top) a = top;
    if (b >= top + rows) b = top + rows - 1;
    r.l = 0; r.r = cbreit; r.t = (short)((a - top) * zh); r.b = (short)((b - top + 1) * zh);
    InvalidateRect(fe, &r, TRUE);
}
static void neu_s(void) { RECT r; r.l = 0; r.r = cbreit; r.t = (short)(rows * zh); r.b = (short)(r.t + zh + 3); InvalidateRect(fe, &r, TRUE); }
static void neu_a(void) { InvalidateRect(fe, 0, TRUE); }
static void balken(void)
{
    ScrollSet(fe, SB_VERT, (short)top, (short)(nz > rows ? nz - rows : 0), (short)rows);
    ScrollSet(fe, SB_HORZ, (short)left, (short)(LMAX > cols ? LMAX - cols : 0), (short)cols);
}
static BYTE hatsel(void) { return sel && (ay != cy || ax != cx); }
static void bereich(WORD *y0, WORD *x0, WORD *y1, WORD *x1)
{
    if (ay < cy || (ay == cy && ax < cx)) { *y0 = ay; *x0 = ax; *y1 = cy; *x1 = cx; }
    else { *y0 = cy; *x0 = cx; *y1 = ay; *x1 = ax; }
}
static void zeigen(void)                          /* Schreibmarke im Bild halten */
{
    WORD c; BYTE n = 0;
    if (!nz) return;
    if (cy >= nz) cy = nz - 1;
    zholen(cy); if (cx > ll) cx = ll;
    if (cy < top) { top = cy; n = 1; } else if (cy >= top + rows) { top = cy - rows + 1; n = 1; }
    c = spalte(lb, ll, cx);
    if (c < left) { left = c > 8 ? c - 8 : 0; n = 1; } else if (c >= left + cols) { left = c - cols + 8; n = 1; }
    if (n) neu_a();
    balken(); neu_s();
}
static void malen(void)
{
    PAINT ps; HDC d; RECT r; WORD z, n, i, c, k, y0, x0, y1, x1, s0, s1; short y, x; char v[100]; BYTE s = hatsel();
    d = BeginPaint(fe, &ps);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK); SetPen(d, C_BLACK);
    if (s) bereich(&y0, &x0, &y1, &x1);
    for (z = top + (WORD)(ps.neu.t / zh), y = (short)((z - top) * zh); z < top + rows && y < ps.neu.b; z++, y += zh) {
        if (z >= nz) break;
        n = zkopie(z, tb);
        for (i = c = k = 0; i < n && c < left + cols; i++) {
            WORD w = tb[i] == '\t' ? (WORD)(TABW - c % TABW) : 1;
            while (w--) { if (c >= left && k < cols && k < sizeof v) v[k++] = tb[i] == '\t' ? ' ' : tb[i]; c++; }
        }
        if (k) TextOut(d, RAND, y, v, (short)k);
        if (s && z >= y0 && z <= y1) {            /* Auswahl invers, Zeilenende als eine Zelle */
            s0 = z == y0 ? spalte(tb, n, x0) : 0; s1 = z == y1 ? spalte(tb, n, x1) : spalte(tb, n, n) + 1;
            if (s0 < left) s0 = left;
            if (s1 > left + cols) s1 = left + cols;
            if (s1 > s0) { r.l = (short)(RAND + (s0 - left) * cw); r.r = (short)(RAND + (s1 - left) * cw); r.t = y; r.b = (short)(y + zh); InvertRect(d, &r); }
        }
        if (z == cy && GetFocus() == fe) {        /* Schreibmarke: Strich, beim Ueberschreiben ganze Zelle */
            c = spalte(tb, n, cx);
            if (c >= left && c < left + cols) {
                x = (short)(RAND + (c - left) * cw);
                if (ueber) { r.l = x; r.r = (short)(x + cw); r.t = y; r.b = (short)(y + zh); InvertRect(d, &r); }
                else { Line(d, x, y, x, (short)(y + zh - 1)); if (x) Line(d, (short)(x - 1), y, (short)(x - 1), (short)(y + zh - 1)); }
            }
        }
    }
    y = (short)(rows * zh);                       /* Statuszeile */
    if (ps.neu.b > y) {
        char t[80];
        Line(d, 0, (short)(y + 1), (short)(cbreit - 1), (short)(y + 1));
        t[0] = 0; scat(t, StrGet(hi, 9), sizeof t); scat(t, " ", sizeof t); szahl(t, cy + 1, sizeof t);
        scat(t, " ", sizeof t); scat(t, StrGet(hi, 26), sizeof t); scat(t, " ", sizeof t); szahl(t, nz, sizeof t);
        scat(t, "   ", sizeof t); scat(t, StrGet(hi, 10), sizeof t); scat(t, " ", sizeof t);
        zholen(cy); szahl(t, spalte(lb, ll, cx) + 1, sizeof t);
        scat(t, "   ", sizeof t); scat(t, StrGet(hi, ueber ? 12 : 11), sizeof t);
        if (meld[0]) { scat(t, "   ", sizeof t); scat(t, meld, sizeof t); }
        TextOut(d, RAND, (short)(y + 3), t, -1);
    }
    EndPaint(fe, &ps);
}
static void groesse(void)
{
    RECT r; GetClientRect(fe, &r); cbreit = r.r;
    rows = (WORD)((r.b - zh - 3) / zh); if (!rows || r.b < zh * 2) rows = 1;
    cols = (WORD)((r.r - RAND) / cw); if (!cols) cols = 1;
    if (cols > 99) cols = 99;
    zeigen(); neu_a();
}
static void melden(WORD id) { scpy(meld, id ? StrGet(hi, id) : "", sizeof meld); neu_s(); }

/* ---------------- Laden und Speichern ---------------- */
static BYTE anh(void)                             /* lb[0..ll) beim Laden hinten anhaengen */
{
    WORD b = nb - 1; char FAR *p;
    if (nz >= MAXZ) return 0;
    if (bl[b].len + ll + 1 > FUELL && !bneu(nb)) return 0;
    b = nb - 1; if (!(p = blk(b))) return 0;
    _fmemcpy(p + bl[b].len, lb, ll); p[bl[b].len + ll] = '\n';
    bl[b].len += ll + 1; bl[b].nz++; nz++; ublk(b); ll = 0; return 1;
}
static void laden(void)
{
    short f; WORD n, i; char c; BYTE ok = 1;
    alle_frei(); nurlesen = geaendert = sel = bak = 0; cy = cx = wx = top = left = 0; meld[0] = 0;
    if (!bneu(0)) { nurlesen = 1; return; }
    ll = 0;
    if (pfad[0] && (f = FileOpen(pfad, FO_READ)) >= 0) {
        CursorBusy(TRUE);
        while (ok && (n = FileRead(f, io, sizeof io)) != 0 && n != 0xFFFF) {
            for (i = 0; i < n; i++) {
                c = io[i];
                if (c == 26) { ok = 2; break; }    /* Strg+Z: Dateiende */
                if (c == '\r') continue;
                if (c == '\n') { if (!anh()) { ok = 0; break; } continue; }
                if (ll >= LMAX) { nurlesen = 1; if (!anh()) { ok = 0; break; } }   /* zu lang: umbrechen, nur lesen */
                lb[ll++] = c;
            }
            if (ok == 2) break;
        }
        FileClose(f);
        CursorBusy(FALSE);
    }
    if (!ok) nurlesen = 1;
    else if (!anh()) nurlesen = 1;                /* letzte Zeile (auch leer) */
    if (!nz) { ll = 0; anh(); }
    lz = kz = 0xFFFF;
    MenuEnable(mn, CM_SPEICHERN, !nurlesen);
    kopf(); balken(); neu_a();
    if (nurlesen) melden(13);
    if (test) { DbgHex(nurlesen ? "RDEDIT: geladen, nur lesen" : "RDEDIT: geladen", nz); DbgPrint(pfad); }
}
static BYTE raus(short f) { WORD n = iop; iop = 0; return FileWrite(f, io, n) == n; }
static void endung(char *d, LPCSTR s, LPCSTR e)  /* s mit neuer Endung e nach d */
{
    WORD i, p = 0xFFFF;
    scpy(d, s, 76);
    for (i = 0; d[i]; i++) if (d[i] == '.') p = i; else if (d[i] == '\\' || d[i] == ':') p = 0xFFFF;
    if (p != 0xFFFF) d[p] = 0;
    scat(d, ".", 80); scat(d, e, 80);
}
static BYTE startdatei(void)
{
    static const char n[] = "CONFIG.SYS\0AUTOEXEC.BAT\0FDCONFIG.SYS\0FDAUTO.BAT\0";
    WORD i = slen(pfad), a, j, k;
    while (i && pfad[i - 1] != '\\' && pfad[i - 1] != ':') i--;
    for (a = 0; n[a]; a += slen(n + a) + 1) {
        for (j = i, k = a; pfad[j] && n[k] && klein(pfad[j]) == klein(n[k]); j++, k++) ;
        if (!pfad[j] && !n[k]) return 1;
    }
    return 0;
}
static BYTE speichern(void)
{
    char tmp[80], alt[80]; short f; WORD b, i, z = 0; char FAR *p; BYTE ok = 1, sb = 0;
    if (nurlesen || !pfad[0]) return 0;
    endung(tmp, pfad, "$$$");
    if ((f = FileCreate(tmp, 0)) < 0) ok = 0;
    else {
        CursorBusy(TRUE); iop = 0;
        for (b = 0; b < nb && ok; b++) {
            if (!(p = blk(b))) { ok = 0; break; }
            for (i = 0; i < bl[b].len && ok; i++) {
                if (iop + 2 > sizeof io && !raus(f)) ok = 0;
                if (p[i] != '\n') io[iop++] = p[i];
                else if (++z < nz) { io[iop++] = '\r'; io[iop++] = '\n'; }   /* nach der letzten Zeile nichts */
            }
            ublk(b);
        }
        if (ok && iop && !raus(f)) ok = 0;
        if (FileClose(f)) ok = 0;
        CursorBusy(FALSE);
        if (!ok) FileDelete(tmp);
    }
    if (ok) {                                     /* neue Fassung ist ganz da: alte weg (Startdatei: als .BAK) */
        if (!bak && startdatei()) { endung(alt, pfad, "BAK"); FileDelete(alt); sb = FileRename(pfad, alt) == 0; bak = 1; }
        else FileDelete(pfad);
        if (FileRename(tmp, pfad)) ok = 0;
    }
    if (ok) { geaendert = 0; kopf(); melden(sb ? 25 : 0); if (test) { DbgPrint(sb ? "RDEDIT: gespeichert, BAK" : "RDEDIT: gespeichert"); } }
    else MsgBox(fe, StrGet(hi, 3), StrGet(hi, 1), MB_OK | MB_ERROR);
    return ok;
}
static void dialog(BYTE sp);
static BYTE weg(void)                             /* vor Schliessen/Oeffnen/Neu: 1 = weiter */
{
    short r;
    if (!geaendert || nurlesen) return 1;
    r = MsgBox(fe, StrGet(hi, 4), StrGet(hi, 1), MB_YESNOCANCEL | MB_ASK);
    if (r == ID_YES) { if (!pfad[0]) dialog(1); else speichern(); return !geaendert; }
    return r == ID_NO;
}
static void dialog(BYTE sp)                       /* Oeffnen / Speichern unter */
{
    FILEDLG d; char p[80];
    scpy(p, pfad, sizeof p);
    d.groesse = sizeof d; d.flags = sp ? FD_SAVE | FD_ASKOVER : FD_OPEN | FD_MUSTEXIST;
    d.titel = StrGet(hi, sp ? 6 : 5); d.filter = StrGet(hi, 7); d.datei = p; d.len = sizeof p;
    if (!sp && !weg()) return;
    if (!FileDialog(fe, &d)) return;
    scpy(pfad, p, sizeof pfad);
    if (sp) { nurlesen = 0; bak = 0; MenuEnable(mn, CM_SPEICHERN, 1); speichern(); kopf(); } else laden();
}

/* ---------------- Bearbeiten ---------------- */
static void geaend(void) { if (!geaendert) { geaendert = 1; kopf(); } }
static BYTE aus_loeschen(void)                    /* Auswahl entfernen */
{
    WORD y0, x0, y1, x1, n;
    if (!hatsel()) { sel = 0; return 1; }
    bereich(&y0, &x0, &y1, &x1);
    n = zkopie(y1, tb); zholen(y0);
    if (x0 + (n - x1) > LMAX) { melden(23); piep(); return 0; }
    _fmemcpy(lb + x0, tb + x1, n - x1); ll = x0 + (n - x1);
    cy = y0; cx = x0; sel = 0;
    if (!schreib()) return 0;
    if (y1 > y0) { zloeschen(y0 + 1, y1 - y0); neu_a(); } else neu_z(y0, y0);
    geaend(); return 1;
}
static void kopieren(void)
{
    WORD y0, x0, y1, x1, z, n, a, e; DWORD g = 1; HMEM h; char FAR *p;
    if (!hatsel()) return;
    bereich(&y0, &x0, &y1, &x1);
    for (z = y0; z <= y1; z++) { n = zkopie(z, tb); a = z == y0 ? x0 : 0; e = z == y1 ? x1 : n; g += e - a + (z < y1 ? 2 : 0); if (g > 65000UL) break; }
    if (g > 65000UL) { melden(22); piep(); return; }
    if (!(h = MemAlloc(g, MEM_MOVABLE | MEM_SHARE))) { melden(24); piep(); return; }
    if (!(p = (char FAR *)MemLock(h))) { MemFree(h); return; }
    for (z = y0, g = 0; z <= y1; z++) {
        n = zkopie(z, tb); a = z == y0 ? x0 : 0; e = z == y1 ? x1 : n;
        _fmemcpy(p + (WORD)g, tb + a, e - a); g += e - a;
        if (z < y1) { p[(WORD)g++] = '\r'; p[(WORD)g++] = '\n'; }
    }
    p[(WORD)g] = 0; MemUnlock(h);
    if (ClipOpen(fe)) { ClipEmpty(); ClipSet(CF_TEXT, h); ClipClose(); dbg("RDEDIT: Ablage", (WORD)g); }
    else MemFree(h);
}
static void einfuegen(void)                       /* Ablage an der Schreibmarke, \r\n oder \n */
{
    HMEM h = ClipGet(CF_TEXT); char FAR *p; WORD n, i, rn, y; BYTE ok = 1;
    if (!h || !aus_loeschen()) return;
    y = cy;
    if (!(p = (char FAR *)MemLock(h))) return;
    n = MemSize(h) > 65535UL ? 65535U : (WORD)MemSize(h);
    zholen(cy); rn = ll - cx; _fmemcpy(tb, lb + cx, rn); ll = cx;   /* Rest der Zeile merken */
    CursorBusy(TRUE);
    for (i = 0; i < n && p[i]; i++) {
        if (p[i] == '\r') continue;
        if (p[i] == '\n') {
            if (!(cy == y ? zsetzen(cy, lb, ll, 0) : zsetzen(cy, lb, ll, 1))) { ok = 0; break; }
            cy++; ll = 0; continue;
        }
        if (ll >= LMAX) { ok = 0; break; }
        lb[ll++] = p[i];
    }
    MemUnlock(h);
    cx = ll;
    if (ll + rn > LMAX) { rn = LMAX - ll; ok = 0; }
    _fmemcpy(lb + ll, tb, rn); ll += rn;
    if (!(cy == y ? zsetzen(cy, lb, ll, 0) : zsetzen(cy, lb, ll, 1))) ok = 0;
    lz = 0xFFFF; CursorBusy(FALSE);
    if (!ok) { melden(24); piep(); }
    if (cy != y) neu_a(); else neu_z(cy, cy);
    geaend();
    dbg("RDEDIT: eingefuegt bis Zeile", cy + 1);
}
static void zeichen(char ch)
{
    if (nurlesen) { piep(); return; }
    if (!aus_loeschen()) return;
    zholen(cy);
    if (ueber && cx < ll) lb[cx] = ch;
    else { if (ll >= LMAX) { melden(23); piep(); return; } _fmemmove(lb + cx + 1, lb + cx, ll - cx); lb[cx] = ch; ll++; }
    cx++; if (schreib()) geaend();
    neu_z(cy, cy); wx = spalte(lb, ll, cx);
}
static void umbruch(void)
{
    if (nurlesen) { piep(); return; }
    if (!aus_loeschen()) return;
    zholen(cy);
    if (!zsetzen(cy + 1, lb + cx, ll - cx, 1)) { melden(24); piep(); return; }
    zholen(cy); ll = cx; schreib();
    cy++; cx = wx = 0; geaend(); neu_a();
}
static void entf(BYTE zurueck)
{
    WORD n;
    if (nurlesen) { piep(); return; }
    if (hatsel()) { aus_loeschen(); return; }
    sel = 0; zholen(cy);
    if (zurueck) {
        if (cx) { cx--; } else if (cy) { cy--; zholen(cy); cx = ll; } else return;
    }
    zholen(cy);
    if (cx < ll) { _fmemmove(lb + cx, lb + cx + 1, ll - cx - 1); ll--; if (schreib()) geaend(); neu_z(cy, cy); }
    else if (cy + 1 < nz) {                       /* Zeilen verbinden */
        n = zkopie(cy + 1, tb); zholen(cy);
        if (ll + n > LMAX) { melden(23); piep(); return; }
        _fmemcpy(lb + ll, tb, n); ll += n;
        if (schreib()) { zloeschen(cy + 1, 1); geaend(); }
        neu_a();
    }
    wx = spalte(lb, ll, cx);
}

/* ---------------- Suchen, Ersetzen, Gehe zu ---------------- */
static short treffer(const char *s, WORD n, WORD ab, LPCSTR m)   /* Stelle von m in s ab ab (ohne Gross/Klein), -1 */
{
    WORD l = slen(m), i, j;
    if (!l) return -1;
    for (i = ab; i + l <= n; i++) { for (j = 0; j < l && klein(s[i + j]) == klein(m[j]); j++) ; if (j == l) return (short)i; }
    return -1;
}
static BYTE weitersuchen(BYTE rund)               /* ab Schreibmarke; rund: am Ende oben weiter */
{
    WORD z = cy, ab = cx, k, n; short t;
    for (k = 0; k <= nz; k++) {
        n = zkopie(z, tb);
        if ((t = treffer(tb, n, ab, such_s)) >= 0) {
            sel = 1; ay = z; ax = (WORD)t; cy = z; cx = (WORD)t + slen(such_s); wx = spalte(tb, n, cx);
            lz = 0xFFFF; zeigen(); neu_a(); melden(0); dbg("RDEDIT: gefunden Zeile", z + 1); return 1;
        }
        ab = 0;
        if (++z >= nz) { if (!rund) break; z = 0; }
    }
    melden(14); piep(); dbg("RDEDIT: nicht gefunden", 0); return 0;
}
static BYTE ersetze_sel(void)                     /* Auswahl (ein Treffer in einer Zeile) durch ers_s */
{
    WORD l = slen(ers_s), y0, x0, y1, x1;
    bereich(&y0, &x0, &y1, &x1); zholen(y0);
    if (ll - (x1 - x0) + l > LMAX) return 0;
    _fmemmove(lb + x0 + l, lb + x1, ll - x1); _fmemcpy(lb + x0, ers_s, l); ll = ll - (x1 - x0) + l;
    cy = y0; cx = x0 + l; sel = 0;
    if (!schreib()) return 0;
    geaend(); return 1;
}
static BOOL RDCALLBACK eproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    (void)p2;
    if (msg == M_INITDIALOG) {
        SendMessage(DlgItem(d, 20), C_SETLIMIT, 40, 0); SendMessage(DlgItem(d, 21), C_SETLIMIT, 40, 0);
        DlgSetText(d, 20, such_s); DlgSetText(d, 21, ers_s); return FALSE;
    }
    if (msg == M_COMMAND && (p1 == ID_OK || p1 == ID_YES)) { DlgGetText(d, 20, such_s, sizeof such_s); DlgGetText(d, 21, ers_s, sizeof ers_s); }
    return FALSE;
}
static void ersetzen(void)
{
    short r = DialogRun(hi, 10, fe, eproc, 0); WORD z, n, l, ab, k = 0, ls, zu = 0; short t; char num[8];
    if (nurlesen || !such_s[0] || (r != ID_OK && r != ID_YES)) return;
    ls = slen(such_s); l = slen(ers_s);
    if (r == ID_YES) {                            /* alle: Zeile fuer Zeile */
        CursorBusy(TRUE);
        for (z = 0; z < nz; z++) {
            n = zkopie(z, tb); ab = 0;
            if (treffer(tb, n, 0, such_s) < 0) continue;
            while ((t = treffer(tb, n, ab, such_s)) >= 0) {
                if (n - ls + l > LMAX) { zu = 1; break; }
                _fmemmove(tb + t + l, tb + t + ls, n - t - ls); _fmemcpy(tb + t, ers_s, l); n = n - ls + l; ab = (WORD)t + l; k++;
            }
            if (!zsetzen(z, tb, n, 0)) { zu = 1; break; }
        }
        CursorBusy(FALSE); sel = 0; lz = 0xFFFF;
        if (k) geaend();
    } else {                                      /* einzeln mit Rueckfrage, bis zum Ende */
        sel = 0;
        while (weitersuchen(0)) {
            UpdateWindow(fe);
            r = MsgBox(fe, StrGet(hi, 20), StrGet(hi, 21), MB_YESNOCANCEL | MB_ASK);
            if (r == ID_CANCEL) break;
            if (r == ID_YES) { if (ersetze_sel()) k++; else zu = 1; }
            else sel = 0;
            neu_a();
        }
    }
    meld[0] = 0; num[0] = 0; szahl(num, k, sizeof num); scat(meld, num, sizeof meld); scat(meld, " ", sizeof meld); scat(meld, StrGet(hi, 19), sizeof meld);
    if (zu) { scat(meld, ", ", sizeof meld); scat(meld, StrGet(hi, 23), sizeof meld); }
    zeigen(); neu_a(); dbg("RDEDIT: ersetzt", k);
}
static void gehe(void)
{
    char t[8]; WORD v = 0, i;
    t[0] = 0;
    if (!InputBox(fe, StrGet(hi, 17), StrGet(hi, 18), t, sizeof t)) return;
    for (i = 0; t[i] >= '0' && t[i] <= '9'; i++) v = v * 10 + (t[i] - '0');
    if (!v) return;
    cy = v > nz ? nz - 1 : v - 1; cx = wx = 0; sel = 0; zeigen(); neu_a();
}

/* ---------------- Befehle und Tasten ---------------- */
static void hinter_sel(void)                      /* Suche ab dem Zeichen nach dem Anfang der Auswahl */
{
    WORD y0, x0, y1, x1;
    if (hatsel()) { bereich(&y0, &x0, &y1, &x1); cy = y0; cx = x0 + 1; }
    sel = 0;
}
static void befehl(WORD id)
{
    switch (id) {
    case CM_NEU: if (weg()) { pfad[0] = 0; laden(); } break;
    case CM_OEFFNEN: dialog(0); break;
    case CM_SPEICHERN: if (!pfad[0]) dialog(1); else speichern(); break;
    case CM_UNTER: dialog(1); break;
    case CM_ENDE: PostMessage(fe, M_CLOSE, 0, 0); break;
    case CM_AUS: if (!nurlesen && hatsel()) { kopieren(); aus_loeschen(); } break;
    case CM_KOPIE: kopieren(); break;
    case CM_EIN: if (!nurlesen) einfuegen(); else piep(); break;
    case CM_LOESCH: entf(0); break;
    case CM_ALLES: sel = 1; ay = 0; ax = 0; cy = nz - 1; cx = zkopie(cy, tb); lz = 0xFFFF; neu_a(); break;
    case CM_UEBER: ueber ^= 1; MenuCheck(mn, CM_UEBER, ueber); neu_z(cy, cy); break;
    case CM_SUCHEN: if (InputBox(fe, StrGet(hi, 15), StrGet(hi, 16), such_s, sizeof such_s) && such_s[0]) { hinter_sel(); weitersuchen(1); } break;
    case CM_WEITER: if (such_s[0]) { hinter_sel(); weitersuchen(1); } else befehl(CM_SUCHEN); break;
    case CM_ERSETZEN: ersetzen(); break;
    case CM_GEHE: gehe(); break;
    }
    zeigen();
}
static void taste(WORD k, WORD ks)
{
    WORD oy = cy, n; BYTE sh = (ks & KS_SHIFT) != 0, ct = (ks & KS_CTRL) != 0, bew = 1;
    if (meld[0]) melden(0);
    if (ct && k < 32) {                           /* Strg+Buchstabe kommt als Steuerzeichen */
        switch (k) {
        case 1: befehl(CM_ALLES); break;
        case 3: befehl(CM_KOPIE); break;
        case 6: befehl(CM_SUCHEN); break;
        case 7: befehl(CM_GEHE); break;
        case 8: befehl(CM_ERSETZEN); break;
        case 14: befehl(CM_NEU); break;
        case 15: befehl(CM_OEFFNEN); break;
        case 19: befehl(CM_SPEICHERN); break;
        case 22: befehl(CM_EIN); break;
        case 24: befehl(CM_AUS); break;
        }
        neu_z(oy, cy); return;
    }
    zholen(cy);
    switch (k) {
    case K_LEFT: if (cx) cx--; else if (cy) { cy--; zholen(cy); cx = ll; } wx = 0xFFFF; break;
    case K_RIGHT: if (cx < ll) cx++; else if (cy + 1 < nz) { cy++; cx = 0; } wx = 0xFFFF; break;
    case 0x173: if (cx) { while (cx && lb[cx - 1] == ' ') cx--; while (cx && lb[cx - 1] != ' ') cx--; } wx = 0xFFFF; break;   /* Strg+Links: Wort */
    case 0x174: while (cx < ll && lb[cx] != ' ') cx++; while (cx < ll && lb[cx] == ' ') cx++; wx = 0xFFFF; break;
    case K_HOME: if (ct) cy = 0; cx = 0; wx = 0xFFFF; break;
    case 0x177: cy = 0; cx = 0; wx = 0xFFFF; break;
    case K_END: if (ct) cy = nz - 1; zholen(cy); cx = ll; wx = 0xFFFF; break;
    case 0x175: cy = nz - 1; zholen(cy); cx = ll; wx = 0xFFFF; break;
    case K_UP: if (cy) cy--; break;
    case K_DOWN: if (cy + 1 < nz) cy++; break;
    case K_PGUP: n = rows > 1 ? rows - 1 : 1; cy = cy > n ? cy - n : 0; top = top > n ? top - n : 0; neu_a(); break;
    case K_PGDN: n = rows > 1 ? rows - 1 : 1; cy = cy + n < nz ? cy + n : nz - 1; if (top + n + rows <= nz) top += n; else top = nz > rows ? nz - rows : 0; neu_a(); break;
    case K_INS: bew = 0; if (sh) befehl(CM_EIN); else if (ct) befehl(CM_KOPIE); else befehl(CM_UEBER); return;
    case K_DEL: bew = 0; if (sh) befehl(CM_AUS); else entf(0); break;
    case K_BACK: bew = 0; entf(1); break;
    case K_ENTER: bew = 0; umbruch(); break;
    case K_TAB: bew = 0; if (!ct && !(ks & KS_ALT)) zeichen('\t'); break;
    case K_ESC: PostMessage(fe, M_CLOSE, 0, 0); return;
    case K_F3: befehl(CM_WEITER); return;
    default: return;
    }
    if (bew) {
        if (k == K_UP || k == K_DOWN || k == K_PGUP || k == K_PGDN) {   /* Spalte merken */
            if (wx == 0xFFFF) { zholen(oy); wx = spalte(lb, ll, cx); }
            lz = 0xFFFF; zholen(cy); cx = stelle(lb, ll, wx);
        } else { lz = 0xFFFF; zholen(cy); if (cx > ll) cx = ll; wx = spalte(lb, ll, cx); }
    }
    (void)sh;
    zeigen(); neu_z(oy, cy);                      /* Anker bleibt: Auswahl aendert sich nur zwischen alter und neuer Zeile */
}
static void bewegen_start(WORD ks)                /* vor einer Bewegung: Umschalt zieht die Auswahl auf */
{
    if (ks & KS_SHIFT) { if (!sel) { sel = 1; ay = cy; ax = cx; } }
    else if (sel) { WORD y0, x0, y1, x1; if (hatsel()) { bereich(&y0, &x0, &y1, &x1); neu_z(y0, y1); } sel = 0; }
}
static BYTE bewegung(WORD k) { return k == K_LEFT || k == K_RIGHT || k == K_UP || k == K_DOWN || k == K_HOME || k == K_END || k == K_PGUP || k == K_PGDN || k == 0x173 || k == 0x174 || k == 0x175 || k == 0x177; }
static void maus(short x, short y, BYTE neu_sel)
{
    WORD z, c;
    if (y < 0) z = top ? top - 1 : 0; else { z = top + (WORD)(y / zh); if ((WORD)(y / zh) >= rows) z = top + rows; }
    if (z >= nz) z = nz - 1;
    c = left + (x > RAND ? (WORD)((x - RAND + cw / 2) / cw) : 0);
    if (neu_sel) { if (hatsel()) neu_a(); sel = 0; }
    neu_z(cy, cy);
    cy = z; lz = 0xFFFF; zholen(cy); cx = stelle(lb, ll, c); wx = spalte(lb, ll, cx);
    if (neu_sel) { sel = 1; ay = cy; ax = cx; }
    zeigen(); neu_z(ay < cy ? ay : cy, ay < cy ? cy : ay);
}
static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    POINT pt;
    if (!fe) fe = w;                              /* M_SIZE kommt schon in CreateWindow */
    switch (msg) {
    case M_PAINT: malen(); return 0;
    case M_SIZE: if (zh) groesse(); return 0;
    case M_SETFOCUS: case M_KILLFOCUS: neu_z(cy, cy); return 0;
    case M_KEY:
        if (bewegung(p1)) bewegen_start(HIWORD(p2));
        taste(p1, HIWORD(p2)); return 0;
    case M_CHAR: if ((BYTE)p1 >= ' ' && !(HIWORD(p2) & (KS_CTRL | KS_ALT))) { WORD oy = cy; zeichen((char)p1); zeigen(); neu_z(oy, cy); } return 0;
    case M_BUTTONDOWN:
        SetFocus(w);
        if ((p1 & 0xFF) == 2) { pt.x = (short)LOWORD(p2); pt.y = (short)HIWORD(p2); ClientToScreen(w, &pt); befehl(MenuPopup(w, mn, 1, pt.x, pt.y)); return 0; }
        if ((short)HIWORD(p2) >= (short)(rows * zh)) return 0;
        if ((p1 >> 8) & KS_SHIFT) { if (!sel) { sel = 1; ay = cy; ax = cx; } maus((short)LOWORD(p2), (short)HIWORD(p2), 0); }
        else maus((short)LOWORD(p2), (short)HIWORD(p2), 1);
        ziehen = 1; CaptureMouse(w); return 0;
    case M_MOUSEMOVE: if (ziehen) maus((short)LOWORD(p2), (short)HIWORD(p2), 0); return 0;
    case M_BUTTONUP: if (ziehen) { ziehen = 0; ReleaseMouse(); } return 0;
    case M_SCROLL:
        if (HIWORD(p2) == SB_VERT) {
            WORD m = nz > rows ? nz - rows : 0;
            switch (p1) {
            case SB_LINEUP: if (top) top--; break;
            case SB_LINEDOWN: top++; break;
            case SB_PAGEUP: top = top > rows ? top - rows : 0; break;
            case SB_PAGEDOWN: top += rows; break;
            case SB_THUMB: top = LOWORD(p2); break;
            case SB_TOP: top = 0; break;
            case SB_BOTTOM: top = m; break;
            }
            if (top > m) top = m;
        } else {
            WORD m = LMAX > cols ? LMAX - cols : 0;
            switch (p1) {
            case SB_LINEUP: if (left) left--; break;
            case SB_LINEDOWN: left++; break;
            case SB_PAGEUP: left = left > cols ? left - cols : 0; break;
            case SB_PAGEDOWN: left += cols; break;
            case SB_THUMB: left = LOWORD(p2); break;
            }
            if (left > m) left = m;
        }
        balken(); neu_a(); return 0;
    case M_MENUOPEN:
        MenuEnable(mn, CM_AUS, !nurlesen && hatsel()); MenuEnable(mn, CM_KOPIE, hatsel());
        MenuEnable(mn, CM_EIN, !nurlesen && ClipGet(CF_TEXT) != 0); MenuEnable(mn, CM_ERSETZEN, !nurlesen);
        return 0;
    case M_COMMAND: befehl(p1); return 0;
    case M_CLOSE: if (weg()) DestroyWindow(w); return 0;
    case M_DESTROY: alle_frei(); PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H;
    (void)show; hi = inst;
    if (cmd && cmd[0] == 'T' && (cmd[1] == ' ' || !cmd[1])) { test = 1; cmd += cmd[1] ? 2 : 1; }
    while (cmd && *cmd == ' ') cmd++;
    if (cmd && *cmd) scpy(pfad, cmd, sizeof pfad);
    wc.name = "RdEdit"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); zh = GetCaps(d, CAPS_TEXTH); cw = TextWidth(d, "M", 1); ReleaseDC(0, d);
    fe = CreateWindow("RdEdit", StrGet(inst, 1), WS_NORMAL | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, (short)(W / 32), (short)(H / 10), (short)(W - W / 16), (short)(H - H / 10 - H / 20), 0, 1, inst);
    if (!fe) return 1;
    mn = MenuLoad(inst, 1); MenuSet(fe, mn);
    groesse(); laden(); zeigen();
    SetFocus(fe);
    if (test) DbgPrint("RDEDIT: bereit");
    while (GetMessage(&m)) DispatchMessage(&m);
    MenuFree(mn);
    return 0;
}
