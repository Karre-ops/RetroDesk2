/* PAPIERK.EXE - Papierkorb (Zubehoer, RD-Menue): zeigt X:\PAPIERK\PAPIERK.LST ("Name|Originalpfad" je Zeile),
 * Wiederherstellen (an den alten Ort), Loeschen (endgueltig), Leeren. Befehlszeile: Laufwerksbuchstabe, sonst C.
 * Die Shell legt beim Loeschen auf der Festplatte Dateien hier ab (SHELL.C papier). */
#include "RD2.H"

#define NMAX 100
#define ZL   80
#define ID_LISTE 40
#define ID_ZURUECK 21
#define ID_WEG   22
#define ID_LEER  23
static HINST hi; static char kb[16], lst[24], z[ZL + 16]; static HMEM hm; static WORD n;
static DWORD rest; static BYTE kaputt;            /* rest: hier beginnen nicht geladene Eintraege (ueber NMAX); kaputt: Lesefehler, nie zurueckschreiben */

static WORD slen(LPCSTR s) { WORD k = 0; while (s[k]) k++; return k; }
static void scpy(char *d, LPCSTR s, WORD k) { while (--k && *s) *d++ = *s++; *d = 0; }
static char FAR *eintrag(WORD i) { char FAR *b = (char FAR *)MemLock(hm); return b ? b + i * ZL : 0; }
static void lesen(void)                           /* PAPIERK.LST -> Tabelle */
{
    short f; WORD k = 0, r; char c; char FAR *e;
    n = 0; rest = 0; kaputt = 0; if ((f = FileOpen(lst, FO_READ)) < 0) return;
    if (!(e = eintrag(0))) { FileClose(f); kaputt = 1; return; }
    SetLastError(0);
    while (n < NMAX && (r = FileRead(f, &c, 1)) == 1) {
        rest++;
        if (c == '\r') continue;
        if (c == '\n') { e[n * ZL + k] = 0; if (k) n++; k = 0; continue; }
        if (k < ZL - 1) e[n * ZL + k++] = c;
    }
    if (k && n < NMAX) { e[n * ZL + k] = 0; n++; }
    if (LastError()) kaputt = 1;                  /* Audit 24.11.: unvollstaendig gelesene Liste nie zurueckschreiben */
    MemUnlock(hm); FileClose(f);
}
static void schreiben(void)
{
    short f, a; WORD i, l, ok = 1, w = n; char FAR *e; char t[24], b[64];   /* erst PAPIERK.$$$, Rest ueber NMAX anhaengen, dann ersetzen */
    if (kaputt) return;
    scpy(t, lst, sizeof t); l = slen(t); t[l - 3] = t[l - 2] = t[l - 1] = '$';
    if ((f = FileCreate(t, 0)) < 0) return;
    if (!(e = eintrag(0))) { FileClose(f); FileDelete(t); return; }
    for (i = 0; i < n && ok; i++) { l = slen(e + i * ZL); ok = FileWrite(f, e + i * ZL, l) == l && FileWrite(f, "\r\n", 2) == 2; }
    MemUnlock(hm);
    if (ok && (a = FileOpen(lst, FO_READ)) >= 0) {
        SetLastError(0); FileSeek(a, (long)rest, 0);
        while (ok && (l = FileRead(a, b, sizeof b)) != 0) { ok = FileWrite(f, b, l) == l; w |= l; }
        if (LastError()) ok = 0;
        FileClose(a);
    }
    if (FileClose(f)) ok = 0;
    if (ok && !w) { FileDelete(t); FileDelete(lst); n = 0; rest = 0; return; }   /* leer: Liste loeschen wie bisher */
    if (ok) { FileDelete(lst); ok = !FileRename(t, lst); }
    if (!ok) FileDelete(t); else lesen();   /* neu laden: rest gilt fuer die neue Datei */
}
static void teile(WORD i, char *name, char *orig)  /* name = X:\PAPIERK\Name, orig = Originalpfad */
{
    char FAR *e = eintrag(i); WORD k = 0, j;
    scpy(name, kb, 16); j = slen(name);
    if (!e) { name[0] = orig[0] = 0; return; }
    while (e[k] && e[k] != '|' && j < 28) name[j++] = e[k++];
    name[j] = 0; if (e[k] == '|') k++;
    for (j = 0; e[k] && j < ZL - 1; ) orig[j++] = e[k++];
    orig[j] = 0; MemUnlock(hm);
}
static void raus(WORD i)                          /* Eintrag i aus der Tabelle */
{
    char FAR *e = eintrag(0); WORD k;
    if (!e) return;
    for (k = (WORD)(i * ZL); k < (WORD)((n - 1) * ZL); k++) e[k] = e[k + ZL];
    MemUnlock(hm); n--;
}
static void fuellen(HWND d)
{
    HWND l = DlgItem(d, ID_LISTE); WORD i; char nm[32];
    SendMessage(l, L_CLEAR, 0, 0);
    for (i = 0; i < n; i++) { teile(i, nm, z); SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)z); }
    if (n) SendMessage(l, L_SETSEL, 0, 0);
    DbgHex("PAPIERK: Eintraege", n);
}
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD i, e; char nm[32];
    (void)p2;
    if (msg == M_INITDIALOG) { fuellen(d); SetFocus(DlgItem(d, ID_LISTE)); return TRUE; }
    if (msg != M_COMMAND) return FALSE;
    i = (WORD)SendMessage(DlgItem(d, ID_LISTE), L_GETSEL, 0, 0);
    if (p1 == ID_ZURUECK || p1 == ID_WEG) {
        if (i >= n) return TRUE;
        teile(i, nm, z);
        if (p1 == ID_ZURUECK) e = FileRename(nm, z);
        else { FileAttr(nm, 0, 1); e = FileDelete(nm); }
        if (e && p1 == ID_ZURUECK) { MsgBox(d, StrGet(hi, 2), StrGet(hi, 1), MB_OK | MB_WARN); return TRUE; }
        DbgPrint(p1 == ID_ZURUECK ? "PAPIERK: zurueck" : "PAPIERK: geloescht"); DbgPrint(z);
        raus(i); schreiben(); fuellen(d); return TRUE;
    }
    if (p1 == ID_LEER) {
        if (!n || MsgBox(d, StrGet(hi, 3), StrGet(hi, 1), MB_YESNO | MB_WARN | MB_DEFNO) != ID_YES) return TRUE;
        for (i = 0; i < n; i++) { teile(i, nm, z); FileAttr(nm, 0, 1); FileDelete(nm); }
        n = 0; schreiben(); DbgPrint("PAPIERK: geleert"); fuellen(d); return TRUE;
    }
    return FALSE;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    (void)show; hi = inst;
    while (cmd && *cmd == ' ') cmd++;
    kb[0] = (char)(cmd && *cmd >= 'A' && *cmd <= 'Z' ? *cmd : cmd && *cmd >= 'a' && *cmd <= 'z' ? *cmd - 32 : 'C');
    kb[1] = ':'; kb[2] = 0; scpy(kb + 2, "\\PAPIERK\\", 12);
    scpy(lst, kb, sizeof lst); scpy(lst + slen(lst), "PAPIERK.LST", 12);
    if (!(hm = MemAlloc((DWORD)NMAX * ZL, MEM_MOVABLE))) return 1;
    lesen();
    DialogRun(inst, 10, 0, dproc, 0);
    MemFree(hm);
    return 0;
}
