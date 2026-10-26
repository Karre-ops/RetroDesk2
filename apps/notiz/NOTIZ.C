/* NOTIZ.C - Zubehoer Notizblock (RD1 1.2: bis 80 Zeilen x 60 Zeichen, Strg+S speichert, Rueckfrage beim Schliessen).
 * Befehlszeile: [T] [Datei]. Ohne Datei NOTIZ.TXT im RD2-Verzeichnis. Zu lange Dateien nur lesend (kein Datenverlust).
 * Das Eingabefeld ist das TEXT-Steuerelement aus RDCTLX (Grenzen ueber C_SETLIMIT). "T": Meldungen fuer test/notiz.sh. */
#include "RD2.H"

#define ZEILEN  80
#define SPALTEN 60
#define GRENZE  (ZEILEN * (SPALTEN + 1))
#define ID_TEXT 10
#define CM_SPEICHERN 101
#define CM_OEFFNEN   102
#define CM_UNTER     103
#define CM_ENDE      109
static HINST hi; static HWND fe, ed; static HMENU mn; static BYTE test, geaendert, nurlesen;
static char pfad[80], titel[100];

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static void kopf(void)                            /* Titel: Notizblock - PFAD [*] [(nur lesen)] */
{
    scpy(titel, StrGet(hi, 1), sizeof titel); scat(titel, " - ", sizeof titel); scat(titel, pfad, sizeof titel);
    if (geaendert) scat(titel, " *", sizeof titel);
    if (nurlesen) { scat(titel, " ", sizeof titel); scat(titel, StrGet(hi, 2), sizeof titel); }
    SetWindowText(fe, titel);
}
static void laden(void)                           /* Datei lesen, Grenzen pruefen; fehlt sie: leer */
{
    HMEM m; char FAR *b; short f; WORD n = 0, i, j, z = 0, s = 0;
    nurlesen = 0; geaendert = 0;
    if (!(m = MemAlloc(8193, MEM_MOVABLE | MEM_ZEROINIT)) || !(b = (char FAR *)MemLock(m))) { if (m) MemFree(m); return; }
    if ((f = FileOpen(pfad, FO_READ)) >= 0) {
        n = FileRead(f, b, 8192); if (n == 0xFFFF) n = 0;
        if (n == 8192) nurlesen = 1;              /* groesser als der Puffer: abgeschnitten, also nur lesen */
        FileClose(f);
    }
    for (i = j = 0; i < n; i++) {                 /* \r weg, Ctrl+Z (Dateiende) beachten, Zeilen und Spalten zaehlen */
        char c = b[i];
        if (c == 26) break;
        if (c == '\r') continue;
        if (c == '\n') { z++; s = 0; } else if (++s > SPALTEN) nurlesen = 1;
        b[j++] = c;
    }
    b[j] = 0;
    if (z >= ZEILEN + (j && b[j - 1] != '\n') || j > GRENZE) nurlesen = 1;
    SendMessage(ed, C_SETLIMIT, nurlesen ? 8192 : GRENZE, nurlesen ? 0 : MAKELONG(SPALTEN, ZEILEN));
    SetWindowData(ed, WD_STIL, nurlesen ? GetWindowData(ed, WD_STIL) | ES_READONLY : GetWindowData(ed, WD_STIL) & ~ES_READONLY);
    SendMessage(ed, C_SETTEXT, 0, (DWORD)b);
    MemUnlock(m); MemFree(m);
    MenuEnable(mn, CM_SPEICHERN, !nurlesen);
    kopf();
    if (test) { DbgPrint(nurlesen ? "NOTIZ: geladen, nur lesen" : "NOTIZ: geladen"); DbgPrint(pfad); }
}
static BYTE speichern(void)                       /* \n -> \r\n, sicher schreiben (erst .$$$) */
{
    HMEM m; char FAR *b; WORD n, i, j; BYTE ok = 0;
    if (nurlesen) return 0;
    if (!(m = MemAlloc(2 * GRENZE + 2, MEM_MOVABLE)) || !(b = (char FAR *)MemLock(m))) { if (m) MemFree(m); return 0; }
    n = (WORD)SendMessage(ed, C_GETTEXT, GRENZE + 1, (DWORD)(b + GRENZE));
    for (i = j = 0; i < n; i++) { if (b[GRENZE + i] == '\n') b[j++] = '\r'; b[j++] = b[GRENZE + i]; }
    ok = FileSaveSafe(pfad, b, j) == 0;
    MemUnlock(m); MemFree(m);
    if (ok) { geaendert = 0; kopf(); if (test) DbgPrint("NOTIZ: gespeichert"); }
    else MsgBox(fe, StrGet(hi, 3), StrGet(hi, 1), MB_OK | MB_ERROR);
    return ok;
}
static BYTE weg(void)                             /* vor dem Schliessen/Oeffnen: 1 = weiter */
{
    short r;
    if (!geaendert || nurlesen) return 1;
    r = MsgBox(fe, StrGet(hi, 4), StrGet(hi, 1), MB_YESNOCANCEL | MB_ASK);
    if (r == ID_YES) return speichern();
    return r == ID_NO;
}
static void dialog(BYTE sp)                       /* Oeffnen / Speichern unter */
{
    FILEDLG d; char p[80];
    scpy(p, pfad, sizeof p);
    d.groesse = sizeof d; d.flags = sp ? FD_SAVE | FD_ASKOVER : FD_OPEN | FD_MUSTEXIST;
    d.titel = StrGet(hi, sp ? 6 : 5); d.filter = StrGet(hi, 7); d.datei = p; d.len = sizeof p;
    if (!sp && !weg()) return;
    if (!FileDialog(fe, &d)) return;
    scpy(pfad, p, sizeof pfad);
    if (sp) { nurlesen = 0; speichern(); kopf(); } else laden();
}
static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    RECT r;
    switch (msg) {
    case M_SIZE: GetClientRect(w, &r); if (ed) MoveWindow(ed, 0, 0, r.r, r.b, TRUE); return 0;
    case M_ACTIVATE: if (p1 && ed) SetFocus(ed); return 0;
    case M_SETFOCUS: if (ed) SetFocus(ed); return 0;
    case M_KEY:                                   /* Strg+S vom Eingabefeld */
        if (p1 == 0x13 && (HIWORD(p2) & KS_CTRL)) speichern();
        else if (p1 == 0x0F && (HIWORD(p2) & KS_CTRL)) dialog(0);
        else if (p1 == K_ESC) PostMessage(w, M_CLOSE, 0, 0);
        else break;
        return 0;
    case M_COMMAND:
        if (p1 == ID_TEXT) { if (HIWORD(p2) == N_CHANGE && !geaendert) { geaendert = 1; kopf(); } return 0; }
        if (p1 == CM_SPEICHERN) speichern();
        else if (p1 == CM_OEFFNEN) dialog(0);
        else if (p1 == CM_UNTER) dialog(1);
        else if (p1 == CM_ENDE) PostMessage(w, M_CLOSE, 0, 0);
        if (ed) SetFocus(ed);
        return 0;
    case M_CLOSE: if (weg()) DestroyWindow(w); return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H, cw, kh, fw, fh; RECT r; SYSINFO si;
    (void)show; hi = inst;
    if (cmd && cmd[0] == 'T' && (cmd[1] == ' ' || !cmd[1])) { test = 1; cmd += cmd[1] ? 2 : 1; }
    while (cmd && *cmd == ' ') cmd++;
    if (cmd && *cmd) scpy(pfad, cmd, sizeof pfad);
    else { KernInfo(&si); scpy(pfad, si.home, sizeof pfad); if (slen(pfad) && pfad[slen(pfad) - 1] != '\\') scat(pfad, "\\", sizeof pfad); scat(pfad, "NOTIZ.TXT", sizeof pfad); }
    wc.name = "RdNotiz"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); kh = GetCaps(d, CAPS_TEXTH); cw = TextWidth(d, "M", 1); ReleaseDC(0, d);
    fw = (short)(SPALTEN * cw + 40); if (fw > W) fw = W;      /* 60 Zeichen, Rand, Rollbalken */
    fh = (short)(H * 2 / 3);
    fe = CreateWindow("RdNotiz", StrGet(inst, 1), WS_NORMAL | WS_VISIBLE, (short)((W - fw) / 2), (short)(H / 8), fw, fh, 0, 1, inst);
    if (!fe) return 1;
    mn = MenuLoad(inst, 1); MenuSet(fe, mn);
    GetClientRect(fe, &r);
    ed = CreateWindow(CL_TEXT, "", WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, r.r, r.b, fe, ID_TEXT, inst);
    (void)kh;
    if (!ed) { DestroyWindow(fe); return 1; }
    laden();
    SetFocus(ed);
    if (test) DbgPrint("NOTIZ: bereit");
    while (GetMessage(&m)) DispatchMessage(&m);
    MenuFree(mn);
    return 0;
}
