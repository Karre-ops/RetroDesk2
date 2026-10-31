/* RDCTLX.C - Zusatz zu RDCTL (verwerfbar): Dateidialog. RDCTL laedt es erst, wenn ein Programm
 * FileDialog ruft. Der Dialog benutzt die Klassen aus RDCTL (EDIT, LIST, STATIC, BUTTON) und
 * DialogIndirect; der Zustand steht in einer Struktur auf dem Stapel des Aufrufers.
 * Dazu die Klassen TEXT (mehrzeilig), COMBO (Klappliste) und SCROLL; RDCTL meldet sie erst
 * an, wenn ein Programm eine davon anlegt.
 * Rechte Liste: [..], [ORDNER], [-A-] Laufwerke. Eingabe von *.X aendert das Muster. */
#include "RD2.H"

#define ID_NAME 30
#define ID_DAT  31
#define ID_ORD  32
#define ID_PFAD 33
#define ID_TYP  34

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
        if (dr > 1 && (DriveInfo(dr | DI_NURTYP, &di) || di.typ == DT_NONE || (di.flags & DI_PHANTOM))) continue;
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
static WORD fteil(LPCSTR f, WORD k, LPSTR o, WORD max)   /* Teil k von "Name|Muster|..." */
{
    WORD i;
    if (!f) return 0;
    while (k && *f) { if (*f++ == '|') k--; }
    if (k || !*f) return 0;
    for (i = 0; i + 1 < max && f[i] && f[i] != '|'; i++) o[i] = f[i];
    o[i] = 0; return 1;
}
static WORD art(LPCSTR p) { WORD a = FileAttr(p, 0, 0); return a == 0xFFFF ? 0 : (a & FA_DIR) ? 2 : 1; }   /* 0 fehlt, 1 Datei, 2 Ordner */
#define istordner(p) (art(p) == 2)
#define gibts(p) (art(p) == 1)

static BOOL RDEXPORT fdproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    FDS FAR *st = (FDS FAR *)GetWindowData(d, 1); char t[80], v[96]; WORD i, n, wild = 0;
    if (msg == M_INITDIALOG) {
        SendMessage(DlgItem(d, ID_NAME), C_SETLIMIT, 79, 0);
        { LPCSTR s = st->fd->datei, a = s; for (; *s; s++) if (*s == '\\' || *s == ':') a = s + 1; DlgSetText(d, ID_NAME, a); }
        for (i = 0; fteil(st->fd->filter, 2 * i, t, 40) && fteil(st->fd->filter, 2 * i + 1, v, 13); i++)
            SendMessage(DlgItem(d, ID_TYP), L_ADD, 0, (DWORD)(LPCSTR)t);
        if (!i) SendMessage(DlgItem(d, ID_TYP), L_ADD, 0, (DWORD)(LPCSTR)"*.*");
        SendMessage(DlgItem(d, ID_TYP), L_SETSEL, 0, 0);
        fuellen(d, st);
        return FALSE;
    }
    if (msg != M_COMMAND) return FALSE;
    if (p1 == ID_DAT && HIWORD(p2) == N_SELECT) { DlgGetText(d, ID_DAT, t, sizeof t); DlgSetText(d, ID_NAME, t); return TRUE; }
    if (p1 == ID_DAT && HIWORD(p2) == N_DBLCLK) { SendMessage(d, M_COMMAND, ID_OK, MAKELONG(0, N_CLICKED)); return TRUE; }
    if (p1 == ID_ORD && HIWORD(p2) == N_DBLCLK) { DlgGetText(d, ID_ORD, t, sizeof t); wechsel(d, st, t); return TRUE; }
    if (p1 == ID_TYP && HIWORD(p2) == N_SELECT) {
        if (!fteil(st->fd->filter, 2 * (WORD)SendMessage(DlgItem(d, ID_TYP), L_GETSEL, 0, 0) + 1, st->pat, sizeof st->pat)) scpy(st->pat, "*.*", sizeof st->pat);
        fuellen(d, st); return TRUE;
    }
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
    if (slen(v) >= st->fd->len) { MsgBox(d, StrGet(0, 16), 0, MB_OK | MB_ERROR); return TRUE; }   /* nicht still kuerzen */
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
static const char *const kn[] = { CL_STATIC, CL_EDIT, CL_LIST, CL_BUTTON, CL_COMBO };
typedef struct { BYTE kl, tx; WORD id; BYTE x, y, w, h, st; } VL;   /* kl: kn[], tx: Text k[tx] (9 = leer), st: 1 Rollbalken, 2 Standardknopf */
static const VL vl[9] = {
    { 0, 0, 0xFFFF, 8, 6, 60, 10, 0 }, { 1, 9, ID_NAME, 8, 17, 160, 12, 0 }, { 0, 3, 0xFFFF, 8, 33, 46, 10, 0 },
    { 0, 9, ID_PFAD, 8, 48, 212, 10, 0 }, { 2, 9, ID_DAT, 8, 60, 96, 60, 1 }, { 2, 9, ID_ORD, 110, 60, 64, 60, 1 },
    { 3, 1, ID_OK, 180, 16, 40, 14, 2 }, { 3, 2, ID_CANCEL, 180, 34, 40, 14, 0 },
    { 4, 9, ID_TYP, 56, 31, 112, 56, 0 } };   /* COMBO zuletzt: liegt aufgeklappt oben */
