/* RDFDISK.C - RD2-Oberflaeche fuer Free FDISK (GPL v2, siehe disk/fdfdisk/COPYING).
 * Aufruf: RDFDISK.EXE            Festplatte einrichten: Partitionen anlegen, loeschen, aktiv setzen, schreiben.
 * Der Rechenkern ist Free FDISK (pcompute.c, pdiskio.c) fast unveraendert; disk/fdglue.c verbindet ihn mit
 * dieser Oberflaeche (fdglue.h). DOS- und BIOS-Aufrufe laufen ueber RDDISK.MOD (Positivliste).
 * Anlegen nach DOS-Regel: das erste DOS-Laufwerk wird primaer und aktiv, weitere werden logische Laufwerke
 * in einer erweiterten Partition. Erst "Schreiben" aendert die Platte; danach Neustart, dann RDFORMAT.
 * Das Laufwerk, auf dem RD2 liegt, laesst sich nicht loeschen. */
#include <dos.h>
#include "RD2.H"
#include "RDDISK.H"
#include "fdglue.h"

#define ID_KOPF    19
#define ID_P1      21
#define ID_P2      22
#define ID_LISTE   30
#define ID_NEU     11
#define ID_WEG     12
#define ID_AKTIV   13

static HINST inst; static HMOD hm; static RDD_INT_F dInt;
static FG_TEIL teil[FG_MAX]; static int nteil, nplatten, platte;
static char heim, txt[200];

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(LPSTR d, LPCSTR s, WORD max) { WORD i; for (i = 0; i + 1 < max && s[i]; i++) d[i] = s[i]; d[i] = 0; }
static void scat(LPSTR d, LPCSTR s, WORD max) { WORD n = slen(d); scpy(d + n, s, max - n); }
static void zahl(LPSTR b, DWORD v) { char t[12]; WORD i = 0, j = 0; do t[i++] = (char)('0' + v % 10); while ((v /= 10) != 0); while (i) b[j++] = t[--i]; b[j] = 0; }
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
static short frage(HWND o, WORD id, LPCSTR a, WORD art)
{
    char m[24]; scpy(m, "RDFDISK: Meldung ", sizeof m); zahl(m + 17, id); DbgPrint(m);
    fmt(txt, id, a, 0); return MsgBox(o, txt, StrGet(inst, 1), art);
}
static short fehler(HWND o, WORD id) { char a[8]; zahl(a, (DWORD)(WORD)fg_fehler); return frage(o, id, a, MB_OK | MB_ERROR); }

/* ---- Anbindung des FDISK-Kerns (fdshim.h) ---------------------------------------------- */
void rd_intr(int nr, union REGPACK *r)            /* intr() von FDISK: REGPACK <-> REGS ueber RDDISK.MOD */
{
    static union REGS e, a; static struct SREGS s;
    e.x.ax = r->w.ax; e.x.bx = r->w.bx; e.x.cx = r->w.cx; e.x.dx = r->w.dx; e.x.si = r->w.si; e.x.di = r->w.di;
    s.ds = r->w.ds; s.es = r->w.es;
    dInt((WORD)nr, (union REGS FAR *)&e, (union REGS FAR *)&a, (struct SREGS FAR *)&s);
    r->w.ax = a.x.ax; r->w.bx = a.x.bx; r->w.cx = a.x.cx; r->w.dx = a.x.dx; r->w.si = a.x.si; r->w.di = a.x.di;
    r->w.ds = s.ds; r->w.es = s.es;
    r->w.flags = (r->w.flags & ~1u) | (a.x.cflag ? 1u : 0u);
}
int rd_int86x(int nr, union REGS *ein, union REGS *aus, struct SREGS *s)
{
    static struct SREGS t;
    if (!s) { segread(&t); s = &t; }
    return dInt((WORD)nr, (union REGS FAR *)ein, (union REGS FAR *)aus, (struct SREGS FAR *)s);
}
static char zeile[82]; static WORD zn;
int rd_putch(int c)                               /* Textausgabe des Kerns (nur Fehlerfaelle): ins Protokoll */
{
    if (c == '\n' || c == '\r' || zn >= sizeof zeile - 1) {
        zeile[zn] = 0; if (zn) DbgPrint(zeile); zn = 0;
        if (c == '\n' || c == '\r') return c;
    }
    zeile[zn++] = (char)c;
    return c;
}
void rd_exit(int code) { (void)code; DbgPrint("RDFDISK: exit im Kern"); }   /* kommt im benutzten Teil nicht vor */
int rd_system(const char *cmd) { (void)cmd; return -1; }
int rd_write(int fd, const void *b, unsigned n) { const char *p = (const char *)b; unsigned i; (void)fd; for (i = 0; i < n; i++) rd_putch(p[i]); return (int)n; }
int rd_isatty(int fd) { (void)fd; return 0; }

