/* RDFORMAT.C - Disketten formatieren und bootfaehig machen (Diskette 2, eigener Code, MIT).
 * Aufruf: RDFORMAT.EXE A:        Formatieren (Dialog: Kapazitaet, Bezeichnung, Schnell, Systemdateien)
 *         RDFORMAT.EXE A: /SYS   vorhandene Diskette bootfaehig machen (Bootsektor + Systemdateien)
 * Die Diskettenzugriffe macht RDDISK.MOD (BIOS). Formatiert wird Spur fuer Spur mit Pruefen;
 * defekte Spuren ausserhalb des Verwaltungsbereichs kommen als defekt in die FAT. Danach
 * Bootsektor (BPB, Bezeichnung, Seriennummer), beide FATs, Hauptverzeichnis.
 * Bootfaehig: FreeDOS-Bootcode aus <RD2>\BOOT12.BIN (GPL, BPB der Diskette bleibt), dazu
 * KERNEL.SYS und, wenn vorhanden, COMMAND.COM aus dem Hauptverzeichnis des Startlaufwerks.
 * Die Systemdateien werden vor dem Formatieren in den Speicher gelesen (eine Diskette, ein Laufwerk).
 * Phantom-B: (ein Laufwerk) wird ueber A: angesprochen. */
#include <i86.h>
#include "RD2.H"
#include "RDDISK.H"

#define ID_LW     19
#define ID_NAME   20
#define ID_SCHNELL 21
#define ID_SYS    22
#define ID_K1     31                              /* 31..33 Kapazitaet */
#define ID_STAND  40
#define ID_BALKEN 41
#define ID_SCHRITT 90

static const DGEO tab[] = {
    {  360, 40, 2,  9,  720, 0xFD, 2,  7, 2 }, {  720, 80, 2,  9, 1440, 0xF9, 2,  7, 3 },
    { 1200, 80, 2, 15, 2400, 0xF9, 1, 14, 7 }, { 1440, 80, 2, 18, 2880, 0xF0, 1, 14, 9 },
    { 2880, 80, 2, 36, 5760, 0xF0, 2, 15, 9 }, {  180, 40, 1,  9,  360, 0xFC, 1,  4, 2 } };
static const BYTE wahl[7][3] = {                  /* BIOS-Typ -> angebotene Formate (Index in tab, 0xFF = keins) */
    { 0, 5, 0xFF }, { 0, 5, 0xFF }, { 2, 0, 0xFF }, { 1, 0xFF, 0xFF }, { 3, 1, 0xFF }, { 4, 3, 1 }, { 4, 3, 1 } };

static HINST inst; static HMOD hm;
static RDD_TYP_F dTyp; static RDD_START_F dStart; static RDD_SPUR_F dSpur; static RDD_SEKTOR_F dSek; static RDD_ENDE_F dEnde;
static WORD lw, doslw, sys, schnell, fehler, kbfrei, kbdef;
static char lwbuch[2], name[12], txt[200], pfad[80];
static const DGEO *g;
static WORD spur, nspur, daten, ncl;
static BYTE defekt[40];                           /* Bit je Spur (bis 320 Spuren) */
static BYTE sek[512], fat[9 * 512];
static BYTE boot[512]; static WORD hatboot;
typedef struct { HMEM h; DWORD n; } DATEI;
static DATEI kern, cmd;
static SYSINFO si;

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(LPSTR d, LPCSTR s, WORD max) { WORD i; for (i = 0; i + 1 < max && s[i]; i++) d[i] = s[i]; d[i] = 0; }
static void scat(LPSTR d, LPCSTR s, WORD max) { WORD n = slen(d); scpy(d + n, s, max - n); }
static void zahl(LPSTR b, DWORD v) { char t[12]; WORD i = 0, j = 0; do t[i++] = (char)('0' + v % 10); while ((v /= 10) != 0); while (i) b[j++] = t[--i]; b[j] = 0; }
static void fmt(LPSTR d, WORD id, LPCSTR a, LPCSTR b)   /* Text id, %s der Reihe nach durch a und b */
{
    LPCSTR s = StrGet(inst, id); WORD n = 0, k = 0; LPCSTR e;
    if (!s) s = "?";
    while (*s && n < sizeof txt - 1) {
        if (s[0] == '%' && s[1] == 's') { e = k++ ? b : a; while (e && *e && n < sizeof txt - 1) d[n++] = *e++; s += 2; }
        else d[n++] = *s++;
    }
    d[n] = 0;
}
static void melde(HWND o, WORD id, LPCSTR a, WORD art) { fmt(txt, id, a, 0); MsgBox(o, txt, StrGet(inst, sys ? 2 : 1), art); }

