/* RDCTL.C - Steuerelemente und Dialoge (verwerfbares Modul, laedt beim ersten Aufruf nach).
 * Klassen BUTTON, CHECK, RADIO, STATIC, GROUP, EDIT und RDDIALOG; DialogRun, DialogEnd,
 * DlgItem, DlgSet/GetText, DlgSet/GetInt, DlgCheck/Checked, MsgBox.
 * Kein Zustand ueber einen Aufruf hinaus: alles steht in den Fensterdaten (data 0..3) oder
 * in Speicher, der dem Programm gehoert. Die Klassen meldet CtlInit an (RDUSER ruft es, wenn
 * ein CreateWindow eine Standardklasse nicht findet); Fensterfunktion = Sprungstueck des
 * Kerns, damit das Modul dazwischen verworfen werden darf.
 * data[3] = 'R','C' + Typ: so erkennen Dialog und Tabulator die eigenen Steuerelemente. */
#include "RD2.H"

#define T_BUTTON 1
#define T_CHECK  2
#define T_RADIO  3
#define T_STATIC 4
#define T_GROUP  5
#define T_EDIT   6
#define T_DLG    7
#define T_LIST   8
#define T_PROG   9
#define MAGIC 0x52430000UL
#define TYP(h) (((GetWindowData(h, 3) & 0xFFFF0000UL) == MAGIC) ? (WORD)GetWindowData(h, 3) : 0)
#define DLG_ENDE 0x80000000UL
#define MAXK 40

/* ---- Hilfen ------------------------------------------------------------------------ */
static DWORD callp(FARPROC f, HWND w, WORD m, WORD p1, DWORD p2)   /* Programmfunktion: DS = SS */
{
    WORD lo = 0, hi = 0;
    _asm {
        push ds
        push w
        push m
        push p1
        push word ptr p2+2
        push word ptr p2
        mov  ax, ss
        mov  ds, ax
        call dword ptr f
        pop  ds
        mov  lo, ax
        mov  hi, dx
    }
    return MAKELONG(lo, hi);
}
static void melde(HWND h, WORD n)                 /* M_COMMAND an die Eltern */
{
    HWND p = GetParent(h);
    if (p) SendMessage(p, M_COMMAND, (WORD)GetWindowData(h, WD_ID), MAKELONG(h, n));
}
static short th_(HDC d) { return GetCaps(d, CAPS_TEXTH); }
/* Text mit &: unterstreicht den Buchstaben dahinter. zeichnen 0 = nur messen */
static short ctext(HDC d, short x, short y, LPCSTR s, int zeichnen)
{
    char b[48]; WORD n = 0, u = 0xFFFF; short w, ux, h = th_(d);
    while (*s && n < sizeof b - 1) { if (*s == '&' && s[1] && s[1] != '&' && u == 0xFFFF) { u = n; s++; continue; } if (*s == '&') s++; b[n++] = *s++; }
    w = TextWidth(d, b, n);
    if (zeichnen && n) {
        TextOut(d, x, y, b, n);
        if (u != 0xFFFF) { ux = (short)(x + TextWidth(d, b, u)); Line(d, ux, (short)(y + h - 1), (short)(ux + TextWidth(d, b + u, 1) - 1), (short)(y + h - 1)); }
    }
    return w;
}
static char mkey(LPCSTR s)
{
    for (; *s; s++) if (*s == '&' && s[1] && s[1] != '&') return (char)(s[1] >= 'a' && s[1] <= 'z' ? s[1] - 32 : s[1]);
    return 0;
}
static void punkte(HDC d, short l, short t, short r, short b)   /* Fokusrahmen gepunktet */
{
    RECT q;
    SetBrush(d, C_BLACK, PAT_GRAY50); SetBkColor(d, C_WHITE);
    q.l = l; q.r = r; q.t = t; q.b = t + 1; FillRect(d, &q); q.t = b - 1; q.b = b; FillRect(d, &q);
    q.t = t; q.r = l + 1; FillRect(d, &q); q.l = r - 1; q.r = r; FillRect(d, &q);
}
static void grau(HDC d, const RECT FAR *r)        /* gesperrt: jedes zweite Pixel weiss */
{
    SetRop(d, ROP_OR); SetBkColor(d, C_BLACK); SetBrush(d, C_WHITE, PAT_GRAY50); FillRect(d, r); SetRop(d, ROP_COPY);
}
static void weiss(HDC d, const RECT FAR *r) { SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, r); }
static WORD gesperrt(HWND h) { return (GetWindowData(h, WD_STIL) & WS_DISABLED) ? 1 : 0; }
static WORD kinder(HWND p, HWND *k)               /* Kinder in Erstellungsreihenfolge */
{
    WORD n = 0, i; HWND c = 0, t;
    while (n < MAXK && (c = GetChild(p, c)) != 0) k[n++] = c;
    for (i = 0; i < n / 2; i++) { t = k[i]; k[i] = k[n - 1 - i]; k[n - 1 - i] = t; }
    return n;
}
static void neu(HWND h) { InvalidateRect(h, 0, FALSE); }
static BOOL bad_(void) { SetLastError(RD_ERR_HANDLE); return FALSE; }

/* ---- Knopf, Kaestchen, Optionsfeld: gemeinsame Maus- und Tastenfuehrung ------------ */
static void ausloesen(HWND h, WORD typ)           /* Klick oder Leertaste */
{
    HWND p, k[MAXK]; WORD n, i, j, a;
    if (typ == T_CHECK) { SetWindowData(h, 0, GetWindowData(h, 0) ? 0 : 1); neu(h); }
    else if (typ == T_RADIO) {                    /* Gruppe: zusammenhaengende Optionsfelder */
        if ((p = GetParent(h)) != 0) {
            n = kinder(p, k);
            for (i = 0; i < n && k[i] != h; i++) ;
            for (a = i; a > 0 && TYP(k[a - 1]) == T_RADIO; a--) ;
            for (j = a; j < n && TYP(k[j]) == T_RADIO; j++)
                if (k[j] != h && GetWindowData(k[j], 0)) { SetWindowData(k[j], 0, 0); neu(k[j]); }
        }
        if (!GetWindowData(h, 0)) { SetWindowData(h, 0, 1); neu(h); }
    }
    melde(h, N_CLICKED);
}
static int maus(HWND h, WORD typ, WORD msg, DWORD p2)   /* 1 = verarbeitet; data 1: Bit 0 gedrueckt, Bit 1 gefangen */
{
    RECT r; short x = (short)LOWORD(p2), y = (short)HIWORD(p2); WORD drin, z = (WORD)GetWindowData(h, 1);
    if (msg == M_BUTTONDOWN) {
        if (gesperrt(h)) return 1;
        SetFocus(h); CaptureMouse(h);
        SetWindowData(h, 1, 3); neu(h); return 1;
    }
    if (!(z & 2)) return 0;
    GetClientRect(h, &r); drin = x >= r.l && x < r.r && y >= r.t && y < r.b;
    if (msg == M_MOUSEMOVE) { if ((z & 1) != drin) { SetWindowData(h, 1, 2 | drin); neu(h); } return 1; }
    ReleaseMouse(); SetWindowData(h, 1, 0); neu(h);
    if (drin) ausloesen(h, typ);
    return 1;
}
/* Tasten, die der Dialog fuehrt (Tab, Eingabe, Esc, Pfeile, Alt+Buchstabe), an die Eltern */
static int weiterkey(HWND h, WORD k, DWORD p2, WORD pfeile)
{
    HWND p;
    if (k == K_TAB || k == K_ENTER || k == K_ESC || (HIWORD(p2) & KS_ALT) ||
        (pfeile && (k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT))) {
        if ((p = GetParent(h)) != 0 && TYP(p) == T_DLG) { SendMessage(p, M_KEY, k, p2); return 1; }
    }
    return 0;
}
/* Lange Texte (29.10.2026): der Fenstertext hat 32 Zeichen; laengere Texte von Static, Check, Radio, Knopf und Gruppe
 * stehen zusaetzlich in einem eigenen Block (Fensterdaten 2), den die Zeichenroutinen bevorzugen. */
