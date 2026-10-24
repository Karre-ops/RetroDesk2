/* RDFORMAT.C - RD2-Oberflaeche fuer FreeDOS FORMAT (GPL v2, siehe disk/fdformat/COPYING).
 * Aufruf: RDFORMAT.EXE A:        Dialog (Kapazitaet, Bezeichnung, Schnell), dann FreeDOS FORMAT
 *         RDFORMAT.EXE C:        Festplatte (FAT12/FAT16): Dialog (Bezeichnung, Schnell), Rueckfrage, FORMAT
 * Das Laufwerk, auf dem RD2 liegt, wird nicht formatiert.
 * Der Formatier-Kern ist FreeDOS FORMAT 0.92a fast unveraendert (disk/fdformat). Er laeuft als
 * fd_main(argc, argv) mit Schaltern wie auf der DOS-Befehlszeile (/F:360 /V:NAME /Q bzw. /U).
 * disk/fdshim.h leitet seine DOS-/BIOS-Aufrufe ueber RDDISK.MOD, seine Textausgabe ins
 * Debug-Protokoll und exit() per longjmp hierher zurueck. Rueckfragen, Fortschritt und Fehler
 * kommen ueber die Funktionen unten (rdfmtui.h) in RD2-Dialoge. */
#include <dos.h>
#include <setjmp.h>
#include "RD2.H"
#include "RDDISK.H"
#include "rdfmtui.h"

#define ID_LW      19
#define ID_NAME    20
#define ID_SCHNELL 21
#define ID_GROESSE 23
#define ID_K1      31                             /* 31..33 Kapazitaet */
#define ID_STAND   40
#define ID_BALKEN  41
#define ID_SCHRITT 90

void fd_main(int argc, char *argv[]);             /* FreeDOS FORMAT main.c */
void Exit(int mserror, int fderror);              /* FreeDOS FORMAT driveio.c */
extern struct { unsigned long gesamt, defekt, frei; } drive_statistics;   /* erste Felder von DS (format.h) */

static const WORD kbtab[] = { 360, 720, 1200, 1440, 2880, 180 };
static const BYTE wahl[7][3] = {                  /* BIOS-Typ -> angebotene Formate (Index in kbtab, 0xFF = keins) */
    { 0, 5, 0xFF }, { 0, 5, 0xFF }, { 2, 0, 0xFF }, { 1, 0xFF, 0xFF }, { 3, 1, 0xFF }, { 4, 3, 1 }, { 4, 3, 1 } };

static HINST inst; static HMOD hm; static RDD_INT_F dInt;
static WORD doslw, hdd, schnell, kb, nopt, abbruch, kr_quelle, kr_code;
static BYTE opt[3];
static char lwbuch[2], name[12], txt[200], zeile[82], letzte[82];
static WORD zn;
static HWND pdlg;
static jmp_buf jb; static int fd_code;

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(LPSTR d, LPCSTR s, WORD max) { WORD i; for (i = 0; i + 1 < max && s[i]; i++) d[i] = s[i]; d[i] = 0; }
static void scat(LPSTR d, LPCSTR s, WORD max) { WORD n = slen(d); scpy(d + n, s, max - n); }
static void zahl(LPSTR b, DWORD v) { char t[12]; WORD i = 0, j = 0; do t[i++] = (char)('0' + v % 10); while ((v /= 10) != 0); while (i) b[j++] = t[--i]; b[j] = 0; }
static void hex2(LPSTR b, WORD v) { static const char h[] = "0123456789ABCDEF"; b[0] = h[(v >> 4) & 15]; b[1] = h[v & 15]; b[2] = 'h'; b[3] = 0; }
static void fmt(LPSTR d, WORD id, LPCSTR a, LPCSTR b)   /* Text id, %s der Reihe nach durch a und b */
{
    LPCSTR s = StrGet(inst, id), e; WORD n = 0, k = 0;
    if (!s) s = "?";
    while (*s && n < sizeof txt - 1) {
        if (s[0] == '%' && s[1] == 's') { e = k++ ? b : a; while (e && *e && n < sizeof txt - 1) d[n++] = *e++; s += 2; }
        else d[n++] = *s++;
    }
    d[n] = 0;
}
static HWND owner(void) { return pdlg; }
static short frage(WORD id, LPCSTR a, WORD art) { char m[24]; scpy(m, "RDFORMAT: Meldung ", sizeof m); zahl(m + 18, id); DbgPrint(m); fmt(txt, id, a, 0); return MsgBox(owner(), txt, StrGet(inst, 1), art); }

