/* DOSART.C - Pruefung der DOS-Startarten (PLAN_DOS Schritte 2-4, 22.11.2026) fuer test/dosstart.sh.
 * Aufruf: DOSART X  mit X =
 *   B  .BAT ueber COMMAND.COM /C (C:\RD2\T.BAT hallo schreibt C:\BAT.TXT)
 *   H  RD2 haelt 6 Dateien offen, DOSCHK H zaehlt die freien Plaetze des DOS-Programms
 *   K  Tasten im Puffer vor dem Start, DOSCHK K: Puffer muss leer sein
 *   A  Startart Schnell: DOSCHK X C:\XMSA.TXT (freies XMS, RD2 ist dort ausgelagert)
 *   V  Startart Voll: DOSCHK X C:\XMSV.TXT, RD2 endet und startet neu (Marke C:\DOSART.MRK)
 *   T  TSRTEST bleibt resident: Rueckgabe gilt, LastError = RD_ERR_RESIDENT */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"
static void taste(WORD k);
#pragma aux taste = "mov ah, 5" "int 16h" parm [cx] modify [ax];
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD r, i; short f[6]; char t[16];
    (void)inst; (void)show;
    switch (cmd[0] | 0x20) {
    case 'b':
        r = RunDos("C:\\RD2\\T.BAT", "hallo", RD_DOS_KEEP);
        DbgHex("DOSART: BAT Rueckgabe", r); if (r == 0xFFFF) DbgHex("DOSART: Fehler", LastError());
        if ((f[0] = FileOpen("C:\\BAT.TXT", FO_READ)) != -1) { r = FileRead(f[0], t, 5); t[r] = 0; FileClose(f[0]); DbgPrint("DOSART: BAT.TXT"); DbgPrint(t); }
        break;
    case 'h':
        for (i = 0; i < 6; i++) f[i] = FileOpen("C:\\RD2\\RD2.EXE", FO_READ | FO_DENYNONE);
        r = RunDos("C:\\RD2\\DOSCHK.COM", "H", RD_DOS_KEEP);
        DbgHex("DOSART: freie Dateiplaetze", r);
        for (i = 0; i < 6; i++) if (f[i] != -1) FileClose(f[i]);
        break;
    case 'k':
        taste(0x1C0D); taste(0x1E61); taste(0x011B);   /* Enter, a, Esc */
        r = RunDos("C:\\RD2\\DOSCHK.COM", "K", RD_DOS_KEEP);
        DbgHex("DOSART: Taste im Puffer", r);
        break;
    case 'a':
        r = RunDos("C:\\RD2\\DOSCHK.COM", "X C:\\XMSA.TXT", RD_DOS_KEEP);
        DbgHex("DOSART: Schnell Rueckgabe", r);
        break;
    case 'v':
        if ((f[0] = FileOpen("C:\\DOSART.MRK", FO_READ)) != -1) { FileClose(f[0]); FileDelete("C:\\DOSART.MRK"); DbgPrint("DOSART: nach Neustart wieder da"); break; }
        f[0] = FileCreate("C:\\DOSART.MRK", 0); FileWrite(f[0], "1", 1); FileClose(f[0]);
        r = RunDos("C:\\RD2\\DOSCHK.COM", "X C:\\XMSV.TXT", RD_DOS_VOLL);
        DbgHex("DOSART: FEHLER, Voll kam zurueck", r); DbgHex("DOSART: Fehler", LastError());
        return 1;
    case 't':
        r = RunDos("C:\\RD2\\TSRTEST.EXE", "", RD_DOS_KEEP);
        DbgHex("DOSART: TSR Rueckgabe", r); DbgHex("DOSART: TSR gemeldet", LastError() == RD_ERR_RESIDENT);
        break;
    }
    DbgPrint("DOSART: fertig");
    return 0;
}
