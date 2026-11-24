/* TAKT32.C - Systemtakt fuer RD2/32 (PLAN_API 2b): Rueckrufe mit Teiler 1 und 8 ueber 2 s, TaktCount, BIOS-Uhr,
 * Warten in einer Schleife ohne API (Rueckrufe kommen im eigenen Code), TaktKill. Ergebnis ueber DbgHex, Ende mit 0. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"

static volatile WORD n1, n8;
static void RDAPI t1(void) { n1++; }
static void RDAPI t8(void) { n8++; }
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD a, b, i; DWORD t0, b0, c0; volatile DWORD __far *bios = (volatile DWORD __far *)(BiosPtr() + 0x46C);   /* Speicherschutz: nur ueber LOW */
    (void)inst; (void)cmd; (void)show;
    a = TaktSet(t1, 1); b = TaktSet(t8, 8);
    DbgHex("TAKT32: Nummern", ((DWORD)a << 16) | b);
    c0 = TaktCount(); b0 = *bios; t0 = TickCount();
    while (TickCount() - t0 < 2000) TaskYield();
    DbgHex("TAKT32: ms", TickCount() - t0);
    DbgHex("TAKT32: Takte", TaktCount() - c0);
    DbgHex("TAKT32: BIOS", *bios - b0);
    DbgHex("TAKT32: Teiler1", n1); DbgHex("TAKT32: Teiler8", n8);
    i = n1; while ((WORD)(n1 - i) < 145) ;       /* ohne API: Rueckrufe im eigenen Code (1 s) */
    DbgPrint("TAKT32: Schleife ok");
    TaktKill(a); i = n1; t0 = TickCount(); while (TickCount() - t0 < 300) TaskYield();
    DbgHex("TAKT32: nach Kill", n1 - i);
    TaktKill(b);
    DbgPrint("TAKT32: Ende");
    return 0;
}
