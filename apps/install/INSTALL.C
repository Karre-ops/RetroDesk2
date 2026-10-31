/* INSTALL.EXE - RetroDesk 2 grafisch auf die Festplatte installieren (Variante B, 29.10.2026).
 * Die Werte (Grafik, Maus, Klang ...) kommen aus RD2SETUP und stehen in RD2.INI der Startdiskette.
 * Ablauf: Zielordner (Ebene fuer Ebene anlegen), Dateien der Disketten 1 bis 3 mit Fortschrittsbalken,
 * Diskettenwechsel per Meldung, RD2.INI, CONFIG.SYS (RD2 als Shell, Programme aus AUTOEXEC.BAT einzeln
 * abfragen, Sicherung CONFIG.RDA), Papierkorb, zuletzt Festplatte startfaehig (RDSYS.EXE per RunDos).
 * Gleiche Regeln wie die Text-Installation in RD2SETUP (Notweg, Taste T).
 * Befehlszeile "T": Testmodus (Ziel C:\RD2, alle Fragen mit Ja, meldet ueber DbgPrint, test/install.sh). */
#include <string.h>
#include "RD2.H"

#define ID_ZIEL   21
#define ID_SYS    22
#define ID_CFG    23
#define ID_FMT    24                              /* C: vorher formatieren (31.10.2026, Karsten) */
#define ID_BALKEN 41
#define ID_TEXT   42
#define PUF 32768U                                /* 30.10.: 32 KB statt 8 KB, weniger Umdrehungen je KB auf der Diskette; sonst 16/8 KB */
#define TEIL 1000                                 /* Balken: je Diskette 1000 Schritte */

static HINST hi; static BYTE test, cmd_f; static HWND dl;
static char quelle[80], ziel[64], pfad[96], z2[96];
static char FAR *pu; static HMEM hpu; static WORD puf;
static DWORD gesamt, fertig; static WORD dnr, ndat, nfehl, verloren;
static char buf[6144], neu[6144], pathz[128];

static const char *const muster[] = { "*.EXE", "*.MOD", "*.DRV", "*.FNT", "*.LNG", "*.BIN", "*.DAT", "*.HLP", "*.COM", "*.TXT", "COPYING", 0 };
static const char *const nicht[] = { "REM", "ECHO", "SET", "PATH", "PROMPT", "CLS", "GOTO", "IF", "CALL", "PAUSE", "FOR",
                                     "SHIFT", "VER", "CD", "CHDIR", "COMMAND", "WIN", "DOSSHELL", "EXIT", 0 };

/* ---- kleine Hilfen (kein sprintf: kein DOS aus der clib) ---------------------- */
static char *cat(char *d, const char FAR *s) { char *e = d + strlen(d); while (*s) *e++ = *s++; *e = 0; return d; }
static char *cpy(char *d, const char FAR *s) { *d = 0; return cat(d, s); }
static char *zahl(char *d, DWORD v) { char t[12]; int i = 11; t[i] = 0; do t[--i] = (char)('0' + v % 10); while (v /= 10); return cat(d, t + i); }
static int gibts(const char *p) { FINDDATA fd; return !FileFindFirst(p, FA_HIDDEN | FA_SYSTEM | FA_RDONLY, &fd); }
static void text(LPCSTR s) { DlgSetText(dl, ID_TEXT, s); UpdateWindow(DlgItem(dl, ID_TEXT)); if (test) DbgPrint(s); }   /* sofort zeichnen: beim Kopieren laeuft keine Nachrichtenschleife */
static void textn(WORD n) { text(StrGet(hi, n)); }
static void balken(void)
{
    DWORD p = (DWORD)dnr * TEIL + (gesamt ? fertig * TEIL / gesamt : 0);
    HWND b = DlgItem(dl, ID_BALKEN);
    SendMessage(b, C_SETPOS, 0, p); UpdateWindow(b);
}
static BOOL frage(LPCSTR s)                  /* Ja/Nein; Testmodus: Ja */
{
    DbgPrint("INSTALL: Frage"); DbgPrint(s);
    if (test) return TRUE;
    return MsgBox(dl, s, StrGet(hi, 1), MB_YESNO | MB_INFO) == ID_YES;
}
static void malen(void) { MSG m; while (PeekMessage(&m, 1)) DispatchMessage(&m); }   /* z. B. Rest der Meldung weg */
static void neustart(void)                        /* INI und DOS-Puffer schreiben, dann Warmstart (wie RDFDISK) */
{
    void (__far *reset)(void) = (void (__far *)(void))0xFFFF0000UL;
    TaskYield();                                  /* Kern schreibt den RD2.INI-Zwischenspeicher */
    DbgPrint("INSTALL: Neustart");
    _asm { mov ah, 0Dh
           int 21h }
    *(WORD __far *)0x00400072UL = 0x1234;         /* Warmstart: kein Speichertest */
    _asm { cli
           mov al, 0FEh
           out 64h, al }                          /* AT: Reset ueber den Tastaturcontroller; XT: Port frei */
    { WORD i; for (i = 0; i < 0xFFFF; i++) ; }
    reset();
}
static void meldung(WORD n, WORD art) { DbgPrint(StrGet(hi, n)); if (test) return; MsgBox(dl, StrGet(hi, n), StrGet(hi, 1), MB_OK | art); }

