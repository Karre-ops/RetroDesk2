/* HILFE.C - Hilfeanzeige (Checkliste Punkt 10, F1). Start ueber HelpShow (RDHILFE.MOD) mit "datei thema",
 * Datei ohne Pfad liegt im RD2-Ordner. Befehlszeile "T datei thema": Testmodus, meldet ueber DbgPrint (test/hilfe.sh).
 * Hilfedatei (.HLP, Text CP437, siehe docs/HILFE.md):
 *   @nummer Titel     beginnt ein Thema (0 = Inhalt)
 *   Text              jede Zeile ist ein Absatz und wird umbrochen; Leerzeile = Abstand
 *   {Text|nummer}     Verweis auf ein anderes Thema
 *   # ...             Kommentar
 * Tasten: Pfeile/Bild/Pos1/Ende blaettern, Tab/Umschalt+Tab Verweis waehlen, Eingabe folgt, Rueckschritt zurueck,
 * I Inhalt, Esc schliesst. Maus: Klick auf Verweis folgt, rechte Taste zurueck. */
#include "RD2.H"

#define TXMAX 4096
#define LNMAX 300
#define LKMAX 40
#define HRMAX 48
static HINST hi; static HWND fe; static BYTE test;
static char datei[80], titel[48], tx[TXMAX]; static WORD tn;
static WORD lz[LKMAX], ll[LKMAX], nlk;           /* Ziel und Zeile je Verweis */
static WORD ln[LNMAX + 1], lk0[LNMAX + 1], nl;   /* Zeilenanfang in tx, Zahl der Verweise davor */
static BYTE lin[LNMAX + 1];                       /* Zeile beginnt mitten in einem Verweis */
static short top, vis, kh, sel = -1;
static struct { short x0, x1, y; BYTE k; } hr[HRMAX]; static WORD nhr;
static WORD hist[8], nh, thema;
static char blk[512]; static WORD bp, bn; static short fh;

