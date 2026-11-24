/* VESA32.C - VESA-Modi fuer RD2/32: zeigt der Reihe nach 800x600, 1024x768 (256 Farben), 640x480 64K Farben und
 * 800x600 16,7 Mio. Farben (linear, ScreenModeVesa), Enter = naechster Modus, Alt+Tab wie VOLL32, Esc beendet.
 * Fehlt ein Modus auf der Karte, wird er uebersprungen. Ins Protokoll: Liste der Modi der Karte. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include <stdio.h>
#include "RD2_32.H"

static const WORD modi[][3] = { { 800, 600, 8 }, { 1024, 768, 8 }, { 640, 480, 16 }, { 800, 600, 32 } };
static SCREENINFO si; static int akt = -1; static HWND fen;
static void zeichnen(void)
{
    WORD x, y; BYTE *z = ScreenPtr();
    for (y = 0; y < si.h; y++, z += si.zeilenbytes) for (x = 0; x < si.w; x++) {
        WORD r = (WORD)((DWORD)x * 256 / si.w), g = (WORD)((DWORD)y * 256 / si.h), b = (WORD)((x ^ y) & 255);
        switch (si.bpp) {
        case 8:  z[x] = (BYTE)((r & 0xE0) | ((g >> 3) & 0x1C) | (b >> 6)); break;   /* Palette 3-3-2 */
        case 15: ((WORD *)z)[x] = (WORD)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)); break;
        case 16: ((WORD *)z)[x] = (WORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); break;
        case 24: z[3 * x] = (BYTE)b; z[3 * x + 1] = (BYTE)g; z[3 * x + 2] = (BYTE)r; break;
        default: ((DWORD *)z)[x] = ((DWORD)r << 16) | ((DWORD)g << 8) | b;
        }
    }
    ScreenUpdate();
    printf("VESA32: %ux%ux%u gezeichnet\n", si.w, si.h, si.bpp);
}
static void naechster(void)
{
    static BYTE pal[768]; WORD i;
    while (++akt < 4) {
        if (ScreenModeVesa(modi[akt][0], modi[akt][1], modi[akt][2], &si)) break;
        printf("VESA32: %ux%ux%u fehlt\n", modi[akt][0], modi[akt][1], modi[akt][2]);
    }
    if (akt == 4) { printf("VESA32: alle Modi gezeigt\n"); ScreenRelease(fen); DestroyWindow(fen); return; }
    if (si.bpp == 8) {
        for (i = 0; i < 256; i++) { pal[3 * i] = (BYTE)((i >> 5) * 9); pal[3 * i + 1] = (BYTE)(((i >> 2) & 7) * 9); pal[3 * i + 2] = (BYTE)((i & 3) * 21); }
        ScreenPalette(0, 256, pal);
    }
    zeichnen();
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_SCREENLOST: printf("VESA32: SCREENLOST\n"); return 0;
    case M_SCREENBACK: printf("VESA32: SCREENBACK\n"); ScreenRestore(); zeichnen(); return 0;
    case M_KEY:
        if (p1 == K_ENTER) { naechster(); return 0; }
        if (p1 == K_ESC) { ScreenRelease(w); DestroyWindow(w); return 0; }
        break;
    case M_DESTROY: PostQuit(0); return 0;
    case M_PAINT: { PAINT ps; BeginPaint(w, &ps); EndPaint(w, &ps); } return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; static VESAMODE l[64]; WORD n, i;
    (void)cmd; (void)show;
    n = ScreenVesaList(l, 64);
    printf("VESA32: %u lineare Modi\n", n);
    for (i = 0; i < n; i++) printf("VESA32: Modus %03X %ux%ux%u\n", l[i].nr, l[i].w, l[i].h, l[i].bpp);
    wc.name = "Vesa32"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) return 1;
    fen = CreateWindow("Vesa32", "RD2/32 VESA", WS_TITLE | WS_BORDER | WS_VISIBLE, 0, 0, 200, 60, 0, 0, inst);
    if (!fen || !ScreenAcquire(fen)) { printf("VESA32: ScreenAcquire Fehler %u\n", LastError()); return 2; }
    naechster();
    while (GetMessage(&m)) DispatchMessage(&m);
    printf("VESA32: Ende\n");
    return 0;
}