/* ---- Dateien -------------------------------------------------------------------- */
static BOOL kopieren(const char *von, const char *nach, DWORD gr)
{
    short a, b; WORD n; BOOL ok = TRUE;
    if ((a = FileOpen(von, FO_READ)) < 0) return FALSE;
    if ((b = FileCreate(nach, 0)) < 0) { FileClose(a); return FALSE; }
    while ((n = FileRead(a, pu, puf)) != 0 && n != 0xFFFF) {
        if (FileWrite(b, pu, n) != n) { ok = FALSE; break; }
        fertig += n; if (fertig > gesamt) fertig = gesamt; balken();
    }
    if (n == 0xFFFF) ok = FALSE;
    FileClose(a); if (FileClose(b)) ok = FALSE;
    (void)gr;
    return ok;
}
static void diskette(void)                        /* alle RD2-Dateien aus quelle nach ziel, Balken dnr */
{
    FINDDATA fd; WORD i, pass;
    gesamt = fertig = 0;
    for (pass = 0; pass < 2; pass++)              /* 1. Groesse zaehlen, 2. kopieren */
        for (i = 0; muster[i]; i++) {
            cpy(pfad, quelle); cat(pfad, muster[i]);
            if (FileFindFirst(pfad, 0, &fd)) continue;
            do {
                if (!pass) { gesamt += fd.groesse; continue; }
                cpy(pfad, quelle); cat(pfad, fd.name); cpy(z2, ziel); cat(z2, fd.name);
                { char t[48]; cpy(t, StrGet(hi, 3)); cat(t, fd.name); text(t); }
                if (kopieren(pfad, z2, fd.groesse)) ndat++; else { nfehl++; DbgPrint("INSTALL: Fehler"); DbgPrint(fd.name); }
            } while (!FileFindNext(&fd));
        }
    fertig = gesamt; balken();
}
/* Universalpaket (alles in einem Festplattenordner): Unterordner samt Inhalt, z. B. SDK von Diskette 4.
   q und z (pfad, z2) enden mit '\', werden erweitert und am Ende wieder hergestellt */