/* ---- Systemdateien ------------------------------------------------------------- */
static BYTE FAR *bei(BYTE FAR *p, DWORD o) { return (BYTE FAR *)MK_FP(FP_SEG(p) + (WORD)(o >> 4), FP_OFF(p) + (WORD)(o & 15)); }
static WORD lies(LPCSTR n, DATEI *d)              /* ganze Datei in einen Block; 0 = ok, 1 fehlt, 2 Speicher */
{
    short f; DWORD gr, o; BYTE FAR *p; WORD k;
    d->h = 0; d->n = 0;
    if ((f = FileOpen(n, FO_READ)) < 0) return 1;
    gr = (DWORD)FileSeek(f, 0, 2); FileSeek(f, 0, 0);
    if (!gr || !(d->h = MemAlloc(gr, MEM_MOVABLE))) { FileClose(f); return 2; }
    p = (BYTE FAR *)MemLock(d->h);
    for (o = 0; o < gr; o += k) { k = gr - o > 16384UL ? 16384U : (WORD)(gr - o); if (FileRead(f, bei(p, o), k) != k) break; }
    MemUnlock(d->h); FileClose(f);
    if (o < gr) { MemFree(d->h); d->h = 0; return 1; }
    d->n = gr;
    return 0;
}
static WORD schreib(LPCSTR n, DATEI *d)           /* 0 = ok */
{
    short f; DWORD o; BYTE FAR *p; WORD k, e = 0;
    if (!d->h) return 0;
    scpy(pfad, lwbuch, sizeof pfad); scat(pfad, ":\\", sizeof pfad); scat(pfad, n, sizeof pfad);
    if ((f = FileCreate(pfad, 0)) < 0) return 1;
    p = (BYTE FAR *)MemLock(d->h);
    for (o = 0; o < d->n; o += k) { k = d->n - o > 16384UL ? 16384U : (WORD)(d->n - o); if (FileWrite(f, bei(p, o), k) != k) { e = 1; break; } }
    MemUnlock(d->h);
    if (FileClose(f)) e = 1;
    return e;
}
static void frei(void) { if (kern.h) MemFree(kern.h); if (cmd.h) MemFree(cmd.h); kern.h = cmd.h = 0; }
static WORD sysvorbereiten(HWND o)                /* BOOT12.BIN, KERNEL.SYS, COMMAND.COM lesen; 0 = ok */
{
    short f; WORD e;
    scpy(pfad, si.home, sizeof pfad); if (slen(pfad) && pfad[slen(pfad) - 1] != '\\') scat(pfad, "\\", sizeof pfad);
    scat(pfad, "BOOT12.BIN", sizeof pfad);
    hatboot = 0;
    if ((f = FileOpen(pfad, FO_READ)) >= 0) { hatboot = FileRead(f, boot, 512) == 512 && boot[510] == 0x55 && boot[511] == 0xAA; FileClose(f); }
    if (!hatboot) { melde(o, 13, "BOOT12.BIN", MB_OK | MB_ERROR); return 1; }
    pfad[0] = (char)('A' + si.bootlw - 1); pfad[1] = ':'; pfad[2] = '\\'; pfad[3] = 0; scat(pfad, "KERNEL.SYS", sizeof pfad);
    if ((e = lies(pfad, &kern)) != 0) { if (e == 2) melde(o, 20, 0, MB_OK | MB_ERROR); else melde(o, 13, pfad, MB_OK | MB_ERROR); return 1; }
    pfad[3] = 0; scat(pfad, "COMMAND.COM", sizeof pfad);
    if (lies(pfad, &cmd) == 2) { frei(); melde(o, 20, 0, MB_OK | MB_ERROR); return 1; }
    return 0;
}
static void bootcode(BYTE *b)                     /* FreeDOS-Bootcode, BPB (3..3Dh) bleibt */
{
    WORD i;
    for (i = 0; i < 3; i++) b[i] = boot[i];
    for (i = 0x3E; i < 512; i++) b[i] = boot[i];
}
static WORD dateien(void)                         /* nach dem Bootsektor: Dateien kopieren; 0 = ok */
{
    if (schreib("KERNEL.SYS", &kern)) return 1;
    if (schreib("COMMAND.COM", &cmd)) return 1;
    return 0;
}

