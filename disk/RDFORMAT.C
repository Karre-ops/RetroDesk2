/* RDFORMAT.C - RD2-Oberflaeche fuer FreeDOS FORMAT (GPL v2, siehe disk/fdformat/COPYING).
 * Aufruf: RDFORMAT.EXE A:        Dialog (Kapazitaet, Bezeichnung, Schnell), dann FreeDOS FORMAT
 *         RDFORMAT.EXE A: /SYS   Bootfaehig machen (FreeDOS SYS 3.6f, disk/fdsys)
 * SYS laeuft als sys_main(argc, argv): Quelle ist das Startlaufwerk von RD2 (KERNEL.SYS, COMMAND.COM
 * nur wenn vorhanden, sonst /UPDATE), Dateien ueber den RD2-Kern (disk/rdsysw.h).
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
#define ID_SYS     22
#define ID_K1      31                             /* 31..33 Kapazitaet */
#define ID_STAND   40
#define ID_BALKEN  41
#define ID_SCHRITT 90
#define ID_SYSTXT  42

void fd_main(int argc, char *argv[]);             /* FreeDOS FORMAT main.c */
int  sys_main(int argc, char **argv);             /* FreeDOS SYS sys.c */
void Exit(int mserror, int fderror);              /* FreeDOS FORMAT driveio.c */
extern struct { unsigned long gesamt, defekt, frei; } drive_statistics;   /* erste Felder von DS (format.h) */

static const WORD kbtab[] = { 360, 720, 1200, 1440, 2880, 180 };
static const BYTE wahl[7][3] = {                  /* BIOS-Typ -> angebotene Formate (Index in kbtab, 0xFF = keins) */
    { 0, 5, 0xFF }, { 0, 5, 0xFF }, { 2, 0, 0xFF }, { 1, 0xFF, 0xFF }, { 3, 1, 0xFF }, { 4, 3, 1 }, { 4, 3, 1 } };