static void baum(char *q, char *z)
{
    FINDDATA fd; WORD lq = strlen(q), lz = strlen(z);
    z[lz - 1] = 0; DirMake(z); z[lz - 1] = '\\';
    cat(q, "*.*");
    if (!FileFindFirst(q, FA_DIR, &fd)) do {
        q[lq] = 0;
        if (fd.name[0] == '.') continue;
        cat(q, fd.name); cat(z, fd.name);
        if (fd.attr & FA_DIR) { cat(q, "\\"); cat(z, "\\"); baum(q, z); }
        else if (kopieren(q, z, fd.groesse)) ndat++; else { nfehl++; DbgPrint("INSTALL: Fehler"); DbgPrint(fd.name); }
        q[lq] = 0; z[lz] = 0;
    } while (!FileFindNext(&fd));
    q[lq] = 0;
}
static BOOL einlegen(const char *marke, WORD txt) /* Diskette mit der Datei marke in quelle; FALSE = auslassen */
{
    cpy(pfad, quelle); cat(pfad, marke);
    while (!gibts(pfad)) {
        DbgPrint(StrGet(hi, txt));
        if (MsgBox(dl, StrGet(hi, txt), StrGet(hi, 1), MB_OKCANCEL | MB_INFO) != ID_OK &&
            MsgBox(dl, StrGet(hi, 17), StrGet(hi, 1), MB_YESNO | MB_WARN) == ID_YES) { malen(); return FALSE; }
        malen();                                  /* Meldung weg, Balken wieder sichtbar */
    }
    return TRUE;
}
static BOOL ordner(void)                          /* Ebene fuer Ebene; ziel endet mit '\' */
{
    char d[64]; WORD i; FINDDATA fd;
    for (i = 3; ziel[i]; i++) if (ziel[i] == '\\') { memcpy(d, ziel, i); d[i] = 0; DirMake(d); }
    return !FileFindFirst(d, FA_DIR, &fd) && (fd.attr & FA_DIR);
}
static WORD lesen(const char *name)               /* Datei nach buf, 0-terminiert; 0xFFFF = zu gross */
{
    short f; WORD n;
    buf[0] = 0;
    if ((f = FileOpen(name, FO_READ)) < 0) return 0;
    n = FileRead(f, (char FAR *)buf, sizeof buf - 1); FileClose(f);
    if (n == 0xFFFF) n = 0;
    buf[n] = 0;
    return n == sizeof buf - 1 ? 0xFFFF : n;
}

/* ---- RD2.INI ---------------------------------------------------------------------- */
static void ini(void)
{
    char a[80], b[80];
    cpy(a, quelle); cat(a, "RD2.INI"); cpy(b, ziel); cat(b, "RD2.INI");
    if (!kopieren(a, b, 0)) nfehl++;
    ProfileSetStr(b, "System", "Installieren", "0");
}