/* ---- Formatieren ----------------------------------------------------------------- */
static WORD st2err(WORD s) { return s == 3 ? 8 : s == 0x80 ? 9 : 11; }
static WORD wsek(WORD l, BYTE *b) { WORD s = dSek(lw, 1, l, (void FAR *)b); if (s) fehler = st2err(s); return s; }
static void fat_set(WORD c, WORD v)
{
    WORD o = c + c / 2;
    if (c & 1) { fat[o] = (BYTE)((fat[o] & 0x0F) | (v << 4)); fat[o + 1] = (BYTE)(v >> 4); }
    else { fat[o] = (BYTE)v; fat[o + 1] = (BYTE)((fat[o + 1] & 0xF0) | ((v >> 8) & 0x0F)); }
}
static WORD istdefekt(WORD l) { WORD k = l / g->spt; return defekt[k >> 3] & (1 << (k & 7)); }
static WORD abschluss(void)                       /* Bootsektor, FATs, Verzeichnis; 0 = ok */
{
    WORD i, k, c, l, n; RDTIME t; DWORD ser; const char *nm = name[0] ? name : "NO NAME";
    for (i = 0; i < 512; i++) sek[i] = 0;
    sek[0] = 0xEB; sek[1] = 0x3C; sek[2] = 0x90;
    { static const char oem[] = "RD2 2.0 "; for (i = 0; i < 8; i++) sek[3 + i] = oem[i]; }
    sek[12] = 2; sek[13] = g->spc; sek[14] = 1; sek[16] = 2;
    sek[17] = (BYTE)(g->root * 16); sek[18] = (BYTE)((g->root * 16) >> 8);
    sek[19] = (BYTE)g->sektoren; sek[20] = (BYTE)(g->sektoren >> 8); sek[21] = g->media;
    sek[22] = g->fsek; sek[24] = (BYTE)g->spt; sek[26] = (BYTE)g->koepfe;
    sek[38] = 0x29;
    TimeGet(&t); ser = ((DWORD)(t.sek + t.hsek * 256U + t.monat) << 16 | (t.std * 256U + t.min + t.tag)) ^ TickCount() ^ 0x1A2B3C4DUL;
    sek[39] = (BYTE)ser; sek[40] = (BYTE)(ser >> 8); sek[41] = (BYTE)(ser >> 16); sek[42] = (BYTE)(ser >> 24);
    for (i = 0; i < 11; i++) sek[43 + i] = ' ';
    for (i = 0; i < 11 && nm[i]; i++) sek[43 + i] = nm[i];
    { static const char ft[] = "FAT12   "; for (i = 0; i < 8; i++) sek[54 + i] = ft[i]; }
    { static const char bm[] = "\xFA\x33\xC0\x8E\xD0\xBC\x00\x7C\xFB\x8E\xD8\xBB\x07\x00\xBE\x60\x7C\xAC\x0A\xC0\x74\x06\xB4\x0E\xCD\x10\xEB\xF5\x32\xE4\xCD\x16\xCD\x19"
                              "\r\nKeine Systemdiskette / Non-system disk\r\n";
      for (i = 0; i < sizeof bm; i++) sek[62 + i] = (BYTE)bm[i]; }   /* Nachricht ab 7C60h, Taste, INT 19h */
    if (sys) bootcode(sek);
    sek[510] = 0x55; sek[511] = 0xAA;
    if (wsek(0, sek)) return 1;
    for (i = 0; i < sizeof fat; i++) fat[i] = 0;
    fat[0] = g->media; fat[1] = fat[2] = 0xFF;
    kbdef = 0;
    for (c = 0; c < ncl; c++) {
        l = daten + c * g->spc;
        if (istdefekt(l) || (g->spc > 1 && istdefekt(l + 1))) { fat_set(c + 2, 0xFF7); kbdef += g->spc; }
    }
    kbdef /= 2;
    kbfrei = (WORD)((DWORD)ncl * g->spc / 2) - kbdef;
    for (k = 0; k < 2; k++)
        for (i = 0; i < g->fsek; i++) if (wsek(1 + k * g->fsek + i, fat + i * 512)) return 1;
    for (i = 0; i < g->root; i++) {
        for (n = 0; n < 512; n++) sek[n] = 0;
        if (!i && name[0]) { for (n = 0; n < 11; n++) sek[n] = ' '; for (n = 0; n < 11 && name[n]; n++) sek[n] = name[n]; sek[11] = FA_LABEL; }
        if (wsek(1 + 2 * g->fsek + i, sek)) return 1;
    }
    return 0;
}
static WORD vorbereiten(void)                     /* Medium einstellen; Schnell: Format pruefen; 0 = ok */
{
    WORD s;
    dStart(lw, g);
    for (s = 0; s < sizeof defekt; s++) defekt[s] = 0;
    nspur = g->zyl * g->koepfe; spur = 0;
    daten = 1 + 2 * g->fsek + g->root;
    ncl = (g->sektoren - daten) / g->spc;
    fehler = 0;
    if (schnell) {
        if ((s = dSek(lw, 0, 0, (void FAR *)sek)) != 0) fehler = st2err(s);
        else if ((sek[19] | sek[20] << 8) != g->sektoren || sek[21] != g->media) fehler = 12;
    }
    return fehler;
}
static short schritt(void)                        /* 1 weiter, 0 fertig, -1 Fehler */
{
    WORD s, c, h;
    if (!schnell && spur < nspur) {
        c = spur / g->koepfe; h = spur % g->koepfe;
        s = dSpur(lw, c, h, 1);
        if (s == 3 || s == 0x80) { fehler = st2err(s); return -1; }
        if (s) {
            if (spur * g->spt < daten) { fehler = 10; return -1; }
            defekt[spur >> 3] |= (BYTE)(1 << (spur & 7));
        }
        spur++;
        return 1;
    }
    return abschluss() ? -1 : 0;
}

