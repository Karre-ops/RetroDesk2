/* PING.C - Phase-2-Test: startet PONG, RECHNE und MEMTEST, tauscht Nachrichten mit PONG,
 * wartet auf Zeitgeber und am Ende auf RECHNE. Alles wird auf COM1 protokolliert. */
#include "RD2.H"
#define M_PING (M_USER + 1)
#define M_PONG (M_USER + 2)
#define M_FERTIG (M_USER + 3)

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    MSG m; HTASK pong, rechne, memt; WORD i, timer = 0, fertig = 0; char arg[2];
    (void)inst; (void)cmd; (void)show;
    arg[0] = (char)('A' + TaskSelf()); arg[1] = 0;            /* eigener Handle fuer die anderen */
    pong   = TaskExec("PONG.EXE", arg, SW_SHOW);
    rechne = TaskExec("RECHNE.EXE", arg, SW_SHOW);
    DbgHex("PING: gestartet PONG/RECHNE", MAKELONG(rechne, pong));
    for (i = 1; i <= 5; i++) {
        PostTask(pong, M_PING, i, 0);
        while (GetMessage(&m) && !(m.msg == M_PONG && m.p1 == i))
            if (m.msg == M_FERTIG) fertig = 1;
        DbgHex("PING: Antwort", i);
    }
    TimerSet(0, 7, 150);
    while (timer < 3 && GetMessage(&m)) {
        if (m.msg == M_TIMER && m.p1 == 7) timer++;
        if (m.msg == M_FERTIG) fertig = 1;
    }
    TimerKill(0, 7);
    DbgPrint(fertig ? "PING: Timer ok, RECHNE war schon fertig" : "PING: Timer ok, RECHNE rechnet noch");
    PostTask(pong, M_CLOSE, 0, 0);
    memt = TaskExec("MEMTEST.EXE", arg, SW_SHOW);
    DbgHex("PING: MEMTEST", memt);
    while (fertig < 2 && GetMessage(&m))                     /* RECHNE und MEMTEST melden sich */
        if (m.msg == M_FERTIG) fertig++;
    DbgPrint("PING: alle fertig");
    return 7;
}