static BOOL RDEXPORT x_FileDialog(HWND owner, FILEDLG FAR *fd)
{
    HMEM hm; BYTE FAR *v, FAR *p; short FAR *q; FDS st; short e; WORD i, n; char k[4][20];
    if (!fd || !fd->datei || fd->len < 4) { SetLastError(RD_ERR_HANDLE); return FALSE; }
    st.fd = fd; st.pat[0] = 0;
    if (!fteil(fd->filter, 1, st.pat, sizeof st.pat)) scpy(st.pat, "*.*", sizeof st.pat);
    if (fd->datei[0] && fd->datei[1] == ':') {    /* Ordner aus dem Vorschlag */
        scpy(st.dir, fd->datei, sizeof st.dir);
        for (i = slen(st.dir); i && st.dir[i - 1] != '\\'; i--) ;
        st.dir[i ? i : 2] = 0; if (!i) scat(st.dir, "\\", sizeof st.dir);
    } else {
        DirGet(0, st.dir); n = slen(st.dir);
        if (st.dir[n - 1] != '\\') scat(st.dir, "\\", sizeof st.dir);
    }
    for (i = 0; i < 4; i++) scpy(k[i], StrGet(0, i == 0 ? 12 : i == 3 ? 15 : i + 1), sizeof k[i]);   /* Datei:, OK, Abbruch, Dateityp: */
    if (!(hm = MemAlloc(512, MEM_MOVABLE))) return FALSE;
    v = (BYTE FAR *)MemLock(hm);
    *(DWORD FAR *)v = 0; q = (short FAR *)(v + 4); q[0] = q[1] = 0; q[2] = 228; q[3] = 126; q[4] = 9;
    { char tt[61]; scpy(tt, fd->titel ? fd->titel : StrGet(0, (fd->flags & FD_SAVE) ? 11 : 10), sizeof tt); p = put(v + 14, tt); }   /* Vorlage hat 512 Bytes */
    for (i = 0; i < 9; i++) { const VL *x = &vl[i]; p = elem(p, kn[x->kl], x->tx < 4 ? k[x->tx] : "", x->id, x->x, x->y, x->w, x->h, x->st == 1 ? WS_VSCROLL : x->st ? ES_DEFAULT : 0); }
    e = DialogIndirect(0, v, owner, (DLGPROC)fdproc, (DWORD)(FDS FAR *)&st);
    MemUnlock(hm); MemFree(hm);
    return e == ID_OK;
}