static BOOL RDCALLBACK pproc(HWND d, WORD msg, WORD p1, DWORD p2)   /* Fortschritt */
{
    short r; char a[8], b[8];
    (void)p2;
    if (msg == M_INITDIALOG) {
        SendMessage(DlgItem(d, ID_BALKEN), C_SETRANGE, 0, schnell ? 1 : nspur);
        PostMessage(d, M_COMMAND, ID_SCHRITT, 0);
        return TRUE;
    }
    if (msg == M_COMMAND && p1 == ID_SCHRITT) {
        r = schritt();
        zahl(a, spur); zahl(b, schnell ? 0 : nspur); fmt(txt, 6, a, b); DlgSetText(d, ID_STAND, txt);
        SendMessage(DlgItem(d, ID_BALKEN), C_SETPOS, 0, schnell ? 1 : spur);
        if (r > 0) PostMessage(d, M_COMMAND, ID_SCHRITT, 0);
        else DialogEnd(d, r == 0 ? ID_OK : ID_NO);
        return TRUE;
    }
    if (msg == M_COMMAND && p1 == ID_CANCEL) { DbgPrint("RDFORMAT: abgebrochen"); DialogEnd(d, ID_CANCEL); return TRUE; }
    return FALSE;
}

static WORD nopt; static BYTE opt[3];
static BOOL RDCALLBACK fproc(HWND d, WORD msg, WORD p1, DWORD p2)    /* Einstellungen */
{
    WORD i, k; DRIVEINFO di; char b[12];
    (void)p2;
    if (msg == M_INITDIALOG) {
        fmt(txt, 3, lwbuch, 0); DlgSetText(d, ID_LW, txt);
        k = 0;
        if (!DriveInfo(doslw, &di)) for (i = 0; i < nopt; i++) if (tab[opt[i]].kb == di.format_kb) k = i;
        for (i = 0; i < 3; i++) {
            HWND c = DlgItem(d, ID_K1 + i);
            if (i >= nopt) { ShowWindow(c, SW_HIDE); continue; }
            if (tab[opt[i]].kb >= 1000) { b[0] = (char)('0' + tab[opt[i]].kb / 1000); b[1] = ','; b[2] = (char)('0' + tab[opt[i]].kb / 100 % 10);
                b[3] = (char)('0' + tab[opt[i]].kb / 10 % 10); b[4] = 0; if (b[3] == '0') b[3] = 0; scat(b, " MB", sizeof b); }
            else { zahl(b, tab[opt[i]].kb); scat(b, " KB", sizeof b); }
            DlgSetText(d, ID_K1 + i, b);
            DlgCheck(d, ID_K1 + i, i == k);
        }
        if (!hatboot) EnableWindow(DlgItem(d, ID_SYS), FALSE);
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        for (i = 0; i < nopt; i++) if (DlgChecked(d, ID_K1 + i)) g = &tab[opt[i]];
        DlgGetText(d, ID_NAME, name, sizeof name);
        for (i = 0; name[i]; i++) if (name[i] >= 'a' && name[i] <= 'z') name[i] -= 32;
        schnell = DlgChecked(d, ID_SCHNELL); sys = DlgChecked(d, ID_SYS);
        return FALSE;                             /* Dialog endet mit ID_OK */
    }
    return FALSE;
}

