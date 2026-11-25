/* RDFORMAT.C - RD2-Oberflaeche fuer FreeDOS FORMAT (GPL v2, siehe src/disk/fdformat/COPYING).
 * Aufruf: RDFORMAT.EXE A:        Dialog (Kapazitaet, Bezeichnung, Schnell), dann FreeDOS FORMAT
 *         RDFORMAT.EXE C:        Festplatte (FAT12/FAT16): Dialog (Bezeichnung, Schnell), Rueckfrage, FORMAT
 * Das Laufwerk, auf dem RD2 liegt, wird nicht formatiert.
 * Der Formatier-Kern ist FreeDOS FORMAT 0.92a fast unveraendert (src/disk/fdformat). Er laeuft als
 * fd_main(argc, argv) mit Schaltern wie auf der DOS-Befehlszeile (/F:360 /V:NAME /Q bzw. /U).
 * src/disk/fdshim.h leitet seine DOS-/BIOS-Aufrufe ueber RDDISK.MOD, seine Textausgabe ins
 * Debug-Protokoll und exit() per longjmp hierher zurueck. Rueckfragen, Fortschritt und Fehler
 * kommen ueber die Funktionen unten (rdfmtui.h) in RD2-Dialoge. */
#include <dos.h>
#include <setjmp.h>
#include <conio.h>
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
static WORD doslw, hdd, schnell, kb, nopt, abbruch, kr_quelle, kr_code, weiter;
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
void rd_bestaetigen(int format) { (void)format; if (weiter) { DbgPrint("RDFORMAT: Meldung 22 entfaellt (fortgesetzt)"); return; } if (frage(22, lwbuch, MB_YESNO | MB_WARN | MB_DEFNO) != ID_YES) { abbruch = 1; Exit(3, 3); } }
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
/* ---- FAT16/FAT32 ab 386 (PLAN_FS 3/4, 25.11.2026) -----------------------------------
 * FAT16 oder FAT32 haengt am Partitionstyp; DOS liest die Tabelle nur beim Start und gibt FORMAT dann
 * die passende Vorgabe (GENIOCTL 0860h). Wechsel: Typ im Eintrag (MBR oder EMBR) aendern, Merker
 * "RD2FMT1" in Sektor 2 der Platte (Zylinder 0, Kopf 0; frei vor der ersten Partition, geht auch von CD),
 * Neustart. Danach startet SHELL "RDFORMAT /W": Merker loeschen, Laufwerk ueber den Startsektor der
 * Partition wiederfinden (gleiche Groesse und Lage laut GENIOCTL; der alte Startsektor ist geloescht), ohne Rueckfrage formatieren,
 * danach INSTALL fortsetzen, wenn es den Wechsel ausgeloest hat. */
#define ID_FAT16 26
#define ID_FAT32 27
#define MAXCHS 16450560UL                         /* 1024 * 255 * 63: dahinter nur LBA */
static BYTE sek[512], bpart[512], bsdos[512], dap[16];
static union REGS r13; static struct SREGS s13;
static WORD ab386, k32, fs_ist, fs_soll, ext, gk, gs;
static struct { BYTE disk, slot, typ; DWORD tab, start, laenge; } pt;   /* Partition des Laufwerks; disk 0 = unbekannt */
static char inst_cl[80];                          /* INSTALL nach dem Formatieren fortsetzen */
static struct { char k[8]; DWORD start, laenge, tab; BYTE soll, schnell, lw; char name[12], inst[80]; } mk;

