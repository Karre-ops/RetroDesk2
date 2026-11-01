/* RDPRN.C - Drucken (RDPRN.MOD), geladen von RDKX bei PrnOpen..PrnAbort (API 0x3C0..0x3C3), ausserdem HelpShow (F1).
 * RD2.INI [Drucker]: Treiber=EPSON9 (-> EPSON9.DRV), Port=1..3 (LPT) oder 0 (Datei), Datei=Pfad
 * (Vorgabe DRUCK.PRN im RD2-Ordner; nicht PRN.*, das ist unter DOS das Druckergeraet), Entwurf=0/1. PrnOpen mit PJ_DIALOG zeigt den Druckdialog
 * (Anschluss, Kopien, Entwurf) und merkt sich die Wahl.
 * Baender: Eine ganze Seite (960 x 792 Punkte) braucht 95 KB, deshalb liegt nur ein Band mit BANDZ
 * Zeilen im Speicher (bei 960 Punkten 5,6 KB). Das Programm zeichnet die ganze Seite in den Drucker-DC,
 * PrnPage gibt das Band aus, verschiebt den Ursprung und meldet PRN_NOCHMAL, bis die Seite (und alle
 * Kopien) gedruckt sind:   do { zeichne(dc, seite); } while ((r = PrnPage(dc)) == PRN_NOCHMAL);
 * Der Drucker-DC gehoert RDPRN: SetOrigin darin nicht benutzen, Groesse aus PRNJOB (breite/hoehe/dpi).
 * LPT: direkt am Port (Adresse aus dem BIOS), Bereitschaft mit Zeitlimit; ist der Drucker nicht bereit,
 * fragt RDPRN "Nochmal versuchen?" (Abbruch -> RD_ERR_ABORT). Nur ein Druckauftrag zur Zeit.
 * Uebersetzt mit -ml: Zeiger auf Stapelvariablen (SS != DS) bleiben gueltig. */
#include <conio.h>
#include <i86.h>
#include "RD2DRV.H"
#pragma intrinsic(inp, outp)

#define BANDZ 48                                  /* Vielfaches der Bandhoehe aller Treiber (8, 24) */
typedef struct { WORD w, h, bpp, bpr; } BMPH_;   /* Kopf eines Bitmap-Blocks (RDGDI), danach die Zeilen */
#define ID_DRV  30
#define ID_LPT1 31
#define ID_LPT2 32
#define ID_DAT  33
#define ID_KOP  34
#define ID_ENT  35

static PRNDRV FAR *dv;
static HMOD dm, selbst;
static HBMP bmp;
static HDC pdc;
static HWND wbes;
static short datei = -1, y0, hoehe;
static WORD bpr, bz, kopie, kopien, port, lpt, fehler;
static BYTE weiss;
static char ini[80], dname[80];

static WORD slen(const char *s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, const char *s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, const char *s, WORD n) { WORD i = slen(d); if (i < n) scpy(d + i, s, n - i); }
static void heim(char *d, const char *datei_)    /* RD2-Ordner + Dateiname */
{
    SYSINFO si; WORD i;
    KernInfo(&si); scpy(d, si.home, 66); i = slen(d);
    if (i && d[i - 1] != '\\') scat(d, "\\", 80);
    scat(d, datei_, 80);
}

/* ---- Ausgabe -------------------------------------------------------------- */
static DWORD ticks(void) { return *(DWORD volatile FAR *)MK_FP(0x40, 0x6C); }
static WORD nichtbereit(void)                     /* Rueckfrage; 0 = nochmal versuchen */
{
    char t[6] = "LPT1"; t[3] = (char)('0' + port);
    if (MsgBox(wbes, StrGet(0, 25), t, MB_OKCANCEL | MB_WARN) == ID_OK) return 0;
    return fehler = RD_ERR_ABORT;
}
static WORD lptbyte(BYTE b)
{
    DWORD t0 = ticks(); BYTE s;
    for (;;) {
        s = (BYTE)inp(lpt + 1);
        if ((s & 0x80) && !(s & 0x20) && (s & 0x08)) break;      /* bereit, Papier da, kein Fehler */
        if (ticks() - t0 > 36) { if (nichtbereit()) return fehler; t0 = ticks(); }   /* 2 s */
    }
    outp(lpt, b); outp(lpt + 2, 0x0D); outp(lpt + 2, 0x0C);       /* Daten, Strobe */
    return 0;
}
static WORD RDEXPORT aus(const BYTE FAR *p, WORD n)
{
    if (fehler) return fehler;
    if (datei >= 0) return FileWrite(datei, p, n) == n ? 0 : (fehler = RD_ERR_DISK);
    while (n--) if (lptbyte(*p++)) return fehler;
    return 0;
}

