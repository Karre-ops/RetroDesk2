/* Kompiliertest RD2.H/RD2DRV.H in 16 und 32 Bit, Groessen fester Strukturen gleich */
#include "RD2DRV.H"
#define FEST(t, n) typedef char chk_##t[(sizeof(t) == (n)) ? 1 : -1];
FEST(MSG, 18)
FEST(RECT, 8)
FEST(POINT, 4)
FEST(RDTIME, 9)
FEST(FINDDATA, 43)
FEST(DRIVEINFO, 30)
FEST(INSTR, 12)
FEST(NETINFO, 24)
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; HDC dc; RECT r;
    switch (msg) {
    case M_PAINT:
        dc = BeginPaint(w, &ps);
        GetClientRect(w, &r);
        SetBrush(dc, C_WHITE, PAT_GRAY50); FillRect(dc, &r);
        TextOut(dc, 8, 8, "Hallo XT", -1);
        EndPaint(w, &ps);
        return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc = {0}; MSG m; HMEM h; char FAR *p;
    (void)cmd; (void)show;
    wc.name = "Hallo"; wc.proc = proc; wc.inst = inst; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    CreateWindow("Hallo", "Hallo", WS_NORMAL | WS_VISIBLE, CW_DEFAULT, CW_DEFAULT, 320, 120, 0, 0, inst);
    h = MemAlloc(1000, MEM_MOVABLE | MEM_ZEROINIT); p = MemLock(h); p[0] = 1; MemUnlock(h); MemFree(h);
    while (GetMessage(&m)) DispatchMessage(&m);
    return (short)m.p1;
}
