/* DOSTEST.C - RunDos: RD2 auslagern, DOS-Programm starten, zurueckkommen.
 * Aufruf: DOSTEST pfad\FREIMEM.EXE ausgabedatei. Prueft Rueckgabecode 42, dass Speicherblock,
 * eigene Daten und Grafik danach wieder stimmen, und den Fehler bei fehlendem Programm. */
#include "RD2.H"
static WORD muster[512];
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    char prog[80], arg[80]; WORD i, j, r, bad = 0; HMEM m; WORD FAR *p; HDC dc; RECT rc = { 10, 10, 50, 30 };
    (void)inst; (void)show;
    for (i = 0; cmd[i] && cmd[i] != ' ' && i < 79; i++) prog[i] = cmd[i];
    prog[i] = 0; while (cmd[i] == ' ') i++;
    for (j = 0; cmd[i] && j < 60; ) arg[j++] = cmd[i++];
    for (i = 0; i < 6; i++) arg[j++] = " hallo"[i];
    arg[j] = 0;
    for (i = 0; i < 512; i++) muster[i] = i * 7 + 3;
    m = MemAlloc(32768u, MEM_MOVABLE); p = (WORD FAR *)MemLock(m);
    for (i = 0; i < 16384; i++) p[i] = i ^ 0x5A5A;
    MemUnlock(m);
    dc = GetDC(0);
    DbgHex("DOSTEST: frei vorher", MemAvail(MEMA_KONV));
    r = RunDos(prog, arg, RD_DOS_KEEP);
    DbgHex("DOSTEST: Rueckgabe", r);
    if (r == 0xFFFF) DbgHex("DOSTEST: Fehler", LastError());
    for (i = 0; i < 512; i++) if (muster[i] != (WORD)(i * 7 + 3)) bad++;
    p = (WORD FAR *)MemLock(m);
    for (i = 0; i < 16384; i++) if (p[i] != (i ^ 0x5A5A)) bad++;
    MemUnlock(m); MemFree(m);
    DbgHex("DOSTEST: Abweichungen", bad);
    DbgHex("DOSTEST: frei nachher", MemAvail(MEMA_KONV));
    if (dc) {
        SetBrush(dc, C_BLACK, PAT_SOLID); FillRect(dc, &rc);
        DbgHex("DOSTEST: Grafik nachher", GetPixel(dc, 20, 20) == C_BLACK);
        ReleaseDC(0, dc);
    }
    r = RunDos("C:\\GIBTSNIX.EXE", "", RD_DOS_KEEP);
    DbgHex("DOSTEST: fehlendes Programm", r);
    DbgHex("DOSTEST: Fehler", LastError());
    DbgPrint("DOSTEST: fertig");
    return 0;
}
