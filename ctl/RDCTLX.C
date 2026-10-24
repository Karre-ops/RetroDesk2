/* RDCTLX.C - Zusatz zu RDCTL (verwerfbar): Dateidialog. RDCTL laedt es erst, wenn ein Programm
 * FileDialog ruft. Der Dialog benutzt die Klassen aus RDCTL (EDIT, LIST, STATIC, BUTTON) und
 * DialogIndirect; der Zustand steht in einer Struktur auf dem Stapel des Aufrufers.
 * Rechte Liste: [..], [ORDNER], [-A-] Laufwerke. Eingabe von *.X aendert das Muster. */
#include "RD2.H"

#define ID_NAME 30
#define ID_DAT  31
#define ID_ORD  32
#define ID_PFAD 33
#define FA_DIR  0x10

typedef struct {
    FILEDLG FAR *fd;
    char dir[80];                                 /* immer mit \ am Ende */
    char pat[13];
} FDS;

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(LPSTR d, LPCSTR s, WORD max) { WORD i; for (i = 0; i + 1 < max && s[i]; i++) d[i] = s[i]; d[i] = 0; }
static void scat(LPSTR d, LPCSTR s, WORD max) { WORD n = slen(d); scpy(d + n, s, max - n); }
static char gross(char c) { return (char)(c >= 'a' && c <= 'z' ? c - 32 : c); }

static void fuellen(HWND d, FDS FAR *st)
{
    HWND ld = DlgItem(d, ID_DAT), lo = DlgItem(d, ID_ORD); FINDDATA f; char b[96]; WORD dr; DRIVEINFO di;
    CursorBusy(TRUE);
    SendMessage(ld, L_CLEAR, 0, 0); SendMessage(lo, L_CLEAR, 0, 0);
    scpy(b, st->dir, sizeof b); scat(b, st->pat, sizeof b);
    if (!FileFindFirst(b, 0, &f)) do SendMessage(ld, L_ADD, 0, (DWORD)(LPCSTR)f.name); while (!FileFindNext(&f));
    scpy(b, st->dir, sizeof b); scat(b, "*.*", sizeof b);
    if (!FileFindFirst(b, FA_DIR, &f)) do {
        if ((f.attr & FA_DIR) && !(f.name[0] == '.' && f.name[1] == 0)) {
            b[0] = '['; scpy(b + 1, f.name, 14); scat(b, "]", sizeof b);
            SendMessage(lo, L_ADD, 0, (DWORD)(LPCSTR)b);
        }
    } while (!FileFindNext(&f));
    for (dr = 1; dr <= 8; dr++) {                 /* Laufwerke; Phantom-B: und fehlende weglassen */
        if (dr > 1 && (DriveInfo(dr, &di) || di.typ == DT_NONE || (di.flags & DI_PHANTOM))) continue;
        b[0] = '['; b[1] = '-'; b[2] = (char)('A' + dr - 1); b[3] = '-'; b[4] = ']'; b[5] = 0;
        SendMessage(lo, L_ADD, 0, (DWORD)(LPCSTR)b);
    }
    DlgSetText(d, ID_PFAD, st->dir);
    CursorBusy(FALSE);
}
static void hoch(FDS FAR *st)                     /* letzten Ordner abschneiden */
{
    WORD n = slen(st->dir);
    if (n <= 3) return;
    for (n -= 2; n > 2 && st->dir[n] != '\\'; n--) ;
    st->dir[n + 1] = 0;
}
static void wechsel(HWND d, FDS FAR *st, LPCSTR e)   /* Eintrag der rechten Liste */
{
    if (e[0] != '[') return;
    if (e[1] == '-') { st->dir[0] = e[2]; st->dir[1] = ':'; st->dir[2] = '\\'; st->dir[3] = 0; }
    else if (e[1] == '.' && e[2] == '.') hoch(st);
    else { WORD n = slen(st->dir); scpy(st->dir + n, e + 1, sizeof st->dir - n); n = slen(st->dir); if (n) st->dir[n - 1] = '\\'; }
    fuellen(d, st);
}
static int istordner(LPCSTR p) { WORD a = FileAttr(p, 0, 0); return a != 0xFFFF && (a & FA_DIR); }
static int gibts(LPCSTR p) { WORD a = FileAttr(p, 0, 0); return a != 0xFFFF && !(a & FA_DIR); }

