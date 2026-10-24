/* DOSTEST.C - RunDos: RD2 auslagern, DOS-Programm starten, zurueckkommen.
 * Aufruf: DOSTEST pfad\FREIMEM.EXE ausgabedatei [N]. Mit N (Diskettenbetrieb): RD_DOS_KEEP muss
 * abgelehnt werden, ohne KEEP startet RD neu; die Marke X:\DOSTEST.MRK zeigt den zweiten Lauf.
 * Ohne N: prueft Rueckgabecode 42, dass Speicherblock,
 * eigene Daten und Grafik danach wieder stimmen, und den Fehler bei fehlendem Programm. */
#include "RD2.H"
static WORD muster[512];
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    char prog[80], arg[80]; WORD i, j, r, bad = 0; HMEM m; WORD FAR *p; HDC dc; DWORD v60; RECT rc = { 10, 10, 50, 30 };
    (void)inst; (void)show;
    for (i = 0; cmd[i] && cmd[i] != ' ' && i < 79; i++) prog[i] = cmd[i];
    prog[i] = 0; while (cmd[i] == ' ') i++;
    for (j = 0; cmd[i] && cmd[i] != ' ' && j < 60; ) arg[j++] = cmd[i++];
    if (cmd[i] == ' ' && (cmd[i + 1] | 0x20) == 'n') {
        static char mrk[] = "A:\\DOSTEST.MRK"; short f;
        mrk[0] = prog[0];
        if ((f = FileOpen(mrk, FO_READ)) != -1) {      /* zweiter oder dritter Lauf */
            char z = 0; FileRead(f, &z, 1); FileClose(f);
            if (z == '1') {                             /* nochmal: jetzt traegt RD beim Rest ein (0D5h) */
                f = FileCreate(mrk, 0); FileWrite(f, "2", 1); FileClose(f);
                DbgPrint("DOSTEST: zweiter Lauf, nochmal RunDos");
                arg[j - 1] = '2';                       /* Ausgabe FREI.TX2 */
                for (i = 0; i < 6; i++) arg[j++] = " zwei."[i];
                arg[j - 1] = 0;
                RunDos(prog, arg, 0);
                DbgPrint("DOSTEST: FEHLER, RunDos kam zurueck"); return 1;
            }
            FileDelete(mrk);
            DbgPrint("DOSTEST: nach Neustart wieder da"); return 0;
        }
        for (i = 0; i < 6; i++) arg[j++] = " hallo"[i];
        arg[j] = 0;
        r = RunDos(prog, arg, RD_DOS_KEEP);
        DbgHex("DOSTEST: mit KEEP", r); DbgHex("DOSTEST: Fehler", LastError());
        f = FileCreate(mrk, 0); FileWrite(f, "1", 1); FileClose(f);
        r = RunDos(prog, arg, 0);
        DbgHex("DOSTEST: FEHLER, RunDos kam zurueck", r);
        return 1;
    }
    for (i = 0; i < 6; i++) arg[j++] = " hallo"[i];
    arg[j] = 0;
    for (i = 0; i < 512; i++) muster[i] = i * 7 + 3;
    m = MemAlloc(32768u, MEM_MOVABLE); p = (WORD FAR *)MemLock(m);
    for (i = 0; i < 16384; i++) p[i] = i ^ 0x5A5A;
    MemUnlock(m);
    dc = GetDC(0);
    DbgHex("DOSTEST: frei vorher", MemAvail(MEMA_KONV));
    v60 = *(DWORD __far *)0x00000180UL;
    r = RunDos(prog, arg, RD_DOS_KEEP);
    DbgHex("DOSTEST: Vektor 60h gleich", *(DWORD __far *)0x00000180UL == v60);
    DbgHex("DOSTEST: frei danach", MemAvail(MEMA_KONV));
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