static WORD sysmachen(HWND o)                     /* /SYS: vorhandene Diskette */
{
    WORD s, i; DRIVEINFO di;
    if (DriveInfo(doslw, &di)) { melde(o, 9, lwbuch, MB_OK | MB_ERROR); return 1; }
    g = &tab[0];
    for (i = 0; i < sizeof tab / sizeof tab[0]; i++) if (tab[i].kb == di.format_kb) g = &tab[i];
    if (sysvorbereiten(o)) return 1;
    if (di.frei < kern.n + cmd.n + 2048) { frei(); melde(o, 11, 0, MB_OK | MB_ERROR); return 1; }
    dStart(lw, g); fehler = 0;
    if ((s = dSek(lw, 0, 0, (void FAR *)sek)) != 0) fehler = st2err(s);
    else if (sek[510] != 0x55 || sek[511] != 0xAA || (sek[11] | sek[12] << 8) != 512) fehler = 12;
    else { bootcode(sek); wsek(0, sek); }
    dEnde(lw, doslw);
    if (!fehler && dateien()) fehler = 11;
    frei();
    if (fehler) { melde(o, fehler, lwbuch, MB_OK | MB_ERROR); return 1; }
    DbgPrint("RDFORMAT: bootfaehig");
    fmt(txt, 17, 0, 0); if (!cmd.n) { LPCSTR h = StrGet(inst, 19); if (h) scat(txt, h, sizeof txt); }
    MsgBox(o, txt, StrGet(inst, 2), MB_OK | MB_INFO);
    return 0;
}

