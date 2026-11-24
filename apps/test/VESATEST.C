/* VESATEST.C - VESA fuer 16-Bit-Programme (PLAN_API Luecke 3): ScreenVesaList, ScreenModeVesa in 8/15/16/24/32 Bit,
 * ScreenPut ueber Bankgrenzen, Lesen ueber ScreenBank, ScreenRestore, SM_VESA wie bisher. test/vesa16.sh. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include <i86.h>
#include "RD2.H"

static WORD fehler; static BYTE zeile[1024 * 4];
static void pruef(LPCSTR was, int ok) { DbgPrint(was); if (!ok) { fehler++; DbgPrint("VESATEST: ^^^ FALSCH"); } }
static BYTE lies(SCREENINFO FAR *si, DWORD o) { ScreenBank((WORD)(o >> 16)); return *(BYTE FAR *)MK_FP(si->seg, (WORD)o); }
static int modus(WORD w, WORD h, WORD bpp)
{
    SCREENINFO si; WORD bp = bpp == 15 ? 2 : (bpp + 7) >> 3, i, j, f = 0; DWORD o;
    if (!ScreenModeVesa(w, h, bpp, &si)) { DbgHex("VESATEST: nicht da", ((DWORD)w << 16) | bpp); return -1; }
    for (i = 0; i < w * bp; i++) zeile[i] = (BYTE)(i * 7 + 3);
    for (j = 0; j < h; j += 37) ScreenPut(0, j, w, 1, zeile, w * bp);        /* Zeilen ueber Bankgrenzen */
    ScreenPut(w / 2, h - 2, w / 2, 2, zeile, 0);                              /* Teilrechteck rechts unten (zweimal dieselbe Zeile) */
    for (j = 0; j < h; j += 37) for (i = 0; i < w * bp; i += 61) { o = (DWORD)j * si.zeilenbytes + i; if (lies(&si, o) != zeile[i]) f++; }
    for (i = 0; i < (w / 2) * bp; i += 13) { o = (DWORD)(h - 1) * si.zeilenbytes + (DWORD)(w / 2) * bp + i; if (lies(&si, o) != zeile[i]) f++; }
    ScreenRestore();                                                          /* derselbe Modus wieder (Alt+Tab) */
    { static union REGS r; r.x.ax = 0x4F03; int86(0x10, &r, &r); DbgHex("VESATEST: Modus nach ScreenRestore", r.x.bx); }
    DbgHex("VESATEST: Modus ok, Abweichungen", ((DWORD)w << 16) | ((DWORD)bpp << 8) | (f > 255 ? 255 : f));
    return f;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    VESAMODE l[64]; WORD n, i, a = 0; int r; HWND w; SCREENINFO si; WNDCLASS wc;
    (void)show; (void)cmd;
    wc.name = "VesaT"; wc.proc = DefWindowProc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    w = CreateWindow("VesaT", "VESA", WS_TITLE | WS_BORDER | WS_VISIBLE, 0, 0, 200, 60, 0, 0, inst);
    pruef("VESATEST: ScreenAcquire", w && ScreenAcquire(w));
    n = ScreenVesaList(l, 64); DbgHex("VESATEST: Modi", n);
    for (i = 0; i < n; i++) if (l[i].w == 800 && l[i].h == 600 && l[i].bpp == 16) a = 1;
    pruef("VESATEST: Liste hat Modi, darunter 800x600 16 Bit", n >= 5 && a);
    pruef("VESATEST: 640x480 8 Bit", modus(640, 480, 8) == 0);
    pruef("VESATEST: 800x600 8 Bit", modus(800, 600, 8) == 0);
    pruef("VESATEST: 800x600 16 Bit", modus(800, 600, 16) == 0);
    pruef("VESATEST: 640x480 15 Bit", modus(640, 480, 15) == 0);
    pruef("VESATEST: 1024x768 8 Bit", modus(1024, 768, 8) == 0);
    r = modus(640, 480, 32); pruef("VESATEST: 640x480 32 Bit (oder 24)", r == 0 || modus(640, 480, 24) == 0);
    SetLastError(0); pruef("VESATEST: 123x45 gibt es nicht -> NOTSUP", !ScreenModeVesa(123, 45, 8, &si) && LastError() == RD_ERR_NOTSUP);
    pruef("VESATEST: SM_VESA wie bisher", ScreenMode(SM_VESA, &si) && si.w == 640 && si.zeilenbytes >= 640 && si.seg);
    ScreenRelease(w); DestroyWindow(w);
    DbgHex("VESATEST: Fehler", fehler);
    DbgPrint("VESATEST: fertig");
    return 0;
}