static void ltset(HWND h, LPCSTR s)
{
    HMEM m = (HMEM)GetWindowData(h, 2); WORD n = 0; LPSTR b;
    SetWindowText(h, s);
    if (m) MemFree(m);
    m = 0; while (s[n]) n++;
    if (n > 31 && (m = MemAlloc(n + 1, MEM_MOVABLE)) != 0 && (b = (LPSTR)MemLock(m)) != 0) { do b[n] = s[n]; while (n--); MemUnlock(m); }
    SetWindowData(h, 2, m);
}
static LPCSTR ltget(HWND h, char *t, WORD n)      /* Text zum Zeichnen; danach ltfrei */
{
    HMEM m = (HMEM)GetWindowData(h, 2); LPCSTR b;
    if (m && (b = (LPCSTR)MemLock(m)) != 0) return b;
    GetWindowText(h, t, n); return t;
}
static void ltfrei(HWND h) { HMEM m = (HMEM)GetWindowData(h, 2); if (m) MemUnlock(m); }
static DWORD gemeinsam(HWND h, WORD typ, WORD msg, WORD p1, DWORD p2)
{
    char t[33];
    switch (msg) {
    case M_CREATE: SetWindowData(h, 3, MAGIC | typ); if (GetWindowData(h, WD_STIL) & ES_CHECKED) SetWindowData(h, 0, 1); return 0;
    case M_SETFOCUS: case M_KILLFOCUS: case M_ENABLE: neu(h); return 0;
    case C_SETTEXT: ltset(h, (LPCSTR)p2); InvalidateRect(h, 0, TRUE); return 0;
    case C_GETTEXT: return GetWindowText(h, (LPSTR)p2, p1);
    case M_DESTROY: { HMEM m = (HMEM)GetWindowData(h, 2); if (m) { MemFree(m); SetWindowData(h, 2, 0); } } return 0;
    case C_SETCHECK: if (GetWindowData(h, 0) != (p1 ? 1UL : 0UL)) { SetWindowData(h, 0, p1 ? 1 : 0); neu(h); } return 0;
    case C_GETCHECK: return GetWindowData(h, 0);
    case C_CLICK: if (!gesperrt(h)) ausloesen(h, typ); return 0;
    case C_MNEMO: GetWindowText(h, t, sizeof t); return mkey(t);
    case M_KEY:
        if (p1 == ' ' && typ <= T_RADIO) { if (!gesperrt(h)) ausloesen(h, typ); return 0; }
        if (weiterkey(h, p1, p2, typ <= T_RADIO)) return 0;
        break;
    case M_BUTTONDOWN: case M_MOUSEMOVE: case M_BUTTONUP:
        if (typ <= T_RADIO && maus(h, typ, msg, p2)) return 0;
        break;
    }
    return DefWindowProc(h, msg, p1, p2);
}

