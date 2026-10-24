/* DOPPFMT.C - startet RDFORMAT zweimal (Audit M4): das zweite muss "Es laeuft schon eine Formatierung"
 * melden und sich beenden. test/doppfmt.sh */
#include "RD2.H"
short RDAPI RdMain(HINST inst, LPCSTR cl, WORD show)
{
    HTASK a, b; DWORD t;
    (void)inst; (void)cl; (void)show;
    a = TaskExec("RDFORMAT.EXE", "B:", SW_SHOW);
    DbgHex("DOPPFMT: erstes", a);
    t = TickCount(); while (TickCount() - t < 36) TaskYield();   /* erstes ist sicher angelaufen */
    b = TaskExec("RDFORMAT.EXE", "B:", SW_SHOW);
    DbgHex("DOPPFMT: zweites", b);
    return 0;
}
