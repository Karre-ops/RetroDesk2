/* RD32STUB.C - 16-Bit-Vorspann fuer RD2/32-Programme (PLAN_32BIT Schritt 4a). Laedt RD32.MOD und startet das
 * 32-Bit-Bild (Ressource 11). Mit BEDARF CPU 386 in der .RC meldet die Shell auf 8088/286 "braucht 386". */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HMOD m; short r;
    if (!(m = ModLoad("RD32.MOD"))) { MsgBox(0, "RD32.MOD fehlt (RD2/32-Programme ab 386).", "RD2/32", MB_OK | MB_ERROR); return 1; }
    r = ((short (RDAPI *)(HINST, LPCSTR, WORD))ModProc(m, 2))(inst, cmd, show);
    ModFree(m);
    return r;
}
