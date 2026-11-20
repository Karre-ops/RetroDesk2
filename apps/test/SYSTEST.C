/* SYSTEST.C - prueft SysInfo und BedarfCheck (API 2.3): gibt alle Felder ueber DbgHex aus und endet mit 0 (ok) oder 1 */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    static RDSYSDATA d; WORD e;
    (void)inst; (void)cmd; (void)show;
    d.groesse = 2;                                /* zu klein: muss abgelehnt werden */
    if (SysInfo(&d) == 0) { DbgPrint("SYSTEST: zu kleine Groesse angenommen -> FEHLER"); return 1; }
    d.groesse = sizeof d;
    e = SysInfo(&d);
    DbgHex("SYSTEST: Rueckgabe", e);
    DbgHex("SYSTEST: cpu", d.cpu); DbgHex("SYSTEST: fpu", d.fpu); DbgHex("SYSTEST: dos", d.dos);
    DbgHex("SYSTEST: kb_konv", d.kb_konv); DbgHex("SYSTEST: kb_frei", d.kb_frei);
    DbgHex("SYSTEST: kb_ems", d.kb_ems); DbgHex("SYSTEST: kb_xms", d.kb_xms);
    DbgHex("SYSTEST: grafik", d.grafik); DbgHex("SYSTEST: sound", d.sound); DbgHex("SYSTEST: netz", d.netz);
    DbgHex("SYSTEST: festplatte", d.festplatte); DbgHex("SYSTEST: d3", d.d3);
    DbgHex("SYSTEST: d3_hersteller", d.d3_hersteller); DbgHex("SYSTEST: d3_geraet", d.d3_geraet);
    {   static const char *const n[] = { "A:\\RD2\\XB1.EXE", "A:\\RD2\\XB2.EXE", "A:\\RD2\\LEER.EXE", "A:\\FDAPM.COM", "A:\\GIBTSNICHT.EXE" };
        WORD i;
        for (i = 0; i < 5; i++) { DbgPrint(n[i]); DbgHex("SYSTEST: Bedarf", BedarfCheck(n[i])); }
    }
    DbgPrint("SYSTEST: Ende");
    return e ? 1 : 0;
}