/* ---- BUTTON ------------------------------------------------------------------------ */
DWORD RDEXPORT c_Button(HWND h, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; RECT r, q; HDC d; char t[33]; short w, x, y, hh;
    if (msg != M_PAINT) return gemeinsam(h, T_BUTTON, msg, p1, p2);
    d = BeginPaint(h, &ps); GetClientRect(h, &r);
    weiss(d, &r); SetPen(d, C_BLACK);
    q = r; q.l++; q.r--; Frame(d, &q); q = r; q.t++; q.b--; Frame(d, &q);   /* abgerundet */
    if (GetWindowData(h, WD_STIL) & ES_DEFAULT) { q.l = r.l + 1; q.t = r.t + 1; q.r = r.r - 1; q.b = r.b - 1; Frame(d, &q); }
    GetWindowText(h, t, sizeof t); hh = th_(d);
    w = ctext(d, 0, 0, t, 0); x = (short)((r.r - w) / 2); y = (short)((r.b - hh) / 2);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK); ctext(d, x, y, t, 1);
    if (GetFocus() == h) punkte(d, (short)(x - 2), (short)(y - 1), (short)(x + w + 2), (short)(y + hh + 1));
    if (GetWindowData(h, 1) & 1) { q.l = r.l + 2; q.t = r.t + 2; q.r = r.r - 2; q.b = r.b - 2; InvertRect(d, &q); }
    if (gesperrt(h)) { q.l = r.l + 2; q.t = r.t + 2; q.r = r.r - 2; q.b = r.b - 2; grau(d, &q); }
    EndPaint(h, &ps);
    return 0;
}
/* ---- CHECK / RADIO ----------------------------------------------------------------- */
static void kasten(HWND h, WORD typ)
{
    PAINT ps; RECT r, q; HDC d; char t[33]; short s, sb, y, w, hh, x;
    d = BeginPaint(h, &ps); GetClientRect(h, &r); weiss(d, &r); hh = th_(d);
    s = (short)(hh - 2); if (s < 6) s = 6;
    sb = (short)((long)s * GetCaps(d, CAPS_ASPY) / GetCaps(d, CAPS_ASPX));   /* auf CGA breiter */
    y = (short)((r.b - s) / 2);
    q.l = 0; q.t = y; q.r = sb; q.b = (short)(y + s);
    SetPen(d, C_BLACK);
    if (typ == T_CHECK) {
        Frame(d, &q);
        if (GetWindowData(h, 0)) { Line(d, 0, y, (short)(sb - 1), (short)(y + s - 1)); Line(d, (short)(sb - 1), y, 0, (short)(y + s - 1)); }
    } else {
        SetBrush(d, C_WHITE, PAT_SOLID); Ellipse(d, &q, 1);
        if (GetWindowData(h, 0)) { q.l += sb / 4 + 1; q.r -= sb / 4 + 1; q.t += s / 4 + 1; q.b -= s / 4 + 1; SetBrush(d, C_BLACK, PAT_SOLID); Ellipse(d, &q, 1); }
    }
    if (GetWindowData(h, 1) & 1) { q.l = 0; q.t = y; q.r = sb; q.b = (short)(y + s); InvertRect(d, &q); }
    x = (short)(sb + 6);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
    w = ctext(d, x, (short)((r.b - hh) / 2), ltget(h, t, sizeof t), 1); ltfrei(h);
    if (GetFocus() == h) punkte(d, (short)(x - 2), (short)((r.b - hh) / 2 - 1), (short)(x + w + 2), (short)((r.b + hh) / 2 + 1));
    if (gesperrt(h)) grau(d, &r);
    EndPaint(h, &ps);
}
DWORD RDEXPORT c_Check(HWND h, WORD msg, WORD p1, DWORD p2)
{
    if (msg == M_PAINT) { kasten(h, T_CHECK); return 0; }
    return gemeinsam(h, T_CHECK, msg, p1, p2);
}
DWORD RDEXPORT c_Radio(HWND h, WORD msg, WORD p1, DWORD p2)
{
    if (msg == M_PAINT) { kasten(h, T_RADIO); return 0; }
    return gemeinsam(h, T_RADIO, msg, p1, p2);
}
/* ---- STATIC / GROUP ---------------------------------------------------------------- */
static LPCSTR mbtxt;                              /* MsgBox-Text: laenger als die 32 Zeichen des Fenstertexts */
DWORD RDEXPORT c_Static(HWND h, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; RECT r; HDC d; char t[33];
    if (msg != M_PAINT) return gemeinsam(h, T_STATIC, msg, p1, p2);
    d = BeginPaint(h, &ps); GetClientRect(h, &r); weiss(d, &r);
    { LPCSTR x = ltget(h, t, sizeof t);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
    if ((WORD)GetWindowData(h, WD_ID) == 0xFFFE && mbtxt) TextBox(d, &r, mbtxt, TB_WRAP);   /* G31 */
    else if (mkey(x)) ctext(d, 0, 0, x, 1); else TextBox(d, &r, x, TB_WRAP);
    ltfrei(h); }
    if (gesperrt(h)) grau(d, &r);
    EndPaint(h, &ps);
    return 0;
}
DWORD RDEXPORT c_Group(HWND h, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; RECT r, q; HDC d; char t[33]; short hh, w;
    if (msg != M_PAINT) return gemeinsam(h, T_GROUP, msg, p1, p2);
    d = BeginPaint(h, &ps); GetClientRect(h, &r); hh = th_(d);
    q = r; q.t = (short)(hh / 2); SetPen(d, C_BLACK); Frame(d, &q);
    GetWindowText(h, t, sizeof t);
    if (t[0]) {
        w = ctext(d, 0, 0, t, 0);
        q.l = 6; q.t = 0; q.r = (short)(w + 10); q.b = hh; weiss(d, &q);
        SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK); ctext(d, 8, 0, t, 1);
    }
    EndPaint(h, &ps);
    return 0;
}
/* ---- EDIT (einzeilig): data 0 = Speicher, 1 = Schreibmarke | erstes sichtbares << 16, 2 = Grenze */
#define ELIM 80
static LPSTR epuf(HWND h) { return (LPSTR)MemLock((HMEM)GetWindowData(h, 0)); }
static void eunlock(HWND h) { MemUnlock((HMEM)GetWindowData(h, 0)); }
static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void eset(HWND h, LPCSTR s)
{
    LPSTR b = epuf(h); WORD i, n = (WORD)GetWindowData(h, 2);
    if (!b) return;
    for (i = 0; i < n && s && s[i]; i++) b[i] = s[i];
    b[i] = 0; eunlock(h);
    SetWindowData(h, 1, MAKELONG(i, 0)); neu(h);
}
static void epaint(HWND h)
{
    PAINT ps; RECT r, q; HDC d; LPSTR b; WORD cur, off, n, i; short hh, x, y; char st[ELIM + 1];
    d = BeginPaint(h, &ps); GetClientRect(h, &r); weiss(d, &r); SetPen(d, C_BLACK); Frame(d, &r);
    hh = th_(d); y = (short)((r.b - hh) / 2);
    cur = LOWORD(GetWindowData(h, 1)); off = HIWORD(GetWindowData(h, 1));
    if ((b = epuf(h)) != 0) {
        n = slen(b);
        if (GetWindowData(h, WD_STIL) & ES_PASSWORD) { for (i = 0; i < n && i < ELIM; i++) st[i] = '*'; st[i] = 0; b = st; }
        q.l = 2; q.t = 1; q.r = (short)(r.r - 2); q.b = (short)(r.b - 1); SetClip(d, &q);
        SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
        if (off < n) TextOut(d, 3, y, b + off, (short)(n - off));
        if (GetFocus() == h) { x = (short)(3 + TextWidth(d, b + off, (short)(cur - off))); Line(d, x, (short)(y - 1), x, (short)(y + hh)); }
        SetClip(d, 0);
        eunlock(h);
    }
    if (gesperrt(h)) grau(d, &r);
    EndPaint(h, &ps);
}
static void esicht(HWND h, WORD cur)              /* Schreibmarke sichtbar halten */
{
    HDC d = GetDC(h); RECT r; LPSTR b; WORD off = HIWORD(GetWindowData(h, 1));
    GetClientRect(h, &r);
    if (cur < off) off = cur;
    if (d && (b = epuf(h)) != 0) {
        while (off < cur && TextWidth(d, b + off, (short)(cur - off)) > r.r - 7) off++;
        eunlock(h);
    }
    if (d) ReleaseDC(h, d);
    SetWindowData(h, 1, MAKELONG(cur, off)); neu(h);
}
static void etaste(HWND h, WORD k, WORD ch)
{
    LPSTR b = epuf(h); WORD cur = LOWORD(GetWindowData(h, 1)), n, i, lim = (WORD)GetWindowData(h, 2), ro;
    DWORD st = GetWindowData(h, WD_STIL); int geaendert = 0;
    if (!b) return;
    n = slen(b); ro = (st & ES_READONLY) ? 1 : 0;
    if (cur > n) cur = n;
    if (ch) {
        if (!ro && n < lim && (!(st & ES_NUMBER) || (ch >= '0' && ch <= '9') || (ch == '-' && cur == 0))) {
            for (i = n + 1; i > cur; i--) b[i] = b[i - 1];
            b[cur++] = (char)ch; geaendert = 1;
        } else SndBeep(880, 30);
    } else switch (k) {
        case K_LEFT: if (cur) cur--; break;
        case K_RIGHT: if (cur < n) cur++; break;
        case K_HOME: cur = 0; break;
        case K_END: cur = n; break;
        case K_BACK: if (!ro && cur) { for (i = --cur; i < n; i++) b[i] = b[i + 1]; geaendert = 1; } break;
        case K_DEL: if (!ro && cur < n) { for (i = cur; i < n; i++) b[i] = b[i + 1]; geaendert = 1; } break;
    }
    eunlock(h);
    esicht(h, cur);
    if (geaendert) melde(h, N_CHANGE);
}
DWORD RDEXPORT c_Edit(HWND h, WORD msg, WORD p1, DWORD p2)
{
    char t[33]; HMEM m; LPSTR b; WORD n, cur, off; HDC d; short x;
    switch (msg) {
    case M_CREATE:
        SetWindowData(h, 3, MAGIC | T_EDIT); SetWindowData(h, 2, ELIM);
        if (!(m = MemAlloc(ELIM + 1, MEM_MOVABLE | MEM_ZEROINIT))) return 0xFFFFFFFFUL;
        SetWindowData(h, 0, m);
        GetWindowText(h, t, sizeof t); eset(h, t);
        return 0;
    case M_DESTROY: if ((m = (HMEM)GetWindowData(h, 0)) != 0) { MemFree(m); SetWindowData(h, 0, 0); } return 0;
    case M_PAINT: epaint(h); return 0;
    case M_SETFOCUS: case M_KILLFOCUS: case M_ENABLE: neu(h); return 0;
    case C_SETTEXT: eset(h, (LPCSTR)p2); return 0;
    case C_GETTEXT:
        if (!p1 || !(b = epuf(h))) return 0;
        for (n = 0; n + 1 < p1 && b[n]; n++) ((LPSTR)p2)[n] = b[n];
        ((LPSTR)p2)[n] = 0; eunlock(h); return n;
    case C_SETLIMIT: SetWindowData(h, 2, p1 && p1 <= ELIM ? p1 : ELIM); return 0;
    case C_MNEMO: return 0;
    case M_KEY:
        if (weiterkey(h, p1, p2, 0)) return 0;
        if (p1 >= 0x100 || p1 == K_BACK) etaste(h, p1, 0);
        return 0;
    case M_CHAR: if (p1 >= ' ' && !(HIWORD(p2) & (KS_CTRL | KS_ALT))) etaste(h, 0, p1); return 0;
    case M_BUTTONDOWN:
        if (gesperrt(h)) return 0;
        SetFocus(h);
        if ((d = GetDC(h)) != 0 && (b = epuf(h)) != 0) {    /* Schreibmarke an die Klickstelle */
            off = HIWORD(GetWindowData(h, 1)); n = slen(b); x = (short)LOWORD(p2);
            for (cur = off; cur < n && 3 + TextWidth(d, b + off, (short)(cur - off + 1)) - TextWidth(d, b + cur, 1) / 2 <= x; cur++) ;
            eunlock(h); ReleaseDC(h, d); d = 0;
            SetWindowData(h, 1, MAKELONG(cur, off)); neu(h);
        }
        if (d) ReleaseDC(h, d);
        return 0;
    }
    return DefWindowProc(h, msg, p1, p2);
}

