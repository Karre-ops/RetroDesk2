/* RD2_32.H - RD2/32-Programme (32 Bit, flach, ab 386), PLAN_32BIT Schritt 4a: Grundfunktionen und Vollbild.
 * SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
 * Bauen: wcc386 -bt=dos -mf -zl -s, linken mit RD32C0 (zuerst) und RD32LIB, wlink format raw bin, dann
 * tools/rd32link.py (Bild als Ressource 11 in den 16-Bit-Vorspann RD32STUB). Handles und Nachrichten wie RD2.H,
 * Zeiger sind 32-Bit-Zeiger; die Bibliothek kopiert Texte und Strukturen fuer den Kern unter 1 MB.
 * Schritt 4a: Fenster mit Nachrichtenschleife (bis vier Klassen), Debugausgabe, MsgBox, Vollbild 13h. */
#ifndef RD2_32_H
#define RD2_32_H
typedef unsigned char BYTE; typedef unsigned short WORD; typedef unsigned long DWORD; typedef WORD BOOL;
typedef WORD HWND, HDC, HINST, HTASK;
typedef DWORD COLOR;
#define TRUE 1
#define FALSE 0
#pragma pack(push, 1)
typedef struct { short x, y; } POINT;
typedef struct { short l, t, r, b; } RECT;
typedef struct { HWND w; WORD msg; WORD p1; DWORD p2; DWORD time; POINT pt; } MSG;
typedef struct { HDC dc; RECT neu; BOOL loeschen; } PAINT;
typedef DWORD (*WNDPROC)(HWND w, WORD msg, WORD p1, DWORD p2);
typedef struct { const char *name; WNDPROC proc; WORD stil, daten; HINST inst; WORD symbol; COLOR hinter; } WNDCLASS;
typedef struct { WORD modus, w, h, bpp, zeilenbytes, seg, ebenen; BYTE *bild; } SCREENINFO;   /* bild: 32-Bit-Zeiger */
#pragma pack(pop)
#define M_DESTROY     0x0002
#define M_PAINT       0x000F
#define M_KEY         0x0100
#define M_SCREENLOST  0x0322
#define M_SCREENBACK  0x0323
#define K_ESC         27
#define WS_TITLE      0x00000001UL
#define WS_BORDER     0x00000040UL
#define WS_VISIBLE    0x00000200UL
#define RD_DEFAULT    0xFFFFFFFFUL
#define MB_OK         0x0000
#define MB_ERROR      0x0010
#define SM_TEXT       0
#define SM_13H        4

short RdMain(HINST inst, const char *cmd, WORD show);   /* vom Programm */
void *rd32_lin(DWORD lin);                        /* lineare Adresse (z. B. 0xA0000) als Zeiger */
DWORD rd32_api(WORD nr, WORD nw, const WORD *w);  /* roh: nw Woerter in Push-Reihenfolge (pascal) */

void  DbgPrint(const char *t);
void  DbgHex(const char *t, DWORD v);
WORD  LastError(void);
DWORD TickCount(void);
void  TaskYield(void);
short MsgBox(HWND owner, const char *text, const char *titel, WORD flags);
WORD  RegisterClass(const WNDCLASS *wc);
HWND  CreateWindow(const char *klasse, const char *titel, DWORD stil, short x, short y, short w, short h, HWND eltern, WORD id, HINST i);
BOOL  DestroyWindow(HWND w);
DWORD DefWindowProc(HWND w, WORD msg, WORD p1, DWORD p2);
BOOL  GetMessage(MSG *m);
DWORD DispatchMessage(const MSG *m);
void  PostQuit(WORD code);
HDC   BeginPaint(HWND w, PAINT *ps);
void  EndPaint(HWND w, PAINT *ps);
WORD  ScreenAcquire(HWND w);
void  ScreenRelease(HWND w);
WORD  ScreenMode(WORD modus, SCREENINFO *si);     /* 4a: SM_13H und SM_TEXT */
void  ScreenPalette(WORD erste, WORD n, const BYTE *rgb);
void  ScreenRestore(void);
#endif
