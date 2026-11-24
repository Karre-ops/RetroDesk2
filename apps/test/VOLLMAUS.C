/* VOLLMAUS.C - Maus und Tasten im Vollbild (PLAN_API 3): Modus 13h, ScreenMouse(320, 200), meldet Bewegung und Tasten
 * mit Vollbild-Koordinaten, KeyState(KS_SCAN + TS_LINKS/TS_RECHTS) gleichzeitig, Esc beendet. Ergebnis ueber DbgHex. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"

static SCREENINFO si; static WORD xmax, ymax, beide;
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_MOUSEMOVE:
        if (LOWORD(p2) > xmax) xmax = LOWORD(p2);
        if (HIWORD(p2) > ymax) ymax = HIWORD(p2);
        return 0;
    case M_BUTTONDOWN: DbgHex("VOLLMAUS: Taste", ((DWORD)(p1 & 0xFF) << 16) | 1); DbgHex("VOLLMAUS: bei", p2); return 0;
    case M_TIMER:
        if (KeyState(KS_SCAN + TS_LINKS) && KeyState(KS_SCAN + TS_RECHTS) && !beide) { beide = 1; DbgPrint("VOLLMAUS: links und rechts zugleich"); }
        return 0;
    case M_KEY: if (p1 == K_ESC) {
        DbgHex("VOLLMAUS: xmax", xmax); DbgHex("VOLLMAUS: ymax", ymax);
        DbgHex("VOLLMAUS: Esc noch gedrueckt", KeyState(KS_SCAN + TS_ESC));
        ScreenRelease(w); DestroyWindow(w); return 0; } break;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HWND w;
    (void)show; (void)cmd;
    wc.name = "VollMaus"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) return 1;
    w = CreateWindow("VollMaus", "Vollbild-Maus", WS_TITLE | WS_BORDER | WS_VISIBLE, 0, 0, 200, 60, 0, 0, inst);
    if (!w || !ScreenAcquire(w) || !ScreenMode(SM_13H, &si)) return 2;
    ScreenMouse(w, 320, 200);
    TimerSet(w, 1, 20);
    DbgPrint("VOLLMAUS: Vollbild");
    while (GetMessage(&m)) DispatchMessage(&m);
    DbgPrint("VOLLMAUS: Ende");
    return 0;
}