/* ---- LIST: data 0 = Speicher (Texte hintereinander, je \0), 1 = Auswahl | oberste << 16, 2 = Anzahl */
#define LBLOCK 256
static LPSTR leintrag(LPSTR b, WORD k) { while (k--) b += slen(b) + 1; return b; }
static short lzeilen(HWND h) { RECT r; HDC d = GetDC(h); short z = 1; if (d) { GetClientRect(h, &r); z = (short)((r.b - 2) / th_(d)); ReleaseDC(h, d); } return z > 0 ? z : 1; }
static void lsicht(HWND h, WORD sel)              /* Auswahl setzen und sichtbar halten */
{
    WORD n = (WORD)GetWindowData(h, 2), top = HIWORD(GetWindowData(h, 1)); short z = lzeilen(h);
    if (sel != 0xFFFF && sel >= n) sel = n ? n - 1 : 0xFFFF;
    if (sel != 0xFFFF) { if (sel < top) top = sel; if (sel >= top + z) top = (WORD)(sel - z + 1); }
    if (n <= (WORD)z) top = 0; else if (top > n - z) top = (WORD)(n - z);
    SetWindowData(h, 1, MAKELONG(sel, top));
    ScrollSet(h, SB_VERT, (short)top, (short)(n > (WORD)z ? n - z : 0), z);
    neu(h);
}
static void lpaint(HWND h)
{
    PAINT ps; RECT r, q; HDC d; LPSTR b; WORD n = (WORD)GetWindowData(h, 2), sel = LOWORD(GetWindowData(h, 1)), top = HIWORD(GetWindowData(h, 1)), k; short hh, y;
    d = BeginPaint(h, &ps); GetClientRect(h, &r); weiss(d, &r); SetPen(d, C_BLACK); Frame(d, &r);
    hh = th_(d); q.l = 1; q.t = 1; q.r = (short)(r.r - 1); q.b = (short)(r.b - 1); SetClip(d, &q);
    if ((b = epuf(h)) != 0) {
        SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
        for (k = top, y = 1; k < n && y < r.b - 1; k++, y += hh) {
            LPSTR e = leintrag(b, k);
            TextOut(d, 3, y, e, -1);
            if (k == sel) { q.t = y; q.b = (short)(y + hh); InvertRect(d, &q); if (GetFocus() == h) punkte(d, 1, y, (short)(r.r - 1), (short)(y + hh)); }
        }
        eunlock(h);
    }
    SetClip(d, 0);
    if (gesperrt(h)) grau(d, &r);
    EndPaint(h, &ps);
}
static WORD ladd(HWND h, LPCSTR t)
{
    HMEM m = (HMEM)GetWindowData(h, 0), m2; LPSTR b, e; WORD n = (WORD)GetWindowData(h, 2), i, l = slen(t) > 32 ? 32 : slen(t); DWORD gr = MemSize(m), voll;
    if (!(b = epuf(h))) return 0xFFFF;
    e = leintrag(b, n); voll = (DWORD)(e - b);
    eunlock(h);
    if (voll + l + 1 > gr) {
        if (gr + LBLOCK > 0xFFF0UL || !(m2 = MemRealloc(m, gr + LBLOCK, MEM_MOVABLE))) { SetLastError(RD_ERR_NOMEM); return 0xFFFF; }
        SetWindowData(h, 0, m2);
    }
    b = epuf(h); e = b + (WORD)voll;
    for (i = 0; i < l; i++) e[i] = t[i];
    e[l] = 0; eunlock(h);
    SetWindowData(h, 2, n + 1); lsicht(h, LOWORD(GetWindowData(h, 1)));
    return n;
}
static void lwahl(HWND h, WORD sel) { WORD alt = LOWORD(GetWindowData(h, 1)); lsicht(h, sel); if (LOWORD(GetWindowData(h, 1)) != alt) melde(h, N_SELECT); }
DWORD RDEXPORT c_List(HWND h, WORD msg, WORD p1, DWORD p2)
{
    HMEM m; LPSTR b, e; WORD n = (WORD)GetWindowData(h, 2), sel = LOWORD(GetWindowData(h, 1)), top = HIWORD(GetWindowData(h, 1)), i, l; short z;
    switch (msg) {
    case M_CREATE:
        SetWindowData(h, 3, MAGIC | T_LIST); SetWindowData(h, 1, MAKELONG(0xFFFF, 0)); SetWindowData(h, 2, 0);
        if (!(m = MemAlloc(LBLOCK, MEM_MOVABLE | MEM_ZEROINIT))) return 0xFFFFFFFFUL;
        SetWindowData(h, 0, m); return 0;
    case M_DESTROY: if ((m = (HMEM)GetWindowData(h, 0)) != 0) { MemFree(m); SetWindowData(h, 0, 0); } return 0;
    case M_PAINT: lpaint(h); return 0;
    case M_SIZE: lsicht(h, sel); return 0;
    case M_SETFOCUS: case M_KILLFOCUS: case M_ENABLE: neu(h); return 0;
    case C_MNEMO: return 0;
    case L_ADD: return ladd(h, (LPCSTR)p2);
    case L_CLEAR: SetWindowData(h, 2, 0); SetWindowData(h, 1, MAKELONG(0xFFFF, 0)); lsicht(h, 0xFFFF); return 0;
    case L_COUNT: return n;
    case L_GETSEL: return sel;
    case L_SETSEL: lsicht(h, p1); return 0;
    case C_GETTEXT: case L_GETTEXT:
        if (msg == C_GETTEXT) { l = p1; p1 = sel; } else l = 33;
        if (!p2 || !l) return 0;
        ((LPSTR)p2)[0] = 0;
        if (p1 >= n || !(b = epuf(h))) return 0;
        e = leintrag(b, p1);
        for (i = 0; i + 1 < l && e[i]; i++) ((LPSTR)p2)[i] = e[i];
        ((LPSTR)p2)[i] = 0; eunlock(h); return i;
    case L_DELETE:
        if (p1 >= n || !(b = epuf(h))) return 0;
        e = leintrag(b, p1); l = slen(e) + 1;
        { LPSTR x = leintrag(b, n); for (; e + l < x; e++) *e = e[l]; }
        eunlock(h); SetWindowData(h, 2, --n);
        if (sel != 0xFFFF && sel >= p1 && sel) sel--; else if (sel >= n) sel = 0xFFFF;
        SetWindowData(h, 1, MAKELONG(sel, top)); lsicht(h, sel); return 0;
    case M_SCROLL:
        z = lzeilen(h);
        switch (p1) {
        case SB_LINEUP: if (top) top--; break;
        case SB_LINEDOWN: top++; break;
        case SB_PAGEUP: top = top > (WORD)z ? top - z : 0; break;
        case SB_PAGEDOWN: top += z; break;
        case SB_THUMB: top = LOWORD(p2); break;
        }
        SetWindowData(h, 1, MAKELONG(sel, top)); lsicht(h, 0xFFFF); SetWindowData(h, 1, MAKELONG(sel, HIWORD(GetWindowData(h, 1)))); neu(h);
        return 0;
    case M_KEY:
        if (weiterkey(h, p1, p2, 0)) return 0;
        z = lzeilen(h);
        switch (p1) {
        case K_UP: lwahl(h, sel == 0xFFFF ? 0 : sel ? sel - 1 : 0); break;
        case K_DOWN: lwahl(h, sel == 0xFFFF ? 0 : sel + 1); break;
        case K_HOME: lwahl(h, 0); break;
        case K_END: lwahl(h, n ? n - 1 : 0); break;
        case K_PGUP: lwahl(h, sel == 0xFFFF || sel < (WORD)z ? 0 : sel - z); break;
        case K_PGDN: lwahl(h, sel == 0xFFFF ? 0 : sel + z); break;
        }
        return 0;
    case M_CHAR:                                  /* Buchstabe: naechster Eintrag damit */
        if (p1 > ' ' && n && (b = epuf(h)) != 0) {
            char c = (char)(p1 >= 'a' && p1 <= 'z' ? p1 - 32 : p1);
            for (i = 1; i <= n; i++) {
                WORD k = (WORD)((sel == 0xFFFF ? n - 1 + i : sel + i) % n); char f = *leintrag(b, k);
                if (f >= 'a' && f <= 'z') f -= 32;
                if (f == c) { eunlock(h); lwahl(h, k); return 0; }
            }
            eunlock(h);
        }
        return 0;
    case M_BUTTONDOWN: case M_DBLCLK:
        if (gesperrt(h)) return 0;
        SetFocus(h);
        { HDC d = GetDC(h); short hh = d ? th_(d) : 8; if (d) ReleaseDC(h, d);
          i = (WORD)(top + ((short)HIWORD(p2) - 1) / hh); }
        if (i < n) { lwahl(h, i); if (msg == M_DBLCLK) melde(h, N_DBLCLK); }
        return 0;
    }
    return DefWindowProc(h, msg, p1, p2);
}
/* ---- PROGRESS: data 0 = Stand, 1 = Hoechstwert ------------------------------------ */
DWORD RDEXPORT c_Progress(HWND h, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; RECT r, q; HDC d; DWORD mx;
    switch (msg) {
    case M_CREATE: SetWindowData(h, 3, MAGIC | T_PROG); SetWindowData(h, 0, 0); SetWindowData(h, 1, 100); return 0;
    case C_SETPOS: case C_SETRANGE:
        SetWindowData(h, msg == C_SETPOS ? 0 : 1, p2); neu(h); return 0;
    case M_PAINT:
        d = BeginPaint(h, &ps); GetClientRect(h, &r); weiss(d, &r); SetPen(d, C_BLACK); Frame(d, &r);
        mx = GetWindowData(h, 1); q = r; q.l++; q.t++; q.b--;
        q.r = (short)(1 + (mx ? (long)(r.r - 2) * (long)(GetWindowData(h, 0) > mx ? mx : GetWindowData(h, 0)) / (long)mx : 0));
        SetBrush(d, C_BLACK, PAT_SOLID); if (q.r > q.l) FillRect(d, &q);
        EndPaint(h, &ps); return 0;
    }
    return DefWindowProc(h, msg, p1, p2);
}