static BOOL RDEXPORT fdproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    FDS FAR *st = (FDS FAR *)GetWindowData(d, 1); char t[80], v[96]; WORD i, n, wild = 0;
    if (msg == M_INITDIALOG) {
        SendMessage(DlgItem(d, ID_NAME), C_SETLIMIT, 79, 0);
        { LPCSTR s = st->fd->datei, a = s; for (; *s; s++) if (*s == '\\' || *s == ':') a = s + 1; DlgSetText(d, ID_NAME, a); }
        fuellen(d, st);
        return FALSE;
    }
    if (msg != M_COMMAND) return FALSE;
    if (p1 == ID_DAT && HIWORD(p2) == N_SELECT) { DlgGetText(d, ID_DAT, t, sizeof t); DlgSetText(d, ID_NAME, t); return TRUE; }
    if (p1 == ID_DAT && HIWORD(p2) == N_DBLCLK) { SendMessage(d, M_COMMAND, ID_OK, MAKELONG(0, N_CLICKED)); return TRUE; }
    if (p1 == ID_ORD && HIWORD(p2) == N_DBLCLK) { DlgGetText(d, ID_ORD, t, sizeof t); wechsel(d, st, t); return TRUE; }
    if (p1 != ID_OK || HIWORD(p2) != N_CLICKED) return FALSE;
    if (GetFocus() == DlgItem(d, ID_ORD)) { DlgGetText(d, ID_ORD, t, sizeof t); wechsel(d, st, t); return TRUE; }
    DlgGetText(d, ID_NAME, t, sizeof t);
    n = slen(t);
    if (!n) return TRUE;
    for (i = 0; i < n; i++) { t[i] = gross(t[i]); if (t[i] == '*' || t[i] == '?') wild = 1; }
    if (n >= 2 && t[1] == ':') scpy(v, t, sizeof v);          /* absolut */
    else if (t[0] == '\\') { v[0] = st->dir[0]; v[1] = ':'; scpy(v + 2, t, sizeof v - 2); }
    else { scpy(v, st->dir, sizeof v); scat(v, t, sizeof v); }
    if (wild) {                                   /* Muster: Ordner und Muster trennen */
        for (i = slen(v); i && v[i - 1] != '\\' && v[i - 1] != ':'; i--) ;
        scpy(st->pat, v + i, sizeof st->pat); v[i] = 0;
        if (i) { scpy(st->dir, v, sizeof st->dir); if (st->dir[i - 1] == ':') scat(st->dir, "\\", sizeof st->dir); }
        DlgSetText(d, ID_NAME, ""); fuellen(d, st);
        return TRUE;
    }
    n = slen(v);
    if ((n == 2 && v[1] == ':') || (n == 3 && v[2] == '\\') || v[n - 1] == '\\' || istordner(v)) {
        if (v[n - 1] != '\\') scat(v, "\\", sizeof v);
        scpy(st->dir, v, sizeof st->dir); DlgSetText(d, ID_NAME, ""); fuellen(d, st);
        return TRUE;
    }
    if ((st->fd->flags & FD_MUSTEXIST) && !gibts(v)) { MsgBox(d, StrGet(0, 14), 0, MB_OK | MB_ERROR); return TRUE; }
    if ((st->fd->flags & FD_ASKOVER) && gibts(v) && MsgBox(d, StrGet(0, 13), 0, MB_YESNO | MB_ASK) != ID_YES) return TRUE;
    scpy(st->fd->datei, v, st->fd->len);
    return FALSE;                                 /* RDCTL beendet den Dialog mit ID_OK */
}

