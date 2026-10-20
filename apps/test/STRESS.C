/* STRESS.C - Dauertest: startet 5 WURM-Tasks, die gleichzeitig Speicher belegen/pruefen,
 * Nachrichten schicken und mit dem 8087 rechnen (Zeitscheiben unterbrechen mitten drin). */
#include "RD2.H"
#define NW 5
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    MSG m; WORD i, fertig = 0, gut = 0, msgs = 0; char arg[3]; DWORD t0 = TickCount();
    (void)inst; (void)cmd; (void)show;
    arg[0] = (char)('A' + TaskSelf()); arg[2] = 0;
    for (i = 0; i < NW; i++) { arg[1] = (char)('1' + i); if (!TaskExec("WURM.EXE", arg, SW_SHOW)) DbgPrint("STRESS: Start FEHLER"); }
    while (fertig < NW && GetMessage(&m)) {
        if (m.msg == M_USER + 10) msgs++;
        if (m.msg == M_USER + 11) { fertig++; if (m.p1 == 1) gut++; }
    }
    DbgHex("STRESS: Nachrichten", msgs);
    DbgHex("STRESS: Dauer ms", TickCount() - t0);
    DbgPrint(gut == NW ? "STRESS: alle Wuermer ok" : "STRESS: FEHLER bei Wuermern");
    return 0;
}
