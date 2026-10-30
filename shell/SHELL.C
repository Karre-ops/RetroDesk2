/* SHELL.C - RD2-Shell Teil 1: Schreibtisch mit Laufwerken, Ordnerfenster (Dateimanager),
 * Systemmenue "RD" (Info, Beenden), Programme starten (RD2 ueber TaskExec, DOS ueber RunDos).
 * Maus: Klick waehlt, Strg+Klick markiert dazu, Doppelklick oeffnet, rechte Taste: Menue "Datei".
 * Tastatur: Pfeile, Eingabe oeffnet, Rueckschritt eine Ebene hoeher, Einfg markiert, F5 Kopieren,
 * F6 Verschieben, F2 Umbenennen, F7 Neuer Ordner, Entf/F8 Loeschen, Alt+Leertaste "RD", F10 Leiste.
 * Teil 2 (24.10.2026): Dateioperationen, auch ganze Ordner; Ziel vorgeschlagen = anderes Ordnerfenster.
 * Teil 3: Ziehen mit der Maus auf ein Ordnerfenster, einen Ordner darin oder ein Laufwerk. Gleiches
 * Laufwerk = Verschieben, anderes = Kopieren; Strg erzwingt Kopieren, Umschalt Verschieben.
 * Rechtsklick auf ein Laufwerk: Oeffnen, Eigenschaften (Platz, Name), Formatieren (folgt mit RDDISK).
 * Meldungen auf COM1 fuer test/desk.sh: "SHELL: bereit", "SHELL: Ordner <pfad> <n>", "SHELL: Start". */
#include "RD2.H"

#define IC_FLOPPY 1
#define IC_HDISK  2
#define IC_ORDNER 3
#define IC_PROG   4
#define IC_DOK    5
#define CM_INFO   1
#define CM_ENDE   2
#define CM_ZUB    3                             /* 3..10: Zubehoer Uhr, Notizblock, Rechner, Optionen, Netzwerk, FTP, IRC, DOS-Fenster */
#define CM_OEFFNEN 101
#define CM_ZU     102
#define CM_HOCH   103
#define CM_KOPIE  104
#define CM_VERSCH 105
#define CM_UMBEN  106
#define CM_MKDIR  107
#define CM_LOESCH 108
#define CM_ANHEFT 109                             /* Programm anheften/loesen (Menue Programme) */
#define CM_PSTART 110                             /* Programmsymbol: starten */
#define CM_PWEG   111                             /* Programmsymbol vom Schreibtisch nehmen */
#define CM_PDESK  112                             /* Ordner: Programm auf den Schreibtisch */
#define CM_FEIG   113                             /* Datei-Eigenschaften (Alt+Eingabe) */
#define CM_SUCH   114                             /* Dateien suchen ab dem Ordner (Strg+F) */
#define CM_LSUCH  304                             /* Dateien suchen auf dem Laufwerk */
#define CM_DKOP   305                             /* Diskette kopieren (DKOPIE.EXE) */
#define CM_PRUEF  306                             /* Datentraeger pruefen (DPRUEF.EXE X:) */
#define CM_PRG    401                             /* 401..412: angeheftete Programme */
static WORD szeit; static BYTE slauf;                /* RD2.INI [Schoner] Zeit= Sekunden (0 = aus), laeuft schon */
#define CM_PLEER  413
#define MAXP      12
#define CM_LWINFO 301
#define CM_FORMAT 302
#define CM_FDISK  303                             /* Festplatte einrichten (RDFDISK; leere Platten haben keinen Buchstaben) */
#define CM_NEU    201
#define CM_SYMB   202                             /* Ansicht: Symbole */
#define CM_LISTE  203                             /* Ansicht: Liste (Name, Groesse, Datum) */
#define CM_VERST  204                             /* Ansicht: versteckte Dateien anzeigen (grau) */
#define CM_SORT   205                             /* 205..208: sortieren nach Name, Typ, Groesse, Datum */
#define MAXE      240                             /* Eintraege je Ordner */
#define MAXO      6                               /* Ordnerfenster */
static short ZB = 100;                            /* Zellenbreite (320 Punkte breit: 60) */

typedef struct { char name[13]; BYTE attr, mark; WORD dat; DWORD gr; } EINTRAG;
typedef struct { HWND w; char pfad[68]; HMEM h; short n, sel, oben, spalten, zeilen; } ORDNER;

static HINST inst; static HWND desk; static HMENU msys, mbar;
static short W, H, mh, th, ih, zh;                /* Schirm, Menueleiste, Textzeile, Symbol, Zellenhoehe */
static char lw[26], lt[26]; static short nlw, lsel;   /* Laufwerke: Buchstabe, Typ */
static ORDNER od[MAXO];
static char buf[200], qv[80], qn[80];            /* Text; Quelle und Ziel beim Kopieren */
static HWND letzt; static HMENU mlw, mps;
static char prg[MAXP][68], prgn[MAXP][9]; static BYTE nprg, psy[MAXP], nps, pm[MAXP], npm;   /* psy: Programme auf dem Schreibtisch */   /* PROGRAMM.LST: Datei, Abschnitt */
static char heim[66];                             /* RD2-Verzeichnis ohne \ am Ende, "" = Wurzel */
static BYTE wl[26];                               /* Wechsel je Laufwerk: 1 = eben gelesen, 2 = Abfrage aus (kein Signal / keine Diskette) */
static HWND zw; static short zx, zy; static BYTE zieht, liste;   /* liste: Listenansicht in allen Ordnern */   /* Ziehen: Quelle, Startpunkt, 1 = gedrueckt, 2 = zieht */
                                /* zuletzt aktives Ordnerfenster vor dem jetzigen */

/* ---- kleine Helfer ---------------------------------------------------------------------- */
static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, n - l); }
static char *zahl(char *p, DWORD v) { char t[11]; short i = 0; do t[i++] = (char)('0' + v % 10); while (v /= 10); while (i) *p++ = t[--i]; *p = 0; return p; }
static void fmt(char *d, LPCSTR t, LPCSTR a, LPCSTR b)   /* %s nacheinander durch a, b */
{
    WORD n = 0;
    while (*t && n < sizeof buf - 1) {
        if (t[0] == '%' && t[1] == 's') { LPCSTR s = a ? a : ""; a = b; b = 0; while (*s && n < sizeof buf - 1) d[n++] = *s++; t += 2; }
        else d[n++] = *t++;
    }
    d[n] = 0;
}
static int endung(LPCSTR n, LPCSTR e)             /* e gross, 3 Zeichen */
{
    while (*n && *n != '.') n++;
    return *n == '.' && (n[1] & 0xDF) == e[0] && (n[2] & 0xDF) == e[1] && (n[3] & 0xDF) == e[2] && !n[4];
}
static ORDNER *ord(HWND w) { short i; for (i = 0; i < MAXO; i++) if (od[i].w == w) return &od[i]; return 0; }
static WORD symbol(const EINTRAG FAR *e)
{
    if (e->attr & FA_DIR) return IC_ORDNER;
    return endung(e->name, "EXE") || endung(e->name, "COM") || endung(e->name, "BAT") ? IC_PROG : IC_DOK;
}
/* Beschriftung mittig unter dem Symbol, gewaehlt invertiert */
static void schild(HDC d, short x, short y, short b, LPCSTR t, int gew)
{
    RECT r; short tw = TextWidth(d, t, -1);
    r.l = x + (b - tw) / 2 - 2; r.r = r.l + tw + 4; r.t = y; r.b = y + th;
    SetBrush(d, gew ? C_BLACK : C_WHITE, PAT_SOLID); FillRect(d, &r);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, gew ? C_WHITE : C_BLACK);
    TextOut(d, r.l + 2, y, t, -1);
}