/* ---- gemeinsame Hilfen (wie RDCTL) ------------------------------------------------- */
#define MAGIC 0x52430000UL
#define T_DLG    7
#define T_TEXT   10
#define T_COMBO  11
#define T_SCROLL 12
static WORD istdlg(HWND p) { return p && GetWindowData(p, 3) == (MAGIC | T_DLG); }
static void melde(HWND h, WORD n) { HWND p = GetParent(h); if (p) SendMessage(p, M_COMMAND, (WORD)GetWindowData(h, WD_ID), MAKELONG(h, n)); }
static int weiter(HWND h, WORD k, DWORD p2)       /* Tab, Esc, Alt+.. an den Dialog; Strg+Buchstabe, Esc und F1 an jede Eltern */
{
    HWND p = GetParent(h);
    if ((k == K_TAB || k == K_ESC || (HIWORD(p2) & KS_ALT)) && istdlg(p)) { SendMessage(p, M_KEY, k, p2); return 1; }
    if ((((HIWORD(p2) & KS_CTRL) && k < 0x20) || k == K_ESC || k == K_F1) && p) {   /* F1: Hilfe des Programms */ SendMessage(p, M_KEY, k, p2); return 1; }
    return 0;
}
static void neu(HWND h) { InvalidateRect(h, 0, FALSE); }
static WORD gesperrt(HWND h) { return (GetWindowData(h, WD_STIL) & WS_DISABLED) ? 1 : 0; }
static void weiss(HDC d, const RECT FAR *r) { SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, r); }
static void grau(HDC d, const RECT FAR *r) { SetRop(d, ROP_OR); SetBkColor(d, C_BLACK); SetBrush(d, C_WHITE, PAT_GRAY50); FillRect(d, r); SetRop(d, ROP_COPY); }
static void punkte(HDC d, short l, short t, short r, short b)
{
    RECT q;
    SetBrush(d, C_BLACK, PAT_GRAY50); SetBkColor(d, C_WHITE);
    q.l = l; q.r = r; q.t = t; q.b = t + 1; FillRect(d, &q); q.t = b - 1; q.b = b; FillRect(d, &q);
    q.t = t; q.r = l + 1; FillRect(d, &q); q.l = r - 1; q.r = r; FillRect(d, &q);
}
static short zh(HWND h) { HDC d = GetDC(h); short z = 8; if (d) { z = GetCaps(d, CAPS_TEXTH); ReleaseDC(h, d); } return z; }

/* ---- TEXT: mehrzeilig. data 0 = Speicher, 1 = Schreibmarke | oberste Zeile << 16,
   2 = Zeichengrenze | Spalten << 16 | Zeilen << 24 (0 = frei). C_SETLIMIT p1 Zeichen (bis TMAX, Puffer waechst),
   p2 = MAKELONG(Spalten, Zeilen). Strg+Taste geht an die Eltern (z. B. Strg+S im Notizblock). */
