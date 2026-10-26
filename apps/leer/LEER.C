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
    if (NetCaps(0) == 0 && LastError() == RD_ERR_NOTSUP)  /* Netz kommt erst mit Punkt 8 (Klang ist seit 26.10. da) */
        DbgPrint("LEER: NetCaps fehlt noch (erwartet)");
    return v == RD2_API_VERSION ? 42 : 1;
}
