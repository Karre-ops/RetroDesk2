/* RDDISK.C - Laufwerkszugriff fuer RDFORMAT/RD2SETUP (verwerfbar, Diskette 2).
 * RD2-Programme rufen DOS und BIOS nie direkt auf. Die Werkzeuge aus FreeDOS (FORMAT, SYS)
 * brauchen aber INT 13h, INT 25h/26h und einige INT-21h-Funktionen. Dieses Modul fuehrt genau
 * diese Aufrufe aus (Positivliste) und lehnt alles andere ab (Carry, AX = 1, Eintrag im Debug-Log).
 * Module laufen nicht im Programmcode: der Kern schaltet waehrend eines Aufrufs nicht um.
 * Export 1: Ende. Export 2: WORD RddInt(WORD nr, union REGS FAR *ein, union REGS FAR *aus, struct SREGS FAR *s)
 *   wie int86x; liefert aus->x.ax, aus->x.cflag = Carry. */
#include <i86.h>
#include "RD2.H"

static union REGS r; static struct SREGS sr;      /* statisch: Module laufen mit SS != DS (-zu) */

/* INT 25h/26h lassen die Flags auf dem Stapel; DS:BX zeigt bei DOS 4+ auf das Paket (CX = FFFFh) */
extern unsigned long abs25(WORD ax, WORD cx, WORD dx, WORD bx, WORD ds);
extern unsigned long abs26(WORD ax, WORD cx, WORD dx, WORD bx, WORD ds);
#pragma aux abs25 = "push bp" "push ds" "mov ds, si" "int 25h" "pop si" "sbb si, si" "pop ds" "pop bp" "mov dx, si" \
    parm [ax] [cx] [dx] [bx] [si] value [dx ax] modify [bx cx di es]
#pragma aux abs26 = "push bp" "push ds" "mov ds, si" "int 26h" "pop si" "sbb si, si" "pop ds" "pop bp" "mov dx, si" \
    parm [ax] [cx] [dx] [bx] [si] value [dx ax] modify [bx cx di es]

static WORD erlaubt(WORD nr, WORD ax)
{
    BYTE ah = (BYTE)(ax >> 8), al = (BYTE)ax;
    if (nr == 0x13 || nr == 0x25 || nr == 0x26) return 1;
    if (nr != 0x21) return 0;
    switch (ah) {
    case 0x0D: case 0x19: case 0x2A: case 0x2C: case 0x30: case 0x32: case 0x36: case 0x44: case 0x59: return 1;
    case 0x33: return al == 0x06;                 /* echte DOS-Version */
    case 0x35: case 0x25: return al == 0x1E;      /* nur Diskettentabelle (INT 1Eh) */
    case 0x73: return 1;                          /* FAT32-Funktionen (DOS 7), sonst Fehler von DOS */
    }
    return 0;
}

static void RDEXPORT d_schluss(void) { }
static WORD RDEXPORT d_Int(WORD nr, union REGS FAR *ein, union REGS FAR *aus, struct SREGS FAR *s)
{
    unsigned long e;
    r = *ein; sr = *s;
    if (!erlaubt(nr, r.x.ax)) {
        DbgPrint("RDDISK: Aufruf abgelehnt");
        r.x.ax = 1; r.x.cflag = 1; *aus = r; return 1;
    }
    if (nr == 0x25 || nr == 0x26) {
        e = nr == 0x25 ? abs25(r.x.ax, r.x.cx, r.x.dx, r.x.bx, sr.ds) : abs26(r.x.ax, r.x.cx, r.x.dx, r.x.bx, sr.ds);
        r.x.ax = (WORD)e; r.x.cflag = (WORD)(e >> 16) ? 1 : 0;
    } else int86x((int)nr, &r, &r, &sr);
    *aus = r; *s = sr;
    return r.x.ax;
}

static struct { WORD n; FARPROC f[2]; } ext = { 2, { (FARPROC)d_schluss, (FARPROC)d_Int } };
void FAR * RDAPI RdModInit(void) { return &ext; }
