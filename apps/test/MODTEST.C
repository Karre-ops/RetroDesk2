/* MODTEST.C - Modul laden, aufrufen, durch Speicherdruck verwerfen lassen,
 * Diskette wechseln lassen (Testskript), erneut aufrufen: Kern muss nach der
 * richtigen Diskette fragen und nachladen. */
#include "RD2.H"
typedef WORD (RDAPI *QFN)(WORD);
typedef WORD (RDAPI *AFN)(void);
static HMEM h[160];
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HMOD m; QFN q; AFN a; WORD k, i; MSG msg;
    (void)inst; (void)cmd; (void)show;
    m = ModLoad("TESTMOD.MOD");
    q = (QFN)ModProc(m, 1); a = (AFN)ModProc(m, 2);
    DbgHex("MODTEST: Quadrat(7)", q(7));
    q(3);
    DbgHex("MODTEST: Aufrufe", a());
    for (k = 0; k < 160 && (h[k] = MemAlloc(4096, MEM_MOVABLE)) != 0; k++) ;   /* Speicherdruck */
    for (i = 0; i < k; i++) MemFree(h[i]);
    DbgPrint("MODTEST: jetzt Diskette wechseln");
    TimerSet(0, 1, 4000);
    while (GetMessage(&msg) && msg.msg != M_TIMER) ;
    TimerKill(0, 1);
    DbgHex("MODTEST: Quadrat(8)", q(8));
    DbgHex("MODTEST: Aufrufe nach Nachladen", a());
    ModFree(m);
    return 0;
}
