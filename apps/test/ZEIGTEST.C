/* ZEIGTEST.C - Phase 4: Mauszeiger-Formen. test/zeiger.sh bewegt die Maus und schickt Tasten:
 *   1: Fadenkreuz ueber dem Fenster (CursorSet)         -> Bild zeig1
 *   2: Sanduhr ohne Mausbewegung (CursorBusy)          -> Bild zeig2
 *   3: Textzeiger, danach Bewegung                      -> Bild zeig3
 * Position und Form meldet das Programm auf COM1, test/zeiger_pruef.py vergleicht die Bilder. */
#include "RD2.H"

static int fertig;
static void pos(LPCSTR t, WORD form)
{
    POINT p; CursorPos(&p);
    DbgHex(t, MAKELONG(p.x, p.y)); DbgHex("ZEIGTEST: Form", form);
}
static DWORD RDCALLBACK proc(HWND h, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps;
    switch (msg) {
    case M_PAINT: BeginPaint(h, &ps); EndPaint(h, &ps); return 0;
    case M_CHAR:
        if (p1 == '1') pos("ZEIGTEST: Z1", CUR_CROSS);
        else if (p1 == '2') { CursorBusy(TRUE); pos("ZEIGTEST: Z2", CUR_WAIT); }
        else if (p1 == '3') { CursorBusy(FALSE); CursorSet(0, CUR_TEXT); DbgPrint("ZEIGTEST: text"); }
        else if (p1 == '4') pos("ZEIGTEST: Z3", CUR_TEXT);
        else if (p1 == 'q') fertig = 1;
        return 0;
    }
    return DefWindowProc(h, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cl, WORD show)
{
    WNDCLASS wc; HWND w; HDC d; short W, H; MSG m;
    (void)cl; (void)show;
    wc.name = "ZeigTest"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); ReleaseDC(0, d);
    w = CreateWindow("ZeigTest", "Zeigertest", WS_NORMAL | WS_VISIBLE, W / 8, H / 6, W * 3 / 4, H * 2 / 3, 0, 1, inst);
    CursorSet(0, CUR_CROSS);
    DbgPrint("ZEIGTEST: bereit");
    while (!fertig && GetMessage(&m)) DispatchMessage(&m);
    DestroyWindow(w);
    DbgPrint("ZEIGTEST: fertig");
    return 0;
}