#define TLIM 4096
#define TMAX 8192
static LPSTR tb(HWND h) { return (LPSTR)MemLock((HMEM)GetWindowData(h, 0)); }
static void tu(HWND h) { MemUnlock((HMEM)GetWindowData(h, 0)); }
/* Ein Durchgang statt drei (8 KB auf dem 8088): Zeilenumbrueche gesamt, *zc = vor pos, *len = Laenge */
static WORD zaehl(LPCSTR b, WORD pos, WORD *zc, WORD *len)
{
    LPCSTR p = b; WORD z = 0, i = 0; char c;
    for (; i < pos && (c = *p) != 0; i++, p++) if (c == '\n') z++;
    *zc = z;
    if (i == pos) for (; (c = *p) != 0; i++, p++) if (c == '\n') z++;
    *len = i; return z;
}
static WORD zanf(LPCSTR b, WORD z) { WORD i = 0; while (z && b[i]) { if (b[i++] == '\n') z--; } return i; }
static WORD zlen(LPCSTR b, WORD a) { WORD n = 0; while (b[a + n] && b[a + n] != '\n') n++; return n; }
static short tsicht(HWND h) { RECT r; short z; GetClientRect(h, &r); z = (short)((r.b - 4) / zh(h)); return z > 0 ? z : 1; }
static void tset(HWND h, WORD cur, WORD top)      /* Schreibmarke sichtbar halten, Balken setzen */
{
    LPSTR b = tb(h); WORD z, n, l; short v = tsicht(h);
    if (!b) return;
    n = zaehl(b, cur, &z, &l) + 1; tu(h);
    if (z < top) top = z;
    if (z >= top + v) top = (WORD)(z - v + 1);
    if (n <= (WORD)v) top = 0; else if (top > n - v) top = (WORD)(n - v);
    SetWindowData(h, 1, MAKELONG(cur, top));
    if (GetWindowData(h, WD_STIL) & WS_VSCROLL) ScrollSet(h, SB_VERT, (short)top, (short)(n > (WORD)v ? n - v : 0), v);
    neu(h);
}
static void tpaint(HWND h)
{
    PAINT ps; RECT r, q; HDC d; LPSTR b; WORD cur = LOWORD(GetWindowData(h, 1)), top = HIWORD(GetWindowData(h, 1)), a, n, z; short hh, y, x;
    d = BeginPaint(h, &ps); GetClientRect(h, &r); weiss(d, &r); SetPen(d, C_BLACK); Frame(d, &r);
    hh = zh(h); q.l = 2; q.t = 1; q.r = (short)(r.r - 1); q.b = (short)(r.b - 1); SetClip(d, &q);
    if ((b = tb(h)) != 0) {
        SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
        a = zanf(b, top);
        for (z = top, y = 2; y < r.b - 1; z++, y += hh) {
            n = zlen(b, a);
            if (n) TextOut(d, 3, y, b + a, (short)n);
            if (GetFocus() == h && cur >= a && cur <= a + n) { x = (short)(3 + TextWidth(d, b + a, (short)(cur - a))); Line(d, x, (short)(y - 1), x, (short)(y + hh)); }
            if (!b[a + n]) break;
            a += n + 1;
        }
        tu(h);
    }
    SetClip(d, 0);
    if (gesperrt(h)) grau(d, &r);
    EndPaint(h, &ps);
}
static void ttaste(HWND h, WORD k, WORD ch)
{
    LPSTR b = tb(h); WORD cur = LOWORD(GetWindowData(h, 1)), top = HIWORD(GetWindowData(h, 1)), n, i, a, sp, z, m, lim = (WORD)GetWindowData(h, 2);
    BYTE sm = (BYTE)(GetWindowData(h, 2) >> 16), zm = (BYTE)(GetWindowData(h, 2) >> 24);
    int ro = (GetWindowData(h, WD_STIL) & ES_READONLY) != 0, ge = 0; short v;
    if (!b) return;
    m = zaehl(b, cur, &z, &n); if (cur > n) cur = n;   /* m = letzte Zeile */
    a = zanf(b, z); sp = cur - a;
    if (ch) {
        if (!ro && n < lim && (ch == '\n' ? !zm || m + 1 < zm : !sm || zlen(b, a) < sm)) { for (i = n + 1; i > cur; i--) b[i] = b[i - 1]; b[cur++] = (char)ch; ge = 1; }
        else SndBeep(880, 30);
    } else switch (k) {
        case K_LEFT: if (cur) cur--; break;
        case K_RIGHT: if (cur < n) cur++; break;
        case K_HOME: cur = a; break;
        case K_END: cur = a + zlen(b, a); break;
        case K_UP: case K_PGUP: case K_DOWN: case K_PGDN:
            v = (k == K_PGUP || k == K_PGDN) ? tsicht(h) : 1;
            if (k == K_UP || k == K_PGUP) z = z > (WORD)v ? z - v : 0;
            else z = z + v > m ? m : z + v;
            a = zanf(b, z); i = zlen(b, a); cur = a + (sp < i ? sp : i);
            break;
        case K_BACK: if (!ro && cur) { for (i = --cur; i < n; i++) b[i] = b[i + 1]; ge = 1; } break;
        case K_DEL: if (!ro && cur < n) { for (i = cur; i < n; i++) b[i] = b[i + 1]; ge = 1; } break;
    }
    tu(h); tset(h, cur, top);
    if (ge) melde(h, N_CHANGE);
}
static DWORD RDEXPORT x_Text(HWND h, WORD msg, WORD p1, DWORD p2)
{
    HMEM m; LPSTR b; WORD i, j, n, top; HDC d; short hh, x, y;
    switch (msg) {
    case M_CREATE:
        SetWindowData(h, 3, MAGIC | T_TEXT); SetWindowData(h, 2, TLIM); SetWindowData(h, 1, 0);
        if (!(m = MemAlloc(TLIM + 1, MEM_MOVABLE | MEM_ZEROINIT))) return 0xFFFFFFFFUL;
        SetWindowData(h, 0, m); tset(h, 0, 0); return 0;
    case M_DESTROY: if ((m = (HMEM)GetWindowData(h, 0)) != 0) { MemFree(m); SetWindowData(h, 0, 0); } return 0;
    case M_PAINT: tpaint(h); return 0;
    case M_SIZE: case M_SETFOCUS: case M_KILLFOCUS: case M_ENABLE: neu(h); return 0;
    case C_MNEMO: return 0;
    case C_SETLIMIT:
        if (!p1 || p1 > TMAX) p1 = TMAX;
        if ((m = (HMEM)GetWindowData(h, 0)) != 0 && MemSize(m) < (DWORD)p1 + 1) {
            if (!(m = MemRealloc(m, (DWORD)p1 + 1, MEM_MOVABLE | MEM_ZEROINIT))) return 1;   /* bleibt beim alten Puffer */
            SetWindowData(h, 0, m);
        }
        SetWindowData(h, 2, p1 | (p2 << 16 & 0x00FF0000UL) | (p2 << 8 & 0xFF000000UL)); return 0;
    case C_SETTEXT:                               /* \r\n und \n; \r faellt weg */
        if (!(b = tb(h))) return 0;
        n = (WORD)GetWindowData(h, 2);
        for (i = j = 0; p2 && ((LPCSTR)p2)[i] && j < n; i++) if (((LPCSTR)p2)[i] != '\r') b[j++] = ((LPCSTR)p2)[i];
        b[j] = 0; tu(h); tset(h, 0, 0); return 0;
    case C_GETTEXT:
        if (!p1 || !(b = tb(h))) return 0;
        for (n = 0; n + 1 < p1 && b[n]; n++) ((LPSTR)p2)[n] = b[n];
        ((LPSTR)p2)[n] = 0; tu(h); return n;
    case M_SCROLL:
        top = HIWORD(GetWindowData(h, 1));
        switch (p1) {
        case SB_LINEUP: if (top) top--; break;
        case SB_LINEDOWN: top++; break;
        case SB_PAGEUP: top = top > (WORD)tsicht(h) ? top - tsicht(h) : 0; break;
        case SB_PAGEDOWN: top += tsicht(h); break;
        case SB_THUMB: top = LOWORD(p2); break;
        }
        if ((b = tb(h)) != 0) { n = zaehl(b, 0, &i, &j); tu(h); if (top > n) top = n; }
        SetWindowData(h, 1, MAKELONG(LOWORD(GetWindowData(h, 1)), top));
        ScrollSet(h, SB_VERT, (short)top, ScrollGet(h, SB_VERT) >= 0 ? (short)0x7FFF : 0, tsicht(h));
        tset(h, LOWORD(GetWindowData(h, 1)) , top);   /* Balken neu; Schreibmarke bleibt */
        SetWindowData(h, 1, MAKELONG(LOWORD(GetWindowData(h, 1)), top)); neu(h);
        return 0;
    case M_KEY:
        if (weiter(h, p1, p2)) return 0;
        if (p1 == K_ENTER) ttaste(h, 0, '\n');
        else if (p1 >= 0x100 || p1 == K_BACK) ttaste(h, p1, 0);
        return 0;
    case M_CHAR: if (p1 >= ' ' && !(HIWORD(p2) & (KS_CTRL | KS_ALT))) ttaste(h, 0, p1); return 0;
    case M_BUTTONDOWN:
        if (gesperrt(h)) return 0;
        SetFocus(h);
        if ((b = tb(h)) != 0 && (d = GetDC(h)) != 0) {
            hh = GetCaps(d, CAPS_TEXTH); top = HIWORD(GetWindowData(h, 1));
            y = (short)HIWORD(p2) - 2; x = (short)LOWORD(p2);
            i = zanf(b, top + (y > 0 ? y / hh : 0)); n = zlen(b, i);
            for (j = 0; j < n && 3 + TextWidth(d, b + i, (short)(j + 1)) - TextWidth(d, b + i + j, 1) / 2 <= x; j++) ;
            ReleaseDC(h, d); tu(h); tset(h, i + j, top);
        } else if (b) tu(h);
        return 0;
    }
    return DefWindowProc(h, msg, p1, p2);
}