/* ---- Dialog ------------------------------------------------------------------------ */
/* data 0 = DLGPROC, 1 = Parameter, 2 = Ergebnis | DLG_ENDE */
static BOOL dproc(HWND h, WORD msg, WORD p1, DWORD p2)
{
    FARPROC f = (FARPROC)GetWindowData(h, 0);
    return f ? (BOOL)callp(f, h, msg, p1, p2) : FALSE;
}
static WORD fokusbar(HWND c) { WORD t = TYP(c); return t && t != T_STATIC && t != T_GROUP && t != T_PROG && !gesperrt(c); }   /* Fortschrittsbalken nimmt keine Tasten */
static void fokusweiter(HWND h, int rueck)
{
    HWND k[MAXK], f = GetFocus(); WORD n = kinder(h, k), i, j;
    for (i = 0; i < n && k[i] != f; i++) ;
    for (j = 1; j <= n; j++) {
        WORD z = (WORD)(rueck ? (i + 2 * n - j) % n : (i == n ? j - 1 : (i + j) % n));
        if (fokusbar(k[z])) { SetFocus(k[z]); return; }
    }
}
static void mnemo(HWND h, char c)                 /* Alt+Buchstabe bzw. Buchstabe */
{
    HWND k[MAXK]; WORD n = kinder(h, k), i, j;
    if (c >= 'a' && c <= 'z') c -= 32;
    for (i = 0; i < n; i++) if ((char)SendMessage(k[i], C_MNEMO, 0, 0) == c && !gesperrt(k[i])) break;
    if (i == n) return;
    if (!fokusbar(k[i])) {                        /* Beschriftung: das naechste Feld */
        for (j = i + 1; j < n && !fokusbar(k[j]); j++) ;
        if (j < n) SetFocus(k[j]);
        return;
    }
    SetFocus(k[i]);
    if (TYP(k[i]) != T_EDIT) SendMessage(k[i], C_CLICK, 0, 0);
}
static const char sc1[] = "qwertzuiop\0\0\0\0asdfghjkl\0\0\0\0\0yxcvbnm";   /* Scancode 10h.. (deutsche Tastatur) */
DWORD RDEXPORT c_Dialog(HWND h, WORD msg, WORD p1, DWORD p2)
{
    HWND k[MAXK], f; WORD n, i;
    switch (msg) {
    case M_CREATE: SetWindowData(h, 3, MAGIC | T_DLG); return 0;
    case M_KEY:
        if (dproc(h, msg, p1, p2)) return 0;
        f = GetFocus();
        if (p1 == K_TAB) { fokusweiter(h, (HIWORD(p2) & KS_SHIFT) != 0); return 0; }
        if (p1 == K_UP || p1 == K_LEFT) { fokusweiter(h, 1); return 0; }
        if (p1 == K_DOWN || p1 == K_RIGHT) { fokusweiter(h, 0); return 0; }
        if (p1 == K_ESC) { SendMessage(h, M_COMMAND, ID_CANCEL, MAKELONG(0, N_CLICKED)); return 0; }
        if (p1 == K_ENTER) {
            if (f && TYP(f) == T_BUTTON && GetParent(f) == h) { SendMessage(f, C_CLICK, 0, 0); return 0; }
            n = kinder(h, k);
            for (i = 0; i < n; i++) if (TYP(k[i]) == T_BUTTON && (GetWindowData(k[i], WD_STIL) & ES_DEFAULT) && !gesperrt(k[i])) { SendMessage(k[i], C_CLICK, 0, 0); return 0; }
            SendMessage(h, M_COMMAND, ID_OK, MAKELONG(0, N_CLICKED));
            return 0;
        }
        if ((HIWORD(p2) & KS_ALT) && p1 >= 0x110 && p1 < 0x110 + sizeof sc1 && sc1[p1 - 0x110]) { mnemo(h, sc1[p1 - 0x110]); return 0; }
        if ((HIWORD(p2) & KS_ALT) && ((p1 >= 0x178 && p1 <= 0x180) || (p1 >= 0x102 && p1 <= 0x10A))) { mnemo(h, (char)('1' + (p1 & 0xFF) - (p1 >= 0x178 ? 0x78 : 0x02))); return 0; }   /* Alt+Ziffer */
        if (p1 > ' ' && p1 < 0x80 && (!f || TYP(f) != T_EDIT)) { mnemo(h, (char)p1); return 0; }
        return 0;
    case M_COMMAND:
        if (!dproc(h, msg, p1, p2) && (p1 == ID_OK || p1 == ID_CANCEL || p1 == ID_YES || p1 == ID_NO) && HIWORD(p2) == N_CLICKED)
            DialogEnd(h, (short)p1);
        return 0;
    case M_CLOSE: SendMessage(h, M_COMMAND, ID_CANCEL, MAKELONG(0, N_CLICKED)); return 0;
    case M_SETFOCUS:                              /* Dialog selbst aktiviert: Fokus aufs erste Feld */
        n = kinder(h, k);
        for (i = 0; i < n; i++) if (fokusbar(k[i])) { SetFocus(k[i]); break; }
        return 0;
    }
    if (msg != M_PAINT && msg != M_ERASEBKGND && msg != M_DESTROY && dproc(h, msg, p1, p2)) return 0;
    return DefWindowProc(h, msg, p1, p2);
}