/* ---- CONFIG.SYS (wie shell_eintragen in RD2SETUP) --------------------------------- */
static BOOL finden(const char *w, char *aus)      /* Programm: direkt, Wurzel, PATH der AUTOEXEC.BAT */
{
    static const char *const ext[] = { "", ".COM", ".EXE" };
    const char *pa = pathz; char d[64]; WORD i, l; BOOL pf = strchr(w, '\\') || strchr(w, ':');
    d[0] = ziel[0]; d[1] = ':'; d[2] = 0;
    for (;;) {
        for (i = 0; i < 3 && !(i && strchr(w, '.')); i++) {
            if (!pf) { cpy(aus, d); cat(aus, "\\"); cat(aus, w); }
            else if (w[1] == ':') cpy(aus, w); else { aus[0] = ziel[0]; aus[1] = ':'; aus[2] = 0; cat(aus, w); }
            cat(aus, ext[i]);
            if (gibts(aus)) { strupr(aus); return TRUE; }
        }
        if (pf || !*pa) return FALSE;
        l = (WORD)strcspn(pa, ";"); if (l > 60) l = 60;
        memcpy(d, pa, l); d[l] = 0; if (l && d[l - 1] == '\\') d[l - 1] = 0;
        pa += l; if (*pa) pa++;
    }
}
static short config(void)                         /* 1 eingetragen (alt gesichert), 3 neu, 2 schon da, -2 nein, -1 zu gross, 0 Fehler */
{
    char cs[16], ab[16], z[110], w[64], pf[80], mw[24]; WORD n, o = 0, m, i; char *a, *e, *q; BOOL alt, mausda = 0, kbda; short f;
    cpy(cs, "C:\\CONFIG.SYS"); cs[0] = ziel[0]; cpy(ab, "C:\\AUTOEXEC.BAT"); ab[0] = ziel[0];
    alt = gibts(cs) || gibts(ab); verloren = 0;
    if ((n = lesen(cs)) == 0xFFFF) return -1;
    strcpy(neu, buf); strupr(neu); kbda = strstr(neu, "KEYB") != 0;
    cpy(z, ziel); cat(z, "RD.EXE"); if (strstr(neu, z)) return 2;
    if (alt && !frage(StrGet(hi, 9))) return -2;
    if (n) { cpy(z, cs); z[strlen(z) - 3] = 0; cat(z, "RDA"); if (!kopieren(cs, z, 0)) return 0; lesen(cs); }
    #define NEU(t) do { m = (WORD)strlen(t); if (o + m >= sizeof neu) return -1; memcpy(neu + o, t, m); o += m; } while (0)
    for (a = buf; *a; a = e) {                    /* alte SHELL=-Zeile bleibt als REM stehen */
        char c;
        for (e = a; *e && *e != '\n'; e++) ;
        if (*e) e++;
        for (q = a; *q == ' ' || *q == '\t'; q++) ;
        if (!strnicmp(q, "SHELL", 5) && (q[5] == '=' || q[5] == ' ')) NEU("REM ");
        c = *e; *e = 0; NEU(a); *e = c;
    }
    if (o && neu[o - 1] != '\n') NEU("\r\n");
    cpy(z, "SHELL="); cat(z, ziel); cat(z, "RD.EXE /P "); cat(z, ziel); cat(z, "SHELL.EXE\r\n"); NEU(z);
    if (alt) {                                    /* Programme aus AUTOEXEC.BAT uebernehmen? */
        lesen(ab); pathz[0] = 0;
        for (a = buf; *a; a = e) {
            for (e = a; *e && *e != '\n'; e++) ;
            q = a; if (*e) *e++ = 0;
            q[strcspn(q, "\r")] = 0;
            while (*q == ' ' || *q == '\t' || *q == '@') q++;
            if (!*q || *q == ':' || *q == ';') continue;
            if (!strnicmp(q, "PATH", 4) && (q[4] == '=' || q[4] == ' ')) {   /* PATH fuer die Suche merken */
                char *p = q + 4; while (*p == ' ' || *p == '=') p++;
                strncpy(pathz, p, sizeof pathz - 1); pathz[sizeof pathz - 1] = 0;
            }
            for (i = 0; i < 2; i++) {             /* LH / LOADHIGH samt /L:-Schaltern ueberspringen */
                if (!strnicmp(q, "LH ", 3)) q += 3; else if (!strnicmp(q, "LOADHIGH ", 9)) q += 9; else break;
                while (*q == ' ' || *q == '/') { if (*q == '/') q += strcspn(q, " "); else q++; }
            }
            m = (WORD)strcspn(q, " \t="); if (!m || m >= sizeof w) continue;
            memcpy(w, q, m); w[m] = 0; strupr(w);
            for (i = 0; nicht[i] && stricmp(w, nicht[i]); i++) ;
            if (nicht[i]) { if (i == 3 || i == 2) verloren = 1; continue; }   /* SET/PATH gelten ohne COMMAND.COM nicht */
            if (strstr(w, "RD.EXE") || strstr(w, "RD2SETUP")) continue;
            if (!finden(w, pf)) continue;
            cpy(z, StrGet(hi, 10)); strncat(z, q, 60);
            if (!frage(z)) continue;
            if (strstr(pf, "MOUSE")) mausda = 1;
            if (strstr(pf, "KEYB")) kbda = 1;       /* eigener Treiber bleibt, kein RDKEYB */
            cpy(z, "INSTALL="); cat(z, pf); strncat(z, q + m, 60); cat(z, "\r\n"); NEU(z);
        }
    }
    cpy(z2, quelle); cat(z2, "RD2.INI");
    cpy(pf, ziel); cat(pf, "RDKEYB.COM");         /* Tastatur DE/US, gilt auch in DOS-Programmen */
    if (!kbda && gibts(pf)) { ProfileGetStr(z2, "System", "Tastatur", "DE", w, 8); cpy(z, "INSTALL="); cat(z, pf); cat(z, " "); cat(z, w); cat(z, "\r\n"); NEU(z); }
    ProfileGetStr(z2, "System", "Maus", "", mw, sizeof mw);
    if (!stricmp(mw, "DOS") || !mw[0]) ProfileGetStr(z2, "System", "MausHW", "", mw, sizeof mw);
    if (!mausda && stricmp(mw, "KEINE")) {        /* eigener Maustreiber RDMAUS mit dem Schalter der Auswahl */
        cpy(pf, ziel); cat(pf, "RDMAUS.COM"); w[0] = 0;
        if (!strnicmp(mw, "SERIELL:COM", 11)) { cpy(w, " /S"); w[3] = mw[11]; w[4] = 0; }
        else if (!strnicmp(mw, "MSYS:COM", 8)) { cpy(w, " /M"); w[3] = mw[8]; w[4] = 0; }
        else if (!stricmp(mw, "PS2")) cpy(w, " /P");
        else if (!strnicmp(mw, "BUS", 3)) cpy(w, " /B");
        else if (!stricmp(mw, "INPORT:238")) cpy(w, " /I2");
        else if (!strnicmp(mw, "INPORT", 6)) cpy(w, " /I1");
        if ((w[0] || !alt) && gibts(pf)) { cpy(z, "INSTALL="); cat(z, pf); cat(z, w); cat(z, "\r\n"); NEU(z); }
    }
    if ((f = FileCreate(cs, 0)) < 0) return 0;
    m = FileWrite(f, (char FAR *)neu, o);
    return !FileClose(f) && m == o ? (alt ? 1 : 3) : 0;
}