/* ---- Anbindung von FreeDOS FORMAT (fdshim.h, rdfmtui.h) ----------------------------- */
int rd_int86x(int nr, union REGS *ein, union REGS *aus, struct SREGS *s)
{
    static struct SREGS t;
    if (nr == 0x21 && ein->h.ah == 0x0B) { aus->h.al = (BYTE)(abbruch ? 0xFF : 0); return aus->x.ax; }   /* Tastatur: Esc = Abbruch */
    if (nr == 0x21 && ein->h.ah == 0x07) { aus->h.al = (BYTE)(abbruch ? 27 : 'N'); return aus->x.ax; }   /* "noch eine Diskette?" nein */
    if (!s) { segread(&t); s = &t; }
#ifdef RDF_SPUR
    { char b[12]; static const char h[] = "0123456789ABCDEF"; WORD v = ein->x.ax; b[0]='I'; b[1]=h[nr>>4&15]; b[2]=h[nr&15]; b[3]=' ';
      b[4]=h[v>>12]; b[5]=h[v>>8&15]; b[6]=h[v>>4&15]; b[7]=h[v&15]; b[8]=0; DbgPrint(b); }
#endif
    return dInt((WORD)nr, (union REGS FAR *)ein, (union REGS FAR *)aus, (struct SREGS FAR *)s);
}
void rd_exit(int code) { fd_code = code; longjmp(jb, 1); }
int rd_putch(int c)                               /* Textausgabe von FORMAT: zeilenweise ins Protokoll */
{
    if (c == '\n' || c == '\r' || zn >= sizeof zeile - 1) {
        zeile[zn] = 0;
        if (zn) { DbgPrint(zeile); scpy(letzte, zeile, sizeof letzte); }
        zn = 0;
        if (c == '\n' || c == '\r') return c;
    }
    zeile[zn++] = (char)c;
    return c;
}
int rd_write(int fd, const void *b, unsigned n) { const char *p = (const char *)b; unsigned i; (void)fd; for (i = 0; i < n; i++) rd_putch(p[i]); return (int)n; }
int rd_isatty(int fd) { (void)fd; return 0; }
int rd_system(const char *cmd) { (void)cmd; return -1; }   /* FORMAT /S wird nicht benutzt */
void rd_einlegen(void) { if (frage(4, lwbuch, MB_OKCANCEL | MB_WARN) != ID_OK) { abbruch = 1; Exit(3, 3); } }
void rd_bestaetigen(int format) { (void)format; if (frage(22, lwbuch, MB_YESNO | MB_WARN) != ID_YES) { abbruch = 1; Exit(3, 3); } }
int rd_label(char *str) { scpy(str, name, 12); return (int)slen(str); }
void rd_kritisch(int quelle, unsigned int code) { kr_quelle = (WORD)quelle; kr_code = code | 0x8000; }   /* 0x8000: Fehler gemeldet (Code 0 ist gueltig) */
void rd_prozent(unsigned long p)
{
    MSG m; char a[8]; static unsigned long naechst;
    if (p >= naechst) { zahl(a, p); fmt(txt, 6, a, 0); DbgPrint(txt); naechst = p + 25; }
    if (pdlg) {
        zahl(a, p); fmt(txt, 6, a, 0); DlgSetText(pdlg, ID_STAND, txt);
        SendMessage(DlgItem(pdlg, ID_BALKEN), C_SETPOS, 0, p);
    }
    while (PeekMessage(&m, 1)) {
#ifdef RDF_SPUR
        { char b[40]; b[0]='M'; zahl(b + 1, m.msg); scat(b, " w", 40); zahl(b + slen(b), (WORD)m.w); scat(b, " d", 40); zahl(b + slen(b), (WORD)pdlg); scat(b, " f", 40); zahl(b + slen(b), (WORD)GetFocus()); scat(b, " a", 40); zahl(b + slen(b), (WORD)GetActive()); if (m.msg != M_PAINT) DbgPrint(b); }
#endif
        DispatchMessage(&m);                      /* Zeichnen, Abbruch-Knopf */
    }
    if (abbruch) Exit(3, 3);
}

