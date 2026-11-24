/* TAKTTEST.C - prueft den Systemtakt 145,6 Hz (PLAN_API Schritt 2): Rueckrufe mit Teiler 1 und 8 ueber 2 s,
 * TaktCount, BIOS-Uhr (18,2 Hz muss bleiben), TaktKill, alle 8 Plaetze belegt. Ergebnis ueber DbgHex, Ende mit 0. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"

static volatile WORD n1, n8, nx;
static void RDAPI __loadds t1(void) { n1++; }
static void RDAPI __loadds t8(void) { n8++; }
static void RDAPI __loadds tx(void) { nx++; }
static DWORD bios(void) { return *(volatile DWORD __far *)0x0040006CL; }
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD a, b, i, k[8], n = 0; DWORD t0, b0, c0;
    (void)inst; (void)cmd; (void)show;
    a = TaktSet(t1, 1); b = TaktSet(t8, 8);
    c0 = TaktCount(); b0 = bios(); t0 = TickCount();
    while (TickCount() - t0 < 2000) TaskYield();
    DbgHex("TAKTTEST: ms", TickCount() - t0);
    DbgHex("TAKTTEST: Takte", TaktCount() - c0);
    DbgHex("TAKTTEST: BIOS", bios() - b0);
    DbgHex("TAKTTEST: Teiler1", n1); DbgHex("TAKTTEST: Teiler8", n8);
    TaktKill(a); i = n1; t0 = TickCount(); while (TickCount() - t0 < 300) TaskYield();
    DbgHex("TAKTTEST: nach Kill", n1 - i);
    for (i = 0; i < 8; i++) if ((k[i] = TaktSet(tx, 1)) != 0) n++;
    DbgHex("TAKTTEST: Plaetze", n);              /* 7: b haelt noch einen */
    for (i = 0; i < 8; i++) TaktKill(k[i]);
    TaktKill(b);
    DbgPrint("TAKTTEST: Ende");
    return 0;
}
