/* FREI.C - Testprogramm fuer frei.sh (PLAN_FS): DriveInfo von C: ueber COM1 */
#include "RD2.H"

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    DRIVEINFO di;
    (void)inst; (void)cmd; (void)show;
    if (DriveInfo(3, &di)) { DbgPrint("FREI: Fehler"); return 1; }
    DbgHex("FREI: frei", di.frei); DbgHex("FREI: gesamt", di.gesamt);
    return 0;
}
