/* RECHNE.C - rechnet ca. 2 Sekunden, ohne je GetMessage aufzurufen (Test der Zeitscheiben) */
#include "RD2.H"
static volatile DWORD x;
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HTASK ping = (HTASK)(cmd[0] - 'A'); DWORD t0 = TickCount(), n = 0; WORD j;
    (void)inst; (void)show;
    DbgPrint("RECHNE: beginnt");
    while (TickCount() - t0 < 2000) {
        for (j = 0; j < 20000; j++) x = x * 3 + j;        /* reine Rechnung im eigenen Code */
        n++;
    }
    DbgHex("RECHNE: fertig, Runden", n);
    PostTask(ping, M_USER + 3, 1, 0);
    return 0;
}