/* ---- Klassen anmelden --------------------------------------------------------------- */
#define EXP_KLASSEN 0x22                           /* Export-Nummer von c_Button, dann der Reihe nach */
static const char * const kn[] = { CL_BUTTON, CL_CHECK, CL_RADIO, CL_STATIC, CL_GROUP, CL_EDIT, "RDDIALOG", CL_LIST, CL_PROGRESS };
static const char * const kx[] = { CL_TEXT, CL_COMBO, CL_SCROLL };
static int gl(LPCSTR a, LPCSTR b) { while (*a && *a == *b) a++, b++; return *a == *b; }
void RDEXPORT c_CtlInit(LPCSTR kl)
{
    WNDCLASS wc; HMOD m; WORD i, j;
    if (!(m = ModLoad("RDCTL.MOD"))) return;      /* schon geladen: nur Zaehler */
    for (i = 0; i < sizeof kn / sizeof kn[0]; i++) {
        wc.name = kn[i]; wc.proc = (WNDPROC)ModProc(m, EXP_KLASSEN + i); wc.stil = CS_SYSTEM; wc.daten = 0;
        wc.inst = 0; wc.symbol = 0; wc.hinter = C_WHITE;
        if (wc.proc) RegisterClass(&wc);
    }
    ModFree(m);
    for (j = 0; kl && j < 3; j++) if (gl(kl, kx[j])) {    /* TEXT, COMBO, SCROLL stehen in RDCTLX */
        if (!(m = ModLoad("RDCTLX.MOD"))) return;
        for (i = 0; i < 3; i++) { wc.name = kx[i]; wc.proc = (WNDPROC)ModProc(m, 3 + i); wc.inst = i ? 0 : (0x8000 | m); if (wc.proc) RegisterClass(&wc); }
        return;                                   /* Referenz behalten (verwerfbar bleibt es), RDUSER gibt sie am Ende frei */
    }
}

/* ---- Dialog aus Vorlage ------------------------------------------------------------ */
/* Vorlage wie RT_DIALOG: DWORD stil, short x,y,w,h, WORD n, Titel\0, je Element
   Klasse\0 Text\0 WORD id, short x,y,w,h, DWORD stil. Masse in Dialogeinheiten. */