static void i13(void) { segread(&s13); s13.es = s13.ds; dInt(0x13, (union REGS FAR *)&r13, (union REGS FAR *)&r13, (struct SREGS FAR *)&s13); }
static BOOL platte(BYTE d)                        /* Geometrie und LBA der Platte d (80h..) */
{
    r13.h.ah = 8; r13.h.dl = d; r13.x.di = 0; i13();
    if (r13.x.cflag || !(r13.h.cl & 63)) return FALSE;
    gk = (WORD)r13.h.dh + 1; gs = r13.h.cl & 63;
    r13.h.ah = 0x41; r13.x.bx = 0x55AA; r13.h.dl = d; i13();
    ext = ab386 && !r13.x.cflag && r13.x.bx == 0xAA55 && (r13.x.cx & 1);   /* LBA nur ab 386 (wie RDFDISK) */
    return TRUE;
}
static BOOL sektor(BYTE d, DWORD lba, BYTE *b, BYTE schreib)   /* TRUE = ok */
{
    if (ext) {
        dap[0] = 16; dap[1] = 0; dap[2] = 1; dap[3] = 0; *(WORD *)(dap + 4) = FP_OFF((void FAR *)b);
        segread(&s13); *(WORD *)(dap + 6) = s13.ds; *(DWORD *)(dap + 8) = lba; *(DWORD *)(dap + 12) = 0;
        r13.h.ah = (BYTE)(schreib ? 0x43 : 0x42); r13.h.al = 0; r13.h.dl = d; r13.x.si = FP_OFF((void FAR *)dap);
    } else {
        DWORD c = lba / ((DWORD)gk * gs); WORD h = (WORD)(lba / gs % gk), sc = (WORD)(lba % gs) + 1;
        if (c > 1023) return FALSE;
        r13.h.ah = (BYTE)(schreib ? 3 : 2); r13.h.al = 1; r13.h.ch = (BYTE)c; r13.h.cl = (BYTE)(((c >> 2) & 0xC0) | sc);
        r13.h.dh = (BYTE)h; r13.h.dl = d; r13.x.bx = FP_OFF((void FAR *)b);
    }
    i13();
    if (r13.x.cflag) { kr_code = r13.h.ah; return FALSE; }
    return TRUE;
}
static BOOL istfat(BYTE t) { return t == 1 || t == 4 || t == 6 || t == 0x0B || t == 0x0C || t == 0x0E; }
static BOOL gleich(const BYTE *a, const BYTE *b) { WORD i; for (i = 0; i < 90; i++) if (a[i] != b[i]) return FALSE; return a[510] == 0x55 && a[511] == 0xAA; }
static BOOL eintrag(BYTE d, DWORD tab, BYTE slot, const BYTE *e, DWORD basis, DWORD such)   /* passt dieser Eintrag? */
{
    DWORD st = basis + *(const DWORD *)(e + 8);
    if (!istfat(e[4])) return FALSE;
    if (such ? st != such : (!sektor(d, st, bpart, 0) || !gleich(bpart, bsdos))) return FALSE;
    pt.disk = d; pt.tab = tab; pt.slot = slot; pt.typ = e[4]; pt.start = st; pt.laenge = *(const DWORD *)(e + 12);
    return TRUE;
}
static BOOL suchen(BYTE nur, DWORD such)          /* such = 0: Partition, deren Startsektor bsdos gleicht */
{
    BYTE d, i, e[64], f[32]; DWORD eb, em; WORD n;
    for (d = 0x80; d < 0x84; d++) {
        if ((nur && d != nur) || !platte(d) || !sektor(d, 0, sek, 0) || sek[510] != 0x55 || sek[511] != 0xAA) continue;
        for (i = 0; i < 64; i++) e[i] = sek[0x1BE + i];
        for (i = 0; i < 4; i++) if (eintrag(d, 0, i, e + i * 16, 0, such)) return TRUE;
        for (i = 0; i < 4; i++) if (e[i * 16 + 4] == 5 || e[i * 16 + 4] == 0x0F) break;
        if (i == 4) continue;
        eb = em = *(DWORD *)(e + i * 16 + 8);
        for (n = 0; n < 24; n++) {                /* Kette der erweiterten Partition */
            if (!sektor(d, em, sek, 0)) break;
            for (i = 0; i < 32; i++) f[i] = sek[0x1BE + i];
            if (eintrag(d, em, 0, f, em, such)) return TRUE;
            if (f[16 + 4] != 5 && f[16 + 4] != 0x0F) break;
            em = eb + *(DWORD *)(f + 16 + 8);
        }
    }
    pt.disk = 0; return FALSE;
}
static BOOL dosbs(WORD lw, BYTE *b)               /* Startsektor eines DOS-Laufwerks: INT 25h, sonst 7305h (FAT32) */
{
    static struct { DWORD s; WORD n; void FAR *p; } pk; static union REGS r; static struct SREGS sr;
    pk.s = 0; pk.n = 1; pk.p = (void FAR *)b; segread(&sr);
    r.x.ax = lw - 1; r.x.cx = 0xFFFF; r.x.bx = FP_OFF((void FAR *)&pk);
    dInt(0x25, (union REGS FAR *)&r, (union REGS FAR *)&r, (struct SREGS FAR *)&sr);
    if (!r.x.cflag) return TRUE;
    r.x.ax = 0x7305; r.x.cx = 0xFFFF; r.h.dl = (BYTE)lw; r.x.si = 0; r.x.bx = FP_OFF((void FAR *)&pk); segread(&sr);
    dInt(0x21, (union REGS FAR *)&r, (union REGS FAR *)&r, (struct SREGS FAR *)&sr);
    return !r.x.cflag;
}
static BOOL geraet(WORD lw, DWORD *ges, DWORD *vor)   /* Vorgabe-BPB des DOS-Laufwerks (GENIOCTL 0860h, FAT32: 4860h) */
{
    static BYTE b[96]; static union REGS r; static struct SREGS sr; WORD c;
    for (c = 0x0860; ; c = 0x4860) {
        WORD i; for (i = 0; i < sizeof b; i++) b[i] = 0;
        segread(&sr); r.x.ax = 0x440D; r.h.bl = (BYTE)lw; r.x.cx = c; r.x.dx = FP_OFF((void FAR *)b);
        dInt(0x21, (union REGS FAR *)&r, (union REGS FAR *)&r, (struct SREGS FAR *)&sr);
        if (!r.x.cflag) break;
        if (c == 0x4860) return FALSE;
    }
    *ges = *(WORD *)(b + 15) ? *(WORD *)(b + 15) : *(DWORD *)(b + 28); *vor = *(DWORD *)(b + 24);   /* BPB ab Byte 7 */
    return TRUE;
}
static BOOL kern32(void)                          /* kann der laufende DOS-Kern FAT32 (7303h)? */
{
    static union REGS r; static struct SREGS sr; static char w[4] = "C:\\"; static BYTE b[48];
    segread(&sr); sr.es = sr.ds; w[0] = lwbuch[0];
    r.x.ax = 0x7303; r.x.dx = FP_OFF((void FAR *)w); r.x.di = FP_OFF((void FAR *)b); r.x.cx = sizeof b;
    dInt(0x21, (union REGS FAR *)&r, (union REGS FAR *)&r, (struct SREGS FAR *)&sr);
    return !r.x.cflag && r.x.ax != 0x7300;
}
static BYTE neutyp(WORD fs)                       /* Typ fuer das Ziel-Dateisystem */
{
    BOOL lba = ext && pt.start + pt.laenge > MAXCHS;
    if (fs == 32) return (BYTE)(lba ? 0x0C : 0x0B);
    return (BYTE)(pt.laenge < 65536UL ? 4 : lba ? 0x0E : 6);
}
static BOOL fs_erlaubt(WORD fs) { return fs == 32 ? k32 && pt.laenge > 128UL * 2048 : pt.laenge < 2048UL * 2048; }
static BOOL merker(BYTE d, BYTE schreib)          /* Sektor 2 der Platte: 0 lesen (TRUE = Merker da), 1 schreiben, 2 loeschen */
{
    WORD i;
    if (!platte(d) || !sektor(d, 1, sek, 0)) return FALSE;
    if (!schreib) { for (i = 0; i < 7; i++) if (sek[i] != (BYTE)"RD2FMT1"[i]) return FALSE; for (i = 0; i < sizeof mk; i++) ((BYTE *)&mk)[i] = sek[i]; return TRUE; }
    if (!(sek[0] == 'R' && sek[1] == 'D' && sek[2] == '2' && sek[3] == 'F'))   /* fremde Daten nie ueberschreiben */
        for (i = 0; i < 512; i++) if (sek[i]) return FALSE;
    for (i = 0; i < 512; i++) sek[i] = 0;
    if (schreib == 1) for (i = 0; i < sizeof mk; i++) sek[i] = ((BYTE *)&mk)[i];
    return sektor(d, 1, sek, 1);
}
static void sichern(void)                         /* Tabellensektor vor dem Aendern nach RD2\PTABn.BAK (wie RDFDISK) */
{
    SYSINFO si; char n[80]; WORD i;
    KernInfo(&si); for (i = 0; i < 66 && si.home[i]; i++) n[i] = si.home[i];
    if (i && n[i - 1] != '\\') n[i++] = '\\';
    n[i] = 0; scat(n, "PTAB", sizeof n); i = slen(n); n[i++] = (char)('1' + (pt.disk & 3)); n[i] = 0; scat(n, ".BAK", sizeof n);
    if (!FileSaveSafe(n, (void FAR *)sek, 512)) DbgPrint(n);   /* von CD: geht nicht, dann ohne Sicherung */
}
static void neustart(void)                        /* DOS-Puffer schreiben, dann Warmstart (wie RDFDISK) */
{
    static union REGS r; static struct SREGS s; void (__far *reset)(void) = (void (__far *)(void))MK_FP(0xFFFF, 0);
    segread(&s); r.h.ah = 0x0D; dInt(0x21, (union REGS FAR *)&r, (union REGS FAR *)&r, (struct SREGS FAR *)&s);
    DbgPrint("RDFORMAT: Neustart");
    *(WORD __far *)MK_FP(0x40, 0x72) = 0x1234;
    _disable(); outp(0x64, 0xFE);
    { WORD i; for (i = 0; i < 0xFFFF; i++) ; }
    reset();
}
static void umstellen(void)                       /* Typ aendern, Merker, Neustart; zurueck nur, wenn es nicht geht (Meldung kam) */
{
    char a[8];
    if (!fs_erlaubt(fs_soll)) { frage(39, 0, MB_OK | MB_WARN); return; }
    scpy(a, fs_soll == 32 ? "FAT32" : "FAT16", sizeof a);
    fmt(txt, 36, lwbuch, a); DbgPrint("RDFORMAT: Meldung 36");
    if (MsgBox(owner(), txt, StrGet(inst, 1), MB_YESNO | MB_WARN | MB_DEFNO) != ID_YES) return;
    scpy(mk.k, "RD2FMT1", sizeof mk.k); mk.start = pt.start; mk.laenge = pt.laenge; mk.tab = pt.tab; mk.lw = (BYTE)doslw;
    mk.soll = (BYTE)fs_soll; mk.schnell = (BYTE)schnell;
    scpy(mk.name, name, sizeof mk.name); scpy(mk.inst, inst_cl, sizeof mk.inst);
    if (!merker(pt.disk, 1)) { frage(38, 0, MB_OK | MB_WARN); return; }
    if (!platte(pt.disk) || !sektor(pt.disk, pt.tab, sek, 0)) { hex2(a, kr_code); frage(40, a, MB_OK | MB_ERROR); merker(pt.disk, 2); return; }
    sichern();
    sek[0x1BE + pt.slot * 16 + 4] = neutyp(fs_soll);
    if (!sektor(pt.disk, pt.tab, sek, 1)) { hex2(a, kr_code); frage(40, a, MB_OK | MB_ERROR); merker(pt.disk, 2); return; }
    { WORD i; for (i = 0; i < 512; i++) sek[i] = 0; }   /* alten Startsektor (und FAT32-Kopie in Sektor 6) loeschen: */
    sektor(pt.disk, pt.start, sek, 1);             /* DOS nimmt sonst das alte Dateisystem statt der Vorgabe zum neuen Typ */
    if (fs_ist == 32) sektor(pt.disk, pt.start + 6, sek, 1);
    DbgPrint(fs_soll == 32 ? "RDFORMAT: Typ FAT32" : "RDFORMAT: Typ FAT16");
    neustart();
}
static BOOL fortsetzen(void)                      /* /W: Merker suchen, Laufwerk wiederfinden; FALSE = nichts zu tun */
{
    BYTE d; WORD lw, k; DRIVEINFO di;
    for (d = 0x80; d < 0x84; d++) if (merker(d, 0)) break;
    if (d == 0x84) return FALSE;
    merker(d, 2);                                 /* zuerst loeschen: nie im Kreis neu starten */
    DbgPrint("RDFORMAT: Merker gefunden");
    if (suchen(d, mk.start))                      /* Laufwerk: gleiche Groesse und Lage wie die Partition (Startsektor ist geloescht) */
        for (k = 0; k <= 24; k++) {               /* zuerst der Buchstabe von vor dem Neustart, dann alle */
            DWORD g, v; lw = k ? k + 2 : mk.lw;
            if (DriveInfo(lw | DI_NURTYP, &di) || di.typ != DT_HARD || !geraet(lw, &g, &v)) continue;
            if (g != mk.laenge || (v != mk.start && v != mk.start - mk.tab)) continue;
            doslw = lw; lwbuch[0] = (char)('A' + lw - 1); lwbuch[1] = 0;
            scpy(name, mk.name, sizeof name); schnell = mk.schnell; scpy(inst_cl, mk.inst, sizeof inst_cl);
            weiter = 1; hdd = 1;
            return TRUE;
        }
    frage(37, 0, MB_OK | MB_ERROR);
    return FALSE;
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
        if (!ab386 || !pt.disk) { ShowWindow(DlgItem(d, ID_FAT16), SW_HIDE); ShowWindow(DlgItem(d, ID_FAT32), SW_HIDE); }
        else {                                    /* ab 386: FAT16/FAT32 (Vorgabe: jetziger Typ oder Wunsch von INSTALL) */
            EnableWindow(DlgItem(d, ID_FAT32), fs_erlaubt(32) || fs_ist == 32);
            EnableWindow(DlgItem(d, ID_FAT16), fs_erlaubt(16) || fs_ist == 16);
            DlgCheck(d, fs_soll == 32 ? ID_FAT32 : ID_FAT16, TRUE);
        }
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        DlgGetText(d, ID_NAME, name, sizeof name);
        for (i = 0; name[i]; i++) if (name[i] >= 'a' && name[i] <= 'z') name[i] -= 32;
        schnell = DlgChecked(d, ID_SCHNELL);
        if (ab386 && pt.disk) fs_soll = DlgChecked(d, ID_FAT32) ? 32 : 16;
    }
    return FALSE;
}

