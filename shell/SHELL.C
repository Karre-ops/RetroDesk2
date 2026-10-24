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
#define CM_OEFFNEN 101
#define CM_ZU     102
#define CM_HOCH   103
#define CM_KOPIE  104
#define CM_VERSCH 105
#define CM_UMBEN  106
#define CM_MKDIR  107
#define CM_LOESCH 108
#define CM_LWINFO 301
#define CM_FORMAT 302
#define CM_NEU    201
#define MAXE      240                             /* Eintraege je Ordner */
#define MAXO      6                               /* Ordnerfenster */
#define ZB        100                             /* Zellenbreite */

typedef struct { char name[13]; BYTE attr, mark; } EINTRAG;
typedef struct { HWND w; char pfad[68]; HMEM h; short n, sel, oben, spalten, zeilen; } ORDNER;

static HINST inst; static HWND desk; static HMENU msys, mbar;
static short W, H, mh, th, ih, zh;                /* Schirm, Menueleiste, Textzeile, Symbol, Zellenhoehe */
static char lw[26], lt[26]; static short nlw, lsel;   /* Laufwerke: Buchstabe, Typ */
static ORDNER od[MAXO];
static char buf[160], qv[80], qn[80];            /* Text; Quelle und Ziel beim Kopieren */
static HWND letzt; static HMENU mlw;
static HWND zw; static short zx, zy; static BYTE zieht;   /* Ziehen: Quelle, Startpunkt, 1 = gedrueckt, 2 = zieht */
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
    o->spalten = b / ZB; if (o->spalten < 1) o->spalten = 1;
    o->zeilen = h / zh; if (o->zeilen < 1) o->zeilen = 1;
    rollen(o);
}
static void einlesen(ORDNER *o)
{
    FINDDATA f; EINTRAG FAR *e; short n = 0, pass;
    CursorBusy(TRUE);
    if (!o->h) o->h = MemAlloc((DWORD)MAXE * sizeof(EINTRAG), MEM_MOVABLE);
    if (o->h && (e = (EINTRAG FAR *)MemLock(o->h)) != 0) {
        scpy(buf, o->pfad, sizeof buf); scat(buf, "*.*", sizeof buf);
        for (pass = 0; pass < 2; pass++)          /* erst Ordner, dann Dateien */
            if (!FileFindFirst(buf, FA_DIR, &f)) do {
                if ((f.attr & FA_LABEL) || (f.name[0] == '.' && !f.name[1])) continue;
                if (((f.attr & FA_DIR) != 0) != (pass == 0) || n >= MAXE) continue;
                { short i; for (i = 0; i < 13; i++) e[n].name[i] = f.name[i]; }
                e[n].mark = 0; e[n++].attr = f.attr;
            } while (!FileFindNext(&f));
        MemUnlock(o->h);
    }
    o->n = n; o->sel = n ? 0 : -1; o->oben = 0;
    CursorBusy(FALSE);
    SetWindowText(o->w, o->pfad);
    rollen(o);
    InvalidateRect(o->w, 0, TRUE);
    { char t[8]; zahl(t, n); fmt(buf, "SHELL: Ordner %s %s", o->pfad, t); DbgPrint(buf); }
}
static void sichtbar(ORDNER *o)
{
    short z = o->sel / o->spalten;
    if (o->sel < 0) return;
    if (z < o->oben) o->oben = z; else if (z >= o->oben + o->zeilen) o->oben = z - o->zeilen + 1;
    rollen(o);
}
static void starten(LPCSTR p)
{
    DbgPrint("SHELL: Start"); DbgPrint(p);
    if (endung(p, "EXE") && TaskExec(p, "", SW_SHOW)) return;
    if (endung(p, "EXE") && LastError() != RD_ERR_FORMAT) { fmt(buf, StrGet(inst, 3), p, 0); MsgBox(0, buf, "RD", MB_OK | MB_ERROR); return; }
    if (RunDos(p, "", RD_DOS_KEEP | RD_DOS_PAUSE)) { fmt(buf, StrGet(inst, 3), p, 0); MsgBox(0, buf, "RD", MB_OK | MB_ERROR); }
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
static void neusel(ORDNER *o, short s)
{
    if (s < 0 || s >= o->n || s == o->sel) return;
    o->sel = s; sichtbar(o); InvalidateRect(o->w, 0, FALSE);
}
/* ---- Dateioperationen ------------------------------------------------------------------ */
static void fehler(LPCSTR was, WORD e)
{
    char t[8]; zahl(t, e); fmt(buf, StrGet(inst, 6), was, t); MsgBox(GetActive(), buf, "RD", MB_OK | MB_ERROR);
}
static int praefix(LPCSTR a, LPCSTR b) { while (*a && (*a | 0x20) == (*b | 0x20)) a++, b++; return !*a && (*b == '\\' || !*b); }
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
    if (FileAttr(qn, 0, 0) != 0xFFFF) {           /* Ziel gibt es schon */
        fmt(buf, StrGet(inst, 12), qn, 0);
        a = MsgBox(GetActive(), buf, "RD", MB_YESNOCANCEL | MB_ASK);
        if (a == ID_CANCEL) return 1;
        if (a != ID_YES) return 0;
        FileAttr(qn, 0, 1);
    }
    if (!(hb = MemAlloc(4096, MEM_MOVABLE))) return RD_ERR_NOMEM;
    if ((a = FileOpen(qv, FO_READ)) < 0) { MemFree(hb); return LastError(); }
    if ((b = FileCreate(qn, 0)) < 0) { e = LastError(); FileClose(a); MemFree(hb); return e; }
    p = (BYTE FAR *)MemLock(hb);
    while ((n = FileRead(a, p, 4096)) > 0) if (FileWrite(b, p, n) != n) { e = RD_ERR_FULL; break; }
    MemUnlock(hb); MemFree(hb);
    FileClose(a); FileClose(b);
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
static void loeschen(ORDNER *o)
{
    EINTRAG FAR *e; short i, n, mk = 0; WORD r = 0; char t[8];
    if (!o->h || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    for (i = 0; i < o->n; i++) mk += e[i].mark;
    if ((n = anzahl(o, e)) == 0) { MemUnlock(o->h); return; }
    zahl(t, n); fmt(buf, StrGet(inst, 7), t, 0);
    if (MsgBox(GetActive(), buf, "RD", MB_YESNO | MB_WARN) != ID_YES) { MemUnlock(o->h); return; }
    CursorBusy(TRUE);
    for (i = 0; i < o->n && !r; i++) if (betrifft(o, e, i, mk)) {
        scpy(qv, o->pfad, sizeof qv); scat(qv, e[i].name, sizeof qv);
        if ((r = loesche(e[i].attr)) != 0) fehler(e[i].name, r);
    }
    MemUnlock(o->h);
    CursorBusy(FALSE);
    DbgPrint("SHELL: geloescht");
    alle_neu();
}
static void umbenennen(ORDNER *o)
{
    EINTRAG FAR *e; char n[13]; WORD r;
    if (o->sel < 0 || !(e = (EINTRAG FAR *)MemLock(o->h))) return;
    scpy(n, e[o->sel].name, sizeof n); MemUnlock(o->h);
    if (n[0] == '.') return;
    scpy(qv, o->pfad, sizeof qv); scat(qv, n, sizeof qv);
    if (!InputBox(GetActive(), StrGet(inst, 8), "RD", n, sizeof n) || !n[0]) return;
    scpy(qn, o->pfad, sizeof qn); scat(qn, n, sizeof qn);
    if ((r = FileRename(qv, qn)) != 0) fehler(n, r);
    DbgPrint("SHELL: umbenannt");
    alle_neu();
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
    short x = px / ZB, i = (o->oben + py / zh) * o->spalten + x;
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

static DWORD RDCALLBACK ordproc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    ORDNER *o = ord(w); PAINT ps; HDC d; RECT r; EINTRAG FAR *e; short i, x, y;
    if (!o) return DefWindowProc(w, msg, p1, p2);
    switch (msg) {
    case M_SIZE: masse(o, (short)LOWORD(p2), (short)HIWORD(p2)); return 0;
    case M_PAINT:
        d = BeginPaint(w, &ps); GetClientRect(w, &r);
        SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &r);
        if (o->h && (e = (EINTRAG FAR *)MemLock(o->h)) != 0) {
            for (i = o->oben * o->spalten; i < o->n; i++) {
                x = (i % o->spalten) * ZB; y = (i / o->spalten - o->oben) * zh;
                if (y >= r.b) break;
                IconDraw(d, x + (ZB - 32) / 2, y + 2, inst, symbol(&e[i]));
                schild(d, x, y + ih + 3, ZB, e[i].name, i == o->sel);
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
        if (p1 == K_LEFT) neusel(o, o->sel - 1);
        else if (p1 == K_RIGHT) neusel(o, o->sel + 1);
        else if (p1 == K_UP) neusel(o, o->sel - o->spalten);
        else if (p1 == K_DOWN) neusel(o, o->sel + o->spalten < o->n ? o->sel + o->spalten : o->n - 1);
        else if (p1 == K_HOME) neusel(o, 0);
        else if (p1 == K_END) neusel(o, o->n - 1);
        else if (p1 == K_ENTER) oeffnen(o);
        else if (p1 == K_BACK) hoch(o);
        else if (p1 == K_INS) { markieren(o, o->sel); neusel(o, o->sel + 1); }
        else if (p1 == K_F5) kopieren(o, 0, 0);
        else if (p1 == K_F6) kopieren(o, 1, 0);
        else if (p1 == K_F2) umbenennen(o);
        else if (p1 == K_F7) neuer_ordner(o);
        else if (p1 == K_DEL || p1 == K_F8) loeschen(o);
        else return DefWindowProc(w, msg, p1, p2);
        return 0;
    case M_COMMAND:
        if (p1 == CM_OEFFNEN) oeffnen(o);
        else if (p1 == CM_HOCH) hoch(o);
        else if (p1 == CM_NEU) einlesen(o);
        else if (p1 == CM_KOPIE) kopieren(o, 0, 0);
        else if (p1 == CM_VERSCH) kopieren(o, 1, 0);
        else if (p1 == CM_UMBEN) umbenennen(o);
        else if (p1 == CM_MKDIR) neuer_ordner(o);
        else if (p1 == CM_LOESCH) loeschen(o);
        else if (p1 == CM_ZU) PostMessage(w, M_CLOSE, 0, 0);
        else PostMessage(desk, M_COMMAND, p1, 0);   /* RD-Befehle */
        return 0;
    case M_ACTIVATE:
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
static void ordner_auf(char l)
{
    DRIVEINFO di; short i; ORDNER *o = 0;
    for (i = 0; i < MAXO; i++) if (!od[i].w) { o = &od[i]; break; }
    if (!o) return;
    buf[0] = l; buf[1] = ':'; buf[2] = 0;
    if (DriveInfo((WORD)(l - 'A' + 1), &di)) { char t[80]; fmt(t, StrGet(inst, 4), buf, 0); MsgBox(0, t, "RD", MB_OK | MB_WARN); return; }
    o->pfad[0] = l; o->pfad[1] = ':'; o->pfad[2] = '\\'; o->pfad[3] = 0;
    o->h = 0; o->n = 0; o->sel = -1; o->oben = 0; o->spalten = 1; o->zeilen = 1;
    o->w = (HWND)1;                               /* belegt; M_SIZE aus CreateWindow kommt vor dem Handle */
    o->w = CreateWindow("RDOrdner", o->pfad, WS_NORMAL | WS_VSCROLL | WS_VISIBLE, CW_DEFAULT, 0, CW_DEFAULT, 0, 0, 0, inst);
    if (!o->w) return;
    { RECT r; GetClientRect(o->w, &r); masse(o, r.r, r.b); }
    MenuSet(o->w, mbar);
    einlesen(o);
}

/* ---- Schreibtisch ----------------------------------------------------------------------- */
static void lwfeld(short k, RECT *r) { r->l = 8; r->r = 8 + 56; r->t = 8 + k * (zh + 6); r->b = r->t + zh; }
static void lwmal(HDC d, short k)
{
    RECT r; char t[3];
    lwfeld(k, &r);
    IconDraw(d, r.l + (56 - 32) / 2, r.t + 2, inst, lt[k] == DT_FLOPPY ? IC_FLOPPY : IC_HDISK);
    t[0] = lw[k]; t[1] = ':'; t[2] = 0;
    schild(d, r.l, r.t + ih + 3, 56, t, k == lsel);
}
static short lwbei(short x, short y)
{
    short i; RECT r;
    for (i = 0; i < nlw; i++) { lwfeld(i, &r); if (x >= r.l && x < r.r && y >= r.t && y < r.b) return i; }
    return -1;
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
static void info(void)
{
    char v[8], f[12], *p; WORD a = KernVersion();
    p = zahl(v, a >> 8); *p++ = '.'; zahl(p, a & 0xFF);
    zahl(f, MemAvail(MEMA_KONV) >> 10);
    fmt(buf, StrGet(inst, 1), v, f);
    { char t[24]; scpy(t, StrGet(inst, 5), sizeof t); MsgBox(GetActive(), buf, t, MB_OK | MB_INFO); }
}
static DWORD RDCALLBACK deskproc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; HDC d; RECT r; short i, y;
    switch (msg) {
    case M_PAINT:
        d = BeginPaint(w, &ps); GetClientRect(w, &r);
        SetBrush(d, C_BLACK, PAT_GRAY50); SetBkColor(d, C_WHITE); FillRect(d, &r);
        for (i = 0; i < nlw; i++) lwmal(d, i);
        EndPaint(w, &ps); return 0;
    case M_BUTTONDOWN: case M_DBLCLK:
        if ((i = lwbei((short)LOWORD(p2), (short)HIWORD(p2))) >= 0) {
            if (i != lsel) { lsel = i; InvalidateRect(w, 0, FALSE); }
            if (msg == M_DBLCLK) ordner_auf(lw[i]);
            else if ((p1 & 0xFF) == 2) {
                y = (short)MenuPopup(w, mlw, 0, (short)LOWORD(p2), (short)HIWORD(p2));
                if (y) PostMessage(w, M_COMMAND, y, 0);
            }
        }
        return 0;
    case M_KEY:
        if (p1 == K_DOWN && lsel < nlw - 1) { lsel++; InvalidateRect(w, 0, FALSE); }
        else if (p1 == K_UP && lsel > 0) { lsel--; InvalidateRect(w, 0, FALSE); }
        else if (p1 == K_ENTER && nlw && (HIWORD(p2) & KS_ALT)) lwinfo();   /* Alt+Eingabe: Eigenschaften */
        else if (p1 == K_ENTER && nlw) ordner_auf(lw[lsel]);
        else return DefWindowProc(w, msg, p1, p2);
        return 0;
    case M_SYSMENU:
        i = (short)MenuPopup(w, msys, 0, 0, -1);
        if (i) PostMessage(w, M_COMMAND, i, 0);
        return 0;
    case M_COMMAND:
        if (p1 == CM_INFO) info();
        else if (p1 == CM_ENDE) { char t[40]; scpy(t, StrGet(inst, 2), sizeof t); if (MsgBox(GetActive(), t, "RD", MB_YESNO | MB_ASK) == ID_YES) PostQuit(0); }
        else if (p1 == CM_OEFFNEN && nlw) ordner_auf(lw[lsel]);
        else if (p1 == CM_LWINFO && nlw) lwinfo();
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
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); th = GetCaps(d, CAPS_TEXTH); ReleaseDC(0, d);
    mh = th + 4; ih = H <= 200 ? 16 : 32; zh = ih + th + 6;
    for (k = 1; k <= 26; k++)                     /* nur Typ, keine Diskette anfassen */
        if (!DriveInfo(k | DI_NURTYP, &di) && di.typ != DT_NONE && !(di.flags & DI_PHANTOM)) { lw[nlw] = (char)('A' + k - 1); lt[nlw++] = (char)di.typ; }
    msys = MenuLoad(inst, 1); mbar = MenuLoad(inst, 2); mlw = MenuLoad(inst, 3);
    wc.name = "RDDesk"; wc.proc = deskproc; wc.stil = CS_DBLCLKS; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    wc.name = "RDOrdner"; wc.proc = ordproc;
    RegisterClass(&wc);
    desk = CreateWindow("RDDesk", "RD", WS_DESKTOP | WS_VISIBLE, 0, mh, W, H - mh, 0, 0, inst);
    if (!desk) return 1;
    MenuSet(desk, mbar);
    DbgPrint("SHELL: bereit");
    while (GetMessage(&m)) DispatchMessage(&m);
    DbgPrint("SHELL: Ende");
    return 0;
}