/* ---- Druckdialog (Vorlage im Speicher, Format RT_DIALOG wie RDCTLX) ------- */
typedef struct { WORD port, kopien, ent; } DWAHL;
static BYTE FAR *put(BYTE FAR *p, LPCSTR s) { do *p++ = *s; while (*s++); return p; }
static BYTE FAR *elem(BYTE FAR *p, LPCSTR kl, LPCSTR tx, WORD id, short x, short y, short w, short h, DWORD st)
{
    short FAR *q;
    p = put(p, kl); p = put(p, tx); q = (short FAR *)p;
    q[0] = (short)id; q[1] = x; q[2] = y; q[3] = w; q[4] = h; *(DWORD FAR *)(q + 5) = st;
    return (BYTE FAR *)(q + 7);
}
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    static DWAHL FAR *w;
    if (msg == M_INITDIALOG) {
        w = (DWAHL FAR *)p2;
        DlgCheck(d, w->port == 1 ? ID_LPT1 : w->port == 2 ? ID_LPT2 : ID_DAT, 1);
        DlgSetInt(d, ID_KOP, w->kopien); DlgCheck(d, ID_ENT, w->ent);
        return TRUE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        BOOL ok; long k = DlgGetInt(d, ID_KOP, &ok);
        w->port = DlgChecked(d, ID_LPT1) ? 1 : DlgChecked(d, ID_LPT2) ? 2 : 0;
        w->kopien = ok && k >= 1 && k <= 99 ? (WORD)k : 1;
        w->ent = DlgChecked(d, ID_ENT) ? 1 : 0;
    }
    return FALSE;
}
static short dialog(HWND owner, DWAHL *w, LPCSTR treiber)
{
    HMEM hm; BYTE FAR *v, FAR *p; short FAR *q; short e; char t[5][24];
    WORD i;
    for (i = 0; i < 5; i++) scpy(t[i], StrGet(0, (WORD)(20 + i)), sizeof t[i]);   /* Drucken, Drucker:, Datei, Kopien:, Entwurf */
    if (!(hm = MemAlloc(512, MEM_MOVABLE))) return 0;
    v = (BYTE FAR *)MemLock(hm);
    *(DWORD FAR *)v = 0; q = (short FAR *)(v + 4); q[0] = q[1] = 0; q[2] = 200; q[3] = 92; q[4] = 10;
    p = put(v + 14, t[0]);
    p = elem(p, CL_STATIC, t[1], 0xFFFF, 8, 6, 44, 10, 0);
    p = elem(p, CL_STATIC, treiber, ID_DRV, 54, 6, 138, 10, 0);
    p = elem(p, CL_RADIO, "LPT&1", ID_LPT1, 8, 22, 50, 10, 0);
    p = elem(p, CL_RADIO, "LPT&2", ID_LPT2, 62, 22, 50, 10, 0);
    p = elem(p, CL_RADIO, t[2], ID_DAT, 116, 22, 76, 10, 0);
    p = elem(p, CL_STATIC, t[3], 0xFFFF, 8, 40, 44, 10, 0);
    p = elem(p, CL_EDIT, "", ID_KOP, 54, 38, 30, 12, ES_NUMBER);
    p = elem(p, CL_CHECK, t[4], ID_ENT, 8, 56, 184, 10, 0);
    p = elem(p, CL_BUTTON, StrGet(0, 2), ID_OK, 36, 72, 56, 14, ES_DEFAULT);
    elem(p, CL_BUTTON, StrGet(0, 3), ID_CANCEL, 108, 72, 56, 14, 0);
    e = DialogIndirect(0, v, owner, (DLGPROC)dproc, (DWORD)(DWAHL FAR *)w);
    MemUnlock(hm); MemFree(hm);
    return e;
}