static HINST inst; static HMOD hm; static RDD_INT_F dInt;
static WORD doslw, sys, schnell, kb, nopt, abbruch, kr_quelle, kr_code, sysok, mitsys, shell;
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
static short frage(WORD id, LPCSTR a, WORD art) { char m[24]; scpy(m, "RDFORMAT: Meldung ", sizeof m); zahl(m + 18, id); DbgPrint(m); fmt(txt, id, a, 0); return MsgBox(owner(), txt, StrGet(inst, sys ? 2 : 1), art); }

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
int rd_write(int fd, const void *b, unsigned n) { const char *p = (const char *)b; unsigned i; if (fd > 2) return (int)FileWrite((short)fd, b, n); for (i = 0; i < n; i++) rd_putch(p[i]); return (int)n; }   /* > 2: Datei (SYS) */
int rd_isatty(int fd) { (void)fd; return 0; }
int rd_system(const char *cmd) { (void)cmd; return -1; }   /* FORMAT /S wird nicht benutzt, SYS ruft RDFORMAT selbst */
void rd_einlegen(void) { if (frage(4, lwbuch, MB_OKCANCEL | MB_WARN) != ID_OK) { abbruch = 1; Exit(3, 3); } }
void rd_bestaetigen(int format) { (void)format; if (frage(22, lwbuch, MB_YESNO | MB_WARN) != ID_YES) { abbruch = 1; Exit(3, 3); } }
int rd_label(char *str) { scpy(str, name, 12); return (int)slen(str); }
void rd_kritisch(int quelle, unsigned int code) { kr_quelle = (WORD)quelle; kr_code = code | 0x8000; }   /* 0x8000: Fehler gemeldet (Code 0 ist gueltig) */
/* ---- Anbindung von FreeDOS SYS (rdsysw.h): Dateien ueber den RD2-Kern ---------------- */
int rd_fopen(const char *n, int fl, ...) { return (fl & 0x0020) ? FileCreate(n, 0) : FileOpen(n, (WORD)(fl & 3)); }   /* 0x0020 = O_CREAT (Watcom fcntl.h) */
int rd_fread(int f, void *b, unsigned n) { return (int)FileRead((short)f, b, n); }
int rd_fclose(int f) { return FileClose((short)f) ? -1 : 0; }
long rd_fseek(int f, long pos, int art) { return FileSeek((short)f, pos, (WORD)art); }
long rd_flen(int f) { long a = FileSeek((short)f, 0, 1), n = FileSeek((short)f, 0, 2); FileSeek((short)f, a, 0); return n; }
int rd_fdel(const char *n) { return FileDelete(n) ? -1 : 0; }
int rd_ffirst(const char *n, unsigned long *g) { static FINDDATA fd; if (FileFindFirst(n, 0x07, &fd)) return 1; *g = fd.groesse; return 0; }
static union REGS sr_r; static struct SREGS sr_s;
static WORD dos21(void) { segread(&sr_s); dInt(0x21, (union REGS FAR *)&sr_r, (union REGS FAR *)&sr_r, (struct SREGS FAR *)&sr_s); return !sr_r.x.cflag; }   /* dInt liefert AX */
unsigned rd_getftime(int f, unsigned *d, unsigned *z) { sr_r.x.ax = 0x5700; sr_r.x.bx = (WORD)f; if (!dos21()) return 1; *d = sr_r.x.dx; *z = sr_r.x.cx; return 0; }
unsigned rd_setftime(int f, unsigned d, unsigned z) { sr_r.x.ax = 0x5701; sr_r.x.bx = (WORD)f; sr_r.x.cx = z; sr_r.x.dx = d; return dos21() ? 0 : 1; }
unsigned rd_diskfree(unsigned lw, struct _diskfree_t *df)
{
    sr_r.x.ax = 0x3600; sr_r.x.dx = lw; dos21();
    if (sr_r.x.ax == 0xFFFF) return 1;
    df->avail_clusters = sr_r.x.bx; df->sectors_per_cluster = sr_r.x.ax; df->bytes_per_sector = sr_r.x.cx; df->total_clusters = sr_r.x.dx;
    return 0;
}
static HMEM sysmem;                               /* SYS puffert jeweils eine Datei ganz */
unsigned rd_dosalloc(unsigned paras, unsigned *seg)
{
    void FAR *p;
    if (sysmem || !(sysmem = MemAlloc((DWORD)paras * 16 + 16, MEM_FIXED))) return 8;
    if (!(p = MemLock(sysmem))) { MemFree(sysmem); sysmem = 0; return 8; }
    *seg = FP_SEG(p) + ((FP_OFF(p) + 15) >> 4);
    return 0;
}
unsigned rd_dosfree(unsigned seg) { (void)seg; if (sysmem) { MemFree(sysmem); sysmem = 0; } return 0; }

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
static int sy_argc; static char *sy_argv[6]; static char quelle[4] = "A:\\", qdatei[16];
static WORD imsys;                               /* 1: Schritt-Dialog fuehrt SYS aus */
static BOOL RDCALLBACK pproc(HWND d, WORD msg, WORD p1, DWORD p2)   /* Fortschritt; FORMAT laeuft hier */
{
    (void)p2;
    if (msg == M_INITDIALOG) { pdlg = d; PostMessage(d, M_COMMAND, ID_SCHRITT, 0); return FALSE; }   /* FALSE: Fokus auf Abbruch */
    if (msg == M_COMMAND && p1 == ID_SCHRITT) {
        fd_code = 0;
        if (!setjmp(jb)) { if (imsys) fd_code = sys_main(sy_argc, sy_argv); else fd_main(fd_argc, fd_argv); }
        if (sysmem) rd_dosfree(0);
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
        EnableWindow(DlgItem(d, ID_SYS), sysok);  /* nur mit KERNEL.SYS auf einer anderen Diskette */
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        for (i = 0; i < nopt; i++) if (DlgChecked(d, ID_K1 + i)) kb = kbtab[opt[i]];
        DlgGetText(d, ID_NAME, name, sizeof name);
        for (i = 0; name[i]; i++) if (name[i] >= 'a' && name[i] <= 'z') name[i] -= 32;
        schnell = DlgChecked(d, ID_SCHNELL);
        mitsys = sysok && DlgChecked(d, ID_SYS);
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

/* ---- Bootfaehig machen (FreeDOS SYS) ----------------------------------------------- */
static WORD sysok_grund;
static void fehler(WORD tid)                      /* FORMAT/SYS endete mit Fehler: Klartext oder letzte Zeile */
{
    WORD k = kr_code & 0xFF; char a[4];
    if (kr_code && (kr_quelle == 1 || kr_quelle == 2)) {   /* FreeDOS: 1 = BIOS (INT 13h-Status), 2 = DOS (INT 24h-Code) */
        if (kr_quelle == 1 ? k == 3 : k == 0) frage(8, 0, MB_OK | MB_ERROR);
        else if (kr_quelle == 1 ? k == 0x80 : k == 2) frage(9, lwbuch, MB_OK | MB_ERROR);
        else { hex2(a, k); frage(25, a, MB_OK | MB_ERROR); }
    } else { fmt(txt, tid, letzte, 0); MsgBox(0, txt, StrGet(inst, sys ? 2 : 1), MB_OK | MB_ERROR); }
}
static void pruefsys(void)                        /* Quelle = Startlaufwerk von RD2, nicht das Ziel */
{
    static SYSINFO si; unsigned long n;
    KernInfo(&si);
    quelle[0] = si.home[0] >= 'a' ? (char)(si.home[0] - 32) : si.home[0];
    scpy(qdatei, quelle, sizeof qdatei); scat(qdatei, "KERNEL.SYS", sizeof qdatei);
    if (quelle[0] == lwbuch[0]) sysok_grund = 27;
    else if (rd_ffirst(qdatei, &n)) sysok_grund = 26;
    else sysok_grund = 0;
    sysok = sysok_grund == 0;
    scpy(qdatei, quelle, sizeof qdatei); scat(qdatei, "COMMAND.COM", sizeof qdatei);
    shell = sysok && !rd_ffirst(qdatei, &n);      /* ohne COMMAND.COM nur Kern und Startsektor (/UPDATE) */
    DbgPrint(sysok ? "RDFORMAT: SYS moeglich" : "RDFORMAT: SYS nicht moeglich");
}
static WORD sysrun(void)                          /* 0 = ok */
{
    static char ziel[3]; short e; WORD k;
    ziel[0] = lwbuch[0]; ziel[1] = ':'; ziel[2] = 0;
    sy_argc = 0; sy_argv[sy_argc++] = "SYS"; sy_argv[sy_argc++] = quelle; sy_argv[sy_argc++] = ziel;
    if (!shell) sy_argv[sy_argc++] = "/UPDATE";
    sy_argv[sy_argc] = 0;
    for (k = 0; k < (WORD)sy_argc; k++) DbgPrint(sy_argv[k]);
    imsys = 1; kr_code = 0; letzte[0] = 0;
    e = DialogRun(inst, 12, 0, pproc, 0);
    imsys = 0;
    if (e == ID_OK) { DbgPrint("RDFORMAT: SYS fertig"); return 0; }
    DbgPrint("RDFORMAT: SYS Fehler"); fehler(29);
    return 1;
}

short RDAPI RdMain(HINST i, LPCSTR cl, WORD show)
{
    WORD k, t; short e; DRIVEINFO di; char a[12], b[12];
    (void)show;
    inst = i;
    while (*cl == ' ') cl++;
    if (!*cl) { DbgPrint("RDFORMAT: kein Laufwerk"); return 1; }
    lwbuch[0] = (char)(cl[0] >= 'a' ? cl[0] - 32 : cl[0]); lwbuch[1] = 0;
    for (k = 0; cl[k]; k++) if (cl[k] == '/' && (cl[k + 1] == 'S' || cl[k + 1] == 's')) sys = 1;
    doslw = (WORD)(lwbuch[0] - 'A' + 1);
    if (DriveInfo(doslw | DI_NURTYP, &di) || di.typ != DT_FLOPPY) { frage(14, 0, MB_OK | MB_ERROR); return 1; }
    if (di.flags & DI_PHANTOM) { doslw = 1; lwbuch[0] = 'A'; }
    if (!(hm = ModLoad("RDDISK.MOD"))) { frage(15, 0, MB_OK | MB_ERROR); return 1; }
    dInt = (RDD_INT_F)ModProc(hm, RDD_INT);
    pruefsys();
    if (sys) {
        if (sysok) {
            if (frage(24, lwbuch, MB_YESNO | MB_WARN) == ID_YES && !sysrun()) frage(28, lwbuch, MB_OK | MB_INFO);
        } else frage(sysok_grund, sysok_grund == 26 ? quelle : lwbuch, MB_OK | MB_ERROR);
        ModFree(hm); return 1;
    }
    t = biostyp(doslw - 1);
    if (!t) { DriveInfo(doslw, &di); t = di.format_kb > 360 ? 3 : 1; }
    for (nopt = 0; nopt < 3 && wahl[t][nopt] != 0xFF; nopt++) opt[nopt] = wahl[t][nopt];
    kb = kbtab[opt[0]];
    DbgPrint("RDFORMAT: bereit");
    if (DialogRun(inst, 10, 0, fproc, 0) != ID_OK) { ModFree(hm); return 1; }
    /* Befehlszeile fuer FreeDOS FORMAT */
    a_lw[0] = lwbuch[0]; a_lw[1] = ':'; a_lw[2] = 0;
    scpy(a_f, "/F:", sizeof a_f); zahl(a, kb); scat(a_f, a, sizeof a_f);
    fd_argc = 0; fd_argv[fd_argc++] = "FORMAT"; fd_argv[fd_argc++] = a_lw; fd_argv[fd_argc++] = a_f;
    if (name[0]) { scpy(a_v, "/V:", sizeof a_v); scat(a_v, name, sizeof a_v); fd_argv[fd_argc++] = a_v; }
    fd_argv[fd_argc++] = schnell ? "/Q" : "/U";
    fd_argv[fd_argc] = 0;
    for (k = 0; k < (WORD)fd_argc; k++) DbgPrint(fd_argv[k]);
    e = DialogRun(inst, 11, 0, pproc, 0);
    if (e == ID_CANCEL) { ModFree(hm); frage(18, 0, MB_OK | MB_WARN); return 1; }
    if (e != ID_OK) { ModFree(hm); DbgPrint("RDFORMAT: Fehler"); fehler(23); return 1; }
    if (mitsys && sysrun()) { ModFree(hm); return 1; }
    ModFree(hm);
    if (DriveInfo(doslw, &di)) di.frei = 0;
    zahl(a, di.frei >> 10); zahl(b, drive_statistics.defekt / 2); fmt(txt, mitsys ? 30 : 7, a, b);
    DbgPrint("RDFORMAT: fertig"); DbgPrint(a);
    MsgBox(0, txt, StrGet(inst, 1), MB_OK | MB_INFO);
    return 0;
}