/* Vorlage im Speicher (Format RT_DIALOG) */
static BYTE FAR *put(BYTE FAR *p, LPCSTR s) { do *p++ = *s; while (*s++); return p; }
static BYTE FAR *elem(BYTE FAR *p, LPCSTR kl, LPCSTR tx, WORD id, short x, short y, short w, short h, DWORD st)
{
    short FAR *q;
    p = put(p, kl); p = put(p, tx); q = (short FAR *)p;
    q[0] = (short)id; q[1] = x; q[2] = y; q[3] = w; q[4] = h; *(DWORD FAR *)(q + 5) = st;
    return (BYTE FAR *)(q + 7);
}
static BOOL RDEXPORT x_FileDialog(HWND owner, FILEDLG FAR *fd)
{
    HMEM hm; BYTE FAR *v, FAR *p; short FAR *q; FDS st; short e; WORD i, n; char k[3][20]; LPCSTR f;
    if (!fd || !fd->datei || fd->len < 4) { SetLastError(RD_ERR_HANDLE); return FALSE; }
    st.fd = fd; st.pat[0] = 0;
    f = fd->filter;                               /* "Texte|*.TXT|..." : erstes Muster */
    if (f) { while (*f && *f != '|') f++; if (*f) f++; for (i = 0; i < 12 && f[i] && f[i] != '|'; i++) st.pat[i] = f[i]; st.pat[i] = 0; }
    if (!st.pat[0]) scpy(st.pat, "*.*", sizeof st.pat);
    if (fd->datei[0] && fd->datei[1] == ':') {    /* Ordner aus dem Vorschlag */
        scpy(st.dir, fd->datei, sizeof st.dir);
        for (i = slen(st.dir); i && st.dir[i - 1] != '\\'; i--) ;
        st.dir[i ? i : 2] = 0; if (!i) scat(st.dir, "\\", sizeof st.dir);
    } else {
        DirGet(0, st.dir); n = slen(st.dir);
        if (st.dir[n - 1] != '\\') scat(st.dir, "\\", sizeof st.dir);
    }
    for (i = 0; i < 3; i++) scpy(k[i], StrGet(0, i == 0 ? 12 : i + 1), sizeof k[i]);   /* Datei:, OK, Abbruch */
    if (!(hm = MemAlloc(512, MEM_MOVABLE))) return FALSE;
    v = (BYTE FAR *)MemLock(hm);
    *(DWORD FAR *)v = 0; q = (short FAR *)(v + 4); q[0] = q[1] = 0; q[2] = 228; q[3] = 124; q[4] = 7;
    p = put(v + 14, fd->titel ? fd->titel : StrGet(0, (fd->flags & FD_SAVE) ? 11 : 10));
    p = elem(p, CL_STATIC, k[0], 0xFFFF, 8, 6, 60, 10, 0);
    p = elem(p, CL_EDIT, "", ID_NAME, 8, 17, 160, 12, 0);
    p = elem(p, CL_STATIC, "", ID_PFAD, 8, 33, 212, 10, 0);
    p = elem(p, CL_LIST, "", ID_DAT, 8, 45, 96, 72, WS_VSCROLL);
    p = elem(p, CL_LIST, "", ID_ORD, 110, 45, 64, 72, WS_VSCROLL);
    p = elem(p, CL_BUTTON, k[1], ID_OK, 180, 16, 40, 14, ES_DEFAULT);
    p = elem(p, CL_BUTTON, k[2], ID_CANCEL, 180, 34, 40, 14, 0);
    e = DialogIndirect(0, v, owner, (DLGPROC)fdproc, (DWORD)(FDS FAR *)&st);
    MemUnlock(hm); MemFree(hm);
    return e == ID_OK;
}

static void RDEXPORT x_ende(void) { }
static struct { WORD n; FARPROC f[2]; } ext = { 2, { (FARPROC)x_ende, (FARPROC)x_FileDialog } };
void FAR * RDAPI RdModInit(void) { return &ext; }