/* ---- COMBO: Klappliste. data 0 = Liste (Kind), 1 = volle Hoehe, 2 = Bit 0 offen, Bit 1 Taste weitergereicht.
   Offen waechst das Fenster nach unten; darunter sollte im Dialog nichts liegen (letztes Element). */
static void kauf(HWND h, int auf)
{
    RECT a; POINT pt; HWND l = (HWND)GetWindowData(h, 0); short bh = (short)(zh(h) + 4);
    if (((GetWindowData(h, 2) & 1) != 0) == (auf != 0)) return;
    GetWindowRect(h, &a); pt.x = a.l; pt.y = a.t; ScreenToClient(GetParent(h), &pt);
    if (auf) SetWindowData(h, 2, 1 | (SetWindowData(h, WD_ZORDER, 0) << 16));   /* 30.10.: offen ueber allen Geschwistern, alter Platz im oberen Wort */
    MoveWindow(h, pt.x, pt.y, (short)(a.r - a.l), auf ? (short)GetWindowData(h, 1) : bh, TRUE);
    if (auf) { MoveWindow(l, 0, bh, (short)(a.r - a.l), (short)((short)GetWindowData(h, 1) - bh), FALSE); ShowWindow(l, SW_SHOW); }
    else { ShowWindow(l, SW_HIDE); SetWindowData(h, WD_ZORDER, GetWindowData(h, 2) >> 16); SetWindowData(h, 2, 0); }   /* Tab-Reihenfolge wie vorher */
    neu(h);
}
static DWORD RDEXPORT x_Combo(HWND h, WORD msg, WORD p1, DWORD p2)
{
    HWND l = (HWND)GetWindowData(h, 0); PAINT ps; RECT r, q; HDC d; char t[34]; short bh, x, y, i; DWORD f;
    switch (msg) {
    case M_CREATE:
        SetWindowData(h, 3, MAGIC | T_COMBO);
        GetClientRect(h, &r); bh = (short)(zh(h) + 4);
        SetWindowData(h, 1, r.b > bh * 2 ? r.b : bh * 5); SetWindowData(h, 2, 0);
        if (!(l = CreateWindow(CL_LIST, "", WS_CHILD | WS_VSCROLL, 0, bh, r.r, (short)(GetWindowData(h, 1) - bh), h, 1, 0))) return 0xFFFFFFFFUL;
        SetWindowData(h, 0, l);
        { RECT a; POINT pt; GetWindowRect(h, &a); pt.x = a.l; pt.y = a.t; ScreenToClient(GetParent(h), &pt); MoveWindow(h, pt.x, pt.y, r.r, bh, FALSE); }
        return 0;
    case M_PAINT:
        d = BeginPaint(h, &ps); GetClientRect(h, &r); bh = (short)(zh(h) + 4); r.b = bh;
        weiss(d, &r); SetPen(d, C_BLACK); Frame(d, &r);
        q = r; q.l = (short)(r.r - bh); Frame(d, &q);
        x = (short)(q.l + bh / 2); y = (short)(bh / 2 - 1);
        for (i = 0; i < 4; i++) Line(d, (short)(x - 3 + i), (short)(y - 1 + i), (short)(x + 3 - i), (short)(y - 1 + i));
        SendMessage(l, C_GETTEXT, sizeof t, (DWORD)(LPSTR)t);
        SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
        q.l = 2; q.t = 1; q.r = (short)(r.r - bh); q.b = (short)(bh - 1); SetClip(d, &q);
        TextOut(d, 4, 2, t, -1); SetClip(d, 0);
        if (GetFocus() == h) punkte(d, 2, 2, (short)(r.r - bh - 1), (short)(bh - 2));
        if (gesperrt(h)) grau(d, &r);
        EndPaint(h, &ps); return 0;
    case M_SETFOCUS: case M_ENABLE: neu(h); return 0;
    case M_KILLFOCUS: if (GetFocus() != l) kauf(h, 0); neu(h); return 0;
    case C_MNEMO: return 0;
    case L_ADD: case L_CLEAR: case L_COUNT: case L_GETSEL: case L_SETSEL: case L_GETTEXT: case L_DELETE: case C_GETTEXT:
        f = SendMessage(l, msg, p1, p2); if (msg != L_COUNT && msg != L_GETSEL && msg != L_GETTEXT && msg != C_GETTEXT) neu(h); return f;
    case M_COMMAND:                               /* von der Liste */
        if (HIWORD(p2) == N_SELECT || HIWORD(p2) == N_DBLCLK) {
            if ((GetWindowData(h, 2) & 3) == 1) { kauf(h, 0); SetFocus(h); }   /* Mausklick in der offenen Liste */
            neu(h); melde(h, N_SELECT);
        }
        return 0;
    case M_KEY:
        f = GetWindowData(h, 2);
        if ((p1 == K_DOWN && (HIWORD(p2) & KS_ALT)) || p1 == K_F4) { kauf(h, !(f & 1)); return 0; }
        if (f & 1) {
            if (p1 == K_ENTER || p1 == K_ESC) { kauf(h, 0); return 0; }
            if (p1 == K_TAB) kauf(h, 0);
        }
        if (weiter(h, p1, p2)) return 0;
        if (p1 == K_ENTER && istdlg(GetParent(h))) { SendMessage(GetParent(h), M_KEY, p1, p2); return 0; }
        if (p1 == K_UP || p1 == K_DOWN || p1 == K_HOME || p1 == K_END || p1 == K_PGUP || p1 == K_PGDN) {
            SetWindowData(h, 2, f | 2); SendMessage(l, M_KEY, p1, p2); SetWindowData(h, 2, f);
        }
        return 0;
    case M_CHAR: SetWindowData(h, 2, GetWindowData(h, 2) | 2); SendMessage(l, M_CHAR, p1, p2); SetWindowData(h, 2, GetWindowData(h, 2) & ~2UL); return 0;
    case M_BUTTONDOWN:
        if (gesperrt(h)) return 0;
        SetFocus(h); kauf(h, !(GetWindowData(h, 2) & 1)); return 0;
    }
    return DefWindowProc(h, msg, p1, p2);
}

