/* SDK 2.1: Jede Allokation und jeden MemLock pruefen. Kein Zeiger nach MemUnlock.
 * Bestehende App-Daten bei Fehlern behalten; hier besitzt die Demo nur einen Testblock. */
#include "RD2.H"
static void fehlermelden(WORD fehler, WORD grund)
{
    DbgHex("SDK: LastError", fehler); DbgHex("SDK: MemError", grund);
    /* Ein Dialog kann bei Speichermangel selbst scheitern. Kein Zwang zum Programmabbruch. */
    MsgBox(0, "Nicht genug Arbeits- oder Auslagerungsspeicher.\nDie Aktion wurde abgebrochen; vorhandene Daten bleiben erhalten.",
           "Speicher", MB_OK | MB_WARN);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HMEM h; BYTE FAR *p; WORD fehler, grund;
    (void)inst; (void)cmd; (void)show;
    h = MemAlloc(32768, MEM_MOVABLE);
    if (!h) { fehler = LastError(); grund = MemError(); fehlermelden(fehler, grund); return 0; }
    p = (BYTE FAR *)MemLock(h);
    if (!p) {
        fehler = LastError(); grund = MemError(); /* vor weiteren Speicheraufrufen sichern */
        MemFree(h); fehlermelden(fehler, grund); return 0;
    }
    p[0] = 42;                                  /* nur im gesperrten Abschnitt auf Daten zugreifen */
    MemUnlock(h); p = 0;
    /* Fuer den naechsten Zugriff erneut MemLock(h) aufrufen und erneut pruefen. */
    MemFree(h);
    return 0;
}
