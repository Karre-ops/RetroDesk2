/* VOLL32.C - erstes RD2/32-Programm (PLAN_32BIT Schritt 4a): wie VOLLTEST, aber 32 Bit.
 * 1 MB Feld im Bild (ueber 64 KB, flach), Fenster, Vollbild 13h mit Farbverlauf direkt in A0000h,
 * Alt+Tab (M_SCREENLOST/M_SCREENBACK), Esc beendet. Befehlszeile: A = gleich wieder beenden, L = VESA 2.0 linear 640x480. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"

static SCREENINFO si; static WORD bilder, modus = SM_13H; static DWORD feld[262144];   /* 1 MB */
static void zeichnen(void)
{
    WORD x, y; BYTE *v = ScreenPtr();
    for (y = 0; y < si.h; y++) for (x = 0; x < si.w; x++) v[(DWORD)y * si.zeilenbytes + x] = (BYTE)(y * 200 / si.h);
    ScreenUpdate();                               /* Speicherschutz: 13h ist ein Schatten */
    bilder++; DbgHex("VOLL32: gezeichnet", bilder);
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_SCREENLOST: DbgPrint("VOLL32: SCREENLOST"); return 0;
    case M_SCREENBACK: DbgPrint("VOLL32: SCREENBACK"); ScreenRestore(); zeichnen(); return 0;
    case M_KEY: if (p1 == K_ESC) { DbgPrint("VOLL32: Esc"); ScreenRelease(w); DestroyWindow(w); return 0; } break;
    case M_DESTROY: PostQuit(0); return 0;
    case M_PAINT: { PAINT ps; BeginPaint(w, &ps); EndPaint(w, &ps); } return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HWND w; static BYTE pal[768]; DWORD i, bad = 0;
    (void)show;
    DbgPrint("VOLL32: 32 Bit laeuft");
    for (i = 0; i < 262144; i++) feld[i] = i * 2654435761UL;
    for (i = 0; i < 262144; i++) if (feld[i] != i * 2654435761UL) bad++;
    DbgHex("VOLL32: 1 MB Feld, Abweichungen", bad);
    wc.name = "Voll32"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) { DbgHex("VOLL32: RegisterClass Fehler", LastError()); return 1; }
    w = CreateWindow("Voll32", "RD2/32 Vollbild", WS_TITLE | WS_BORDER | WS_VISIBLE, 0, 0, 200, 60, 0, 0, inst);
    if (!w || !ScreenAcquire(w)) { DbgHex("VOLL32: ScreenAcquire Fehler", LastError()); return 2; }
    if (cmd[0] == 'L' || cmd[1] == 'L') modus = SM_LFB;
    if (!ScreenMode(modus, &si)) { ScreenRelease(w); return 3; }
    for (i = 0; i < 256; i++) { pal[i * 3] = (BYTE)(i >> 2); pal[i * 3 + 1] = (BYTE)(63 - (i >> 2)); pal[i * 3 + 2] = 0; }   /* gruen nach rot */
    ScreenPalette(0, 256, pal);
    DbgPrint("VOLL32: Vollbild");
    zeichnen();
    if (cmd[0] == 'A') { DbgPrint("VOLL32: automatisch Ende"); ScreenRelease(w); DestroyWindow(w); }   /* Test ohne Tastatur */
    while (GetMessage(&m)) DispatchMessage(&m);
    DbgPrint("VOLL32: Ende");
    return 0;
}