/* ---- Ordnerfenster ---------------------------------------------------------------------- */
static void rollen(ORDNER *o)
{
    short z = (o->n + o->spalten - 1) / o->spalten, m = z > o->zeilen ? z - o->zeilen : 0;
    if (o->oben > m) o->oben = m;
    ScrollSet(o->w, SB_VERT, o->oben, m, o->zeilen);
}
static void masse(ORDNER *o, short b, short h)
{
    o->spalten = liste ? 1 : b / ZB; if (o->spalten < 1) o->spalten = 1;
    o->zeilen = h / (liste ? th + 2 : zh); if (o->zeilen < 1) o->zeilen = 1;
    rollen(o);
}
static BYTE verst;                                /* versteckte/System-Dateien zeigen (RD2.STA liste Bit 1) */
static void grau_ueber(HDC d, short l, short t, short rr, short b)   /* versteckt: 50 % aufhellen */
{
    RECT q; q.l = l; q.t = t; q.r = rr; q.b = b;
    SetRop(d, ROP_OR); SetBkColor(d, C_BLACK); SetBrush(d, C_WHITE, PAT_GRAY50); FillRect(d, &q); SetRop(d, ROP_COPY);
}
static int vor(LPCSTR a, LPCSTR b)                /* a vor b, ohne Gross/Klein */
{
    while (*a && (*a | 0x20) == (*b | 0x20)) a++, b++;
    return (BYTE)(*a | 0x20) < (BYTE)(*b | 0x20);
}
static void zeile(HDC d, short y, short b, const EINTRAG FAR *e, int gew)   /* Listenansicht */
{
    RECT r; char t[12], *p; short w;
    r.l = 0; r.r = b; r.t = y; r.b = y + th + 2;
    SetBrush(d, gew ? C_BLACK : C_WHITE, PAT_SOLID); FillRect(d, &r);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, gew ? C_WHITE : C_BLACK);
    { char n[16]; short i; n[0] = e->mark ? '*' : ' '; for (i = 0; i < 13; i++) n[i + 1] = e->name[i];
      if (e->attr & FA_DIR) scat(n, "\\", sizeof n); TextOut(d, 2, y + 1, n, -1); }
    if (!(e->attr & FA_DIR)) { zahl(t, e->gr); w = TextWidth(d, t, -1); TextOut(d, 196 - w, y + 1, t, -1); }
    if (e->name[0] == '.') return;
    p = zahl(t, 100 + (e->dat & 31)); t[0] = '.';   /* TT.MM.JJJJ aus dem DOS-Datum */
    TextOut(d, 206, y + 1, t + 1, -1); w = 206 + TextWidth(d, t + 1, -1);
    zahl(t, 100 + ((e->dat >> 5) & 15)); t[0] = '.'; TextOut(d, w, y + 1, t, -1); w += TextWidth(d, t, -1);
    p = t; *p++ = '.'; zahl(p, 1980 + (e->dat >> 9)); TextOut(d, w, y + 1, t, -1);
}
static BYTE sortart;                              /* 0 Name, 1 Typ, 2 Groesse, 3 Datum (neueste zuerst); RD2.STA liste Bit 2..3 */
static LPCSTR typ(LPCSTR n) { while (*n && *n != '.') n++; return n; }
static int vorher(const EINTRAG *t, const EINTRAG FAR *e)   /* t vor e? Ordner immer nach Namen */
{
    if (!(t->attr & FA_DIR)) switch (sortart) {
    case 1: { LPCSTR a = typ(t->name), b = typ(e->name); short i;
              for (i = 0; a[i] && (a[i] | 0x20) == (b[i] | 0x20); i++) ;
              if (a[i] != b[i]) return vor(a, b); break; }
    case 2: if (t->gr != e->gr) return t->gr < e->gr; break;
    case 3: if (t->dat != e->dat) return t->dat > e->dat; break;
    }
    return vor(t->name, e->name);
}
static void sortieren(EINTRAG FAR *e, short n)   /* Einfuegen; Ordner bleiben vorn */
{
    short i, j; EINTRAG t;
    for (i = 1; i < n; i++) {
        t = e[i];
        for (j = i; j > 0 && ((e[j - 1].attr ^ t.attr) & FA_DIR ? 0 : vorher(&t, &e[j - 1])); j--) e[j] = e[j - 1];
        e[j] = t;
    }
}
static void sortwahl(BYTE a)                      /* alle Ordnerfenster neu ordnen, ohne neu zu lesen */
{
    short i; EINTRAG FAR *e;
    sortart = a; for (i = 0; i < 4; i++) MenuCheck(mbar, CM_SORT + i, i == a);
    DbgHex("SHELL: Sortierung", a);
    for (i = 0; i < MAXO; i++) if (od[i].w && od[i].h && (e = (EINTRAG FAR *)MemLock(od[i].h)) != 0) {
        sortieren(e, od[i].n);
        if (od[i].n) { DbgPrint(e[0].name); DbgPrint(e[od[i].n - 1].name); }
        MemUnlock(od[i].h); od[i].sel = od[i].n ? 0 : -1; od[i].oben = 0; rollen(&od[i]); InvalidateRect(od[i].w, 0, TRUE);
    }
}
static WORD einlesen(ORDNER *o)                   /* 0 = gelesen, sonst Fehler (z. B. keine Diskette) */
{
    FINDDATA f; EINTRAG FAR *e; short n = 0, pass; WORD r = 0, x;
    CursorBusy(TRUE);
    if (!o->h) o->h = MemAlloc((DWORD)MAXE * sizeof(EINTRAG), MEM_MOVABLE);
    if (o->h && (e = (EINTRAG FAR *)MemLock(o->h)) != 0) {
        scpy(buf, o->pfad, sizeof buf); scat(buf, "*.*", sizeof buf);
        for (pass = 0; pass < 2; pass++)          /* erst Ordner, dann Dateien */
            if ((x = FileFindFirst(buf, (WORD)(FA_DIR | (verst ? FA_HIDDEN | FA_SYSTEM : 0)), &f)) != 0) { if (x != RD_ERR_NOTFOUND) r = x; }
            else do {
                if ((f.attr & FA_LABEL) || (f.name[0] == '.' && !f.name[1])) continue;
                if (((f.attr & FA_DIR) != 0) != (pass == 0) || n >= MAXE) continue;
                { short i; for (i = 0; i < 13; i++) e[n].name[i] = f.name[i]; }
                e[n].mark = 0; e[n].dat = f.datum; e[n].gr = f.groesse; e[n++].attr = f.attr;
            } while (!FileFindNext(&f));
        sortieren(e, n);
        if (n) { DbgPrint(e[0].name); DbgPrint(e[n - 1].name); }   /* test/liste.sh: Sortierung */
        MemUnlock(o->h);
    }
    o->n = n; o->sel = n ? 0 : -1; o->oben = 0;
    CursorBusy(FALSE);
    SetWindowText(o->w, o->pfad);
    rollen(o);
    InvalidateRect(o->w, 0, TRUE);
    { char t[8]; zahl(t, n); fmt(buf, "SHELL: Ordner %s %s", o->pfad, t); DbgPrint(buf); }
    return r;
}
static void sichtbar(ORDNER *o)
{
    short z = o->sel / o->spalten;
    if (o->sel < 0) return;
    if (z < o->oben) o->oben = z; else if (z >= o->oben + o->zeilen) o->oben = z - o->zeilen + 1;
    rollen(o);
}
static void sta_schreiben(void);
static void wechsel(char l);
static WORD werkzeug(char *p, LPCSTR n);
static void starten(LPCSTR p)
{
    BYTE dos;
    DbgPrint("SHELL: Start"); DbgPrint(p);
    {   static const char tx[] = "TXT\0INI\0DOC\0NFO\0DIZ\0LOG\0";   /* Textdateien: RDEDIT (Programmdiskette), sonst Notizblock */
        char n[80]; WORD i;
        for (i = 0; i < sizeof tx - 1; i += 4) if (endung(p, tx + i) && ((werkzeug(n, "RDEDIT.EXE") && TaskExec(n, p, SW_SHOW)) || (werkzeug(n, "NOTIZ.EXE") && TaskExec(n, p, SW_SHOW)))) return; }
    if (endung(p, "EXE") && TaskExec(p, "", SW_SHOW)) return;
    dos = !endung(p, "EXE") || LastError() == RD_ERR_FORMAT;   /* merken: sta_schreiben aendert LastError */
    if (dos) sta_schreiben();                     /* vor RunDos (Diskettenbetrieb: Neustart) */
    if (!dos || (RunDos(p, "", RD_DOS_KEEP | RD_DOS_PAUSE) == 0xFFFF &&   /* 0xFFFF = Fehler; zurueck nur mit */
        (LastError() != RD_ERR_NOTSUP || RunDos(p, "", RD_DOS_PAUSE) == 0xFFFF))) {   /* XMS/Platte, sonst Neustart */
        char t[8];                                /* Speichermangel mit freiem Speicher melden (Checkliste 1) */
        WORD f = LastError();
        zahl(t, MemAvail(MEMA_KONV) >> 10);
        fmt(buf, StrGet(inst, f == RD_ERR_NOMEM ? 20 : 3), p, t);
        DbgHex("SHELL: Startfehler", f);
        if (f == RD_ERR_NOMEM) DbgPrint("SHELL: zu wenig Speicher");
        MsgBox(0, buf, "RD", MB_OK | MB_ERROR);
    }
}
static void oeffnen(ORDNER *o)
{
    EINTRAG FAR *e; char n[13]; BYTE a; short i;
    if (o->sel < 0 || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    for (i = 0; i < 13; i++) n[i] = e[o->sel].name[i];
    a = e[o->sel].attr; MemUnlock(o->h);
    if (a & FA_DIR) {
        if (n[0] == '.') {                        /* ".." : eine Ebene hoeher */
            i = (short)slen(o->pfad) - 1;
            while (i > 0 && o->pfad[i - 1] != '\\') i--;
            o->pfad[i] = 0;
        } else { scat(o->pfad, n, sizeof o->pfad); scat(o->pfad, "\\", sizeof o->pfad); }
        einlesen(o);
        return;
    }
    scpy(buf, o->pfad, sizeof buf); scat(buf, n, sizeof buf);
    starten(buf);
}
static void hoch(ORDNER *o)
{
    short i = (short)slen(o->pfad) - 1;
    if (i <= 2) return;                           /* "A:\" */
    while (i > 0 && o->pfad[i - 1] != '\\') i--;
    o->pfad[i] = 0; einlesen(o);
}
static void zellrect(ORDNER *o, short i, RECT *r)  /* Client-Rechteck des Eintrags i */
{
    if (liste) { RECT c; GetClientRect(o->w, &c); r->l = 0; r->r = c.r; r->t = (i - o->oben) * (th + 2); r->b = r->t + th + 2; }
    else { r->l = (i % o->spalten) * ZB; r->r = r->l + ZB; r->t = (i / o->spalten - o->oben) * zh; r->b = r->t + zh; }
}
static void zelle_neu(ORDNER *o, short i) { RECT r; zellrect(o, i, &r); InvalidateRect(o->w, &r, FALSE); }
static void neusel(ORDNER *o, short s)
{
    if (s < 0 || s >= o->n || s == o->sel) return;
    { short alt = o->sel, ob = o->oben;
      o->sel = s; sichtbar(o);
      if (o->oben != ob || alt < 0) { InvalidateRect(o->w, 0, FALSE); return; }   /* gerollt: alles */
      zelle_neu(o, alt); UpdateWindow(o->w); zelle_neu(o, s); }   /* sonst nur alte und neue Zelle */
}
/* ---- Dateioperationen ------------------------------------------------------------------ */
static void fehler(LPCSTR was, WORD e)
{
    char t[8]; zahl(t, e == RD_ERR_NOMEM ? MemAvail(MEMA_KONV) >> 10 : e); fmt(buf, StrGet(inst, e == RD_ERR_NOMEM ? 20 : 6), was, t); MsgBox(GetActive(), buf, "RD", MB_OK | MB_ERROR);
}
static int praefix(LPCSTR a, LPCSTR b) { while (*a && (*a | 0x20) == (*b | 0x20)) a++, b++; return !*a && (*b == '\\' || !*b); }
/* Systemdateien: Startdateien in der Wurzel, alles mit Attribut System, das RD2-Verzeichnis samt Inhalt
 * und die Ordner darueber. Loeschen, Ersetzen, Verschieben und Umbenennen fragen dann noch einmal (Nein vorgewaehlt). */
static const char sysn[] = "CONFIG.SYS\0AUTOEXEC.BAT\0KERNEL.SYS\0IO.SYS\0MSDOS.SYS\0IBMBIO.COM\0IBMDOS.COM\0COMMAND.COM\0RD.EXE\0RD2.INI\0";
static int gleichi(LPCSTR a, LPCSTR b) { while (*a && (*a | 0x20) == (*b | 0x20)) a++, b++; return !*a && !*b; }
static int wichtig(LPCSTR p, WORD attr)
{
    LPCSTR n, s;
    if (attr & FA_SYSTEM) return 1;
    if (heim[0] && (praefix(heim, p) || praefix(p, heim))) return 1;
    for (n = p, s = p; *s; s++) if (*s == '\\') n = s + 1;
    if (n != p + 3) return 0;                     /* nicht in der Wurzel */
    for (s = sysn; *s; s += slen(s) + 1) if (gleichi(n, s)) return 1;
    return 0;
}
static int sicher(LPCSTR p, WORD attr, WORD frage)   /* 1 = weitermachen */
{
    LPCSTR n, s;
    if (!wichtig(p, attr)) return 1;
    for (n = p, s = p; *s; s++) if (*s == '\\') n = s + 1;
    DbgPrint("SHELL: Systemwarnung");
    fmt(buf, StrGet(inst, 15), n, 0); scat(buf, StrGet(inst, frage), sizeof buf);
    return MsgBox(GetActive(), buf, "RD", MB_YESNO | MB_WARN | MB_DEFNO) == ID_YES;
}
/* qv nach qn kopieren (Datei oder ganzer Ordner), bei weg danach Quelle loeschen. 0 = ok, 1 = Abbruch */
static WORD kop(BYTE attr, int weg)
{
    WORD e = 0, lv = slen(qv), ln = slen(qn), n; FINDDATA f; short a, b; HMEM hb; BYTE FAR *p;
    if (attr & FA_DIR) {
        if (praefix(qv, qn)) return RD_ERR_FORMAT;      /* Ordner in sich selbst */
        if (FileAttr(qn, 0, 0) == 0xFFFF && (e = DirMake(qn)) != 0) return e;
        scat(qv, "\\*.*", sizeof qv);
        if (!FileFindFirst(qv, FA_DIR | FA_HIDDEN | FA_SYSTEM, &f)) do {
            if (f.name[0] == '.' || (f.attr & FA_LABEL)) continue;
            qv[lv] = '\\'; scpy(qv + lv + 1, f.name, sizeof qv - lv - 1);
            qn[ln] = '\\'; scpy(qn + ln + 1, f.name, sizeof qn - ln - 1);
            e = kop(f.attr, weg);
            qv[lv] = 0; qn[ln] = 0;
            if (e) break;                         /* Suchzustand liegt in f, qv braucht kein Muster mehr */
        } while (!FileFindNext(&f));
        qv[lv] = 0;
        if (!e && weg) e = DirRemove(qv);
        return e;
    }
    if ((n = FileAttr(qn, 0, 0)) != 0xFFFF) {     /* Ziel gibt es schon */
        fmt(buf, StrGet(inst, 12), qn, 0);
        a = MsgBox(GetActive(), buf, "RD", MB_YESNOCANCEL | MB_ASK);
        if (a == ID_CANCEL) return 1;
        if (a != ID_YES || !sicher(qn, n, 17)) return 0;
        FileAttr(qn, 0, 1);
    }
    if (!(hb = MemAlloc(4096, MEM_MOVABLE))) return RD_ERR_NOMEM;
    if ((a = FileOpen(qv, FO_READ)) < 0) { MemFree(hb); return LastError(); }
    if ((b = FileCreate(qn, 0)) < 0) { e = LastError(); FileClose(a); MemFree(hb); return e; }
    p = (BYTE FAR *)MemLock(hb);
    SetLastError(0);                              /* FileRead liefert auch bei Lesefehler 0: danach pruefen */
    while ((n = FileRead(a, p, 4096)) > 0) if (FileWrite(b, p, n) != n) { e = RD_ERR_FULL; break; }
    if (!e && LastError()) e = LastError();       /* Lesefehler: Kopie verwerfen, Quelle behalten */
    MemUnlock(hb); MemFree(hb);
    FileClose(a);
    if (FileClose(b) && !e) e = LastError() ? LastError() : RD_ERR_DISK;
    if (e) { FileDelete(qn); return e; }
    if (attr & (FA_RDONLY | FA_HIDDEN | FA_SYSTEM)) FileAttr(qn, attr & 7, 1);
    if (weg) { if (attr & FA_RDONLY) FileAttr(qv, 0, 1); e = FileDelete(qv); }
    return e;
}
static void alle_neu(void) { short i; for (i = 0; i < MAXO; i++) if (od[i].w) einlesen(&od[i]); }
static short anzahl(ORDNER *o, EINTRAG FAR *e)    /* markierte, sonst die Auswahl */
{
    short i, n = 0;
    for (i = 0; i < o->n; i++) if (e[i].mark) n++;
    return n ? n : (o->sel >= 0 && e[o->sel].name[0] != '.');
}
static int betrifft(ORDNER *o, EINTRAG FAR *e, short i, short n) { return e[i].name[0] != '.' && (n ? e[i].mark : i == o->sel); }
static void kopieren(ORDNER *o, int weg, LPCSTR wohin)   /* wohin 0: fragen */
{
    EINTRAG FAR *e; short i, m, mk = 0; WORD r = 0; char ziel[68]; ORDNER *z = ord(letzt);
    if (!o->h || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    for (i = 0; i < o->n; i++) mk += e[i].mark;
    if (!anzahl(o, e)) { MemUnlock(o->h); return; }
    if (wohin) scpy(ziel, wohin, sizeof ziel);
    else {
        scpy(ziel, z && z != o ? z->pfad : o->pfad, sizeof ziel);
        if (!InputBox(GetActive(), StrGet(inst, weg ? 11 : 10), "RD", ziel, sizeof ziel)) { MemUnlock(o->h); return; }
    }
    m = (short)slen(ziel); if (m && ziel[m - 1] != '\\') scat(ziel, "\\", sizeof ziel);
    if (praefix(o->pfad, ziel) && slen(o->pfad) == slen(ziel)) { MemUnlock(o->h); return; }   /* derselbe Ordner */
    if (weg) for (i = 0; i < o->n; i++) if (betrifft(o, e, i, mk)) {
        scpy(qv, o->pfad, sizeof qv); scat(qv, e[i].name, sizeof qv);
        if (!sicher(qv, e[i].attr, 18)) { MemUnlock(o->h); return; }
    }
    CursorBusy(TRUE);
    for (i = 0; i < o->n && !r; i++) if (betrifft(o, e, i, mk)) {
        scpy(qv, o->pfad, sizeof qv); scat(qv, e[i].name, sizeof qv);
        scpy(qn, ziel, sizeof qn); scat(qn, e[i].name, sizeof qn);
        if (weg && (qv[0] | 0x20) == (qn[0] | 0x20) && !(e[i].attr & FA_DIR) && !FileRename(qv, qn)) continue;
        DbgPrint(qn);
        if ((r = kop(e[i].attr, weg)) > 1) fehler(e[i].name, r);
    }
    MemUnlock(o->h);
    CursorBusy(FALSE);
    DbgPrint(weg ? "SHELL: verschoben" : "SHELL: kopiert");
    alle_neu();
}
static WORD loesche(BYTE attr)                    /* qv samt Inhalt */
{
    WORD e = 0, lv = slen(qv); FINDDATA f;
    if (attr & FA_DIR) {
        scat(qv, "\\*.*", sizeof qv);
        if (!FileFindFirst(qv, FA_DIR | FA_HIDDEN | FA_SYSTEM, &f)) do {
            if (f.name[0] == '.' || (f.attr & FA_LABEL)) continue;
            qv[lv] = '\\'; scpy(qv + lv + 1, f.name, sizeof qv - lv - 1);
            e = loesche(f.attr); qv[lv] = 0;
            if (e) break;
        } while (!FileFindNext(&f));
        qv[lv] = 0;
        return e ? e : DirRemove(qv);
    }
    if (attr & (FA_RDONLY | FA_HIDDEN | FA_SYSTEM)) FileAttr(qv, 0, 1);
    return FileDelete(qv);
}
/* Papierkorb (nur Festplatte): Datei nach X:\PAPIERK\ umbenennen, PAPIERK.LST "Name|Originalpfad" anhaengen.
   Ordner und Umschalt+Entf loeschen sofort. PAPIERK.EXE zeigt, stellt wieder her, leert. */
static BYTE endg;
static BYTE platte(char l) { short i; for (i = 0; i < nlw; i++) if (lw[i] == l) return lt[i] == DT_HARD; return 0; }
static WORD papier(LPCSTR name)                   /* qv = Datei */
{
    char n[13]; short f, k; WORD r;
    qn[0] = qv[0]; qn[1] = ':'; qn[2] = 0; scat(qn, "\\PAPIERK", sizeof qn); DirMake(qn); scat(qn, "\\", sizeof qn);
    scpy(n, name, sizeof n); k = (short)slen(qn); scat(qn, n, sizeof qn);
    for (f = 0; f < 36 && FileFindFirst(qn, FA_HIDDEN | FA_SYSTEM | FA_RDONLY | FA_ARCH, (FINDDATA *)buf) == 0; f++) qn[k] = (char)(f < 10 ? '0' + f : 'A' + f - 10);
    if ((r = FileRename(qv, qn)) != 0) return r;
    scpy(n, qn + k, sizeof n);
    qn[k] = 0; scat(qn, "PAPIERK.LST", sizeof qn);
    if ((f = FileOpen(qn, FO_RW)) < 0 && (f = FileCreate(qn, 0)) < 0) return RD_ERR_DISK;
    FileSeek(f, 0, 2); FileWrite(f, n, slen(n)); FileWrite(f, "|", 1); FileWrite(f, qv, slen(qv)); FileWrite(f, "\r\n", 2); FileClose(f);
    return 0;
}
static void loeschen(ORDNER *o)
{
    EINTRAG FAR *e; short i, n, mk = 0; WORD r = 0; char t[8]; BYTE pk = (BYTE)(!endg && platte(o->pfad[0]));
    if (!o->h || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    for (i = 0; i < o->n; i++) mk += e[i].mark;
    if ((n = anzahl(o, e)) == 0) { MemUnlock(o->h); return; }
    zahl(t, n); fmt(buf, StrGet(inst, pk ? 39 : 7), t, 0);
    if (MsgBox(GetActive(), buf, "RD", MB_YESNO | MB_WARN) != ID_YES) { MemUnlock(o->h); return; }
    for (i = 0; i < o->n; i++) if (betrifft(o, e, i, mk)) {
        scpy(qv, o->pfad, sizeof qv); scat(qv, e[i].name, sizeof qv);
        if (!sicher(qv, e[i].attr, 16)) { MemUnlock(o->h); return; }
    }
    CursorBusy(TRUE);
    for (i = 0; i < o->n && !r; i++) if (betrifft(o, e, i, mk)) {
        scpy(qv, o->pfad, sizeof qv); scat(qv, e[i].name, sizeof qv);
        if ((r = (pk && !(e[i].attr & FA_DIR)) ? papier(e[i].name) : loesche(e[i].attr)) != 0) fehler(e[i].name, r);
    }
    MemUnlock(o->h);
    CursorBusy(FALSE);
    DbgPrint(pk ? "SHELL: Papierkorb" : "SHELL: geloescht");
    alle_neu();
}
static void umbenennen(ORDNER *o)
{
    EINTRAG FAR *e; char n[13]; WORD r, at;
    if (o->sel < 0 || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    scpy(n, e[o->sel].name, sizeof n); at = e[o->sel].attr; MemUnlock(o->h);
    if (n[0] == '.') return;
    scpy(qv, o->pfad, sizeof qv); scat(qv, n, sizeof qv);
    if (!sicher(qv, at, 19)) return;
    if (!InputBox(GetActive(), StrGet(inst, 8), "RD", n, sizeof n) || !n[0]) return;
    scpy(qn, o->pfad, sizeof qn); scat(qn, n, sizeof qn);
    if ((r = FileRename(qv, qn)) != 0) fehler(n, r);
    DbgPrint("SHELL: umbenannt");
    alle_neu();
}
/* ---- Datei-Eigenschaften: Name, Groesse, Datum/Zeit, Attribute (Schreibgeschuetzt, Versteckt, System, Archiv) ---- */
static char ez[3][48]; static WORD eat;
static BOOL RDCALLBACK eproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD i;
    (void)p2;
    if (msg == M_INITDIALOG) {
        for (i = 0; i < 3; i++) DlgSetText(d, (WORD)(41 + i), ez[i]);
        for (i = 0; i < 4; i++) DlgCheck(d, (WORD)(31 + i), (eat & (i == 3 ? FA_ARCH : 1 << i)) != 0);
    }
    if (msg == M_COMMAND && p1 == ID_OK)
        for (eat &= FA_DIR, i = 0; i < 4; i++) if (DlgChecked(d, (WORD)(31 + i))) eat |= i == 3 ? FA_ARCH : 1 << i;
    return FALSE;
}
static void z2(char *p, WORD v) { p[0] = (char)('0' + v / 10 % 10); p[1] = (char)('0' + v % 10); }
static void eigenschaften(ORDNER *o)
{
    EINTRAG FAR *e; FINDDATA fd; char *p; WORD at, r;
    if (o->sel < 0 || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    scpy(buf, e[o->sel].name, 13); MemUnlock(o->h);
    if (buf[0] == '.') return;
    scpy(qv, o->pfad, sizeof qv); scat(qv, buf, sizeof qv);
    if (FileFindFirst(qv, FA_RDONLY | FA_HIDDEN | FA_SYSTEM | FA_DIR | FA_ARCH, &fd)) { fehler(buf, LastError()); return; }
    scpy(ez[0], StrGet(inst, 32), sizeof ez[0]); scat(ez[0], qv, sizeof ez[0]);
    scpy(ez[1], StrGet(inst, 33), sizeof ez[1]);
    if (fd.attr & FA_DIR) scat(ez[1], StrGet(inst, 31), sizeof ez[1]);
    else { p = ez[1]; while (*p) p++; p = zahl(p, fd.groesse); scpy(p, " Bytes", 8); }
    scpy(ez[2], StrGet(inst, 34), sizeof ez[2]); p = ez[2]; while (*p) p++;
    z2(p, fd.datum & 31); p[2] = '.'; z2(p + 3, (fd.datum >> 5) & 15); p[5] = '.'; p = zahl(p + 6, 1980 + (fd.datum >> 9));
    *p++ = ' '; z2(p, fd.zeit >> 11); p[2] = ':'; z2(p + 3, (fd.zeit >> 5) & 63); p[5] = 0;
    eat = at = fd.attr;
    DbgPrint("SHELL: Eigenschaften"); DbgPrint(ez[1]); DbgPrint(ez[2]);
    if (DialogRun(inst, 20, GetActive(), eproc, 0) != ID_OK || eat == at) return;
    if ((at & FA_SYSTEM) && !sicher(qv, at, 19)) return;
    if ((r = FileAttr(qv, (WORD)(eat & ~FA_DIR), 1)) == 0xFFFF) {   /* liefert sonst die Attribute */ MsgBox(GetActive(), StrGet(inst, 35), "RD", MB_OK | MB_WARN); DbgHex("SHELL: Attribut Fehler", r); return; }
    DbgHex("SHELL: Attribute", eat);
    alle_neu();
}
static ORDNER *ordner_auf(char l, LPCSTR pfad, RECT *wr);
/* ---- Dateien suchen: Muster mit * und ? (8.3), ab einem Ordner durch alle Unterordner (8 Ebenen), 100 Treffer ---- */
#define SMAX 100
#define SLEN 66
static HMEM sh; static WORD sn; static short ssel; static char sm[13], sp[SLEN + 14];
static BYTE teil(const char *n, const char *m, WORD k)   /* Namens- oder Endungsteil, hoechstens k Zeichen */
{
    WORD i;
    for (i = 0; i < k; i++) {
        if (*m == '*') return 1;
        if (!*m) return (BYTE)!*n;
        if (*m == '?') { if (*n) n++; m++; continue; }
        if (!*n || (*n & 0xDF) != (*m & 0xDF)) return 0;
        n++; m++;
    }
    return 1;
}
static BYTE passt(const char *n)                  /* n gegen sm (beide NAME.EXT) */
{
    char a[13], b[13], *q; const char *e1, *e2;
    scpy(a, n, 13); scpy(b, sm, 13);
    q = a; while (*q && *q != '.') q++; e1 = *q ? q + 1 : q; *q = 0;
    q = b; while (*q && *q != '.') q++; e2 = *q ? q + 1 : "*"; if (!*q && b[0] == '*') e2 = "*"; else if (!*q) e2 = ""; *q = 0;
    return teil(a, b, 8) && teil(e1, e2, 3);
}
static void durch(WORD l, WORD tief)              /* sp[0..l) = Ordner mit \ am Ende */
{
    FINDDATA f; char FAR *t; WORD k;
    sp[l] = 0; scat(sp, "*.*", sizeof sp);
    if (FileFindFirst(sp, FA_DIR | FA_HIDDEN | FA_SYSTEM | FA_RDONLY | FA_ARCH, &f)) return;
    do {
        if (f.name[0] == '.' || sn >= SMAX) continue;
        sp[l] = 0;
        if (passt(f.name) && (t = (char FAR *)MemLock(sh)) != 0) {
            t += sn * SLEN; for (k = 0; k < l && k < SLEN - 13; k++) t[k] = sp[k];
            for (t += k, k = 0; f.name[k]; k++) t[k] = f.name[k];
            t[k] = (char)((f.attr & FA_DIR) ? '\\' : 0); t[k + (t[k] != 0)] = 0;
            MemUnlock(sh); sn++;
        }
        if ((f.attr & FA_DIR) && tief < 8 && l + 13 < SLEN) {
            sp[l] = 0; scat(sp, f.name, sizeof sp); k = (WORD)slen(sp); sp[k++] = '\\'; sp[k] = 0;
            durch(k, (WORD)(tief + 1));
            sp[l] = 0; scat(sp, "*.*", sizeof sp);   /* DTA gehoert f, Suche geht weiter */
        }
    } while (!FileFindNext(&f));
}
static BOOL RDCALLBACK sproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    HWND l = DlgItem(d, 40); char FAR *t; WORD i;
    if (msg == M_INITDIALOG) {
        DlgSetText(d, 41, buf);
        if ((t = (char FAR *)MemLock(sh)) != 0) { for (i = 0; i < sn; i++) SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)(t + i * SLEN)); MemUnlock(sh); }
        if (sn) SendMessage(l, L_SETSEL, 0, 0);
        SetFocus(l);
    }
    if (msg == M_COMMAND && p1 == 40 && HIWORD(p2) == N_DBLCLK) { ssel = (short)SendMessage(l, L_GETSEL, 0, 0); DialogEnd(d, ID_OK); return TRUE; }
    if (msg == M_COMMAND && p1 == ID_OK) ssel = (short)SendMessage(l, L_GETSEL, 0, 0);
    return FALSE;
}
static void suchen(LPCSTR ab)                     /* ab: Ordner mit \ am Ende */
{
    char z[8], *q; char FAR *t; WORD i;
    scpy(sp, ab, sizeof sp); fmt(buf, StrGet(inst, 36), sp, 0);
    if (!sm[0]) scpy(sm, "*.*", sizeof sm);
    if (!InputBox(GetActive(), buf, "RD", sm, sizeof sm) || !sm[0]) return;
    for (q = sm; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    if (!sh && !(sh = MemAlloc((DWORD)SMAX * SLEN, MEM_MOVABLE))) { fehler(sm, RD_ERR_NOMEM); return; }
    sn = 0; ssel = -1; CursorBusy(TRUE); durch((WORD)slen(sp), 0); CursorBusy(FALSE);
    zahl(z, sn); scpy(qv, ab, sizeof qv); fmt(buf, StrGet(inst, 37), z, qv);
    DbgPrint("SHELL: Suche"); DbgPrint(buf);
    if (!sn) { MsgBox(GetActive(), StrGet(inst, 38), "RD", MB_OK | MB_INFO); return; }
    if ((t = (char FAR *)MemLock(sh)) != 0) { DbgPrint((LPCSTR)t); MemUnlock(sh); }
    if (DialogRun(inst, 21, GetActive(), sproc, 0) != ID_OK || ssel < 0 || ssel >= (short)sn) return;
    if (!(t = (char FAR *)MemLock(sh))) return;
    t += ssel * SLEN; for (i = 0; t[i] && i < sizeof qv - 1; i++) qv[i] = t[i]; qv[i] = 0; MemUnlock(sh);
    if (i && qv[i - 1] == '\\') qv[--i] = 0;     /* Ordner-Treffer: den Ordner selbst nicht, sondern den darueber */
    while (i && qv[i - 1] != '\\') i--; qv[i] = 0;
    DbgPrint("SHELL: Suche oeffnet"); DbgPrint(qv);
    ordner_auf(qv[0], qv, 0);
}
static void neuer_ordner(ORDNER *o)
{
    char n[13]; WORD r;
    n[0] = 0;
    if (!InputBox(GetActive(), StrGet(inst, 9), "RD", n, sizeof n) || !n[0]) return;
    scpy(qn, o->pfad, sizeof qn); scat(qn, n, sizeof qn);
    if ((r = DirMake(qn)) != 0) fehler(n, r);
    DbgPrint("SHELL: Ordner angelegt");
    alle_neu();
}
static short treffer(ORDNER *o, short px, short py)   /* Eintrag unter Client-Punkt, -1 = keiner */
{
    short x = liste ? 0 : px / ZB, i = (o->oben + py / (liste ? th + 2 : zh)) * o->spalten + x;
    return px >= 0 && py >= 0 && x < o->spalten && i < o->n ? i : -1;
}
static short lwbei(short x, short y);
static void fallen(ORDNER *o, short x, short y, WORD ks)   /* Ziehen beendet bei Client-Punkt der Quelle */
{
    POINT p; HWND t; ORDNER *z; char ziel[68]; short i; EINTRAG FAR *e; int weg;
    p.x = x; p.y = y; ClientToScreen(o->w, &p);
    if ((t = WindowFromPoint(p.x, p.y)) == 0) return;
    ziel[0] = 0;
    if ((z = ord(t)) != 0) {
        scpy(ziel, z->pfad, sizeof ziel);
        ScreenToClient(t, &p);
        if ((i = treffer(z, p.x, p.y)) >= 0 && z->h && (e = (EINTRAG FAR *)MemLock(z->h)) != 0) {
            if ((e[i].attr & FA_DIR) && !(z == o && (i == o->sel || e[i].mark))) {
                if (e[i].name[0] == '.') { char *q = ziel + slen(ziel) - 1; while (q > ziel + 2 && q[-1] != '\\') q--; *q = 0; }
                else { scat(ziel, e[i].name, sizeof ziel); scat(ziel, "\\", sizeof ziel); }
            }
            MemUnlock(z->h);
        }
    } else if (t == desk) {
        ScreenToClient(t, &p);
        if ((i = lwbei(p.x, p.y)) >= 0) { ziel[0] = lw[i]; ziel[1] = ':'; ziel[2] = '\\'; ziel[3] = 0; }
    }
    if (!ziel[0]) return;
    weg = (ziel[0] | 0x20) == (o->pfad[0] | 0x20);
    if (ks & KS_CTRL) weg = 0; else if (ks & KS_SHIFT) weg = 1;
    DbgPrint(ziel);
    kopieren(o, weg, ziel);
}
static void markieren(ORDNER *o, short i)
{
    EINTRAG FAR *e;
    if (i < 0 || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    if (e[i].name[0] != '.') e[i].mark ^= 1;
    MemUnlock(o->h); InvalidateRect(o->w, 0, FALSE);
}
static void setzen(ORDNER *o, short a, short b)   /* a..b markieren (Umschalt+Klick/Pfeil, Strg+A) */
{
    EINTRAG FAR *e; short t;
    if (a > b) { t = a; a = b; b = t; }
    if (a < 0 || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    for (; a <= b && a < o->n; a++) if (e[a].name[0] != '.') e[a].mark = 1;
    MemUnlock(o->h); InvalidateRect(o->w, 0, FALSE);
}
static void springen(ORDNER *o, char c)           /* Buchstabe: naechster Eintrag mit diesem Anfang */
{
    EINTRAG FAR *e; short i, k;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (o->n < 1 || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    for (k = 1; k <= o->n; k++) { i = (o->sel + k) % o->n; if (e[i].name[0] == c) break; }
    MemUnlock(o->h);
    if (k <= o->n) neusel(o, i);
}

static WORD werkzeug(char *p, LPCSTR n);
static int schl(const char FAR *b, WORD n, LPCSTR k)   /* Zeile beginnt mit Schluessel k (ohne Gross/Klein) */
{
    for (; *k; b++, k++, n--) if (!n || (*b | 0x20) != (*k | 0x20)) return 0;
    return 1;
}
static short muell(short i);
static void prgladen(void)                        /* PROGRAMM.LST lesen, Menue Programme fuellen */
{
    char p[80], t[24], sec[9]; HMEM h; char FAR *b; short f; WORD n = 0, i, k; BYTE ds = 0;
    nprg = nps = 0; sec[0] = 0; prg[0][0] = 0;
    werkzeug(p, "PROGRAMM.LST");
    if ((f = FileOpen(p, 0)) >= 0) {
        if ((h = MemAlloc(4096, MEM_MOVABLE)) != 0 && (b = (char FAR *)MemLock(h)) != 0) {
            n = FileRead(f, b, 4095); if (n == 0xFFFF) n = 0;
            for (i = 0; i <= n && nprg < MAXP; ) {
                while (i < n && (b[i] == ' ' || b[i] == '\t')) i++;
                if (i >= n || b[i] == '[') {                       /* Abschnitt zu Ende: uebernehmen */
                    if (sec[0] && prg[nprg][0]) { scpy(prgn[nprg], sec, 9); if (ds) psy[nps++] = nprg; nprg++; }
                    if (i >= n) break;
                    for (k = 0, i++; i < n && b[i] != ']' && b[i] != '\r' && b[i] != '\n'; i++) if (k < 8) sec[k++] = b[i];
                    sec[k] = 0; prg[nprg][0] = 0; ds = 0;
                } else if (sec[0] && schl(b + i, n - i, "Datei=")) {
                    for (k = 0, i += 6; i < n && b[i] != '\r' && b[i] != '\n' && b[i] != ' ' && b[i] != ';'; i++) if (k < 67) prg[nprg][k++] = b[i];
                    prg[nprg][k] = 0;
                } else if (sec[0] && schl(b + i, n - i, "Schreibtisch=")) ds = b[i + 13] == '1';
                while (i < n && b[i] != '\n') i++;
                if (i < n) i++;
            }
            MemUnlock(h);
        }
        if (h) MemFree(h);
        FileClose(f);
    }
    for (i = k = 0; i < nprg; i++) {              /* Papierkorb steht im RD-Menue (Symbol bleibt) */
        LPCSTR b = prg[i], q; for (q = b; *q; q++) if (*q == '\\' || *q == ':') b = q + 1;
        if (muell(i)) continue;
        t[0] = '&'; t[1] = (char)(k < 9 ? '1' + k : 'A' + k - 9); t[2] = ' '; scpy(t + 3, prgn[i], sizeof t - 3);
        pm[k] = (BYTE)i; MenuText(mbar, CM_PRG + k++, t);
    }
    npm = (BYTE)k;
    for (; k < MAXP; k++) MenuText(mbar, CM_PRG + k, 0);
    MenuText(mbar, CM_PLEER, 0);                  /* kein Platzhalter */
    if (lsel >= nlw + nps) lsel = nlw + nps - 1;
    if (desk) InvalidateRect(desk, 0, FALSE);
}
static void pdesk(LPCSTR sec, LPCSTR v) { char p[80]; werkzeug(p, "PROGRAMM.LST"); ProfileSetStr(p, sec, "Schreibtisch", v); prgladen(); }
static void anheften(ORDNER *o, BYTE sd)         /* gewaehltes Programm anheften, schon angeheftet: loesen; sd: Schreibtisch umschalten */
{
    EINTRAG FAR *e; char p[80], sec[9]; short i, k;
    if (o->sel < 0 || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    k = symbol(&e[o->sel]) == IC_PROG;
    scpy(buf, o->pfad, sizeof buf); scat(buf, e[o->sel].name, sizeof buf);
    for (i = 0; i < 8 && e[o->sel].name[i] && e[o->sel].name[i] != '.'; i++) sec[i] = e[o->sel].name[i];
    sec[i] = 0; MemUnlock(o->h);
    if (!k) return;
    werkzeug(p, "PROGRAMM.LST");
    for (i = 0; i < nprg && !gleichi(prg[i], buf); i++) ;
    if (sd && i < nprg) { for (k = 0; k < nps && psy[k] != i; k++) ; pdesk(prgn[i], k < nps ? 0 : "1"); return; }
    if (i < nprg) ProfileSetStr(p, prgn[i], "Datei", 0);
    else if (nprg >= MAXP) { MsgBox(GetActive(), StrGet(inst, 29), "RD", MB_OK | MB_WARN); return; }
    else if (ProfileSetStr(p, sec, "Datei", buf)) { fehler(buf, LastError()); return; }
    else if (sd) ProfileSetStr(p, sec, "Schreibtisch", "1");
    DbgPrint(i < nprg ? "SHELL: geloest" : "SHELL: angeheftet");
    prgladen();
}
static void ansicht(BYTE l)                       /* fuer alle Ordnerfenster */
{
    short i; RECT r;
    liste = l; MenuCheck(mbar, CM_SYMB, !l); MenuCheck(mbar, CM_LISTE, l);
    DbgHex("SHELL: Ansicht", l);
    for (i = 0; i < MAXO; i++) if (od[i].w) { GetClientRect(od[i].w, &r); masse(&od[i], r.r, r.b); sichtbar(&od[i]); InvalidateRect(od[i].w, 0, TRUE); }
}
static DWORD RDCALLBACK ordproc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    ORDNER *o = ord(w); PAINT ps; HDC d; RECT r; EINTRAG FAR *e; short i, x, y;
    if (!o) return DefWindowProc(w, msg, p1, p2);
    switch (msg) {
    case M_SIZE: DbgHex("SHELL: Groesse", p2); if (p1 != SIZE_MIN) masse(o, (short)LOWORD(p2), (short)HIWORD(p2)); return 0;
    case M_PAINT:
        d = BeginPaint(w, &ps); GetClientRect(w, &r);
        SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &ps.neu);   /* nur der ungueltige Teil */
        if (o->h && (e = (EINTRAG FAR *)MemLock(o->h)) != 0) {
            for (i = o->oben * o->spalten; i < o->n; i++) {
                if (liste) { y = (i - o->oben) * (th + 2); if (y >= r.b || y >= ps.neu.b) break; if (y + th + 2 <= ps.neu.t) continue; zeile(d, y, r.r, &e[i], i == o->sel);
                             if ((e[i].attr & (FA_HIDDEN | FA_SYSTEM)) && i != o->sel) grau_ueber(d, 0, y, r.r, (short)(y + th + 2)); continue; }
                x = (i % o->spalten) * ZB; y = (i / o->spalten - o->oben) * zh;
                if (y >= r.b || y >= ps.neu.b) break;
                if (y + zh <= ps.neu.t || x >= ps.neu.r || x + ZB <= ps.neu.l) continue;   /* ausserhalb: nicht zeichnen */
                IconDraw(d, x + (ZB - 32) / 2, y + 2, inst, symbol(&e[i]));
                schild(d, x, y + ih + 3, ZB, e[i].name, i == o->sel);
                if ((e[i].attr & (FA_HIDDEN | FA_SYSTEM)) && i != o->sel) grau_ueber(d, x, y, (short)(x + ZB), (short)(y + zh));
                if (e[i].mark) { RECT m; m.l = x + 4; m.t = y + 2; m.r = m.l + 6; m.b = m.t + 6; SetBrush(d, C_BLACK, PAT_SOLID); FillRect(d, &m); }
            }
            MemUnlock(o->h);
        }
        EndPaint(w, &ps); return 0;
    case M_BUTTONDOWN: case M_DBLCLK:
        if ((i = treffer(o, (short)LOWORD(p2), (short)HIWORD(p2))) >= 0) {
            if (msg == M_BUTTONDOWN && (p1 & 0xFF) == 1 && !((p1 >> 8) & KS_CTRL) && o->h && (e = (EINTRAG FAR *)MemLock(o->h)) != 0) {
                if (!e[i].mark) { short k, n = 0; for (k = 0; k < o->n; k++) { n |= e[k].mark; e[k].mark = 0; } if (n) InvalidateRect(w, 0, FALSE); }
                MemUnlock(o->h);
                zw = w; zx = (short)LOWORD(p2); zy = (short)HIWORD(p2); zieht = 1; CaptureMouse(w);
            }
            if (msg == M_BUTTONDOWN && (p1 >> 8) & KS_SHIFT && o->sel >= 0) { setzen(o, o->sel, i); zieht = 0; ReleaseMouse(); }   /* Umschalt+Klick: Bereich */
            neusel(o, i);
            if (msg == M_BUTTONDOWN && (p1 >> 8) & KS_CTRL) markieren(o, i);
            else if (msg == M_DBLCLK) oeffnen(o);
            else if ((p1 & 0xFF) == 2) {          /* rechts: Menue "Datei" am Zeiger */
                i = (short)MenuPopup(w, mbar, 0, (short)LOWORD(p2), (short)HIWORD(p2));
                if (i) PostMessage(w, M_COMMAND, i, 0);
            }
        }
        return 0;
    case M_MOUSEMOVE:
        if (zieht == 1 && zw == w) {
            x = (short)LOWORD(p2) - zx; y = (short)HIWORD(p2) - zy;
            if (x > 4 || x < -4 || y > 4 || y < -4) { zieht = 2; CursorSet(0, CUR_HAND); DbgPrint("SHELL: zieht"); }
        }
        return 0;
    case M_BUTTONUP:
        if (zieht && zw == w) {
            i = zieht; zieht = 0; ReleaseMouse(); CursorSet(0, CUR_ARROW);
            if (i == 2) fallen(o, (short)LOWORD(p2), (short)HIWORD(p2), p1 >> 8);
        }
        return 0;
    case M_SCROLL:
        i = o->oben;
        if (p1 == SB_LINEUP) i--; else if (p1 == SB_LINEDOWN) i++;
        else if (p1 == SB_PAGEUP) i -= o->zeilen; else if (p1 == SB_PAGEDOWN) i += o->zeilen;
        else if (p1 == SB_THUMB) i = (short)LOWORD(p2);
        if (i < 0) i = 0;
        if (i != o->oben) { o->oben = i; rollen(o); InvalidateRect(w, 0, FALSE); }
        return 0;
    case M_KEY:
        if (p1 == K_F1) { HelpShow(w, "RD2.HLP", 5); return 0; }
        y = (HIWORD(p2) & KS_SHIFT) && (p1 == K_UP || p1 == K_DOWN || p1 == K_LEFT || p1 == K_RIGHT);   /* Umschalt+Pfeil: mitmarkieren */
        if (y) setzen(o, o->sel, o->sel);
        if (p1 == K_LEFT) neusel(o, o->sel - 1);
        else if (p1 == K_RIGHT) neusel(o, o->sel + 1);
        else if (p1 == K_UP) neusel(o, o->sel - o->spalten);
        else if (p1 == K_DOWN) neusel(o, o->sel + o->spalten < o->n ? o->sel + o->spalten : o->n - 1);
        else if (p1 == K_HOME) neusel(o, 0);
        else if (p1 == K_END) neusel(o, o->n - 1);
        else if ((p1 == K_ENTER || p1 == 0x11C) && (HIWORD(p2) & KS_ALT)) eigenschaften(o);   /* 11C: Alt+Eingabe vom erweiterten BIOS */   /* Alt+Eingabe: Eigenschaften */
        else if (p1 == K_ENTER) oeffnen(o);
        else if (p1 == 6 && (HIWORD(p2) & KS_CTRL)) suchen(o->pfad);   /* Strg+F */
        else if (p1 == K_BACK) hoch(o);
        else if (p1 == K_INS) { markieren(o, o->sel); neusel(o, o->sel + 1); }
        else if (HIWORD(p2) & KS_ALT) return DefWindowProc(w, msg, p1, p2);   /* Alt+Leertaste usw. */
        else if (p1 == ' ') markieren(o, o->sel);
        else if (p1 == 1) setzen(o, 0, o->n - 1);  /* Strg+A: alle */
        else if (p1 == K_PGUP) neusel(o, o->sel - o->spalten * o->zeilen < 0 ? 0 : o->sel - o->spalten * o->zeilen);
        else if (p1 == K_PGDN) neusel(o, o->sel + o->spalten * o->zeilen < o->n ? o->sel + o->spalten * o->zeilen : o->n - 1);
        else if (p1 == K_TAB) {                   /* naechstes Ordnerfenster */
            for (i = 1; i <= MAXO; i++) { ORDNER *q = &od[(o - od + i) % MAXO]; if (q->w && q != o) { SetActive(q->w); break; } }
        }
        else if (p1 == 0x15D && o->sel >= 0) {    /* Umschalt+F10: Menue "Datei" an der Auswahl */
            short x = liste ? 8 : (o->sel % o->spalten) * ZB + 8, yy = (o->sel / o->spalten - o->oben) * (liste ? th + 2 : zh) + 4;
            i = (short)MenuPopup(w, mbar, 0, x, yy);
            if (i) PostMessage(w, M_COMMAND, i, 0);
        }
        else if (p1 > ' ' && p1 < 0x100) springen(o, (char)p1);
        else if (p1 == K_F5) kopieren(o, 0, 0);
        else if (p1 == K_F6) kopieren(o, 1, 0);
        else if (p1 == K_F2) umbenennen(o);
        else if (p1 == K_F7) neuer_ordner(o);
        else if (p1 == K_DEL || p1 == K_F8) { endg = (BYTE)((HIWORD(p2) & KS_SHIFT) != 0); loeschen(o); endg = 0; }   /* Umschalt+Entf: endgueltig */
        else return DefWindowProc(w, msg, p1, p2);
        if (y) setzen(o, o->sel, o->sel);
        return 0;
    case M_COMMAND:
        if (p1 == CM_OEFFNEN) oeffnen(o);
        else if (p1 == CM_HOCH) hoch(o);
        else if (p1 == CM_NEU) { wl[o->pfad[0] - 'A'] = 1; einlesen(o); }   /* gibt die Wechselabfrage wieder frei */
        else if (p1 == CM_SYMB || p1 == CM_LISTE) ansicht(p1 == CM_LISTE);
        else if (p1 >= CM_SORT && p1 < CM_SORT + 4) sortwahl((BYTE)(p1 - CM_SORT));
        else if (p1 == CM_VERST) { verst ^= 1; MenuCheck(mbar, CM_VERST, verst); DbgHex("SHELL: versteckte", verst); alle_neu(); }
        else if (p1 == CM_KOPIE) kopieren(o, 0, 0);
        else if (p1 == CM_VERSCH) kopieren(o, 1, 0);
        else if (p1 == CM_UMBEN) umbenennen(o);
        else if (p1 == CM_FEIG) eigenschaften(o);
        else if (p1 == CM_SUCH) suchen(o->pfad);
        else if (p1 == CM_MKDIR) neuer_ordner(o);
        else if (p1 == CM_LOESCH) loeschen(o);
        else if (p1 == CM_ANHEFT || p1 == CM_PDESK) anheften(o, p1 == CM_PDESK);
        else if (p1 == CM_ZU) PostMessage(w, M_CLOSE, 0, 0);
        else PostMessage(desk, M_COMMAND, p1, 0);   /* RD-Befehle */
        return 0;
    case M_ACTIVATE:
        if (p1) { DbgPrint("SHELL: aktiv"); DbgPrint(o->pfad); wechsel(o->pfad[0]); }
        if (!p1) letzt = w;                       /* fuer das Kopierziel */
        return DefWindowProc(w, msg, p1, p2);
    case M_DESTROY:
        if (letzt == w) letzt = 0;
        if (o->h) MemFree(o->h);
        o->h = 0; o->w = 0;
        return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
static ORDNER *ordner_auf(char l, LPCSTR pfad, RECT *wr)   /* pfad, wr: aus RD2.STA (sonst 0) */
{
    DRIVEINFO di; short i; ORDNER *o = 0;
    for (i = 0; i < MAXO; i++) if (!od[i].w) { o = &od[i]; break; }
    if (!o) return 0;
    buf[0] = l; buf[1] = ':'; buf[2] = 0;
    if (DriveInfo((WORD)(l - 'A' + 1), &di)) { char t[80]; fmt(t, StrGet(inst, 4), buf, 0); MsgBox(0, t, "RD", MB_OK | MB_WARN); return 0; }
    o->pfad[0] = l; o->pfad[1] = ':'; o->pfad[2] = '\\'; o->pfad[3] = 0;
    if (pfad) scpy(o->pfad, pfad, sizeof o->pfad);
    o->h = 0; o->n = 0; o->sel = -1; o->oben = 0; o->spalten = 1; o->zeilen = 1;
    o->w = (HWND)1;                               /* belegt; M_SIZE aus CreateWindow kommt vor dem Handle */
    o->w = CreateWindow("RDOrdner", o->pfad, WS_NORMAL | WS_VSCROLL | WS_VISIBLE, CW_DEFAULT, 0, CW_DEFAULT, 0, 0, 0, inst);
    if (!o->w) return 0;
    if (wr) MoveWindow(o->w, wr->l, wr->t, wr->r - wr->l, wr->b - wr->t, TRUE);
    { RECT r; GetClientRect(o->w, &r); masse(o, r.r, r.b); }
    MenuSet(o->w, mbar);
    einlesen(o);
    if (wl[l - 'A'] != 2) wl[l - 'A'] = 1;        /* eben gelesen: meldet das Laufwerk trotzdem Wechsel, hat es kein Signal */
    return o;
}

/* ---- RD2.STA: Zustand (SPEC 8), beim Beenden und vor RunDos ------------------------------- */
typedef struct { char pfad[68]; RECT r; short sel, oben; } STAO;
typedef struct { char kopf[4]; WORD ver; BYTE liste, lsel, n, akt; STAO o[MAXO]; } STA;
static STA z;
static void sta_schreiben(void)
{
    char p[80]; short i; HWND a = GetActive();
    z.kopf[0] = 'R'; z.kopf[1] = 'D'; z.kopf[2] = 'S'; z.kopf[3] = '1'; z.ver = 1; z.liste = (BYTE)(liste | verst << 1 | sortart << 2); z.lsel = (BYTE)lsel; z.n = 0; z.akt = 0xFF;
    for (i = 0; i < MAXO; i++) if (od[i].w) {
        STAO *s = &z.o[z.n];
        scpy(s->pfad, od[i].pfad, sizeof s->pfad); GetWindowRect(od[i].w, &s->r); s->sel = od[i].sel; s->oben = od[i].oben;
        if (od[i].w == a) z.akt = z.n;
        z.n++;
    }
    werkzeug(p, "RD2.STA");
    if (!FileSaveSafe(p, &z, 10 + (DWORD)z.n * sizeof(STAO))) DbgPrint("SHELL: Zustand gesichert");
}
static void sta_lesen(void)
{
    char p[80]; short f, i; WORD n; ORDNER *o; HWND a = 0;
    werkzeug(p, "RD2.STA");
    if ((f = FileOpen(p, 0)) < 0) return;
    n = FileRead(f, &z, sizeof z); FileClose(f);
    if (n == 0xFFFF || n < 10 || z.kopf[0] != 'R' || z.kopf[1] != 'D' || z.kopf[2] != 'S' || z.kopf[3] != '1' || z.ver != 1 || z.n > MAXO || n < 10 + z.n * sizeof(STAO)) return;   /* unbekannt: ignorieren */
    liste = (BYTE)(z.liste & 1); verst = (BYTE)(z.liste >> 1 & 1); sortart = (BYTE)(z.liste >> 2 & 3); if (z.lsel < nlw) lsel = z.lsel;
    MenuCheck(mbar, CM_SYMB, !liste); MenuCheck(mbar, CM_LISTE, liste); MenuCheck(mbar, CM_VERST, verst); { short k; for (k = 0; k < 4; k++) MenuCheck(mbar, CM_SORT + k, k == sortart); }
    for (i = 0; i < z.n; i++) {
        STAO *s = &z.o[i];
        s->pfad[sizeof s->pfad - 1] = 0;
        if (!(o = ordner_auf(s->pfad[0], s->pfad, &s->r))) continue;
        if (s->sel < o->n) o->sel = s->sel;
        if (s->oben >= 0 && s->oben < o->n) o->oben = s->oben;
        rollen(o); InvalidateRect(o->w, 0, FALSE);
        if (i == z.akt) a = o->w;
    }
    if (a) SetActive(a);
    DbgPrint("SHELL: Zustand geladen");
}

/* ---- Schreibtisch ----------------------------------------------------------------------- */
static void lwfeld(short k, RECT *r)              /* Laufwerke, darunter Programme; volle Spalte: naechste */
{
    short nr = (H - mh - 16) / (zh + 6); if (nr < 1) nr = 1;
    r->l = 8 + (k / nr) * 64; r->r = r->l + 56; r->t = 8 + (k % nr) * (zh + 6); r->b = r->t + zh;
}
static BYTE mvoll;                                /* Muell hat Eintraege (X:\PAPIERK\PAPIERK.LST auf einer Festplatte) */
static short muell(short i) { LPCSTR b = prg[i], q; for (q = b; *q; q++) if (*q == '\\' || *q == ':') b = q + 1; return gleichi(b, "PAPIERK.EXE"); }
static BYTE muellvoll(void)
{
    short i; FINDDATA f; char p[24];
    for (i = 0; i < nlw; i++) if (lt[i] != DT_FLOPPY) { p[0] = lw[i]; scpy(p + 1, ":\\PAPIERK\\PAPIERK.LST", sizeof p - 1); if (!FileFindFirst(p, 0, &f) && f.groesse) return 1; }
    return 0;
}
static void lwmal(HDC d, short k)
{
    RECT r; char t[3];
    lwfeld(k, &r);
    if (k >= nlw) { short m = muell(psy[k - nlw]);   /* Muell (PAPIERK.EXE): leer Deckel zu, voll Deckel schief (30.10.2026) */
        IconDraw(d, r.l + (56 - 32) / 2, r.t + 2, inst, m ? (mvoll ? 7 : 6) : IC_PROG);
        schild(d, r.l, r.t + ih + 3, 56, m ? StrGet(inst, 40) : prgn[psy[k - nlw]], k == lsel); return; }
    IconDraw(d, r.l + (56 - 32) / 2, r.t + 2, inst, lt[k] == DT_FLOPPY ? IC_FLOPPY : IC_HDISK);
    t[0] = lw[k]; t[1] = ':'; t[2] = 0;
    schild(d, r.l, r.t + ih + 3, 56, t, k == lsel);
}
static short lwbei(short x, short y)
{
    short i; RECT r;
    for (i = 0; i < nlw + nps; i++) { lwfeld(i, &r); if (x >= r.l && x < r.r && y >= r.t && y < r.b) return i; }
    return -1;
}
static WORD werkzeug(char *p, LPCSTR n)            /* <RD2>\n, 1 = vorhanden */
{
    SYSINFO si; FINDDATA f;
    KernInfo(&si); scpy(p, si.home, 80);
    if (slen(p) && p[slen(p) - 1] != '\\') scat(p, "\\", 80);
    scat(p, n, 80);
    return FileFindFirst(p, 0, &f) == 0;
}
#define rdformat(p) werkzeug(p, "RDFORMAT.EXE")
static void einrichten(void)                      /* Laufwerksmenue: Festplatte einrichten */
{
    char p[80];
    if (!werkzeug(p, "RDFDISK.EXE")) return;
    DbgPrint("SHELL: Festplatte einrichten");
    if (!TaskExec(p, "", SW_SHOW)) { fmt(buf, StrGet(inst, 3), p, 0); MsgBox(0, buf, "RD", MB_OK | MB_ERROR); }
}
static void formatieren(void)                     /* Laufwerksmenue: Formatieren */
{
    char p[80], c[8];
    if (!rdformat(p)) return;
    c[0] = lw[lsel]; c[1] = ':'; c[2] = 0;
    DbgPrint("SHELL: Formatieren"); DbgPrint(c);
    if (!TaskExec(p, c, SW_SHOW)) { fmt(buf, StrGet(inst, 3), p, 0); MsgBox(0, buf, "RD", MB_OK | MB_ERROR); }
}
static void lwinfo(void)
{
    DRIVEINFO di; char a[48], g[12], f[12], *p; WORD e;
    a[0] = lw[lsel]; a[1] = ':'; a[2] = 0;
    CursorBusy(TRUE); e = DriveInfo((WORD)(lw[lsel] - 'A' + 1), &di); CursorBusy(FALSE);
    if (e) { fmt(buf, StrGet(inst, 4), a, 0); MsgBox(GetActive(), buf, "RD", MB_OK | MB_WARN); return; }
    p = zahl(g, di.gesamt >> 10); scpy(p, " KB", 4);
    p = zahl(f, di.frei >> 10); scpy(p, " KB", 4);
    if (di.label[0]) { scat(a, " ", sizeof a); scat(a, di.label, sizeof a); }
    fmt(buf, StrGet(inst, 13), a, g); scat(buf, f, sizeof buf);
    MsgBox(GetActive(), buf, StrGet(inst, 14), MB_OK | MB_INFO);
    DbgPrint("SHELL: Laufwerksinfo");
}
static LPCSTR nte(LPCSTR t, WORD i) { while (i-- && *t) while (*t++); return t; }   /* i-tes Wort aus "a\0b\0" */
static void zk(char *p, WORD v) { p = zahl(p, v / 10); *p++ = ','; zahl(p, v % 10); }   /* 0,1-Einheiten */
static void info(void)                            /* wie RD1: Prozessor, Takt, Speicher, DOS, Grafik, Sound */
{
    static const char cpu[] = "8088\0" "8086\0" "NEC V20\0" "80186\0" "80286\0" "80386\0" "80486\0" "Pentium\0";
    static const char gfx[] = "?\0" "CGA\0" "Hercules\0" "VGA mono\0" "EGA\0" "VGA\0" "VESA\0" "EGA 320\0" "EGA mono\0" "EGA 4\0" "VGA 4\0" "EGA 200\0" "VESA 16\0" "MCGA\0" "VESA 800\0" "VESA 1024\0";
    SYSINFO si; char t[12], *p; WORD a = KernVersion();
    KernInfo(&si);
    p = zahl(t, a >> 8); *p++ = '.'; zahl(p, a & 0xFF);
    fmt(buf, StrGet(inst, 1), t, 0);                                        /* RetroDesk 2, API x.y */
    scat(buf, StrGet(inst, 21), sizeof buf); scat(buf, nte(cpu, si.cpu), sizeof buf);
    if (si.mhz10) { scat(buf, ", ca. ", sizeof buf); zk(t, si.mhz10); scat(buf, t, sizeof buf); scat(buf, " MHz", sizeof buf); }
    scat(buf, StrGet(inst, 22), sizeof buf);                                 /* Koprozessor */
    if (si.fpu && si.cpu >= CPU_486) scat(buf, StrGet(inst, 30), sizeof buf);   /* 486DX und spaeter: eingebaut */
    else if (si.fpu) { zahl(t, si.fpu == 87 ? 8087 : 80000u + si.fpu); scat(buf, t, sizeof buf); } else scat(buf, StrGet(inst, 26), sizeof buf);
    scat(buf, StrGet(inst, 23), sizeof buf); zahl(t, si.kb_konv); scat(buf, t, sizeof buf);   /* Speicher */
    scat(buf, StrGet(inst, 24), sizeof buf); zahl(t, (WORD)(MemAvail(MEMA_KONV) >> 10)); scat(buf, t, sizeof buf); scat(buf, " KB", sizeof buf);
    if (si.kb_ext) { scat(buf, " + XMS ", sizeof buf); zahl(t, si.kb_ext); scat(buf, t, sizeof buf); scat(buf, " KB", sizeof buf); }
    scat(buf, "\nDOS ", sizeof buf); p = zahl(t, si.dos >> 8); *p++ = '.'; zahl(p, si.dos & 0xFF); scat(buf, t, sizeof buf);
    scat(buf, StrGet(inst, 25), sizeof buf); scat(buf, nte(gfx, si.grafik <= GFX_VESA1024 ? si.grafik : 0), sizeof buf);   /* Grafik */
    scat(buf, " ", sizeof buf); zahl(t, W); scat(buf, t, sizeof buf); scat(buf, "x", sizeof buf); zahl(t, H); scat(buf, t, sizeof buf);
    scat(buf, StrGet(inst, 27), sizeof buf);                                 /* Sound */
    scat(buf, si.sound & (SC_OPL2 | SC_OPL3) ? "AdLib" : si.sound & SC_SPEAKER ? "PC-Speaker" : StrGet(inst, 26), sizeof buf);
    { char u[24]; scpy(u, StrGet(inst, 5), sizeof u); MsgBox(GetActive(), buf, u, MB_OK | MB_INFO); }
}
static void uhr(void)                             /* HH:MM rechts in der Menueleiste, jede Sekunde */
{
    RDTIME t; char z[8]; HDC d; RECT r; short w;
    TimeGet(&t);
    zahl(z, 100 + t.std); z[0] = z[1]; z[1] = z[2]; z[2] = ':'; zahl(z + 3, 100 + t.min); z[3] = z[4]; z[4] = z[5]; z[5] = 0;
    if (!(d = GetDC(0))) return;
    w = TextWidth(d, z, -1); r.r = W - 4; r.l = r.r - w - 8; r.t = 0; r.b = mh - 1;
    SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &r);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK); TextOut(d, r.l + 4, 1, z, -1);
    ReleaseDC(0, d);
}
/* Diskettenwechsel (SPEC 9): Wechselleitung ueber DriveInfo(DI_NURTYP), ohne Laufwerkszugriff.
   Gewechselt: alle Ordner des Laufwerks zur Wurzel und neu lesen. Meldet das Laufwerk gleich danach
   wieder einen Wechsel, hat es kein Signal (immer "gewechselt"): Abfrage aus bis "Neu einlesen". */
static BYTE tk;
static void wechsel(char l)
{
    DRIVEINFO di; short i, k = l - 'A'; WORD f = 0;
    if (k < 0 || k > 25 || wl[k] == 2 || DriveInfo((WORD)(k + 1) | DI_NURTYP, &di) || di.typ != DT_FLOPPY) return;
    if (!(di.flags & DI_CHANGE)) { wl[k] = 0; return; }
    if (wl[k]) { wl[k] = 2; DbgPrint("SHELL: kein Wechselsignal"); return; }
    DbgPrint("SHELL: Diskettenwechsel");
    for (i = 0; i < MAXO; i++) if (od[i].w && od[i].pfad[0] == l) { od[i].pfad[3] = 0; f |= einlesen(&od[i]); }
    wl[k] = f ? 2 : 1;                            /* keine Diskette (Abbruch): bis "Neu einlesen" ruhen */
}
static COLOR dvorn = C_BLACK, dhint = C_WHITE;     /* Schreibtisch: Punktmuster, bunt einfarbig tuerkis */
static DWORD RDCALLBACK deskproc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; HDC d; RECT r; short i, y;
    switch (msg) {
    case M_PAINT:
        d = BeginPaint(w, &ps); GetClientRect(w, &r);
        SetBrush(d, dvorn, PAT_GRAY50); SetBkColor(d, dhint); FillRect(d, &r);
        for (i = 0; i < nlw + nps; i++) lwmal(d, i);
        EndPaint(w, &ps); return 0;
    case M_BUTTONDOWN: case M_DBLCLK:
        if ((i = lwbei((short)LOWORD(p2), (short)HIWORD(p2))) >= 0) {
            if (i != lsel) { lsel = i; InvalidateRect(w, 0, FALSE); }
            if (i >= nlw) { if (msg == M_DBLCLK) starten(prg[psy[i - nlw]]);
                else if ((p1 & 0xFF) == 2 && (y = (short)MenuPopup(w, mps, 0, (short)LOWORD(p2), (short)HIWORD(p2))) != 0) PostMessage(w, M_COMMAND, y, 0); }
            else if (msg == M_DBLCLK) ordner_auf(lw[i], 0, 0);
            else if ((p1 & 0xFF) == 2) {
                char p[80]; WORD an = (lt[i] == DT_FLOPPY || lt[i] == DT_HARD) && rdformat(p);
                MenuEnable(mlw, CM_FORMAT, an); MenuEnable(mlw, CM_DKOP, lt[i] == DT_FLOPPY);
                MenuEnable(mlw, CM_FDISK, werkzeug(p, "RDFDISK.EXE"));
                y = (short)MenuPopup(w, mlw, 0, (short)LOWORD(p2), (short)HIWORD(p2));
                if (y) PostMessage(w, M_COMMAND, y, 0);
            }
        }
        return 0;
    case M_KEY:
        if (p1 == K_DOWN && lsel < nlw + nps - 1) { lsel++; InvalidateRect(w, 0, FALSE); }
        else if (p1 == K_UP && lsel > 0) { lsel--; InvalidateRect(w, 0, FALSE); }
        else if (p1 == K_ENTER && lsel >= nlw) starten(prg[psy[lsel - nlw]]);
        else if ((p1 == K_ENTER || p1 == 0x11C) && nlw && (HIWORD(p2) & KS_ALT)) lwinfo();
        else if (p1 == 6 && (HIWORD(p2) & KS_CTRL) && lsel < nlw) PostMessage(w, M_COMMAND, CM_LSUCH, 0);   /* Strg+F */   /* Alt+Eingabe: Eigenschaften */
        else if (p1 == K_ENTER && nlw) ordner_auf(lw[lsel], 0, 0);
        else if (p1 == K_F1) HelpShow(w, "RD2.HLP", 2);
        else return DefWindowProc(w, msg, p1, p2);
        return 0;
    case M_SYSMENU:
        i = (short)MenuPopup(w, msys, 0, 0, -1);
        if (i) PostMessage(w, M_COMMAND, i, 0);
        return 0;
    case M_COMMAND:
        if (p1 == CM_INFO) info();
        else if (p1 == CM_ENDE) { char t[40]; scpy(t, StrGet(inst, 2), sizeof t); if (MsgBox(GetActive(), t, "RD", MB_YESNO | MB_ASK) == ID_YES) { sta_schreiben(); PostQuit(0); } }
        else if (p1 == CM_OEFFNEN && lsel < nlw) ordner_auf(lw[lsel], 0, 0);
        else if (p1 == CM_LWINFO && lsel < nlw) lwinfo();
        else if (p1 == CM_DKOP) { char p[80]; if (werkzeug(p, "DKOPIE.EXE")) starten(p); }
        else if (p1 == CM_PRUEF && lsel < nlw) { char p[80], c[4]; c[0] = lw[lsel]; c[1] = ':'; c[2] = 0; if (werkzeug(p, "DPRUEF.EXE")) TaskExec(p, c, SW_SHOW); }
        else if (p1 == CM_LSUCH && lsel < nlw) { char a[4]; a[0] = lw[lsel]; a[1] = ':'; a[2] = '\\'; a[3] = 0; suchen(a); }
        else if (p1 == CM_FORMAT && lsel < nlw) formatieren();
        else if (p1 == CM_PSTART && lsel >= nlw) starten(prg[psy[lsel - nlw]]);
        else if (p1 == CM_PWEG && lsel >= nlw) pdesk(prgn[psy[lsel - nlw]], 0);
        else if (p1 == CM_FDISK) einrichten();
        else if (p1 >= CM_ZUB && p1 < CM_ZUB + 14) { static const char *zn[] = { "UHR.EXE", "NOTIZ.EXE", "RECHNER.EXE", "OPTIONEN.EXE", "RDNET.EXE", "RDFTP.EXE", "RDIRC.EXE", "RDSHELL.EXE", "PAPIERK.EXE", "TASKMAN.EXE", "MAUS.EXE", "SCHONER.EXE", "INSTALL.EXE", "RDEDIT.EXE" }; char p[80];
            if (werkzeug(p, zn[p1 - CM_ZUB])) starten(p); }
        else if (p1 == CM_SYMB || p1 == CM_LISTE) ansicht(p1 == CM_LISTE);
        else if (p1 >= CM_SORT && p1 < CM_SORT + 4) sortwahl((BYTE)(p1 - CM_SORT));
        else if (p1 == CM_VERST) { verst ^= 1; MenuCheck(mbar, CM_VERST, verst); DbgHex("SHELL: versteckte", verst); alle_neu(); }
        else if (p1 >= CM_PRG && p1 < CM_PRG + npm) starten(prg[pm[p1 - CM_PRG]]);
        return 0;
    case M_TIMER:
        if (!slauf) uhr();                        /* Uhr in der Menueleiste ruht, solange der Schoner laeuft */
        if (szeit && InputIdle() / 1000 >= (DWORD)szeit) { char u[80]; if (!slauf && werkzeug(u, "SCHONER.EXE")) TaskExec(u, "S", SW_SHOW); slauf = 1; } else slauf = 0;   /* Bildschirmschoner: einmal je Ruhephase */
        if ((tk ^= 1) == 0) {                     /* alle 2 s: Diskettenwechsel, Muell voll/leer */
            BYTE v = muellvoll();
            for (i = 0; i < MAXO; i++) if (od[i].w) wechsel(od[i].pfad[0]);
            if (v != mvoll) { mvoll = v; InvalidateRect(desk, 0, FALSE); }
        }
        return 0;
    case M_CLOSE: return 0;                       /* der Schreibtisch bleibt */
    }
    return DefWindowProc(w, msg, p1, p2);
}

short RDAPI RdMain(HINST i, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; DRIVEINFO di; WORD k;
    (void)cmd; (void)show;
    inst = i;
    { SYSINFO si; KernInfo(&si); scpy(heim, si.home, sizeof heim); k = slen(heim); if (k && heim[k - 1] == '\\') heim[--k] = 0; if (k <= 2) heim[0] = 0; }
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); th = GetCaps(d, CAPS_TEXTH); if (W <= 320) ZB = 60;
    if (GetCaps(d, CAPS_FARBEN) > 2) {             /* Schreibtischfarbe: RD2.INI [Farben] Schreibtisch= 0..15 (Optionen), sonst tuerkis */
        static const COLOR f16[16] = { RGB(0,0,0), RGB(0,0,170), RGB(0,170,0), RGB(0,170,170), RGB(170,0,0), RGB(170,0,170), RGB(170,85,0), RGB(170,170,170),
            RGB(85,85,85), RGB(85,85,255), RGB(85,255,85), RGB(85,255,255), RGB(255,85,85), RGB(255,85,255), RGB(255,255,85), RGB(255,255,255) };
        char p[80]; short f = werkzeug(p, "RD2.INI") ? ProfileGetInt(p, "Farben", "Schreibtisch", -1) : -1;
        dvorn = dhint = f >= 0 && f < 16 ? f16[f] : RGB(0, 128, 128);
    }
    ReleaseDC(0, d);
    mh = th + 4; ih = H <= 200 ? 16 : 32; zh = ih + th + 6;
    for (k = 1; k <= 26; k++)                     /* nur Typ, keine Diskette anfassen */
        if (!DriveInfo(k | DI_NURTYP, &di) && di.typ != DT_NONE && !(di.flags & DI_PHANTOM)) { lw[nlw] = (char)('A' + k - 1); lt[nlw++] = (char)di.typ; }
    msys = MenuLoad(inst, 1); mbar = MenuLoad(inst, 2); mlw = MenuLoad(inst, 3); mps = MenuLoad(inst, 4);
    wc.name = "RDDesk"; wc.proc = deskproc; wc.stil = CS_DBLCLKS; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    wc.name = "RDOrdner"; wc.proc = ordproc;
    RegisterClass(&wc);
    desk = CreateWindow("RDDesk", "RD", WS_DESKTOP | WS_VISIBLE, 0, mh, W, H - mh, 0, 0, inst);
    if (!desk) return 1;
    MenuSet(desk, mbar);
    TimerSet(desk, 1, 1000); uhr(); prgladen(); sta_lesen(); mvoll = muellvoll();
    { char p[80], u[80]; if (werkzeug(p, "RD2.INI")) szeit = (WORD)ProfileGetInt(p, "Schoner", "Zeit", 300);
      if (werkzeug(p, "RD2.INI") && ProfileGetInt(p, "Netz", "Zeit", 0) && werkzeug(u, "UHR.EXE")) TaskExec(u, "N", SW_HIDE); }   /* Netzzeit beim Start */
    { SYSINFO si; char p[80], u[80]; KernInfo(&si);   /* Menuepunkt nur von Diskette; RD2SETUP "I" startet die Installation auch aus einem Festplattenordner */
      MenuEnable(msys, CM_ZUB + 12, (si.home[0] | 0x20) < 'c' && werkzeug(u, "INSTALL.EXE"));
      MenuEnable(msys, CM_ZUB + 13, werkzeug(u, "RDEDIT.EXE"));     /* Editor liegt auf der Programmdiskette */
      if (werkzeug(p, "RD2.INI") && ProfileGetInt(p, "System", "Installieren", 0) == 1 && werkzeug(u, "INSTALL.EXE"))
          { ProfileSetStr(p, "System", "Installieren", "0"); TaskExec(u, "", SW_SHOW); }
      if (werkzeug(p, "RD2.INI") && ProfileGetStr(p, "System", "GrafikAlt", "", u, 4) && werkzeug(u, "OPTIONEN.EXE")) TaskExec(u, "B", SW_SHOW); }   /* neue Grafik: "Behalten?" */
    DbgPrint("SHELL: bereit");
    while (GetMessage(&m)) DispatchMessage(&m);
    DbgPrint("SHELL: Ende");
    return 0;
}
