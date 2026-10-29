/* SCHONER.EXE - Bildschirmschoner. Einstellungen (Art, Wartezeit, Vorschau) seit 29.10.2026 in OPTIONEN.EXE.
 * "S": Schoner zeigen (startet die Shell nach RD2.INI [Schoner] Zeit= Sekunden ohne Eingabe, 0 = aus, Standard 300).
 * Art= 0 Sternenfeld, 1 Uhr, 2 Schwarz. Ende bei Taste, Mausklick oder Mausbewegung; die Eingabe geht nicht weiter
 * an die Programme darunter. */
#include "RD2.H"

#define NS 48
static char ini[80]; static short W, H, th, art, mx = -1, my, ux, uy, uw;
static short sx[NS], sy[NS], sz[NS], px[NS], py[NS]; static WORD zufall = 12345, takt;

static short rnd(short n) { zufall = zufall * 25173u + 13849u; return (short)((zufall >> 4) % (WORD)n); }
static void neu(short i) { sx[i] = (short)(rnd(2000) - 1000); sy[i] = (short)(rnd(2000) - 1000); sz[i] = (short)(200 + rnd(800)); px[i] = -1; }
static void sterne(HDC d)
{
    short i, x, y;
    for (i = 0; i < NS; i++) {
        if (px[i] >= 0) Pixel(d, px[i], py[i], C_BLACK);
        sz[i] -= 16;
        if (sz[i] <= 16) { neu(i); continue; }
        x = (short)(W / 2 + (long)sx[i] * (W / 4) / sz[i]); y = (short)(H / 2 + (long)sy[i] * (H / 4) / sz[i]);
        if (x < 0 || y < 0 || x >= W || y >= H) { neu(i); continue; }
        Pixel(d, x, y, C_WHITE); px[i] = x; py[i] = y;
        if (sz[i] < 300 && x + 1 < W) Pixel(d, x + 1, y, C_WHITE);   /* nahe Sterne doppelt breit */
    }
}
static void uhr(HDC d)
{
    RDTIME t; char s[6]; RECT r;
    TimeGet(&t);
    s[0] = (char)('0' + t.std / 10); s[1] = (char)('0' + t.std % 10); s[2] = ':'; s[3] = (char)('0' + t.min / 10); s[4] = (char)('0' + t.min % 10); s[5] = 0;
    if (uw) { r.l = ux; r.t = uy; r.r = (short)(ux + uw); r.b = (short)(uy + th); SetBrush(d, C_BLACK, PAT_SOLID); FillRect(d, &r); }
    uw = TextWidth(d, s, 5); ux = rnd((short)(W - uw)); uy = rnd((short)(H - th));
    SetTextColor(d, C_WHITE); SetBkColor(d, C_BLACK); TextOut(d, ux, uy, s, 5);
}
static void ende(HWND w) { DbgPrint("SCHONER: aus"); DestroyWindow(w); PostQuit(0); }
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    HDC d; short x, y;
    switch (msg) {
    case M_TIMER:
        d = GetDC(w);
        if (!takt++) { HDC s0 = GetDC(0); RECT r; r.l = 0; r.t = 0; r.r = W; r.b = H; SetBrush(s0, C_BLACK, PAT_SOLID); FillRect(s0, &r); ReleaseDC(0, s0); }   /* ganzer Schirm samt Menueleiste */
        if (art == 0) sterne(d); else if (art == 1 && takt % 5 == 1) uhr(d);
        ReleaseDC(w, d);
        return 0;
    case M_MOUSEMOVE:
        x = (short)LOWORD(p2); y = (short)HIWORD(p2);
        if (mx < 0) { mx = x; my = y; return 0; }  /* erste Meldung: Ausgangspunkt */
        if (x - mx > 4 || mx - x > 4 || y - my > 4 || my - y > 4) ende(w);
        return 0;
    case M_KEY: case M_BUTTONDOWN: case M_DBLCLK: ende(w); return 0;
    case M_CHAR: case M_BUTTONUP: return 0;       /* nicht weiterreichen */
    }
    return DefWindowProc(w, msg, p1, p2);
}
static void zeigen(HINST inst)
{
    WNDCLASS wc; MSG m; HDC d; HWND w; short i;
    art = ProfileGetInt(ini, "Schoner", "Art", 0);
    wc.name = "RdSchoner"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = C_BLACK;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); th = GetCaps(d, CAPS_TEXTH); ReleaseDC(0, d);
    zufall = (WORD)TickCount(); for (i = 0; i < NS; i++) { neu(i); sz[i] = (short)(16 + rnd(1000)); }
    w = CreateWindow("RdSchoner", "", WS_POPUP | WS_VISIBLE, 0, 0, W, H, 0, 0, inst);
    if (!w) return;
    SetActive(w); SetFocus(w); CaptureMouse(w);
    TimerSet(w, 1, art == 0 ? 55 : 1000);
    DbgHex("SCHONER: an", (DWORD)art);
    while (GetMessage(&m)) DispatchMessage(&m);
    ReleaseMouse();
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    SYSINFO si; short n;
    (void)show;
    KernInfo(&si); for (n = 0; si.home[n] && n < 66; n++) ini[n] = si.home[n];
    if (n && ini[n - 1] != '\\') ini[n++] = '\\';
    { static const char f[] = "RD2.INI"; short k; for (k = 0; f[k]; k++) ini[n++] = f[k]; ini[n] = 0; }
    if (cmd && cmd[0] == 'S') zeigen(inst);     /* Einstellungen: RD > Optionen, Karte Schoner */
    return 0;
}