/* ---- Festplatte startfaehig: nur wenn C: kein System hat und KERNEL.SYS auf der Diskette liegt ---- */
static BOOL sys_moeglich(void)
{
    char p[20];
    if ((ziel[0] & 0xDF) != 'C' || (quelle[0] & 0xDF) > 'B') return FALSE;
    cpy(p, "C:\\KERNEL.SYS"); if (gibts(p)) return FALSE;
    cpy(p, "C:\\IO.SYS"); if (gibts(p)) return FALSE;
    cpy(p, "C:\\IBMBIO.COM"); if (gibts(p)) return FALSE;
    p[0] = quelle[0]; p[1] = ':'; p[2] = '\\'; p[3] = 0; cat(p, "KERNEL.SYS"); if (!gibts(p)) return FALSE;
    cpy(pfad, quelle); cat(pfad, "RDSYS.EXE");
    return gibts(pfad);
}

static BOOL lebt(HTASK t) { TASKINFO ti; HTASK h; for (h = TaskFirst(&ti); h; h = TaskNext(h, &ti)) if (h == t) return TRUE; return FALSE; }
static BOOL formatieren(void)                     /* C: mit RDFORMAT (Diskette 2) formatieren; TRUE = C: ist danach leer */
{
    HTASK t; FINDDATA fd;
    if (!frage(StrGet(hi, 25))) return FALSE;
    if (!einlegen("RDFORMAT.EXE", 4)) return FALSE;
    DbgPrint("INSTALL: RDFORMAT");
    if (!(t = TaskExec(pfad, "C:", SW_SHOW))) return FALSE;
    ShowWindow(dl, SW_HIDE);                       /* Tasten gehen an RDFORMAT, nicht an diesen Dialog */
    while (lebt(t)) {                              /* RDFORMAT fragt selbst noch einmal und zeigt den Fortschritt */
        MSG m;
        while (PeekMessage(&m, 1)) if ((m.msg & 0xFF00) != M_KEY && m.msg != M_COMMAND && (m.msg < M_MOUSEMOVE || m.msg > M_DBLCLK)) DispatchMessage(&m);
        TaskYield();
    }
    ShowWindow(dl, SW_SHOW); SetActive(dl); malen();
    einlegen("RD.EXE", 6);
    return FileFindFirst("C:\\*.*", FA_DIR | FA_HIDDEN | FA_SYSTEM | FA_RDONLY, &fd) != 0;   /* nichts mehr auf C: */
}
static void install2(void)
{
    WORD n; short c; char t[100]; BOOL sys;
    DlgGetText(dl, ID_ZIEL, ziel, sizeof ziel - 1); strupr(ziel);
    n = (WORD)strlen(ziel); if (n && ziel[n - 1] != '\\') cat(ziel, "\\");
    if (strlen(ziel) < 4 || ziel[1] != ':' || (ziel[0] & 0xDF) < 'C') { meldung(16, MB_ERROR); return; }
    if (!stricmp(ziel, quelle)) { meldung(8, MB_ERROR); return; }
    if (DlgChecked(dl, ID_FMT)) {
        EnableWindow(DlgItem(dl, ID_OK), FALSE); EnableWindow(DlgItem(dl, ID_CANCEL), FALSE);
        n = formatieren();
        EnableWindow(DlgItem(dl, ID_OK), TRUE); EnableWindow(DlgItem(dl, ID_CANCEL), TRUE);
        if (!n) { DbgPrint("INSTALL: nicht formatiert"); meldung(26, MB_WARN); return; }
        DbgPrint("INSTALL: formatiert"); DlgCheck(dl, ID_SYS, 1);   /* leere Platte: FreeDOS-Kern drauf */
    }
    if (!ordner()) { meldung(7, MB_ERROR); return; }
    sys = DlgChecked(dl, ID_SYS) && sys_moeglich();
    EnableWindow(DlgItem(dl, ID_OK), FALSE); EnableWindow(DlgItem(dl, ID_CANCEL), FALSE);
    CursorBusy(TRUE);
    ndat = nfehl = 0; dnr = 0;
    diskette();                                   /* Diskette 1 (oder alles, wenn die Quelle eine Festplatte ist) */
    if ((quelle[0] & 0xDF) > 'B') {               /* Festplattenordner: SDK (Diskette 4) mit, falls vorhanden */
        cpy(pfad, quelle); cat(pfad, "SDK");
        if (FileAttr(pfad, 0, 0) != 0xFFFF) { DbgPrint("INSTALL: SDK"); text("SDK"); cat(pfad, "\\"); cpy(z2, ziel); cat(z2, "SDK\\"); baum(pfad, z2); }
    }
    else {
        static const char *const mk[] = { "RDFDISK.EXE", "RDNET.EXE" };
        for (n = 0; n < 2; n++) {
            dnr = n + 1; balken();
            cpy(pfad, quelle); cat(pfad, mk[n]);
            if (gibts(pfad)) continue;            /* lag schon auf Diskette 1 (grosse Diskette) */
            CursorBusy(FALSE);
            if (einlegen(mk[n], 4 + n)) { CursorBusy(TRUE); diskette(); } else CursorBusy(TRUE);
        }
        CursorBusy(FALSE);
        einlegen("RD.EXE", 6);                    /* RD2 laeuft von Diskette 1: zurueck, bevor es weitergeht */
        CursorBusy(TRUE);
    }
    dnr = 3; gesamt = fertig = 0; balken();
    ini();
    c = DlgChecked(dl, ID_CFG) ? config() : -2;
    CursorBusy(FALSE);
    SendMessage(DlgItem(dl, ID_BALKEN), C_SETPOS, 0, 4L * TEIL);   /* fertig: Balken ganz voll */
    cpy(z2, ziel); cat(z2, "RD2.STA"); FileDelete(z2);   /* alter Fensterzustand der vorigen Installation weg */
    cpy(pfad, ziel); cat(pfad, "PAPIERK.EXE");    /* Papierkorb auf den Schreibtisch (nur neue PROGRAMM.LST) */
    cpy(z2, ziel); cat(z2, "PROGRAMM.LST");
    if (gibts(pfad) && !gibts(z2)) { ProfileSetStr(z2, "Papierk", "Datei", pfad); ProfileSetStr(z2, "Papierk", "Schreibtisch", "1"); }
    cpy(t, StrGet(hi, 11)); zahl(t, ndat); cat(t, StrGet(hi, nfehl ? 12 : 20));
    if (c == 1) cat(t, StrGet(hi, 19)); else if (c == 3) cat(t, StrGet(hi, 21)); else if (c == 0 || c == -1) cat(t, StrGet(hi, 22));
    text(t);
    DbgHex("INSTALL: Dateien", ndat); DbgHex("INSTALL: Fehler", nfehl); DbgHex("INSTALL: CONFIG", (WORD)c);
    if (verloren) meldung(15, MB_INFO);
    if (sys) {                                    /* zuletzt: RD2 verlaesst kurz die Grafik (ohne XMS: Neustart von RD2) */
        char a[8];
        meldung(13, MB_INFO); malen();
        a[0] = quelle[0]; a[1] = ':'; a[2] = '\\'; a[3] = ' '; a[4] = ziel[0]; a[5] = ':'; a[6] = 0;
        cpy(t, a); cat(t, " /UPDATE");
        cpy(pfad, quelle); cat(pfad, "RDSYS.EXE");
        DbgPrint("INSTALL: RDSYS");
        if (RunDos(pfad, t, RD_DOS_KEEP) == 0xFFFF && LastError() == RD_ERR_NOTSUP) {
            meldung(14, MB_INFO);                 /* ohne XMS: RD2 endet, RDSYS laeuft, RD2 startet neu */
            DbgPrint("INSTALL: fertig, Neustart");
            RunDos(pfad, t, 0);
        }
    }
    DbgPrint("INSTALL: fertig");
    DbgPrint(StrGet(hi, 24));
    if (!test && MsgBox(dl, StrGet(hi, 24), StrGet(hi, 1), MB_YESNO | MB_ASK) == ID_YES) neustart();
    EnableWindow(DlgItem(dl, ID_CANCEL), TRUE);
    DlgSetText(dl, ID_CANCEL, StrGet(hi, 23));
}