/* ---- Liste -------------------------------------------------------------------------- */
static void eintrag(LPSTR b, const FG_TEIL *t)    /* "C:  Primaer  FAT16  aktiv   20 MB" */
{
    static const char hx[] = "0123456789ABCDEF"; char z[12]; WORD n;
    b[0] = t->lw ? t->lw : ' '; b[1] = t->lw ? ':' : ' '; b[2] = ' '; b[3] = ' '; b[4] = 0;
    if (t->art == FG_LOG) scat(b, "  ", 60);       /* eingerueckt unter der erweiterten */
    scat(b, StrGet(inst, t->art == FG_PRI ? 4 : t->art == FG_EXT ? 5 : t->art == FG_LOG ? 6 : 7), 60);
    if (t->art == FG_PRI || t->art == FG_LOG) {
        scat(b, "  ", 60);
        if (t->typ == 1) scat(b, "FAT12", 60);
        else if (t->dos && t->typ != 0x0B && t->typ != 0x0C) scat(b, "FAT16", 60);
        else if (t->typ == 0x0B || t->typ == 0x0C) scat(b, "FAT32", 60);
        else { scat(b, StrGet(inst, 9), 60); z[0] = ' '; z[1] = hx[t->typ >> 4]; z[2] = hx[t->typ & 15]; z[3] = 'h'; z[4] = 0; scat(b, z, 60); }
        if (t->aktiv) { scat(b, "  ", 60); scat(b, StrGet(inst, 8), 60); }
    }
    n = slen(b); while (n < 36) b[n++] = ' '; b[n] = 0;
    zahl(z, t->mb ? t->mb : 1); scat(z, " MB", sizeof z);
    for (n = slen(z); n < 9; n++) scat(b, " ", 60);
    scat(b, z, 60);
}
static void knoepfe(HWND d)
{
    WORD s = (WORD)SendMessage(DlgItem(d, ID_LISTE), L_GETSEL, 0, 0); const FG_TEIL *t = s < (WORD)nteil ? &teil[s] : 0;
    int alslog;
    EnableWindow(DlgItem(d, ID_NEU), t && (t->art == FG_FREIP || t->art == FG_FREIL) && fg_maxmb(t, &alslog) > 0);
    EnableWindow(DlgItem(d, ID_WEG), t && (t->art == FG_PRI || t->art == FG_EXT || t->art == FG_LOG));
    EnableWindow(DlgItem(d, ID_AKTIV), t && t->art == FG_PRI && t->dos && !t->aktiv);
    EnableWindow(DlgItem(d, ID_OK), fg_geaendert());
}
static void fuellen(HWND d, WORD sel)
{
    HWND l = DlgItem(d, ID_LISTE); char b[64], a[4], m[12]; int i;
    fg_platte(platte);
    a[0] = (char)('1' + platte); a[1] = 0; zahl(m, fg_plattemb());
    fmt(txt, 3, a, m); DlgSetText(d, ID_KOPF, txt);
    nteil = fg_liste(teil);
    SendMessage(l, L_CLEAR, 0, 0);
    for (i = 0; i < nteil; i++) { eintrag(b, &teil[i]); SendMessage(l, L_ADD, 0, (DWORD)(LPSTR)b); DbgPrint(b); }
    if (sel >= (WORD)nteil) sel = nteil ? (WORD)nteil - 1 : 0xFFFF;
    SendMessage(l, L_SETSEL, sel, 0);
    knoepfe(d);
    DbgPrint("RDFDISK: Liste bereit");
}
static void anlegen(HWND d, const FG_TEIL *t)
{
    char b[12], m[12]; int alslog; unsigned long max = fg_maxmb(t, &alslog), v; WORD i;
    zahl(m, max); scpy(b, m, sizeof b);
    fmt(txt, 10, m, 0);
    if (!InputBox(d, txt, StrGet(inst, 1), b, sizeof b)) return;
    for (i = 0, v = 0; b[i] >= '0' && b[i] <= '9' && v < 100000UL; i++) v = v * 10 + (b[i] - '0');
    if (!i || b[i] || !v || v > max) { frage(d, 22, m, MB_OK | MB_WARN); return; }
    if (fg_anlegen(t, v)) fehler(d, 19);
}
static void loeschen(HWND d, const FG_TEIL *t)
{
    char a[3]; int i;
    a[0] = t->lw; a[1] = ':'; a[2] = 0;
    if (t->art == FG_EXT) { for (i = 0; i < nteil; i++) if (teil[i].art == FG_LOG && teil[i].lw == heim) t = &teil[i]; }
    if (heim && t->lw == heim) { a[0] = heim; frage(d, 20, a, MB_OK | MB_ERROR); return; }
    if (t->art == FG_EXT) { if (frage(d, 12, 0, MB_YESNO | MB_WARN) != ID_YES) return; }
    else if (frage(d, 11, t->lw ? a : "", MB_YESNO | MB_WARN) != ID_YES) return;
    if (fg_loeschen(t)) fehler(d, 19);
}
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD s = (WORD)SendMessage(DlgItem(d, ID_LISTE), L_GETSEL, 0, 0);
    if (msg == M_INITDIALOG) {
        if (nplatten < 2) { ShowWindow(DlgItem(d, ID_P1), SW_HIDE); ShowWindow(DlgItem(d, ID_P2), SW_HIDE); }
        fuellen(d, 0);
        SendMessage(d, M_SETFOCUS, 0, 0);
        return FALSE;
    }
    if (msg != M_COMMAND) return FALSE;
    if (p1 == ID_LISTE) { if (HIWORD(p2) == N_SELECT) knoepfe(d); return TRUE; }
    if ((p1 == ID_P1 || p1 == ID_P2) && platte != (int)(p1 - ID_P1)) { platte = p1 - ID_P1; fuellen(d, 0); return TRUE; }
    if (s >= (WORD)nteil && (p1 == ID_NEU || p1 == ID_WEG || p1 == ID_AKTIV)) return TRUE;
    if (p1 == ID_NEU) {                           /* danach gleich den restlichen freien Platz waehlen */
        anlegen(d, &teil[s]); fuellen(d, s);
        for (s = 0; s < (WORD)nteil && teil[s].art != FG_FREIP && teil[s].art != FG_FREIL; s++) ;
        if (s < (WORD)nteil) { SendMessage(DlgItem(d, ID_LISTE), L_SETSEL, s, 0); knoepfe(d); }
        return TRUE;
    }
    if (p1 == ID_WEG) { loeschen(d, &teil[s]); fuellen(d, s); return TRUE; }
    if (p1 == ID_AKTIV) { if (fg_aktiv(&teil[s])) fehler(d, 19); fuellen(d, s); return TRUE; }
    if (p1 == ID_OK) {
        if (!fg_geaendert() || frage(d, 13, 0, MB_YESNO | MB_ASK) != ID_YES) return TRUE;
        DbgPrint("RDFDISK: schreiben");
        if (fg_schreiben()) { fehler(d, 18); DialogEnd(d, ID_NO); return TRUE; }
        DbgPrint("RDFDISK: geschrieben");
        frage(d, 14, 0, MB_OK | MB_INFO);
        DialogEnd(d, ID_OK);
        return TRUE;
    }
    if (p1 == ID_CANCEL) {
        if (fg_geaendert() && frage(d, 15, 0, MB_YESNO | MB_ASK) != ID_YES) return TRUE;
        DialogEnd(d, ID_CANCEL);
        return TRUE;
    }
    return FALSE;
}