short RDAPI RdMain(HINST i, LPCSTR cl, WORD show)
{
    WORD k, t; short e; DRIVEINFO di; char a[8], b[8];
    (void)show;
    inst = i;
    while (*cl == ' ') cl++;
    if (!*cl) { DbgPrint("RDFORMAT: kein Laufwerk"); return 1; }
    lwbuch[0] = (char)(cl[0] >= 'a' ? cl[0] - 32 : cl[0]); lwbuch[1] = 0;
    for (k = 0; cl[k]; k++) if (cl[k] == '/' && (cl[k + 1] == 'S' || cl[k + 1] == 's')) sys = 1;
    doslw = (WORD)(lwbuch[0] - 'A' + 1);
    KernInfo(&si);
    if (DriveInfo(doslw | DI_NURTYP, &di) || di.typ != DT_FLOPPY) { melde(0, 14, 0, MB_OK | MB_ERROR); return 1; }
    if (di.flags & DI_PHANTOM) { doslw = 1; lwbuch[0] = 'A'; }
    lw = doslw - 1;
    if (!(hm = ModLoad("RDDISK.MOD"))) { melde(0, 15, 0, MB_OK | MB_ERROR); return 1; }
    dTyp = (RDD_TYP_F)ModProc(hm, RDD_TYP); dStart = (RDD_START_F)ModProc(hm, RDD_START); dSpur = (RDD_SPUR_F)ModProc(hm, RDD_SPUR);
    dSek = (RDD_SEKTOR_F)ModProc(hm, RDD_SEKTOR); dEnde = (RDD_ENDE_F)ModProc(hm, RDD_ENDE);
    DbgPrint(sys ? "RDFORMAT: SYS" : "RDFORMAT: bereit");
    if (sys) {
        fmt(txt, 16, lwbuch, 0);
        e = MsgBox(0, txt, StrGet(inst, 2), MB_YESNO | MB_ASK) == ID_YES ? (short)sysmachen(0) : 1;
        ModFree(hm);
        return e;
    }
    t = dTyp(lw);
    if (!t) { DriveInfo(doslw, &di); t = di.format_kb > 360 ? 3 : 1; }   /* XT: BIOS kennt den Typ nicht */
    for (nopt = 0; nopt < 3 && wahl[t][nopt] != 0xFF; nopt++) opt[nopt] = wahl[t][nopt];
    scpy(pfad, si.home, sizeof pfad); if (slen(pfad) && pfad[slen(pfad) - 1] != '\\') scat(pfad, "\\", sizeof pfad);
    scat(pfad, "BOOT12.BIN", sizeof pfad);
    { short f = FileOpen(pfad, FO_READ); hatboot = f >= 0; if (f >= 0) FileClose(f); }
    g = &tab[opt[0]];
    if (DialogRun(inst, 10, 0, fproc, 0) != ID_OK) { ModFree(hm); return 1; }
    if (sys && sysvorbereiten(0)) { ModFree(hm); return 1; }
    fmt(txt, 4, lwbuch, 0);
    if (MsgBox(0, txt, StrGet(inst, 1), MB_OKCANCEL | MB_WARN) != ID_OK) { frei(); ModFree(hm); return 1; }
    DbgPrint("RDFORMAT: Start");
    CursorBusy(TRUE);
    if (vorbereiten()) e = ID_NO;
    else { CursorBusy(FALSE); e = DialogRun(inst, 11, 0, pproc, 0); CursorBusy(TRUE); }
    dEnde(lw, doslw);
    if (e == ID_OK && sys && dateien()) { fehler = 11; e = ID_NO; }
    frei();
    CursorBusy(FALSE);
    if (e == ID_CANCEL) { melde(0, 18, 0, MB_OK | MB_WARN); ModFree(hm); return 1; }
    if (e != ID_OK) { DbgPrint("RDFORMAT: Fehler"); melde(0, fehler ? fehler : 11, lwbuch, MB_OK | MB_ERROR); ModFree(hm); return 1; }
    zahl(a, kbfrei); zahl(b, kbdef); fmt(txt, 7, a, b);
    if (sys) { LPCSTR h = StrGet(inst, 21); if (h) scat(txt, h, sizeof txt); if (!cmd.n) { h = StrGet(inst, 19); if (h) scat(txt, h, sizeof txt); } }
    DbgPrint("RDFORMAT: fertig"); DbgPrint(a);
    MsgBox(0, txt, StrGet(inst, 1), MB_OK | MB_INFO);
    ModFree(hm);
    return 0;
}