#define RAND 6
static short zeile(char *b, WORD max)            /* naechste Zeile der Datei ohne CR/LF, -1 am Ende */
{
    WORD k = 0; char c;
    for (;;) {
        if (bp >= bn) { bn = FileRead(fh, blk, sizeof blk); bp = 0; if (!bn || bn == 0xFFFF) { bn = 0; return k ? (short)k : -1; } }
        c = blk[bp++];
        if (c == '\n') break;
        if (c != '\r' && k < max - 1) b[k++] = c;
    }
    b[k] = 0; return (short)k;
}
static WORD zahl(const char *p, WORD *i) { WORD v = 0; while (p[*i] >= '0' && p[*i] <= '9') v = (WORD)(v * 10 + (p[(*i)++] - '0')); return v; }
static void anhaengen(const char *z)              /* eine Absatzzeile nach tx, {Text|n} -> 01 Text 02 */
{
    WORD i = 0, j;
    while (z[i] && tn < TXMAX - 3) {
        if (z[i] == '{') {
            for (j = i + 1; z[j] && z[j] != '|' && z[j] != '}'; j++) ;
            if (z[j] == '|' && nlk < LKMAX) {
                WORD k = j + 1, n = zahl(z, &k);
                if (z[k] == '}') {
                    lz[nlk++] = n; tx[tn++] = 1;
                    for (i++; i < j && tn < TXMAX - 2; i++) tx[tn++] = z[i];
                    tx[tn++] = 2; i = (WORD)(k + 1); continue;
                }
            }
        }
        tx[tn++] = z[i++];
    }
    tx[tn++] = '\n';
}
static BOOL laden(WORD n)                         /* Thema n in tx; FALSE: Datei fehlt */
{
    char z[200]; short l; BYTE drin = 0, gef = 0; WORD i;
    tn = 0; nlk = 0; titel[0] = 0;
    if ((fh = FileOpen(datei, FO_READ)) < 0) return FALSE;
    bp = bn = 0;
    while ((l = zeile(z, sizeof z)) >= 0) {
        if (z[0] == '#') continue;
        if (z[0] == '@') {
            if (drin) break;
            i = 1; if (zahl(z, &i) == n) {
                drin = gef = 1; while (z[i] == ' ') i++;
                { WORD k = 0; while (z[i] && k < sizeof titel - 1) titel[k++] = z[i++]; titel[k] = 0; }
            }
            continue;
        }
        if (drin) anhaengen(z);
    }
    FileClose(fh);
    if (!gef && n) return laden(0);
    thema = n; return TRUE;
}
static short breite(HDC d, WORD a, WORD e)        /* Breite von tx[a..e) ohne Marken */
{
    short w = 0; WORD s;
    while (a < e) {
        if (tx[a] == 1 || tx[a] == 2) { a++; continue; }
        for (s = a; a < e && tx[a] != 1 && tx[a] != 2; a++) ;
        w = (short)(w + TextWidth(d, tx + s, (short)(a - s)));
    }
    return w;
}
static void umbruch(void)                         /* Zeilen ln[] fuer die Fensterbreite */
{
    RECT r; HDC d; short W, x = 0, sw; WORD p = 0, e, k = 0; BYTE in = 0;
    GetClientRect(fe, &r); W = (short)(r.r - 2 * RAND); vis = (short)((r.b - RAND) / kh); if (vis < 1) vis = 1;
    if (!(d = GetDC(fe))) return;
    sw = TextWidth(d, " ", 1); nl = 0; ln[0] = 0; lk0[0] = 0; lin[0] = 0;
    while (p < tn && nl < LNMAX) {
        if (tx[p] == '\n') { p++; ln[++nl] = p; lk0[nl] = k; lin[nl] = in = 0; x = 0; continue; }
        if (tx[p] == ' ') { if (x) x = (short)(x + sw); p++; if (!x) ln[nl] = p; continue; }
        for (e = p; e < tn && tx[e] != ' ' && tx[e] != '\n'; e++) ;
        { short ww = breite(d, p, e); WORD q;
          if (x && x + ww > W) { ln[++nl] = p; lk0[nl] = k; lin[nl] = in; x = 0; }
          for (q = p; q < e; q++) { if (tx[q] == 1) { in = 1; if (k < LKMAX) ll[k++] = nl; } else if (tx[q] == 2) in = 0; }
          x = (short)(x + ww); p = e; }
    }
    if (p >= tn) ln[nl] = tn; else ln[++nl] = p;
    ReleaseDC(fe, d);
    if (top > nl - vis) top = (short)(nl - vis);
    if (top < 0) top = 0;
    ScrollSet(fe, SB_VERT, top, nl > vis ? (short)(nl - vis) : 0, vis);
}
static void unter(HDC d, short x0, short x, short y, WORD k)   /* Verweis unterstreichen, merken, Auswahl umkehren */
{
    RECT q;
    Line(d, x0, (short)(y + kh - 2), (short)(x - 1), (short)(y + kh - 2));
    if (nhr < HRMAX) { hr[nhr].x0 = x0; hr[nhr].x1 = x; hr[nhr].y = y; hr[nhr].k = (BYTE)k; nhr++; }
    if ((short)k == sel) { q.l = (short)(x0 - 1); q.t = y; q.r = (short)(x + 1); q.b = (short)(y + kh); InvertRect(d, &q); }
}
static void zeichnen(HDC d)
{
    RECT r; short i, y, x, x0; WORD a, e, s, k, nx; BYTE in;
    GetClientRect(fe, &r); SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &r);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK); SetPen(d, C_BLACK); nhr = 0;
    for (i = top, y = RAND; i < (short)nl && i < top + vis; i++, y = (short)(y + kh)) {
        a = ln[i]; e = ln[i + 1]; nx = lk0[i]; in = lin[i]; k = (WORD)(nx - 1); x = x0 = RAND;
        while (a < e && tx[a] != '\n') {
            if (tx[a] == 1) { in = 1; k = nx++; x0 = x; a++; continue; }
            if (tx[a] == 2) { if (in) unter(d, x0, x, y, k); in = 0; a++; continue; }
            for (s = a; a < e && tx[a] != 1 && tx[a] != 2 && tx[a] != '\n'; a++) ;
            TextOut(d, x, y, tx + s, (short)(a - s)); x = (short)(x + TextWidth(d, tx + s, (short)(a - s)));
        }
        if (in) unter(d, x0, x, y, k);            /* Verweis geht in der naechsten Zeile weiter */
    }
}
static void titelsetzen(void)
{
    char t[64]; LPCSTR h = StrGet(hi, 1); WORD i = 0, k = 0;
    while (h && h[k] && i < 20) t[i++] = h[k++];
    t[i++] = ':'; t[i++] = ' ';
    for (k = 0; titel[k] && i < sizeof t - 1; k++) t[i++] = titel[k];
    t[i] = 0; SetWindowText(fe, t);
}
static void melden(void)                          /* Testmodus: Thema, Titel, Zeilen, Verweise */
{
    char z[80]; WORD i = 0, k, v;
    if (!test) return;
    { static const char p[] = "HILFE: Thema "; for (k = 0; p[k]; k++) z[i++] = p[k]; }
    { char b[6]; WORD n = 0; v = thema; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) z[i++] = b[--n]; }
    z[i++] = ' ';
    for (k = 0; titel[k] && i < 60; k++) z[i++] = titel[k];
    z[i] = 0; DbgPrint(z);
    DbgHex("HILFE: Zeilen", nl); DbgHex("HILFE: Verweise", nlk); DbgHex("HILFE: Auswahl", (WORD)sel);
}
static void zeigen(WORD n, BOOL merken)
{
    if (merken && nh < 8) hist[nh++] = thema; else if (merken) { WORD i; for (i = 1; i < 8; i++) hist[i - 1] = hist[i]; hist[7] = thema; }
    if (!laden(n)) return;
    top = 0; sel = nlk ? 0 : -1;
    titelsetzen(); umbruch(); InvalidateRect(fe, 0, TRUE); melden();
}
static void sichtbar(void)                        /* gewaehlten Verweis ins Bild holen */
{
    if (sel < 0) return;
    if ((short)ll[sel] < top) top = (short)ll[sel];
    if ((short)ll[sel] >= top + vis) top = (short)(ll[sel] - vis + 1);
    ScrollSet(fe, SB_VERT, top, nl > vis ? (short)(nl - vis) : 0, vis);
}
static void blaettern(short n)
{
    short alt = top, m = nl > vis ? (short)(nl - vis) : 0;
    top = (short)(top + n); if (top > m) top = m; if (top < 0) top = 0;
    if (top != alt) { ScrollSet(fe, SB_VERT, top, m, vis); InvalidateRect(fe, 0, FALSE); }
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; WORD i;
    switch (msg) {
    case M_PAINT: BeginPaint(w, &ps); zeichnen(ps.dc); EndPaint(w, &ps); return 0;
    case M_SIZE: if (fe) { umbruch(); InvalidateRect(w, 0, FALSE); } return 0;
    case M_SCROLL:
        switch (p1) {
        case SB_LINEUP: blaettern(-1); break; case SB_LINEDOWN: blaettern(1); break;
        case SB_PAGEUP: blaettern((short)-vis); break; case SB_PAGEDOWN: blaettern(vis); break;
        case SB_THUMB: blaettern((short)((short)LOWORD(p2) - top)); break;
        case SB_TOP: blaettern(-32000); break; case SB_BOTTOM: blaettern(32000); break;
        }
        return 0;
    case M_KEY:
        switch (p1) {
        case K_ESC: DestroyWindow(w); return 0;
        case K_UP: blaettern(-1); return 0; case K_DOWN: blaettern(1); return 0;
        case K_PGUP: blaettern((short)-vis); return 0; case K_PGDN: blaettern(vis); return 0;
        case K_HOME: blaettern(-32000); return 0; case K_END: blaettern(32000); return 0;
        case K_TAB:
            if (!nlk) return 0;
            if (HIWORD(p2) & KS_SHIFT) sel = (short)(sel <= 0 ? nlk - 1 : sel - 1); else sel = (short)(sel + 1 >= (short)nlk ? 0 : sel + 1);
            sichtbar(); InvalidateRect(w, 0, FALSE); if (test) DbgHex("HILFE: Auswahl", (WORD)sel); return 0;
        case K_ENTER: if (sel >= 0) zeigen(lz[sel], TRUE); return 0;
        case K_BACK: if (nh) zeigen(hist[--nh], FALSE); return 0;
        }
        break;
    case M_CHAR: if (p1 == 'i' || p1 == 'I') { if (thema) zeigen(0, TRUE); return 0; } break;
    case M_BUTTONDOWN:
        if ((p1 & 0xFF) == 2) { if (nh) zeigen(hist[--nh], FALSE); return 0; }
        { short x = (short)LOWORD(p2), y = (short)HIWORD(p2);
          for (i = 0; i < nhr; i++) if (x >= hr[i].x0 && x < hr[i].x1 && y >= hr[i].y && y < hr[i].y + kh) { sel = hr[i].k; zeigen(lz[sel], TRUE); return 0; } }
        return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H; WORD i = 0, k = 0, n;
    (void)show; hi = inst; DbgPrint("HILFE: Start");
    if (cmd && cmd[0] == 'T' && cmd[1] == ' ') { test = 1; cmd += 2; }
    while (cmd && *cmd == ' ') cmd++;
    if (cmd) for (k = 0; cmd[k] && cmd[k] != ' '; k++) ;
    if (!k) { static const char s[] = "RD2.HLP"; cmd = s; k = 7; }
    { WORD j; BYTE pfad = 0; for (j = 0; j < k; j++) if (cmd[j] == ':' || cmd[j] == '\\') pfad = 1;
      if (!pfad) { SYSINFO si; KernInfo(&si); while (si.home[i] && i < 66) { datei[i] = si.home[i]; i++; } if (i && datei[i - 1] != '\\') datei[i++] = '\\'; }
      for (j = 0; j < k && i < sizeof datei - 1; j++) datei[i++] = cmd[j]; datei[i] = 0; }
    i = k; while (cmd[i] == ' ') i++;
    { WORD j = 0; char b[8]; while (cmd[i] && j < 7) b[j++] = cmd[i++]; b[j] = 0; j = 0; n = zahl(b, &j); }
    if (test) DbgPrint(datei);
    if (!laden(n)) { MsgBox(0, StrGet(inst, 2), StrGet(inst, 1), MB_OK | MB_ERROR); DbgPrint("HILFE: Datei fehlt"); return 1; }
    wc.name = "RdHilfe"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); kh = GetCaps(d, CAPS_TEXTH); ReleaseDC(0, d);
    fe = CreateWindow("RdHilfe", StrGet(inst, 1), WS_NORMAL | WS_VSCROLL | WS_VISIBLE,
                      (short)(W / 8), (short)(H / 8), (short)(W * 3 / 4), (short)(H * 3 / 4), 0, 1, inst);
    if (!fe) return 1;
    sel = nlk ? 0 : -1; titelsetzen(); umbruch(); InvalidateRect(fe, 0, TRUE); melden();
    if (test) DbgPrint("HILFE: bereit");
    while (GetMessage(&m)) DispatchMessage(&m);
    return 0;
}
