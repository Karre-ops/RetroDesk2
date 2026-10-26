/* OPTIONEN.C - Zubehoer Optionen (RD1 1.2: Systemklaenge, Start nach dem Booten, Netzzeit beim Booten).
 * Systemklaenge: <RD2>\RD2.INI [System] Systemklang=1/0 (RDSND). Netzzeit: RD2.INI [Netz] Zeit=1 (Shell startet UHR.EXE N).
 * Start nach dem Booten: RD2 startet als SHELL= in CONFIG.SYS (RD2SETUP). Aus: die RD-Zeile und INSTALL=-Zeilen, deren
 * Programm auch in AUTOEXEC.BAT steht, werden "REM RD2 ", die alte "REM SHELL="-Zeile gilt wieder. An: umgekehrt.
 * Aus geht nur, wenn danach eine Shell da ist (alte SHELL=-Datei oder COMMAND.COM), sonst startet der Rechner nicht.
 * Befehlszeile "T [Ordner]": Testmodus, CONFIG.SYS/AUTOEXEC.BAT/COMMAND.COM aus dem Ordner (test/optionen.sh). */
#include "RD2.H"

#define ID_KLANG 21
#define ID_START 22
#define ID_NETZ  23
#define CMAX 8192
static HINST hi; static BYTE test; static char ord[64], cs[80], home[66], ini[80];
static HMEM hm; static char FAR *ein, FAR *aus, FAR *ae; static WORD nein, nae;

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static BYTE gr(BYTE c) { return (BYTE)(c >= 'a' && c <= 'z' ? c - 32 : c); }
static BYTE beginnt(const char FAR *a, LPCSTR k) { while (*k) if (gr(*a++) != (BYTE)*k++) return 0; return 1; }
static const char FAR *suche(const char FAR *a, WORD n, LPCSTR k)   /* k gross, in a[0..n) ohne Gross/Klein */
{
    WORD i, l = slen(k);
    for (i = 0; i + l <= n; i++) { WORD j = 0; while (j < l && gr(a[i + j]) == (BYTE)k[j]) j++; if (j == l) return a + i; }
    return 0;
}
static BYTE gibts(LPCSTR p) { FINDDATA f; return FileFindFirst(p, 0, &f) == 0; }
static WORD lesen(LPCSTR n, char FAR *b)
{
    short f; WORD r = 0;
    if ((f = FileOpen(n, FO_READ)) >= 0) { r = FileRead(f, b, CMAX - 1); if (r == 0xFFFF) r = 0; FileClose(f); }
    b[r] = 0; return r;
}
static const char FAR *sp(const char FAR *a) { while (*a == ' ' || *a == '\t') a++; return a; }
static BYTE shellz(const char FAR *a) { a = sp(a); return beginnt(a, "SHELL") && (*sp(a + 5) == '='); }
static WORD zlen(const char FAR *a) { WORD n = 0; while (a[n] && a[n] != '\n') n++; return n; }
static BYTE rdz(const char FAR *a) { return suche(a, zlen(a), "RD.EXE") != 0; }
static BYTE an(void)                              /* aktive SHELL=-Zeile mit RD.EXE? */
{
    const char FAR *a;
    for (a = ein; *a; a += zlen(a) + (a[zlen(a)] != 0)) if (shellz(a) && rdz(a)) return 1;
    return 0;
}
static BYTE inae(const char FAR *a)               /* INSTALL=-Programm steht auch in AUTOEXEC.BAT */
{
    char n[13]; WORD i = 0; const char FAR *e, FAR *s;
    a = sp(sp(a) + 7); if (*a == '=') a = sp(a + 1);
    for (e = a; *e && *e != ' ' && *e != '\r' && *e != '\n'; e++) ;
    for (s = e; s > a && s[-1] != '\\' && s[-1] != ':'; s--) ;
    while (s < e && *s != '.' && i < 8) n[i++] = (char)gr(*s++);
    n[i] = 0;
    return i && suche(ae, nae, n) != 0;
}
static WORD put(WORD o, const char FAR *s, WORD n) { while (n-- && o < CMAX - 1) aus[o++] = *s++; return o; }
/* Rueckgabe 0 ok, 1 Datei nicht schreibbar, 2 keine Shell fuer "aus" */
static WORD schalten(BYTE soll)
{
    const char FAR *a; WORD o = 0, l, alt = 0; BYTE neu = 0; char p[80];
    for (a = ein; *a; a += l + (a[l] != 0)) {
        l = zlen(a);
        if (soll) {
            if (beginnt(sp(a), "REM RD2 ")) { a = sp(a) + 8; l = zlen(a); if (shellz(a)) neu = 1; }
            else if (shellz(a) && !rdz(a)) o = put(o, "REM ", 4);
            else if (shellz(a)) neu = 1;
        } else {
            if ((shellz(a) && rdz(a)) || (beginnt(sp(a), "INSTALL") && inae(a))) o = put(o, "REM RD2 ", 8);
            else if (!alt && beginnt(sp(a), "REM ") && shellz(sp(a) + 4) && !rdz(a)) {   /* alte Shell wieder */
                WORD i = 0; const char FAR *q = sp(sp(a) + 4); q = sp(sp(q + 5) + 1);
                while (q[i] && q[i] != ' ' && q[i] != '\r' && q[i] != '\n' && i < sizeof p - 1) { p[i] = q[i]; i++; }
                p[i] = 0;
                if (gibts(p)) { a = sp(a) + 4; l = zlen(a); alt = 1; }
            }
        }
        o = put(o, a, l); if (a[l]) o = put(o, "\n", 1);
    }
    if (soll && !neu) {                           /* keine RD-Zeile mehr da: neu anlegen wie RD2SETUP */
        if (o && aus[o - 1] != '\n') o = put(o, "\r\n", 2);
        o = put(o, "SHELL=", 6); o = put(o, home, slen(home)); o = put(o, "RD.EXE /P ", 10);
        o = put(o, home, slen(home)); o = put(o, "SHELL.EXE\r\n", 11);
    }
    if (!soll && !alt) { scpy(p, ord, sizeof p); scat(p, "COMMAND.COM", sizeof p); if (!gibts(p)) return 2; }
    if (test) { WORD i = 0, j; while (i < o) { for (j = 0; i < o && aus[i] != '\n' && j < 78; i++) if (aus[i] != '\r') p[j++] = aus[i];
                p[j] = 0; i++; DbgPrint(p); } }   /* neue CONFIG.SYS Zeile fuer Zeile */
    return FileSaveSafe(cs, aus, o) ? 1 : 0;
}
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD e; BYTE s;
    (void)p2;
    if (msg == M_INITDIALOG) {
        DlgCheck(d, ID_KLANG, ProfileGetInt(ini, "System", "Systemklang", 1) != 0);
        DlgCheck(d, ID_START, an());
        DlgCheck(d, ID_NETZ, ProfileGetInt(ini, "Netz", "Zeit", 0) != 0);
        if (test) DbgPrint(an() ? "OPTIONEN: bereit, Start an" : "OPTIONEN: bereit, Start aus");
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        ProfileSetStr(ini, "System", "Systemklang", DlgChecked(d, ID_KLANG) ? "1" : "0");
        ProfileSetStr(ini, "Netz", "Zeit", DlgChecked(d, ID_NETZ) ? "1" : "0");
        s = (BYTE)DlgChecked(d, ID_START);
        if (s != an()) {
            e = schalten(s);
            if (test) DbgHex("OPTIONEN: CONFIG.SYS", e);
            if (e) { MsgBox(d, StrGet(hi, e == 2 ? 3 : 2), StrGet(hi, 1), MB_OK | MB_WARN); DlgCheck(d, ID_START, !s); return TRUE; }
        }
        if (test) DbgPrint("OPTIONEN: gespeichert");
    }
    return FALSE;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    SYSINFO si; WORD n;
    (void)show; hi = inst;
    KernInfo(&si); scpy(home, si.home, sizeof home); if (slen(home) && home[slen(home) - 1] != '\\') scat(home, "\\", sizeof home);
    scpy(ini, home, sizeof ini); scat(ini, "RD2.INI", sizeof ini);   /* im RD2-Verzeichnis wie RD2SETUP */
    ord[0] = (char)('@' + (si.bootlw ? si.bootlw : 3)); ord[1] = ':'; ord[2] = '\\'; ord[3] = 0;
    if (cmd && cmd[0] == 'T') { test = 1; cmd++; while (*cmd == ' ') cmd++; if (*cmd) scpy(ord, cmd, sizeof ord); }
    if (!(hm = MemAlloc(3L * CMAX, MEM_MOVABLE)) || !(ein = (char FAR *)MemLock(hm))) return 1;
    aus = ein + CMAX; ae = aus + CMAX;
    scpy(cs, ord, sizeof cs); scat(cs, "AUTOEXEC.BAT", sizeof cs); nae = lesen(cs, ae);
    scpy(cs, ord, sizeof cs); scat(cs, "CONFIG.SYS", sizeof cs); nein = lesen(cs, ein);
    n = (WORD)DialogRun(inst, 10, 0, dproc, 0);
    (void)n; (void)nein;
    MemUnlock(hm); MemFree(hm);
    return 0;
}