short RDAPI RdMain(HINST i, LPCSTR cl, WORD show)
{
    SYSINFO si; short e; char a[8];
    (void)show; (void)cl;
    inst = i;
    {   /* nur ein RDFDISK zugleich */
        TASKINFO ti; HTASK h, ich = TaskSelf(); WORD k; static const char n[] = "Festplatte";
        for (h = TaskFirst(&ti); h; h = TaskNext(h, &ti)) {
            for (k = 0; n[k] && ti.name[k] == n[k]; k++) ;
            if (h != ich && !n[k] && !ti.name[k]) { frage(0, 23, 0, MB_OK | MB_WARN); return 1; }
        }
    }
    KernInfo(&si);
    heim = (char)(si.home[0] & 0xDF); if (heim < 'C' || heim > 'Z') heim = 0;
    if (!(hm = ModLoad("RDDISK.MOD"))) { frage(0, 21, 0, MB_OK | MB_ERROR); return 1; }
    dInt = (RDD_INT_F)ModProc(hm, RDD_INT);
    nplatten = fg_start();
    if (fg_fehler) { zahl(a, (WORD)fg_fehler); frage(0, 17, a, MB_OK | MB_ERROR); ModFree(hm); return 1; }
    if (nplatten <= 0) { frage(0, 16, 0, MB_OK | MB_WARN); ModFree(hm); return 1; }
    if (nplatten > 2) nplatten = 2;
    DbgPrint("RDFDISK: bereit");
    e = DialogRun(inst, 10, 0, dproc, 0);
    ModFree(hm);
    return e == ID_OK ? 0 : 1;
}
