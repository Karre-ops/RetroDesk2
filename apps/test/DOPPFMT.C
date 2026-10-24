/* DOPPFMT.C - startet RDFORMAT zweimal (Audit M4): das zweite muss "Es laeuft schon eine Formatierung"
 * melden und sich beenden. Mit F: RDFDISK, dann RDFORMAT C: (Meldung 35); mit G: RDFORMAT B:, dann RDFDISK
 * (Meldung 24). test/doppfmt.sh */
#include "RD2.H"
short RDAPI RdMain(HINST inst, LPCSTR cl, WORD show)
{
    HTASK a, b; DWORD t;
    (void)inst; (void)cl; (void)show;
    if (cl[0] == 'F' || cl[0] == 'G') {          /* G23: RDFDISK und RDFORMAT (Festplatte) nicht zugleich */
        a = cl[0] == 'F' ? TaskExec("RDFDISK.EXE", "", SW_SHOW) : TaskExec("RDFORMAT.EXE", "B:", SW_SHOW);
        DbgHex("DOPPFMT: erstes", a);
        t = TickCount(); while (TickCount() - t < 36) TaskYield();
        b = cl[0] == 'F' ? TaskExec("RDFORMAT.EXE", "C:", SW_SHOW) : TaskExec("RDFDISK.EXE", "", SW_SHOW);
        DbgHex("DOPPFMT: zweites", b);
        return 0;
    }
    a = TaskExec("RDFORMAT.EXE", "B:", SW_SHOW);
    DbgHex("DOPPFMT: erstes", a);
    t = TickCount(); while (TickCount() - t < 36) TaskYield();   /* erstes ist sicher angelaufen */
    b = TaskExec("RDFORMAT.EXE", "B:", SW_SHOW);
    DbgHex("DOPPFMT: zweites", b);
    return 0;
}