static LPCSTR ueber(LPCSTR s) { while (*s) s++; return s + 1; }
static short lauf(HINST inst, const BYTE FAR *v, HWND owner, DLGPROC proc, DWORD param)
{
    HWND dlg, c; HDC d; short cw, ch, x, y, w, hh, sw, sh, dw, dh; WORD n, i, id; DWORD st, es; LPCSTR titel, kl, tx;
    RECT r, a; MSG m; const short FAR *q; short e[4]; DWORD erg;
    c_CtlInit(0);
    if (!(d = GetDC(0))) return -1;
    cw = TextWidth(d, "0", 1); ch = GetCaps(d, CAPS_TEXTH); sw = GetCaps(d, CAPS_W); sh = GetCaps(d, CAPS_H);
    ReleaseDC(0, d);
    st = *(const DWORD FAR *)v; q = (const short FAR *)(v + 4);
    x = q[0]; y = q[1]; w = (short)(q[2] * cw / 4); hh = (short)(q[3] * ch / 8); n = (WORD)q[4];
    titel = (LPCSTR)(v + 14);
    dlg = CreateWindow("RDDIALOG", titel, (st & ~(WS_VISIBLE | WS_SIZE | WS_MIN)) | WS_TITLE | WS_BORDER | WS_CLOSE | WS_POPUP, 0, 0, w, hh, 0, 0, inst);
    if (!dlg) return -1;
    SetWindowData(dlg, 0, (DWORD)proc); SetWindowData(dlg, 1, param);
    GetWindowRect(dlg, &a); GetClientRect(dlg, &r);
    dw = (short)(w + (a.r - a.l) - r.r); dh = (short)(hh + (a.b - a.t) - r.b);   /* aussen fuer gewuenschte Innengroesse */
    if (x || y) { x = (short)(x * cw / 4); y = (short)(y * ch / 8); }
    else { x = (short)((sw - dw) / 2); y = (short)((sh - dh) / 2); }
    MoveWindow(dlg, x, y, dw, dh, FALSE);
    tx = ueber(titel);
    for (i = 0; i < n; i++) {
        kl = tx; tx = ueber(kl); q = (const short FAR *)ueber(tx);
        id = (WORD)q[0]; e[0] = (short)(q[1] * cw / 4); e[1] = (short)(q[2] * ch / 8); e[2] = (short)(q[3] * cw / 4); e[3] = (short)(q[4] * ch / 8);
        es = *(const DWORD FAR *)(q + 5);
        c = CreateWindow(kl, ueber(kl), es | WS_CHILD | WS_VISIBLE, e[0], e[1], e[2], e[3], dlg, id, inst);
        if (c && slen(ueber(kl)) > 31) SendMessage(c, C_SETTEXT, 0, (DWORD)ueber(kl));   /* lange Texte (ltset) */
        tx = (LPCSTR)(q + 7);
    }
    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(dlg, SW_SHOW); SetActive(dlg);
    if (!dproc(dlg, M_INITDIALOG, 0, param)) SendMessage(dlg, M_SETFOCUS, 0, 0);
    while (!((erg = GetWindowData(dlg, 2)) & DLG_ENDE) && IsWindow(dlg)) {
        if (!GetMessage(&m)) { PostQuit((WORD)m.p1); erg = DLG_ENDE | (WORD)ID_CANCEL; break; }
        DispatchMessage(&m);
    }
    if (owner) EnableWindow(owner, TRUE);
    if (IsWindow(dlg)) DestroyWindow(dlg);
    if (owner && IsWindow(owner)) SetActive(owner);
    return (short)LOWORD(erg);
}
short RDEXPORT c_DialogRun(HINST inst, WORD id, HWND owner, DLGPROC proc, DWORD param)
{
    HMEM r = ResLoad(inst, RT_DIALOG, id); const BYTE FAR *v; short e = -1;
    if (!r) return -1;
    if ((v = (const BYTE FAR *)MemLock(r)) != 0) { e = lauf(inst, v, owner, proc, param); MemUnlock(r); }
    ResFree(r);
    return e;
}
short RDEXPORT c_DialogIndirect(HINST inst, const void FAR *v, HWND owner, DLGPROC proc, DWORD param) { return v ? lauf(inst, (const BYTE FAR *)v, owner, proc, param) : -1; }
BOOL RDEXPORT c_FileDialog(HWND owner, FILEDLG FAR *fd)    /* steht in RDCTLX.MOD */
{
    HMOD m = ModLoad("RDCTLX.MOD"); BOOL (__far __pascal *f)(HWND, FILEDLG FAR *); BOOL r = FALSE;
    if (!m) return FALSE;
    if ((f = (BOOL (__far __pascal *)(HWND, FILEDLG FAR *))ModProc(m, 2)) != 0) r = f(owner, fd);
    ModFree(m);
    return r;
}
void RDEXPORT c_DialogEnd(HWND dlg, short e) { SetWindowData(dlg, 2, DLG_ENDE | (WORD)e); }
HWND RDEXPORT c_DlgItem(HWND dlg, WORD id)
{
    HWND c = 0;
    while ((c = GetChild(dlg, c)) != 0) if ((WORD)GetWindowData(c, WD_ID) == id) return c;
    SetLastError(RD_ERR_NOTFOUND);
    return 0;
}
void RDEXPORT c_DlgSetText(HWND dlg, WORD id, LPCSTR s) { HWND c = c_DlgItem(dlg, id); if (c) SendMessage(c, C_SETTEXT, 0, (DWORD)s); }
WORD RDEXPORT c_DlgGetText(HWND dlg, WORD id, LPSTR b, WORD n) { HWND c = c_DlgItem(dlg, id); if (b && n) b[0] = 0; return c ? (WORD)SendMessage(c, C_GETTEXT, n, (DWORD)b) : 0; }
void RDEXPORT c_DlgSetInt(HWND dlg, WORD id, long v)
{
    char b[12], *p = b + 11; DWORD u = v < 0 ? (DWORD)-v : (DWORD)v;
    *p = 0;
    do { *--p = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) *--p = '-';
    c_DlgSetText(dlg, id, p);
}
long RDEXPORT c_DlgGetInt(HWND dlg, WORD id, BOOL FAR *ok)
{
    char b[16]; WORD i = 0; long v = 0; int neg = 0, gut = 0;
    c_DlgGetText(dlg, id, b, sizeof b);
    while (b[i] == ' ') i++;
    if (b[i] == '-') { neg = 1; i++; }
    for (; b[i] >= '0' && b[i] <= '9'; i++) { v = v * 10 + (b[i] - '0'); gut = 1; }
    while (b[i] == ' ') i++;
    if (b[i]) gut = 0;
    if (ok) *ok = gut;
    return neg ? -v : v;
}
void RDEXPORT c_DlgCheck(HWND dlg, WORD id, WORD an) { HWND c = c_DlgItem(dlg, id); if (c) { if (an && TYP(c) == T_RADIO) SendMessage(c, C_CLICK, 0, 0); else SendMessage(c, C_SETCHECK, an, 0); } }
WORD RDEXPORT c_DlgChecked(HWND dlg, WORD id) { HWND c = c_DlgItem(dlg, id); return c ? (WORD)SendMessage(c, C_GETCHECK, 0, 0) : 0; }

