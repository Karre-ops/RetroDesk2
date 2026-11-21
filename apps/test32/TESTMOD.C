/* TESTMOD.C - Pruefung der 32-Bit-Module (rd32_modload): laedt TESTM32.M32 zweimal, ruft Exporte, Fenster mit
 * Prozedur im Modul, Speicher ueber das Programm, Freigabe. Ergebnis im RD2-Protokoll, Ende mit 0. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include <stdio.h>
#include <string.h>
#include "RD2_32.H"

static int n, fehl;
static void ist(int ok, const char *was) { n++; if (!ok) { fehl++; printf("TESTMOD FEHLER: %s\n", was); } }

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    void *m, *m2; int (*add)(int, int), (*zl)(void), (*zl2)(void), (*sp)(void), (*fe)(HINST); const char *(*tx)(double); DWORD (*tk)(void);
    (void)cmd; (void)show;
    m = rd32_modload("TESTM32.M32"); ist(m != 0, "rd32_modload");
    if (!m) { printf("TESTMOD: LastError %u\n", LastError()); return 1; }
    add = rd32_modproc(m, "Addiere"); zl = rd32_modproc(m, "Zaehler"); tx = rd32_modproc(m, "Text");
    tk = rd32_modproc(m, "Ticks"); sp = rd32_modproc(m, "Speicher"); fe = rd32_modproc(m, "Fenster");
    ist(add && zl && tx && tk && sp && fe && !rd32_modproc(m, "Fehlt"), "rd32_modproc");
    if (add && zl && tx && tk && sp && fe) {
        ist(add(2, 3) == 5, "Addiere (RdModInit gelaufen)");
        ist(zl() == 1 && zl() == 2, "Zaehler");
        ist(!strcmp(tx(2.0), "Wurzel 1.414214"), "Text mit sprintf und sqrt im Modul");
        ist(tk() > 0, "RD2-API aus dem Modul");
        ist(sp() == 1, "malloc/realloc im Modul (Speicher ueber das Programm)");
        ist(fe(inst) == 1234, "Fenster mit Prozedur im Modul");
        m2 = rd32_modload("TESTM32.M32"); zl2 = m2 ? rd32_modproc(m2, "Zaehler") : 0;
        ist(m2 && m2 != m && zl2 && zl2() == 1 && zl() == 3, "zweite Ladung mit eigenen Daten");
        if (m2) rd32_modfree(m2);
        ist(zl() == 4, "erste Ladung nach Freigabe der zweiten");
    }
    ist(!rd32_modload("NIX.M32") && LastError() == RD_ERR_NOTFOUND, "fehlende Datei");
    rd32_modfree(m);
    m = rd32_modload("TESTM32.M32"); ist(m != 0, "erneut laden");   /* bleibt geladen: Programmende gibt es frei */
    printf("TESTMOD: %d Pruefungen, Fehler %d\n", n, fehl);
    return 0;
}
