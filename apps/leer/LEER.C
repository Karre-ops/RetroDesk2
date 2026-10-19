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
    if (SndCaps() == 0 && LastError() == RD_ERR_NOTSUP)   /* Sound kommt erst in Phase 6 */
        DbgPrint("LEER: SndCaps fehlt noch (erwartet)");
    return v == RD2_API_VERSION ? 42 : 1;
}
