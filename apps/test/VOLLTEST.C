/* VOLLTEST.C - Test Vollbild (API 2.3, PLAN_32BIT Schritt 2): Modus 13h mit Farbverlauf, Alt+Tab und zurueck, Esc beendet.
 * Befehlszeile: X = Mode X, V = VESA, C = CGA, E = EGA, G = VGA 16 Farben (sonst 13h). */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"

static SCREENINFO si; static WORD modus = SM_13H, bilder;
static void zeichnen(void)                        /* Zeile y in Farbe y (256 Farben), sonst Streifen */
{
    BYTE FAR *v = (BYTE FAR *)((DWORD)si.seg << 16); WORD x, y, e;
    if (si.bpp == 8 && si.ebenen == 1 && si.w == 320) { for (y = 0; y < 200; y++) for (x = 0; x < 320; x++) v[y * 320 + x] = (BYTE)y; }
    else if (si.bpp == 8 && si.ebenen == 4) {     /* Mode X: Ebene je Spalte mod 4 */
        for (e = 0; e < 4; e++) { outpw(0x3C4, (WORD)(0x0002 | (0x100 << e))); for (y = 0; y < 240; y++) for (x = 0; x < 80; x++) v[y * 80 + x] = (BYTE)y; }
    } else if (modus == SM_VESA) {
        DWORD o; WORD bank = 0xFFFF;
        for (y = 0; y < 480; y++) for (x = 0; x < 640; x++) { o = (DWORD)y * 640 + x; if ((WORD)(o >> 16) != bank) ScreenBank(bank = (WORD)(o >> 16)); v[(WORD)o] = (BYTE)(y >> 1); }
    } else {                                       /* Ebenen-/CGA-Modi: Streifen */
        for (y = 0; y < (si.seg == 0xB800 ? 0x3E80u : si.zeilenbytes * si.h); y++) v[y] = (BYTE)((y / si.zeilenbytes) & 1 ? 0xFF : 0x55);
    }
    bilder++; DbgHex("VOLLTEST: gezeichnet", bilder);
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_SCREENLOST: DbgPrint("VOLLTEST: SCREENLOST"); return 0;
    case M_SCREENBACK: DbgPrint("VOLLTEST: SCREENBACK"); ScreenRestore(); zeichnen(); return 0;
    case M_KEY: if (p1 == K_ESC) { DbgPrint("VOLLTEST: Esc"); ScreenRelease(w); DestroyWindow(w); return 0; } break;
    case M_DESTROY: PostQuit(0); return 0;
    case M_PAINT: { PAINT ps; BeginPaint(w, &ps); EndPaint(w, &ps); } return 0;   /* geparkt: nur der Titel */
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HWND w; BYTE pal[768]; WORD i;
    (void)show;
    if (cmd) switch (cmd[0] | 0x20) { case 'x': modus = SM_X; break; case 'v': modus = SM_VESA; break; case 'c': modus = SM_CGA4; break; case 'e': modus = SM_EGA; break; case 'g': modus = SM_VGA16; break; }
    wc.name = "VollTest"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) return 1;
    w = CreateWindow("VollTest", "Vollbild-Test", WS_TITLE | WS_BORDER | WS_VISIBLE, 0, 0, 200, 60, 0, 0, inst);
    if (!w || !ScreenAcquire(w)) { DbgHex("VOLLTEST: ScreenAcquire Fehler", LastError()); return 2; }
    if (!ScreenMode(modus, &si)) { DbgHex("VOLLTEST: ScreenMode Fehler", LastError()); ScreenRelease(w); return 3; }
    for (i = 0; i < 256; i++) { pal[i * 3] = (BYTE)(i >> 2); pal[i * 3 + 1] = 0; pal[i * 3 + 2] = (BYTE)(63 - (i >> 2)); }   /* rot nach blau */
    if (si.bpp == 8) ScreenPalette(0, 256, pal);
    DbgHex("VOLLTEST: Vollbild", modus);
    zeichnen();
    while (GetMessage(&m)) DispatchMessage(&m);
    DbgPrint("VOLLTEST: Ende");
    return 0;
}