static BOOL laeuft(const char *n)                 /* laeuft ein anderes Programm mit diesem Namen? */
{
    TASKINFO ti; HTASK h, ich = TaskSelf(); WORD k;
    for (h = TaskFirst(&ti); h; h = TaskNext(h, &ti)) { for (k = 0; n[k] && ti.name[k] == n[k]; k++) ; if (h != ich && !n[k] && !ti.name[k]) return TRUE; }
    return FALSE;
}
short RDAPI RdMain(HINST i, LPCSTR cl, WORD show)
{
    WORD k, t, fs_wunsch = 0; short e; DRIVEINFO di; SYSINFO si; char a[12], b[12]; LPCSTR p;
    (void)show;
    inst = i;
    if (laeuft("Formatieren")) { frage(34, 0, MB_OK | MB_WARN); return 1; }   /* sonst zwei Formatierungen auf demselben Laufwerk */
    while (*cl == ' ') cl++;
    KernInfo(&si); ab386 = si.cpu >= CPU_386;
    if (cl[0] == '/' && (cl[1] & 0xDF) == 'W') {  /* SHELL nach dem Start: Formatieren nach Typwechsel fortsetzen (PLAN_FS 4) */
        if (!ab386 || !(hm = ModLoad("RDDISK.MOD"))) return 0;
        dInt = (RDD_INT_F)ModProc(hm, RDD_INT);
        if (!fortsetzen()) { ModFree(hm); return 0; }
        fmt(txt, 41, lwbuch, 0); DbgPrint(txt);
        goto los;
    }
    if (!*cl) { DbgPrint("RDFORMAT: kein Laufwerk"); return 1; }
    for (p = cl; *p; p++) if (p[0] == '/') {      /* von INSTALL: /FS:16 oder /FS:32, /I:Befehl (Rest der Zeile) */
        if ((p[1] & 0xDF) == 'F' && (p[2] & 0xDF) == 'S' && p[3] == ':') fs_wunsch = p[4] == '3' ? 32 : 16;
        if ((p[1] & 0xDF) == 'I' && p[2] == ':') { scpy(inst_cl, p + 3, sizeof inst_cl); break; }
    }
    lwbuch[0] = (char)(cl[0] >= 'a' ? cl[0] - 32 : cl[0]); lwbuch[1] = 0;
    doslw = (WORD)(lwbuch[0] - 'A' + 1);
    if (DriveInfo(doslw | DI_NURTYP, &di) || (di.typ != DT_FLOPPY && di.typ != DT_HARD)) { frage(14, 0, MB_OK | MB_ERROR); return 1; }
    hdd = di.typ == DT_HARD;
    if (hdd && laeuft("Festplatte")) { frage(35, 0, MB_OK | MB_WARN); return 1; }   /* G23: Partitionen aendern sich gerade */
    if (!hdd && (di.flags & DI_PHANTOM)) { doslw = 1; lwbuch[0] = 'A'; }
    if ((si.home[0] & 0xDF) == lwbuch[0]) { frage(31, lwbuch, MB_OK | MB_ERROR); return 1; }   /* RD2 laeuft von dort */
    if (!(hm = ModLoad("RDDISK.MOD"))) { frage(15, 0, MB_OK | MB_ERROR); return 1; }
    dInt = (RDD_INT_F)ModProc(hm, RDD_INT);
    DbgPrint(hdd ? "RDFORMAT: Festplatte" : "RDFORMAT: Diskette");
    if (hdd) {
        if (ab386) {                              /* Partition des Laufwerks und ihr Typ (FAT16/FAT32) */
            k32 = kern32();
            if (dosbs(doslw, bsdos)) suchen(0, 0);
            if (pt.disk) {
                fs_ist = pt.typ == 0x0B || pt.typ == 0x0C ? 32 : 16;
                fs_soll = fs_wunsch && fs_erlaubt(fs_wunsch) ? fs_wunsch : fs_ist;
                DbgPrint(fs_ist == 32 ? "RDFORMAT: ist FAT32" : "RDFORMAT: ist FAT16");
            } else DbgPrint("RDFORMAT: Partition nicht gefunden");
        }
        DbgPrint("RDFORMAT: bereit");
        if (DialogRun(inst, 12, 0, hproc, 0) != ID_OK) { ModFree(hm); return 1; }
        if (ab386 && pt.disk && fs_soll != fs_ist) { umstellen(); ModFree(hm); return 1; }   /* kehrt nur zurueck, wenn es nicht geht */
    } else {
        t = biostyp(doslw - 1);
        if (!t) { DriveInfo(doslw, &di); t = di.format_kb > 360 ? 3 : 1; }
        for (nopt = 0; nopt < 3 && wahl[t][nopt] != 0xFF; nopt++) opt[nopt] = wahl[t][nopt];
        kb = kbtab[opt[0]];
        DbgPrint("RDFORMAT: bereit");
        if (DialogRun(inst, 10, 0, fproc, 0) != ID_OK) { ModFree(hm); return 1; }
    }
los:
    /* Befehlszeile fuer FreeDOS FORMAT */
    a_lw[0] = lwbuch[0]; a_lw[1] = ':'; a_lw[2] = 0;
    fd_argc = 0; fd_argv[fd_argc++] = "FORMAT"; fd_argv[fd_argc++] = a_lw;
    if (!hdd) { scpy(a_f, "/F:", sizeof a_f); zahl(a, kb); scat(a_f, a, sizeof a_f); fd_argv[fd_argc++] = a_f; }
    if (name[0]) { scpy(a_v, "/V:", sizeof a_v); scat(a_v, name, sizeof a_v); fd_argv[fd_argc++] = a_v; }
    fd_argv[fd_argc++] = schnell ? "/Q" : "/U";
    fd_argv[fd_argc] = 0;
    for (k = 0; k < (WORD)fd_argc; k++) DbgPrint(fd_argv[k]);
    e = DialogRun(inst, 11, 0, pproc, 0);
    { union REGS r; struct SREGS s; r.h.ah = 0x0D; s.ds = s.es = 0; dInt(0x21, (union REGS FAR *)&r, (union REGS FAR *)&r, (struct SREGS FAR *)&s); }   /* DOS-Puffer verwerfen: neue FAT (Audit 24.11.) */
    ModFree(hm);
    if (e == ID_CANCEL) { frage(hdd ? 33 : 18, 0, MB_OK | MB_WARN); return 1; }
    if (e != ID_OK) { DbgPrint("RDFORMAT: Fehler"); fehler(23); return 1; }
    if (DriveInfo(doslw, &di)) di.frei = 0;
    zahl(a, di.frei >> 10); zahl(b, drive_statistics.defekt / 2); fmt(txt, 7, a, b);
    DbgPrint("RDFORMAT: fertig"); DbgPrint(a);
    MsgBox(0, txt, StrGet(inst, 1), MB_OK | MB_INFO);
    if (weiter && inst_cl[0]) {                   /* INSTALL hatte den Wechsel ausgeloest: dort weiter */
        for (k = 0; inst_cl[k] && inst_cl[k] != ' '; k++) ;
        e = inst_cl[k] ? (short)k + 1 : (short)k; inst_cl[k] = 0;
        DbgPrint("RDFORMAT: weiter mit INSTALL");
        TaskExec(inst_cl, inst_cl + e, SW_SHOW);
    }
    return 0;
}
