/* DISKFRAG.C - Diskettenabfrage des Kerns im Grafikbetrieb (Audit K2): Fenster offen, dann DiskRequest
 * nach einer Diskette, die nicht drin ist. test/diskfrage.sh macht ein Bild vom Kasten, drueckt Esc
 * und prueft, dass das Bild danach wieder heil ist. */
#include "RD2.H"
static DWORD RDCALLBACK wproc(HWND h, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps;
    if (msg == M_PAINT) { BeginPaint(h, &ps); TextOut(ps.dc, 8, 8, "Fenster unter der Abfrage", -1); EndPaint(h, &ps); return 0; }
    return DefWindowProc(h, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cl, WORD show)
{
    WNDCLASS wc; HWND w; MSG m; WORD e; DRIVEINFO di;
    (void)cl; (void)show;
    wc.name = "DiskFrag"; wc.proc = wproc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    w = CreateWindow("DiskFrag", "Diskettenabfrage", WS_NORMAL | WS_VISIBLE, 40, 40, 400, 160, 0, 1, inst);
    while (PeekMessage(&m, 1)) DispatchMessage(&m);
    DriveInfo(1, &di);
    DbgPrint("DISKFRAG: frage");
    e = DiskRequest(1, di.serial ^ 0x5A5A5A5AUL, "RD2 PROG");   /* falsche Diskette: Abfrage, Esc */
    DbgHex("DISKFRAG: Esc ->", e);
    while (PeekMessage(&m, 1)) DispatchMessage(&m);
    DbgPrint("DISKFRAG: nach Esc");
    { DWORD t = TickCount(); while (TickCount() - t < 1500) { if (PeekMessage(&m, 1)) DispatchMessage(&m); else TaskYield(); } }
    DbgPrint("DISKFRAG: Ende");
    DestroyWindow(w);
    return 0;
}