/* ---- Band ------------------------------------------------------------------ */
static void neu(void)                             /* Band weiss, Ursprung auf die naechsten Zeilen */
{
    BMPH_ FAR *b;
    BYTE FAR *z; WORD n = bpr * bz;
    b = (BMPH_ FAR *)MemLock(bmp); z = (BYTE FAR *)(b + 1);
    while (n--) *z++ = weiss;
    MemUnlock(bmp);
    SetOrigin(pdc, 0, (short)-y0);
}
static WORD ausband(void)
{
    BMPH_ FAR *b; BYTE FAR *z; WORD n = bpr * bz, e, zl = (WORD)(hoehe - y0 < (short)bz ? hoehe - y0 : bz);
    b = (BMPH_ FAR *)MemLock(bmp); z = (BYTE FAR *)(b + 1);
    if (weiss) { BYTE FAR *x = z; while (n--) { *x = (BYTE)~*x; x++; } }   /* Treiber: 1 = Punkt */
    e = dv->band(z, bpr, zl);
    MemUnlock(bmp);
    return e;
}
static void schluss(BOOL abbruch)
{
    if (!dv) return;
    dv->close(abbruch);                           /* nach einem Ausgabefehler sendet aus() nichts mehr */
    if (pdc) DCFree(pdc);
    if (bmp) BmpFree(bmp);
    if (datei >= 0) FileClose(datei);
    dv->h.done(); ModFree(dm);
    dv = 0; pdc = 0; bmp = 0; datei = -1; dm = 0;
}

/* ---- API ------------------------------------------------------------------- */
static HDC RDEXPORT p_PrnOpen(HWND owner, PRNJOB FAR *job)
{
    char n[14]; FARPROC f; PRNDRV FAR *t; DWAHL w; WORD e;
    if (dv) { SetLastError(RD_ERR_BUSY); return 0; }
    if (!job || job->groesse < sizeof(PRNJOB)) { SetLastError(RD_ERR_HANDLE); return 0; }
    heim(ini, "RD2.INI");
    ProfileGetStr(ini, "Drucker", "Treiber", "EPSON9", n, 9); scat(n, ".DRV", sizeof n);
    w.port = (WORD)ProfileGetInt(ini, "Drucker", "Port", 1); if (w.port > 3) w.port = 1;
    w.ent = ProfileGetInt(ini, "Drucker", "Entwurf", 0) != 0;
    w.kopien = job->kopien ? job->kopien : 1;
    if (!(dm = ModLoad(n))) { MsgBox(owner, StrGet(0, 26), n, MB_OK | MB_ERROR); return 0; }
    f = ModProc(dm, 1); t = f ? (PRNDRV FAR *)((void FAR * (RDAPI *)(void))f)() : 0;
    if (!t || t->h.typ != DRV_PRN || t->h.groesse < sizeof(PRNDRV) || t->h.init(0)) {
        ModFree(dm); dm = 0; MsgBox(owner, StrGet(0, 26), n, MB_OK | MB_ERROR); SetLastError(RD_ERR_NOTSUP); return 0;
    }
    if (job->flags & PJ_DIALOG) {
        char tn[17]; WORD i; for (i = 0; i < 16 && t->h.name[i]; i++) tn[i] = t->h.name[i]; tn[i] = 0;
        if (dialog(owner, &w, tn) != ID_OK) { t->h.done(); ModFree(dm); dm = 0; SetLastError(RD_ERR_ABORT); return 0; }
        { char z[4]; z[0] = (char)('0' + w.port); z[1] = 0; ProfileSetStr(ini, "Drucker", "Port", z);
          z[0] = (char)('0' + w.ent); ProfileSetStr(ini, "Drucker", "Entwurf", z); }
        job->kopien = w.kopien;
    }
    if (w.ent) job->flags |= PJ_ENTWURF; else job->flags &= ~PJ_ENTWURF;
    wbes = owner; port = w.port; kopien = w.kopien; kopie = 0; fehler = 0; y0 = 0; datei = -1;
    if (!port) {
        ProfileGetStr(ini, "Drucker", "Datei", "", dname, sizeof dname);
        if (!dname[0]) heim(dname, "DRUCK.PRN");
        if ((datei = FileCreate(dname, 0)) < 0) { e = RD_ERR_DISK; goto weg; }
    } else {
        lpt = *(WORD FAR *)MK_FP(0x40, 8 + 2 * (port - 1));
        if (!lpt) { MsgBox(owner, StrGet(0, 27), 0, MB_OK | MB_ERROR); e = RD_ERR_NOTSUP; goto weg; }
        outp(lpt + 2, 0x0C);                      /* Init aus, Select */
    }
    dv = t;
    if ((e = dv->open(job, aus)) != 0) goto weg2;
    hoehe = job->hoehe; bpr = ((WORD)job->breite + 7) >> 3; bz = BANDZ;
    if (!(bmp = BmpCreate((WORD)job->breite, bz, 1)) || !(pdc = BmpDC(bmp))) { e = RD_ERR_NOMEM; goto weg2; }
    Pixel(pdc, 0, 0, C_WHITE);                    /* welches Bit ist weiss? */
    { BMPH_ FAR *b = (BMPH_ FAR *)MemLock(bmp); weiss = (*(BYTE FAR *)(b + 1) & 0x80) ? 0xFF : 0; MemUnlock(bmp); }
    neu();
    selbst = ModLoad("RDPRN.MOD");                /* bleibt geladen bis PrnClose/PrnAbort */
    return pdc;
weg2:
    schluss(TRUE); SetLastError(e); return 0;
weg:
    t->h.done(); ModFree(dm); dm = 0; SetLastError(e); return 0;
}
static WORD RDEXPORT p_PrnPage(HDC dc, void FAR *x)   /* x: RDKX ruft alle Exporte mit zwei Parametern */
{
    WORD e;
    (void)x;
    if (!dv || dc != pdc) return RD_ERR_HANDLE;
    if (fehler) return fehler;
    if ((e = ausband()) != 0) return e;
    y0 += (short)bz;
    if (y0 < hoehe) { neu(); return PRN_NOCHMAL; }
    if ((e = dv->page()) != 0) return e;
    y0 = 0;
    if (++kopie < kopien) { neu(); return PRN_NOCHMAL; }
    kopie = 0; neu();
    return 0;
}
static WORD RDEXPORT p_PrnClose(HDC dc, void FAR *x)
{
    WORD e;
    (void)x;
    if (!dv || dc != pdc) return RD_ERR_HANDLE;
    e = fehler; schluss(FALSE);
    if (selbst) { HMOD s = selbst; selbst = 0; ModFree(s); }   /* RDKX haelt beim Aufruf noch einen Verweis */
    return e;
}
static WORD RDEXPORT p_PrnAbort(HDC dc, void FAR *x)
{
    (void)x;
    if (!dv || dc != pdc) return RD_ERR_HANDLE;
    schluss(TRUE);
    if (selbst) { HMOD s = selbst; selbst = 0; ModFree(s); }
    return 0;
}
static void RDEXPORT p_Ende(void) { schluss(TRUE); selbst = 0; }   /* RD endet mit offenem Auftrag */