/* ---- Dialoge ----------------------------------------------------------------------- */
static int fd_argc; static char *fd_argv[8]; static char a_lw[3], a_f[8], a_v[16];
static BOOL RDCALLBACK pproc(HWND d, WORD msg, WORD p1, DWORD p2)   /* Fortschritt; FORMAT laeuft hier */
{
    (void)p2;
    if (msg == M_INITDIALOG) { pdlg = d; PostMessage(d, M_COMMAND, ID_SCHRITT, 0); return FALSE; }   /* FALSE: Fokus auf Abbruch */
    if (msg == M_COMMAND && p1 == ID_SCHRITT) {
        fd_code = 0;
        if (!setjmp(jb)) fd_main(fd_argc, fd_argv);
        rd_putch('\n');
        pdlg = 0;
        DialogEnd(d, abbruch ? ID_CANCEL : fd_code == 0 ? ID_OK : ID_NO);
        return TRUE;
    }
    if (msg == M_COMMAND && p1 == ID_CANCEL) { DbgPrint("RDFORMAT: Abbruch gewaehlt"); abbruch = 1; return TRUE; }   /* endet beim naechsten Fortschritt */
    return FALSE;
}
static void kbtext(LPSTR b, WORD k)
{
    if (k >= 1000) { b[0] = (char)('0' + k / 1000); b[1] = ','; b[2] = (char)('0' + k / 100 % 10); b[3] = (char)('0' + k / 10 % 10); b[4] = 0; if (b[3] == '0') b[3] = 0; scat(b, " MB", 12); }
    else { zahl(b, k); scat(b, " KB", 12); }
}
static BOOL RDCALLBACK fproc(HWND d, WORD msg, WORD p1, DWORD p2)    /* Einstellungen */
{
    WORD i, k = 0; DRIVEINFO di; char b[12];
    (void)p2;
    if (msg == M_INITDIALOG) {
        fmt(txt, 3, lwbuch, 0); DlgSetText(d, ID_LW, txt);
        if (!DriveInfo(doslw, &di)) for (i = 0; i < nopt; i++) if (kbtab[opt[i]] == di.format_kb) k = i;
        for (i = 0; i < 3; i++) {
            if (i >= nopt) { ShowWindow(DlgItem(d, ID_K1 + i), SW_HIDE); continue; }
            kbtext(b, kbtab[opt[i]]); DlgSetText(d, ID_K1 + i, b); DlgCheck(d, ID_K1 + i, i == k);
        }
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        for (i = 0; i < nopt; i++) if (DlgChecked(d, ID_K1 + i)) kb = kbtab[opt[i]];
        DlgGetText(d, ID_NAME, name, sizeof name);
        for (i = 0; name[i]; i++) if (name[i] >= 'a' && name[i] <= 'z') name[i] -= 32;
        schnell = DlgChecked(d, ID_SCHNELL);
    }
    return FALSE;
}

static WORD biostyp(WORD lw)                      /* INT 13h 08h ueber RDDISK; 0 = unbekannt (XT) */
{
    static union REGS r; static struct SREGS s;
    segread(&s); r.h.ah = 8; r.h.dl = (BYTE)lw; r.x.bx = 0;
    dInt(0x13, (union REGS FAR *)&r, (union REGS FAR *)&r, (struct SREGS FAR *)&s);
    return (r.x.cflag || r.h.bl > 6) ? 0 : r.h.bl;
}

static void fehler(WORD tid)                      /* FORMAT endete mit Fehler: Klartext oder letzte Zeile */
{
    WORD k = kr_code & 0xFF; char a[4];
    if (kr_code && (kr_quelle == 1 || kr_quelle == 2)) {   /* FreeDOS: 1 = BIOS (INT 13h-Status), 2 = DOS (INT 24h-Code) */
        if (kr_quelle == 1 ? k == 3 : k == 0) frage(8, 0, MB_OK | MB_ERROR);
        else if (kr_quelle == 1 ? k == 0x80 : k == 2) frage(9, lwbuch, MB_OK | MB_ERROR);
        else { hex2(a, k); frage(25, a, MB_OK | MB_ERROR); }
    } else { fmt(txt, tid, letzte, 0); MsgBox(0, txt, StrGet(inst, 1), MB_OK | MB_ERROR); }
}
static BOOL RDCALLBACK hproc(HWND d, WORD msg, WORD p1, DWORD p2)    /* Einstellungen Festplatte */
{
    DRIVEINFO di; char b[12]; WORD i;
    (void)p2;
    if (msg == M_INITDIALOG) {
        fmt(txt, 3, lwbuch, 0); DlgSetText(d, ID_LW, txt);
        if (DriveInfo(doslw, &di)) scpy(b, "?", sizeof b); else { zahl(b, di.gesamt >> 20); scat(b, " MB", sizeof b); }
        fmt(txt, 32, b, 0); DlgSetText(d, ID_GROESSE, txt);
        DlgCheck(d, ID_SCHNELL, TRUE);
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        DlgGetText(d, ID_NAME, name, sizeof name);
        for (i = 0; name[i]; i++) if (name[i] >= 'a' && name[i] <= 'z') name[i] -= 32;
        schnell = DlgChecked(d, ID_SCHNELL);
    }
    return FALSE;
}