/* ---- MsgBox: Vorlage im Speicher bauen ---------------------------------------------- */
static BYTE FAR *put(BYTE FAR *p, LPCSTR s) { do *p++ = *s; while (*s++); return p; }
static BYTE FAR *elem(BYTE FAR *p, LPCSTR kl, LPCSTR tx, WORD id, short x, short y, short w, short h, DWORD st)
{
    short FAR *q;
    p = put(p, kl); p = put(p, tx); q = (short FAR *)p;
    q[0] = (short)id; q[1] = x; q[2] = y; q[3] = w; q[4] = h; *(DWORD FAR *)(q + 5) = st;
    return (BYTE FAR *)(q + 7);
}
short RDEXPORT c_MsgBox(HWND owner, LPCSTR text, LPCSTR titel, WORD fl)
{
    HMEM hm; BYTE FAR *v, FAR *p; short FAR *q; HDC d; short cw, tw, zw, zeilen, w, h, bx, n, i; LPCSTR s;
    static const WORD knr[4][3] = { { 2, 0, 0 }, { 2, 3, 0 }, { 4, 5, 3 }, { 4, 5, 0 } };   /* Texte in DE.LNG */
    static const WORD kid[4][3] = { { ID_OK, 0, 0 }, { ID_OK, ID_CANCEL, 0 }, { ID_YES, ID_NO, ID_CANCEL }, { ID_YES, ID_NO, 0 } };
    WORD j, art = (fl & 7) == MB_YESNO ? 3 : (fl & 7) == MB_YESNOCANCEL ? 2 : (fl & 7) == MB_OKCANCEL ? 1 : 0;
    char kt[3][16]; short erg;
    if (!text) text = "";
    if (!(d = GetDC(0))) return -1;
    cw = TextWidth(d, "0", 1);
    zw = (short)(48 * 4);                          /* hoechstens 48 Zeichen je Zeile (Dialogeinheiten) */
    for (s = text, w = zeilen = 0; ; s++) {        /* je Zeile messen: breiteste bestimmt w, lange umbrechen */
        LPCSTR a = s; while (*s && *s != '\n') s++;
        tw = (short)((long)TextWidth(d, a, (short)(s - a)) * 4 / cw);
        zeilen += tw / zw + 1; if (tw > w) w = tw;
        if (!*s) break;
    }
    ReleaseDC(0, d);
    if (w > zw) w = zw;
    n = kid[art][2] ? 3 : kid[art][1] ? 2 : 1;
    if (w < n * 48 - 8) w = (short)(n * 48 - 8);
    w += 16; h = (short)(zeilen * 8 + 34);
    if (!(hm = MemAlloc(512, MEM_MOVABLE))) return -1;
    v = (BYTE FAR *)MemLock(hm);
    *(DWORD FAR *)v = 0; q = (short FAR *)(v + 4); q[0] = q[1] = 0; q[2] = w; q[3] = h; q[4] = (short)(n + 1);
    p = put(v + 14, titel ? titel : StrGet(0, (fl & 0xF0) == MB_ERROR ? 6 : 1));
    p = elem(p, CL_STATIC, "", 0xFFFE, 8, 6, (short)(w - 16), (short)(zeilen * 8), 0);   /* Text ueber mbtxt */
    bx = (short)((w - (n * 48 - 8)) / 2);
    for (j = 0; j < (WORD)n; j++) {                /* MB_DEFNO: Nein zuerst anlegen (Fokus, Vorgabe), Platz bleibt */
        LPCSTR t; WORD c;
        i = (short)((fl & MB_DEFNO) && j < 2 && n > 1 ? 1 - j : j);
        t = StrGet(0, knr[art][i]);
        for (c = 0; c < 15 && t && t[c]; c++) kt[i][c] = t[c];
        kt[i][c] = 0;
        p = elem(p, CL_BUTTON, kt[i], kid[art][i], (short)(bx + i * 48), (short)(h - 18), 40, 14, j == 0 ? ES_DEFAULT : 0);
    }
    s = mbtxt; mbtxt = text;                       /* verschachtelt: alten Text danach zurueck */
    erg = lauf(0, v, owner, 0, 0);
    mbtxt = s;
    MemUnlock(hm); MemFree(hm);
    return erg;
}

/* ---- InputBox ---------------------------------------------------------------------- */
typedef struct { LPSTR b; WORD n; } IBX;
static BOOL RDEXPORT ibproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    IBX FAR *x = (IBX FAR *)GetWindowData(d, 1); HWND e = c_DlgItem(d, 20);
    (void)p2;
    if (msg == M_INITDIALOG) { SendMessage(e, C_SETLIMIT, x->n - 1, 0); SendMessage(e, C_SETTEXT, 0, (DWORD)x->b); }
    if (msg == M_COMMAND && p1 == ID_OK) SendMessage(e, C_GETTEXT, x->n, (DWORD)x->b);
    return FALSE;
}
BOOL RDEXPORT c_InputBox(HWND owner, LPCSTR frage, LPCSTR titel, LPSTR buf, WORD len)
{
    HMEM hm; BYTE FAR *v, FAR *p; short FAR *q; IBX x; short e; char k[2][16]; WORD i, j;
    if (!buf || len < 2) return bad_();
    if (!(hm = MemAlloc(384, MEM_MOVABLE))) return FALSE;
    v = (BYTE FAR *)MemLock(hm);
    *(DWORD FAR *)v = 0; q = (short FAR *)(v + 4); q[0] = q[1] = 0; q[2] = 200; q[3] = 62; q[4] = 4;
    p = put(v + 14, titel ? titel : StrGet(0, 1));
    p = elem(p, CL_STATIC, frage ? frage : "", 0xFFFF, 8, 6, 184, 10, 0);
    p = elem(p, CL_EDIT, "", 20, 8, 20, 184, 12, 0);
    for (i = 0; i < 2; i++) { LPCSTR t = StrGet(0, 2 + i); for (j = 0; j < 15 && t && t[j]; j++) k[i][j] = t[j]; k[i][j] = 0; }
    p = elem(p, CL_BUTTON, k[0], ID_OK, 52, 42, 44, 14, ES_DEFAULT);
    p = elem(p, CL_BUTTON, k[1], ID_CANCEL, 104, 42, 44, 14, 0);
    x.b = buf; x.n = len;
    e = lauf(0, v, owner, (DLGPROC)ibproc, (DWORD)(IBX FAR *)&x);
    MemUnlock(hm); MemFree(hm);
    return e == ID_OK;
}

static void RDEXPORT c_ende(void) { }
#define C(n) (FARPROC)c_##n
/* Exporte: 1 = Ende, 2 + (API - 0x2C0) bis 0x2DF (CtlInit), dann 0x22.. die Klassen */
static struct { WORD n; FARPROC f[0x2A]; } ext = { 0x2A, {
    C(ende), C(DialogRun), C(DialogEnd), C(DlgItem), C(DlgSetText), C(DlgGetText), C(DlgSetInt), C(DlgGetInt),
    C(DlgCheck), C(DlgChecked), C(DialogIndirect), 0, 0, 0, 0, 0,
    0, C(MsgBox), C(FileDialog), C(InputBox), 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    C(CtlInit), C(Button), C(Check), C(Radio), C(Static), C(Group), C(Edit), C(Dialog), C(List), C(Progress),
} };
void FAR * RDAPI RdModInit(void) { return &ext; }