/* ---- HelpShow (API 0x2D3, Nr. 6): startet HILFE.EXE aus dem RD2-Ordner mit "datei thema" und kehrt sofort zurueck.
 * Liegt hier, weil RDKX keinen Platz fuer ein eigenes Lademodul hatte (27.10.2026). ---- */
typedef struct { WORD t; LPCSTR d; } HARG;    /* so liegen die Parameter von HelpShow auf dem Stapel (pascal) */

static WORD RDEXPORT h_Show(HWND w, HARG FAR *a)
{
    SYSINFO si; char p[80], c[90]; WORD i = 0, k = 0, t; char z[6];
    (void)w;
    KernInfo(&si);
    while (si.home[i] && i < 66) { p[i] = si.home[i]; i++; }
    if (i && p[i - 1] != '\\') p[i++] = '\\';
    { static const char n[] = "HILFE.EXE"; for (k = 0; k < sizeof n; k++) p[i + k] = n[k]; }
    k = 0;
    if (a && a->d) while (a->d[k] && k < 80) { c[k] = a->d[k]; k++; }
    c[k++] = ' ';
    t = a ? a->t : 0; i = 0;
    do { z[i++] = (char)('0' + t % 10); t /= 10; } while (t && i < 5);
    while (i) c[k++] = z[--i];
    c[k] = 0;
    return TaskExec(p, c, SW_SHOW) != 0;
}
static struct { WORD n; FARPROC f[6]; } ext = { 6, {
    (FARPROC)p_Ende, (FARPROC)p_PrnOpen, (FARPROC)p_PrnPage, (FARPROC)p_PrnClose, (FARPROC)p_PrnAbort, (FARPROC)h_Show } };
void FAR * RDAPI RdModInit(void) { return &ext; }
