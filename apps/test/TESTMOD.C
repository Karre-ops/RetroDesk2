/* TESTMOD.C - Testmodul (verwerfbar): Quadrat und Aufrufzaehler.
 * Der Zaehler steht im Datensegment des Moduls und ist nach dem Nachladen wieder 0. */
#include "RD2.H"
static WORD aufrufe;
WORD RDEXPORT Quadrat(WORD x) { aufrufe++; return x * x; }
WORD RDEXPORT Aufrufe(void)   { return aufrufe; }
static struct { WORD n; FARPROC f[2]; } ex = { 2, { (FARPROC)Quadrat, (FARPROC)Aufrufe } };
void FAR * RDAPI RdModInit(void) { return &ex; }
