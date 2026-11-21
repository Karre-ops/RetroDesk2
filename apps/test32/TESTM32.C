/* TESTM32.C - 32-Bit-Beispielmodul fuer RD2/32 (TESTM32.M32, geladen von TESTMOD): eigene Daten je Ladung,
 * C-Laufzeit mit Gleitkomma und malloc, RD2-API, ein Fenster mit Fensterprozedur im Modul. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "RD2_32.H"

static int zaehl, init; static char puf[64];
static void ende(void) { DbgPrint("TESTM32: Ende"); }
int RdMod32Init(void) { init = 42; atexit(ende); return 0; }

int Addiere(int a, int b) { return a + b + (init == 42 ? 0 : 1000); }
int Zaehler(void) { return ++zaehl; }
const char *Text(double x) { sprintf(puf, "Wurzel %.6f", sqrt(x)); return puf; }
DWORD Ticks(void) { return TickCount(); }
int Speicher(void)
{
    char *p = malloc(300000UL), *q;
    if (!p) return 0;
    memset(p, 7, 300000UL); q = realloc(p, 600000UL);
    if (!q || q[299999UL] != 7) return 0;
    free(q); return 1;
}
static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    if (msg == M_USER + 5) return 1000 + p1;
    return DefWindowProc(w, msg, p1, p2);
}
int Fenster(HINST inst)
{
    WNDCLASS wc; HWND w; DWORD r;
    wc.name = "TestM32"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) return -1;
    if (!(w = CreateWindow(wc.name, "Modul", WS_TITLE | WS_BORDER, 10, 10, 200, 100, 0, 0, inst))) return -2;
    r = SendMessage(w, M_USER + 5, 234, 0);
    DestroyWindow(w);
    return (int)r;
}
