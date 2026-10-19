/* LEER.C - kleinstes RD2-Programm (Phase 1): meldet sich ueber DbgPrint und endet */
#include "RD2.H"

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD v = KernVersion();
    (void)show;
    DbgHex("LEER: Kern-API", v);
    DbgHex("LEER: HINST", inst);
    DbgPrint("LEER: Befehlszeile:");
    DbgPrint(cmd);
    if (MemAlloc(100, MEM_MOVABLE) == 0)          /* gibt es in Phase 1 noch nicht */
        DbgPrint("LEER: MemAlloc fehlt noch (erwartet)");
    return v == RD2_API_VERSION ? 42 : 1;
}