/* ---- SCROLL: eigener Rollbalken (WS_VSCROLL oder WS_HSCROLL angeben). data 0 = Stand, 1 = Hoechstwert, 2 = Seite */
static WORD sbar(HWND h) { return (GetWindowData(h, WD_STIL) & WS_HSCROLL) ? SB_HORZ : SB_VERT; }
static void sset(HWND h, long p)
{
    long mx = (long)GetWindowData(h, 1);
    if (p < 0) p = 0;
    if (p > mx) p = mx;
    SetWindowData(h, 0, (DWORD)p);
    ScrollSet(h, sbar(h), (short)p, (short)mx, (short)GetWindowData(h, 2));
}
static DWORD RDEXPORT x_Scroll(HWND h, WORD msg, WORD p1, DWORD p2)
{
    long p = (long)GetWindowData(h, 0), pg = (long)GetWindowData(h, 2), alt = p; PAINT ps; RECT r; HDC d;
    switch (msg) {
    case M_CREATE: SetWindowData(h, 3, MAGIC | T_SCROLL); SetWindowData(h, 1, 100); SetWindowData(h, 2, 10); sset(h, 0); return 0;
    case C_SETPOS: sset(h, (long)p2); return 0;
    case C_SETRANGE: SetWindowData(h, 1, p2); sset(h, p); return 0;
    case C_GETPOS: return (DWORD)p;
    case C_MNEMO: return 0;
    case M_SETFOCUS: case M_KILLFOCUS: neu(h); return 0;
    case M_PAINT:
        d = BeginPaint(h, &ps); GetClientRect(h, &r); weiss(d, &r);
        if (GetFocus() == h && r.r > 2 && r.b > 2) punkte(d, 0, 0, r.r, r.b);
        EndPaint(h, &ps); return 0;
    case M_SCROLL: case M_KEY:
        if (msg == M_KEY) {
            if (weiter(h, p1, p2)) return 0;
            if (p1 == K_UP || p1 == K_LEFT) p1 = SB_LINEUP; else if (p1 == K_DOWN || p1 == K_RIGHT) p1 = SB_LINEDOWN;
            else if (p1 == K_PGUP) p1 = SB_PAGEUP; else if (p1 == K_PGDN) p1 = SB_PAGEDOWN;
            else if (p1 == K_HOME) p1 = SB_TOP; else if (p1 == K_END) p1 = SB_BOTTOM;
            else { if (p1 == K_ENTER && istdlg(GetParent(h))) SendMessage(GetParent(h), M_KEY, p1, p2); return 0; }
        }
        switch (p1) {
        case SB_LINEUP: p--; break;
        case SB_LINEDOWN: p++; break;
        case SB_PAGEUP: p -= pg; break;
        case SB_PAGEDOWN: p += pg; break;
        case SB_THUMB: p = (short)LOWORD(p2); break;
        case SB_TOP: p = 0; break;
        case SB_BOTTOM: p = (long)GetWindowData(h, 1); break;
        }
        sset(h, p);
        if ((long)GetWindowData(h, 0) != alt) melde(h, N_CHANGE);
        return 0;
    case M_BUTTONDOWN: if (!gesperrt(h)) SetFocus(h); return 0;
    }
    return DefWindowProc(h, msg, p1, p2);
}

static void RDEXPORT x_ende(void) { }
static struct { WORD n; FARPROC f[5]; } ext = { 5, { (FARPROC)x_ende, (FARPROC)x_FileDialog, (FARPROC)x_Text, (FARPROC)x_Combo, (FARPROC)x_Scroll } };
void FAR * RDAPI RdModInit(void) { return &ext; }
