/* MODTEST.C - Modul laden, aufrufen, Speicher bis zum Rand fuellen (die Reserve zum
 * Nachladen muss bleiben), zweites Modul laden (verdraengt das erste), Diskette wechseln
 * lassen (Testskript), erstes erneut aufrufen: Kern muss nach der richtigen Diskette
 * fragen und nachladen, obwohl der Speicher sonst voll ist. */
#include "RD2.H"
typedef WORD (RDAPI *QFN)(WORD);
typedef WORD (RDAPI *AFN)(void);
static HMEM h[180];
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HMOD m, m2; QFN q; AFN a; WORD k, i; MSG msg; DRIVEINFO di; DWORD alt; char altlab[12];
    (void)inst; (void)cmd; (void)show;
    m = ModLoad("TESTMOD.MOD");
    q = (QFN)ModProc(m, 1); a = (AFN)ModProc(m, 2);
    DbgHex("MODTEST: Quadrat(7)", q(7));
    q(3);
    DbgHex("MODTEST: Aufrufe", a());
    for (k = 0; k < 150 && (h[k] = MemAlloc(4096, MEM_MOVABLE)) != 0; k++) ;   /* Speicherdruck */
    while (k < 180 && (h[k] = MemAlloc(128, MEM_MOVABLE)) != 0) k++;         /* bis auf den letzten Rest */
    DbgPrint(a() == 2 ? "MODTEST: Modul nach Fuellen noch da" : "MODTEST: Modul nach Fuellen weg");
    m2 = ModLoad("TESTMOD2.MOD");                  /* passt nur in die Reserve: verdraengt TESTMOD */
    DbgHex("MODTEST: zweites Modul", m2 ? ((QFN)ModProc(m2, 1))(5) : 0);
    MemCompact(0xFFFFFUL);                         /* Reserve reicht fuer das groesste verwerfbare Modul (RDKX),
                                                      daher hier ausdruecklich verwerfen lassen */
    DriveInfo(1, &di); alt = di.serial; for (k = 0; k < 12; k++) altlab[k] = di.label[k];
    DbgPrint("MODTEST: jetzt Diskette wechseln");
    TimerSet(0, 1, 500);                           /* warten, bis eine andere Diskette drin ist (max. 60 s) */
    for (i = 0; i < 120 && GetMessage(&msg); )
        if (msg.msg == M_TIMER) { i++; if (!DriveInfo(1, &di) && (di.serial != alt || di.label[0] != altlab[0])) break; }
    TimerKill(0, 1);
    DbgPrint(i < 120 ? "MODTEST: andere Diskette erkannt" : "MODTEST: keine andere Diskette");
    DbgHex("MODTEST: Quadrat(8)", q(8));
    DbgHex("MODTEST: Aufrufe nach Nachladen", a());
    for (i = 0; i < k; i++) MemFree(h[i]);
    ModFree(m2);
    ModFree(m);
    return 0;
}