static void installieren(void) { static BYTE laeuft; if (laeuft) return; laeuft = 1; install2(); laeuft = 0; }   /* nicht doppelt (Enter waehrend RDFORMAT) */
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    (void)p2;
    if (msg == M_INITDIALOG) {
        dl = d;
        cpy(ziel, "C:\\RD2"); DlgSetText(d, ID_ZIEL, ziel);
        DlgCheck(d, ID_SYS, 1); DlgCheck(d, ID_CFG, 1);
        SendMessage(DlgItem(d, ID_BALKEN), C_SETRANGE, 0, 4 * TEIL);
        cpy(ziel, "C:\\RD2\\");
        if (!sys_moeglich()) { DlgCheck(d, ID_SYS, 0); EnableWindow(DlgItem(d, ID_SYS), FALSE); }
        if ((quelle[0] & 0xDF) > 'B') EnableWindow(DlgItem(d, ID_FMT), FALSE);   /* nur beim Start von Diskette: C: ist dann frei */
        if (test && cmd_f) DlgCheck(d, ID_FMT, 1);
        textn(2);
        if (test) PostMessage(d, M_COMMAND, ID_OK, 0);
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) { installieren(); if (test) DialogEnd(d, ID_CANCEL); return TRUE; }
    return FALSE;
}

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    SYSINFO si; WORD i;
    (void)show; hi = inst; test = (BYTE)(cmd && cmd[0] == 'T'); cmd_f = (BYTE)(test && cmd[1] == 'F');   /* Test "TF": mit Formatieren */
    KernInfo(&si);
    for (i = 0; si.home[i] && i < 66; i++) quelle[i] = si.home[i];
    quelle[i] = 0; if (i && quelle[i - 1] != '\\') cat(quelle, "\\");
    for (puf = PUF; puf >= 8192 && !(hpu = MemAlloc(puf, 0)); puf >>= 1) ;
    if (!hpu || !(pu = (char FAR *)MemLock(hpu))) { MsgBox(0, StrGet(inst, 18), StrGet(inst, 1), MB_OK | MB_ERROR); return 1; }
    DbgPrint("INSTALL: Start"); DbgPrint(quelle);
    DialogRun(inst, 10, 0, dproc, 0);
    MemUnlock(hpu); MemFree(hpu);
    return 0;
}