short RDAPI RdMain(HINST i, LPCSTR cl, WORD show)
{
    WORD k, t; short e; DRIVEINFO di; SYSINFO si; char a[12], b[12];
    (void)show;
    inst = i;
    {   /* nur ein RDFORMAT zugleich (sonst zwei Formatierungen auf demselben Laufwerk) */
        TASKINFO ti; HTASK h, ich = TaskSelf(); static const char n[] = "Formatieren";
        for (h = TaskFirst(&ti); h; h = TaskNext(h, &ti)) {
            for (k = 0; n[k] && ti.name[k] == n[k]; k++) ;
            if (h != ich && !n[k] && !ti.name[k]) { frage(34, 0, MB_OK | MB_WARN); return 1; }
        }
    }
    while (*cl == ' ') cl++;
    if (!*cl) { DbgPrint("RDFORMAT: kein Laufwerk"); return 1; }
    lwbuch[0] = (char)(cl[0] >= 'a' ? cl[0] - 32 : cl[0]); lwbuch[1] = 0;
    doslw = (WORD)(lwbuch[0] - 'A' + 1);
    if (DriveInfo(doslw | DI_NURTYP, &di) || (di.typ != DT_FLOPPY && di.typ != DT_HARD)) { frage(14, 0, MB_OK | MB_ERROR); return 1; }
    hdd = di.typ == DT_HARD;
    if (!hdd && (di.flags & DI_PHANTOM)) { doslw = 1; lwbuch[0] = 'A'; }
    KernInfo(&si);
    if ((si.home[0] & 0xDF) == lwbuch[0]) { frage(31, lwbuch, MB_OK | MB_ERROR); return 1; }   /* RD2 laeuft von dort */
    if (!(hm = ModLoad("RDDISK.MOD"))) { frage(15, 0, MB_OK | MB_ERROR); return 1; }
    dInt = (RDD_INT_F)ModProc(hm, RDD_INT);
    DbgPrint(hdd ? "RDFORMAT: Festplatte" : "RDFORMAT: Diskette");
    if (hdd) {
        DbgPrint("RDFORMAT: bereit");
        if (DialogRun(inst, 12, 0, hproc, 0) != ID_OK) { ModFree(hm); return 1; }
    } else {
        t = biostyp(doslw - 1);
        if (!t) { DriveInfo(doslw, &di); t = di.format_kb > 360 ? 3 : 1; }
        for (nopt = 0; nopt < 3 && wahl[t][nopt] != 0xFF; nopt++) opt[nopt] = wahl[t][nopt];
        kb = kbtab[opt[0]];
        DbgPrint("RDFORMAT: bereit");
        if (DialogRun(inst, 10, 0, fproc, 0) != ID_OK) { ModFree(hm); return 1; }
    }
    /* Befehlszeile fuer FreeDOS FORMAT */
    a_lw[0] = lwbuch[0]; a_lw[1] = ':'; a_lw[2] = 0;
    fd_argc = 0; fd_argv[fd_argc++] = "FORMAT"; fd_argv[fd_argc++] = a_lw;
    if (!hdd) { scpy(a_f, "/F:", sizeof a_f); zahl(a, kb); scat(a_f, a, sizeof a_f); fd_argv[fd_argc++] = a_f; }
    if (name[0]) { scpy(a_v, "/V:", sizeof a_v); scat(a_v, name, sizeof a_v); fd_argv[fd_argc++] = a_v; }
    fd_argv[fd_argc++] = schnell ? "/Q" : "/U";
    fd_argv[fd_argc] = 0;
    for (k = 0; k < (WORD)fd_argc; k++) DbgPrint(fd_argv[k]);
    e = DialogRun(inst, 11, 0, pproc, 0);
    ModFree(hm);
    if (e == ID_CANCEL) { frage(hdd ? 33 : 18, 0, MB_OK | MB_WARN); return 1; }
    if (e != ID_OK) { DbgPrint("RDFORMAT: Fehler"); fehler(23); return 1; }
    if (DriveInfo(doslw, &di)) di.frei = 0;
    zahl(a, di.frei >> 10); zahl(b, drive_statistics.defekt / 2); fmt(txt, 7, a, b);
    DbgPrint("RDFORMAT: fertig"); DbgPrint(a);
    MsgBox(0, txt, StrGet(inst, 1), MB_OK | MB_INFO);
    return 0;
}
