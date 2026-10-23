/* MENUTEST.C - Phase 4: Menueleiste, Klappmenues (Tastatur), Haekchen/grau, Rollbalken, ScrollWindow.
 * Das Testskript (test/menu.sh) schickt nach "MENUTEST: bereit" Tasten:
 *   F10, rechts, runter  (Bild: Ansicht offen, "Gross" markiert)  Enter  -> M_COMMAND 202
 *   F10, n                                                          -> M_COMMAND 101
 *   F10, Esc                                                        -> nichts
 * Danach muss der Schirm unter den Menues wieder genau so aussehen wie vorher. */
#include "RD2.H"

static WORD fehler, cmd[4], ncmd, scrl[8], nscrl, gemalt;
static HWND w;
static short W, H, A;
#define HT_VSCROLL 4
#define M_NCBUTTONDOWN 0x00A1

static void ok(int b, LPCSTR t) { DbgPrint(t); DbgPrint(b ? "  -> ok" : "  -> FEHLER"); if (!b) fehler++; }
static void pumpen(void) { MSG m; while (PeekMessage(&m, TRUE)) DispatchMessage(&m); }
static COLOR px(short x, short y) { HDC d = GetDC(0); COLOR c = GetPixel(d, x, y); ReleaseDC(0, d); return c; }
static DWORD summe(short x0, short y0, short x1, short y1)   /* Pruefsumme eines Schirmbereichs */
{
    HDC d = GetDC(0); DWORD s = 0; short x, y;
    for (y = y0; y < y1; y++) for (x = x0; x < x1; x++) s = s * 3 + (GetPixel(d, x, y) == C_BLACK ? 1 : 0) + y;
    ReleaseDC(0, d);
    return s;
}
static DWORD RDCALLBACK proc(HWND h, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; RECT r; HDC d;
    switch (msg) {
    case M_PAINT:
        d = BeginPaint(h, &ps); GetClientRect(h, &r);
        SetPen(d, C_BLACK); Line(d, 0, 20, r.r - 1, 20);        /* Linie fuer ScrollWindow */
        SetBkMode(d, BK_TRANSPARENT); TextOut(d, 4, 30, "Menuetest", -1);
        EndPaint(h, &ps); gemalt++;
        return 0;
    case M_COMMAND: if (ncmd < 4) cmd[ncmd++] = p1; return 0;
    case M_SCROLL:
        if (nscrl < 8) scrl[nscrl++] = p1;
        if (p1 == SB_LINEDOWN) ScrollSet(h, HIWORD(p2), ScrollGet(h, HIWORD(p2)) + 10, 100, 20);
        return 0;
    }
    return DefWindowProc(h, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cl, WORD show)
{
    WNDCLASS wc; RECT r, c; HMENU m; MSG msg; HDC d; DWORD vor, t0; short x0, y0, x1, y1, sx;
    (void)cl; (void)show;
    wc.name = "MenuTest"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); A = GetCaps(d, CAPS_TEXTH) + 3; ReleaseDC(0, d);
    if (A > 16) A = 16;                           /* Pfeillaenge im senkrechten Balken */
    w = CreateWindow("MenuTest", "Menuetest", WS_NORMAL | WS_VSCROLL | WS_HSCROLL | WS_VISIBLE, W / 8, H / 6, W / 2, H / 2, 0, 1, inst);
    m = MenuLoad(inst, 1);
    ok(w && m, "MENUTEST: Fenster und MenuLoad");
    MenuSet(w, m); pumpen();
    ok(px(4, 4) == C_WHITE && px(W - 4, 4) == C_WHITE, "MENUTEST: Menueleiste weiss");

    /* Rollbalken */
    ScrollSet(w, SB_VERT, 0, 100, 20); ScrollSet(w, SB_HORZ, 50, 100, 50);
    ok(ScrollGet(w, SB_VERT) == 0 && ScrollGet(w, SB_HORZ) == 50, "MENUTEST: ScrollSet/ScrollGet");
    GetWindowRect(w, &r); GetClientRect(w, &c);
    sx = r.r - 9;                                 /* Mitte des senkrechten Balkens */
    {   POINT p; p.x = 0; p.y = 0; ClientToScreen(w, &p); y0 = p.y; y1 = p.y + c.b; }
    ok(px(sx, y0 + A + 3) == C_WHITE && px(sx, y1 - A - 3) != px(sx + 1, y1 - A - 3), "MENUTEST: Daumen oben, Spur gerastert");
    ScrollSet(w, SB_VERT, 100, 100, 20);
    ok(px(sx, y1 - A - 3) == C_WHITE && px(sx, y0 + A + 3) != px(sx + 1, y0 + A + 3), "MENUTEST: Daumen unten");
    ScrollSet(w, SB_VERT, 0, 100, 20);
    PostMessage(w, M_NCBUTTONDOWN, HT_VSCROLL, MAKELONG(sx, y1 - 3)); PostMessage(w, M_BUTTONUP, 1, 0);
    pumpen();
    ok(nscrl >= 1 && scrl[0] == SB_LINEDOWN && ScrollGet(w, SB_VERT) == 10, "MENUTEST: Klick auf Pfeil unten -> SB_LINEDOWN");
    PostMessage(w, M_NCBUTTONDOWN, HT_VSCROLL, MAKELONG(sx, y1 - 30)); PostMessage(w, M_BUTTONUP, 1, 0);
    pumpen();
    ok(nscrl >= 2 && scrl[nscrl - 1] == SB_PAGEDOWN, "MENUTEST: Klick in die Spur -> SB_PAGEDOWN");

    /* ScrollWindow: Linie bei y 20 wandert auf y 10 */
    {   POINT p; p.x = 0; p.y = 0; ClientToScreen(w, &p); x0 = p.x;
        ok(px(x0 + 30, y0 + 20) == C_BLACK, "MENUTEST: Linie bei y 20");
        gemalt = 0; ScrollWindow(w, 0, -10, 0); pumpen();
        ok(px(x0 + 30, y0 + 10) == C_BLACK && gemalt == 1, "MENUTEST: ScrollWindow schiebt, Streifen neu gemalt"); }

    /* Haekchen und grau */
    MenuCheck(m, 202, TRUE); MenuCheck(m, 202, FALSE); MenuEnable(m, 102, FALSE);
    ok(KeyState(0) == 0, "MENUTEST: KeyState ohne Umschalttaste");

    x1 = W / 2 + 40; vor = summe(0, 0, x1, H / 2);
    DbgPrint("MENUTEST: bereit");
    for (t0 = TickCount(); ncmd < 2 && TickCount() - t0 < 30000; )
        if (PeekMessage(&msg, TRUE)) DispatchMessage(&msg); else TaskYield();
    for (t0 = TickCount(); TickCount() - t0 < 2000; ) if (PeekMessage(&msg, TRUE)) DispatchMessage(&msg); else TaskYield();
    pumpen();
    ok(ncmd == 2 && cmd[0] == 202 && cmd[1] == 101, "MENUTEST: Tastatur: Ansicht/Gross, dann Datei/Neu");
    ok(summe(0, 0, x1, H / 2) == vor, "MENUTEST: Schirm unter den Menues wiederhergestellt");
    DestroyWindow(w); MenuFree(m);
    DbgHex("MENUTEST: Fehler", fehler);
    DbgPrint("MENUTEST: fertig");
    return (short)fehler;
}
