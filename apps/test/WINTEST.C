/* WINTEST.C - Phase 4: Fenster, Ueberlappen, Neuzeichnen, Aktivieren, Verschieben, Kinder.
 * Prueft ueber GetPixel auf dem Schirm, meldet "WINTEST: bild" (Bildschirmfoto), danach
 * Schliessen und Aufraeumen, zum Schluss "WINTEST: fertig". Laeuft auf CGA, Hercules und VGA. */
#include "RD2.H"

static WORD fehler, gemalt[3];
static HWND a, b, kind;
static short W, H;

static void ok(int b_, LPCSTR t) { DbgPrint(t); DbgPrint(b_ ? "  -> ok" : "  -> FEHLER"); if (!b_) fehler++; }
static void pumpen(void)
{
    MSG m;
    while (PeekMessage(&m, TRUE)) DispatchMessage(&m);
}
static COLOR px(short x, short y) { HDC d = GetDC(0); COLOR c = GetPixel(d, x, y); ReleaseDC(0, d); return c; }

static HWND abkind;                               /* G26: Kind aus M_CREATE eines abgebrochenen Fensters */
static DWORD RDCALLBACK abproc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    if (msg == M_CREATE) { abkind = CreateWindow("TestFenster", "", WS_BORDER | WS_VISIBLE, 2, 2, 20, 20, w, 9, 0); return 0xFFFFFFFFUL; }
    return DefWindowProc(w, msg, p1, p2);
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; RECT r; HDC d; char t[24];
    if (msg == M_PAINT) {
        d = BeginPaint(w, &ps);
        GetClientRect(w, &r);
        if (w == kind) { SetBrush(d, C_BLACK, PAT_GRAY50); FillRect(d, &r); gemalt[2]++; }
        else {
            GetWindowText(w, t, sizeof t);
            SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
            TextOut(d, 4, 2, t, -1);
            r.l = r.r / 2; r.t = r.b / 2;         /* rechtes unteres Viertel schwarz */
            SetBrush(d, C_BLACK, PAT_SOLID); FillRect(d, &r);
            gemalt[w == a ? 0 : 1]++;
        }
        EndPaint(w, &ps);
        return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; RECT ra, rb, rc; POINT p; DWORD t0; MSG m; HDC d; short y0;
    (void)show;
    if (cmd && cmd[0] == 'L') { GetDC(0); for (;;) TaskYield(); }   /* Kind fuer M12: DC offen lassen, wird beendet */
    if (cmd && cmd[0] == 'K') {                   /* Kind fuer G27: Klasse des anderen Programms nicht nutzbar */
        ok(!CreateWindow("TestFenster", "x", WS_VISIBLE, 0, 20, 40, 40, 0, 1, inst) && LastError() == RD_ERR_BUSY, "WINTEST: G27 fremde Klasse abgelehnt");
        return 0;
    }
    wc.name = "TestFenster"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    ok(RegisterClass(&wc) != 0, "WINTEST: RegisterClass");
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); ReleaseDC(0, d);
    y0 = H / 8;
    a = CreateWindow("TestFenster", "Fenster A", WS_TITLE | WS_BORDER | WS_CLOSE | WS_VISIBLE, W / 16, y0, W / 2, H / 2, 0, 1, inst);
    b = CreateWindow("TestFenster", "Fenster B", WS_TITLE | WS_BORDER | WS_CLOSE | WS_VISIBLE, W / 3, y0 + H / 4, W / 2, H / 2, 0, 2, inst);
    ok(a && b, "WINTEST: zwei Fenster");
    pumpen();
    ok(gemalt[0] >= 1 && gemalt[1] >= 1, "WINTEST: beide bekommen M_PAINT");
    ok(GetActive() == b, "WINTEST: das neueste ist aktiv");
    GetWindowRect(a, &ra); GetWindowRect(b, &rb);
    p.x = (ra.r + rb.l) / 2; p.y = (ra.b + rb.t) / 2;   /* Ueberlappung */
    ok(WindowFromPoint(p.x, p.y) == b, "WINTEST: oben liegt B");
    GetClientRect(b, &rc); p.x = 0; p.y = 0; ClientToScreen(b, &p);
    ok(px(p.x + rc.r - 2, p.y + rc.b - 2) == C_BLACK && px(p.x + 2, p.y + rc.b - 2) == C_WHITE, "WINTEST: B gezeichnet (Viertel schwarz, Rest weiss)");
    ok(px((ra.r + rb.l) / 2, ra.b - 3) == C_WHITE, "WINTEST: B verdeckt das schwarze Viertel von A");

    SetActive(a); pumpen();
    ok(GetActive() == a && WindowFromPoint((ra.r + rb.l) / 2, (ra.b + rb.t) / 2) == a, "WINTEST: SetActive holt A nach oben");
    ok(px((ra.r + rb.l) / 2, ra.b - 3) == C_BLACK, "WINTEST: A neu gezeichnet ueber B");

    kind = CreateWindow("TestFenster", "", WS_BORDER | WS_VISIBLE, 8, 20, W / 8, H / 10, a, 3, inst);
    pumpen();
    GetWindowRect(kind, &rc);
    ok(kind && GetParent(kind) == a && WindowFromPoint(rc.l + 3, rc.t + 3) == kind && gemalt[2] >= 1, "WINTEST: Kindfenster");
    ok(px(rc.l, rc.t) == C_BLACK && px(rc.l + 1, rc.t + 1) != px(rc.l + 2, rc.t + 1), "WINTEST: Kind mit Rahmen und Raster");

    MoveWindow(b, W - W / 2 - 4, H - H / 3 - 4, W / 2, H / 3, TRUE); pumpen();
    GetWindowRect(b, &rb);
    ok(rb.r == W - 4 && rb.b == H - 4, "WINTEST: MoveWindow");
    ok(px(4, H - 6) != px(5, H - 6), "WINTEST: frei gewordener Schreibtisch mit Raster");
    GetClientRect(b, &rc); p.x = 0; p.y = 0; ClientToScreen(b, &p);
    ok(px(p.x + rc.r - 2, p.y + rc.b - 2) == C_BLACK, "WINTEST: B nach dem Verschieben neu gezeichnet");

    DbgPrint("WINTEST: bild");
    for (t0 = TickCount(); TickCount() - t0 < 2500; ) { if (PeekMessage(&m, TRUE)) DispatchMessage(&m); else TaskYield(); }

    PostMessage(b, M_CLOSE, 0, 0); pumpen();
    ok(!IsWindow(b) && GetActive() == a, "WINTEST: M_CLOSE schliesst B, A bleibt aktiv");
    ok(MsgRegister("RD_WIN") >= 0xC000 && MsgRegister("rd_win") == MsgRegister("RD_WIN"), "WINTEST: MsgRegister");
    {   /* M12: 25 Programme mit offenem DC notbeenden (mehr als die 24 DCs von RDGDI), danach muss GetDC noch gehen */
        WORD k, y; HTASK t;
        for (k = 0; k < 25; k++) {
            if ((t = TaskExec("WINTEST.EXE", "L", SW_SHOW)) != 0) { for (y = 0; y < 30; y++) TaskYield(); TaskKill(t, 1); }
            for (y = 0; y < 10; y++) TaskYield();
        }
        d = GetDC(a);
        ok(d != 0, "WINTEST: nach 25 beendeten Programmen mit offenem DC geht GetDC noch");
        if (d) ReleaseDC(a, d);
        ok(!TaskExec("UEBER.EXE", "", SW_SHOW) && LastError() == RD_ERR_FORMAT, "WINTEST: G12 Programmgroesse ueber 64K Paragraphen abgelehnt");
    }
    {   /* Audit 3b: G26 M_CREATE abgebrochen samt Kind, G27 fremde Klasse, G28 Fokus auf verstecktem Fenster */
        WNDCLASS c2 = wc; HWND f; WORD y;
        c2.name = "AbbrFenster"; c2.proc = abproc;
        ok(RegisterClass(&c2) != 0 && !CreateWindow("AbbrFenster", "", WS_VISIBLE, 0, 20, 60, 60, 0, 1, inst) && LastError() == RD_ERR_ABORT
           && abkind && !IsWindow(abkind), "WINTEST: G26 abgebrochenes Fenster nimmt sein Kind mit");
        if (TaskExec("WINTEST.EXE", "K", SW_SHOW)) for (y = 0; y < 40; y++) TaskYield();
        f = CreateWindow("TestFenster", "", WS_BORDER | WS_VISIBLE, 4, 4, 30, 20, a, 7, inst);
        SetFocus(f); ShowWindow(f, SW_HIDE);
        ok(f && GetFocus() != f, "WINTEST: G28 verstecktes Fenster verliert den Fokus");
        DestroyWindow(f);
    }
    DestroyWindow(a);
    ok(!IsWindow(a) && !IsWindow(kind) && !GetActive(), "WINTEST: DestroyWindow samt Kind");
    DbgHex("WINTEST: Fehler", fehler);
    DbgPrint("WINTEST: fertig");
    return (short)fehler;
}
