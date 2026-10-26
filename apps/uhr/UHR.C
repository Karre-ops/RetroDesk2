/* UHR.C - Zubehoer Uhr (RD1 1.2: grosse Digitalanzeige mit Datum, Stellen, Netz, BIOS).
 * Ziffern als 7-Segment aus Rechtecken, passt sich CGA/Hercules/VGA an. "Netz" folgt mit RDTCP (Punkt 8).
 * Befehlszeile "T": Testmodus, meldet die Anzeige ueber DbgPrint (test/uhr.sh). */
#include "RD2.H"

#define ID_STELLEN 1
#define ID_NETZ    2
#define ID_BIOS    3
static HINST hi; static HWND fe; static short dw, dh, sd, x0, y0, ty, kh; static BYTE alt[8], test;
static char datum[40]; static RDTIME tt;      /* Testmodus: feste Zeit, damit Bildvergleiche gehen */
static void zeit(RDTIME *t) { if (test) *t = tt; else TimeGet(t); }

static void z2(char *p, WORD v) { p[0] = (char)('0' + v / 10 % 10); p[1] = (char)('0' + v % 10); }
static WORD lies(const char *p, WORD *i)          /* Zahl ab p[*i], Trennzeichen ueberspringen */
{
    WORD v = 0;
    while (p[*i] && (p[*i] < '0' || p[*i] > '9')) (*i)++;
    while (p[*i] >= '0' && p[*i] <= '9') v = v * 10 + (WORD)(p[(*i)++] - '0');
    return v;
}
/* Segmente a b c d e f g als Bits 0..6 */
static const BYTE seg[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
static void box(HDC d, short l, short t, short r, short b, COLOR c)
{
    RECT q; q.l = l; q.t = t; q.r = r; q.b = b; SetBrush(d, c, PAT_SOLID); FillRect(d, &q);
}
static void ziffer(HDC d, short x, short y, BYTE z)  /* z 0..9, 10 = Doppelpunkt */
{
    short m = (short)(y + dh / 2 - sd / 2); BYTE s;
    box(d, x, y, (short)(x + dw), (short)(y + dh), C_WHITE);
    if (z == 10) { box(d, (short)(x + dw / 2 - sd / 2), (short)(y + dh / 4), (short)(x + dw / 2 + sd - sd / 2), (short)(y + dh / 4 + sd), C_BLACK);
                   box(d, (short)(x + dw / 2 - sd / 2), (short)(y + dh * 3 / 4 - sd), (short)(x + dw / 2 + sd - sd / 2), (short)(y + dh * 3 / 4), C_BLACK); return; }
    s = seg[z];
    if (s & 1)  box(d, x, y, (short)(x + dw), (short)(y + sd), C_BLACK);
    if (s & 2)  box(d, (short)(x + dw - sd * 2), y, (short)(x + dw), (short)(y + dh / 2), C_BLACK);
    if (s & 4)  box(d, (short)(x + dw - sd * 2), (short)(y + dh / 2), (short)(x + dw), (short)(y + dh), C_BLACK);
    if (s & 8)  box(d, x, (short)(y + dh - sd), (short)(x + dw), (short)(y + dh), C_BLACK);
    if (s & 16) box(d, x, (short)(y + dh / 2), (short)(x + sd * 2), (short)(y + dh), C_BLACK);
    if (s & 32) box(d, x, y, (short)(x + sd * 2), (short)(y + dh / 2), C_BLACK);
    if (s & 64) box(d, x, m, (short)(x + dw), (short)(m + sd), C_BLACK);
}
static void anzeige(HDC d, BOOL alles)            /* nur geaenderte Stellen neu: kein Flackern */
{
    RDTIME t; BYTE n[8]; short i, x; char z[8];
    zeit(&t);
    z2(z, t.std); z2(z + 3, t.min); z2(z + 6, t.sek);
    for (i = 0; i < 8; i++) n[i] = (BYTE)(i == 2 || i == 5 ? 10 : z[i] - '0');
    for (i = 0, x = x0; i < 8; i++, x = (short)(x + (n[i - 1] == 10 ? dw / 2 : dw + dw / 3))) {
        if (alles || n[i] != alt[i]) ziffer(d, (short)(n[i] == 10 ? x - dw / 4 : x), y0, n[i]);
        alt[i] = n[i];
    }
    {   char dt[40]; WORD k = 0; LPCSTR w = StrGet(hi, (WORD)(10 + t.wtag % 7));
        while (w && *w && k < 20) dt[k++] = *w++;
        dt[k++] = ','; dt[k++] = ' '; z2(dt + k, t.tag); k += 2; dt[k++] = '.'; z2(dt + k, t.monat); k += 2; dt[k++] = '.';
        z2(dt + k, t.jahr / 100); z2(dt + k + 2, t.jahr % 100); k += 4; dt[k] = 0;
        for (i = 0; dt[i] == datum[i] && dt[i]; i++) ;
        if (alles || dt[i] != datum[i]) {
            RECT r; GetClientRect(fe, &r); r.t = ty; r.b = (short)(ty + kh); SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &r);
            SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
            TextOut(d, (short)((r.r - TextWidth(d, dt, -1)) / 2), ty, dt, -1);
            for (i = 0; i <= (short)k; i++) datum[i] = dt[i];
        }
        if (test && alles) { DbgPrint("UHR: Anzeige"); z[2] = ':'; z[5] = ':'; DbgPrint(z); DbgPrint(dt); }
    }
}
static void stellen(void)                         /* zwei Eingaben: Zeit, dann Datum */
{
    RDTIME t; char b[16]; WORD i;
    zeit(&t);
    z2(b, t.std); b[2] = ':'; z2(b + 3, t.min); b[5] = ':'; z2(b + 6, t.sek); b[8] = 0;
    if (!InputBox(fe, StrGet(hi, 2), StrGet(hi, 1), b, sizeof b)) return;
    i = 0; t.std = (BYTE)lies(b, &i); t.min = (BYTE)lies(b, &i); t.sek = (BYTE)lies(b, &i); t.hsek = 0;
    z2(b, t.tag); b[2] = '.'; z2(b + 3, t.monat); b[5] = '.'; z2(b + 6, t.jahr / 100); z2(b + 8, t.jahr % 100); b[10] = 0;
    if (!InputBox(fe, StrGet(hi, 3), StrGet(hi, 1), b, sizeof b)) return;
    i = 0; t.tag = (BYTE)lies(b, &i); t.monat = (BYTE)lies(b, &i); t.jahr = lies(b, &i); if (t.jahr < 100) t.jahr += 2000;
    if (TimeSet(&t, 0)) { MsgBox(fe, StrGet(hi, 4), StrGet(hi, 1), MB_OK | MB_ERROR); return; }
    if (test) { RDTIME k; TimeGet(&k); tt = t; tt.wtag = k.wtag; z2(b, k.std); b[2] = ':'; z2(b + 3, k.min); b[5] = ' '; z2(b + 6, k.tag); b[8] = '.'; z2(b + 9, k.monat); b[11] = 0; DbgPrint("UHR: gestellt"); DbgPrint(b); }
    InvalidateRect(fe, 0, TRUE);
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps;
    switch (msg) {
    case M_PAINT: BeginPaint(w, &ps); anzeige(ps.dc, TRUE); EndPaint(w, &ps); return 0;
    case M_TIMER: { HDC d = GetDC(w); if (d) { anzeige(d, FALSE); ReleaseDC(w, d); } return 0; }
    case M_KEY:                                   /* Alt+S/Eingabe Stellen, Alt+B BIOS, Esc schliesst */
        if (p1 == K_ESC) { DestroyWindow(w); return 0; }
        if (p1 == K_ENTER || p1 == 0x11F) p1 = ID_STELLEN; else if (p1 == 0x130) p1 = ID_BIOS; else break;
        /* weiter wie Knopf */
    case M_COMMAND:
        if (p1 == ID_STELLEN) stellen();
        else if (p1 == ID_BIOS) { RDTIME t; TimeGet(&t); TimeSet(&t, 1); MsgBox(w, StrGet(hi, 5), StrGet(hi, 1), MB_OK | MB_INFO); }
        SetFocus(w);                              /* Tasten wieder an das Fenster */
        return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H, fw, fh, bw, bx, i; RECT r;
    (void)show; hi = inst; test = cmd && cmd[0] == 'T';
    DbgPrint("UHR: Start"); DbgPrint(cmd);
    wc.name = "RdUhr"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); kh = GetCaps(d, CAPS_TEXTH); ReleaseDC(0, d);
    dw = (short)(W / 20); dh = (short)(H / 6); sd = (short)(dh / 12 < 2 ? 2 : dh / 12); if (sd * 2 > dw / 3) sd = (short)(dw / 6);
    fw = (short)(6 * dw + 6 * dw / 3 + dw + 4 * sd + 24);   /* 6 Ziffern, Abstaende, 2 Doppelpunkte */
    fh = (short)(dh + kh * 4 + 40);
    fe = CreateWindow("RdUhr", StrGet(inst, 1), WS_TITLE | WS_CLOSE | WS_MIN | WS_BORDER | WS_TOOL | WS_VISIBLE,
                      (short)((W - fw) / 2), (short)(H / 6), fw, fh, 0, 1, inst);
    if (!fe) { DbgHex("UHR: kein Fenster", LastError()); return 1; }
    GetClientRect(fe, &r);
    x0 = (short)((r.r - (6 * dw + 4 * (dw / 3) + 2 * (dw / 2) + dw / 3)) / 2); y0 = (short)(kh / 2 + 4);
    ty = (short)(y0 + dh + kh / 2 + 2);
    bw = (short)((r.r - 32) / 3); if (bw > 80) bw = 80; bx = (short)((r.r - 3 * bw - 16) / 2);
    for (i = 0; i < 3; i++)
        CreateWindow(CL_BUTTON, StrGet(inst, (WORD)(6 + i)), WS_CHILD | WS_VISIBLE | (i == 0 ? ES_DEFAULT : 0) | (i == 1 ? WS_DISABLED : 0),
                     (short)(bx + i * (bw + 8)), (short)(r.b - kh - 12), bw, (short)(kh + 8), fe, (WORD)(ID_STELLEN + i), inst);
    if (test) { tt.jahr = 2026; tt.monat = 10; tt.tag = 25; tt.wtag = 0; tt.std = 12; tt.min = 34; tt.sek = 56; DbgPrint("UHR: bereit"); }
    TimerSet(fe, 1, 1000);
    while (GetMessage(&m)) DispatchMessage(&m);
    return 0;
}
