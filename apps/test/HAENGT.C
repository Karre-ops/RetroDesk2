/* HAENGT.C - haengt absichtlich (Test fuer Strg+Alt+Rueckschritt) */
#include "RD2.H"
static volatile WORD x;
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    (void)inst; (void)cmd; (void)show;
    DbgPrint("HAENGT: haenge jetzt");
    for (;;) x++;
}
