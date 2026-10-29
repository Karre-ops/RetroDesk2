/* RUNDOS.C - RunDos (SPEC 14): DOS-Programm mit fast vollem Speicher starten.
 * Der Kern und der belegte Teil der Arena gehen in den XMS-Speicher oder, ohne XMS, in eine
 * Datei auf der Festplatte (RD2SWAP.$$$ im RD2-Verzeichnis). Ein kleiner Rest (SWAP.ASM)
 * ganz oben im Speicher startet das Programm und holt danach alles an dieselbe Adresse
 * zurueck. Der Rest liegt als RDSWAP.BIN neben RD.EXE und kostet so keinen Kernspeicher.
 * Ohne XMS und ohne Festplatte (Neustart, nur ohne RD_DOS_KEEP): alle Programme enden, RD
 * beendet sich, der Rest startet das Programm und danach RD.EXE /W:seg neu. Ruft dieses RD
 * wieder RunDos, traegt es das Programm in den Block seg ein und endet mit Code 0D5h.
 * RD_DOS_NEU (Optionen, Grafikwechsel): immer dieser Weg, ohne Programm; RD2 startet nur neu. */
#include "KERN.H"

void (__far __pascal *k_gdipause)(WORD an);
void (__far __pascal *k_sndpause)(WORD an);
void (__far __pascal *k_netpause)(WORD an);      /* RDTCP traegt sich ueber rd_api[RDO_NETHAKEN] (Adresse dieser Variable) ein */
void k_pausen(WORD an) { if (k_sndpause) k_sndpause(an); if (k_netpause) k_netpause(an); }   /* 0 aus, 1 wieder an, 2 Abfrage */

#pragma pack(1)
typedef struct {
    void __far *xent; WORD xh, fh, psp, kpar, aseg, apar, swseg, swpar;
    void __far *ret; WORD ss, sp, exitc, err, flags;
    WORD env; char __far *cmd; BYTE __far *f1, __far *f2;
    BYTE mv[16]; WORD cur, rest, dir, xo[2], top, mode;
    char path[80]; char tail[128]; BYTE fcb[32]; char rdp[80]; WORD spx;
} STUB;
#pragma pack()
#define STUBP 120                                 /* Paragraphen, mit Stack: passt in die Reserve von mem_init */

static WORD d21(WORD ax, WORD bx, WORD cx, WORD dx)   /* INT 21h, DS = Kern; CARRY gilt danach */
{
    kr.ax = ax; kr.bx = bx; kr.cx = cx; kr.dx = dx; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    return kr.ax;
}
WORD k_wseg, k_rseg;                              /* /W:seg (laeuft unter einem Rest), Rest fuer den Neustart */
static void cpy(char __far *d, LPCSTR q, WORD max)
{
    while (--max && q && *q) *d++ = *q++;
    *d = 0;
}
static WORD neustart(void)                        /* alle Programme beenden; main startet danach den Rest */
{
    WORD i;
    for (i = 1; i < MAXTASK; i++) if (i != k_cur && (k_task[i].state == T_READY || k_task[i].state == T_WAIT)) TaskKill(i, 0);
    return TaskKill(k_cur, 0);
}
WORD k_stub(WORD mode)                            /* Block ganz oben, RDSWAP.BIN laden; 0 = Fehler (k_lasterr) */
{
    WORD seg, strat, h, n = 0; STUB __far *s;
    strat = d21(0x5800, 0, 0, 0);
    d21(0x5801, 2, 0, 0);                         /* zuletzt passend: ganz oben */
    seg = d21(0x4800, STUBP, 0, 0); if (CARRY) seg = 0;
    d21(0x5801, strat, 0, 0);
    if (!seg) return FAIL(RD_ERR_NOMEM);
    h = d21(0x3D00, 0, 0, (WORD)k_hpath("RDSWAP.BIN"));
    if (!CARRY) { kr.ax = 0x3F00; kr.bx = h; kr.cx = STUBP * 16 - 256; kr.dx = 0; kr.ds = seg; rd_int(0x21, &kr); n = CARRY ? 0 : kr.ax; d21(0x3E00, h, 0, 0); }
    if (n < 500) { kr.ax = 0x4900; kr.es = seg; rd_int(0x21, &kr); return FAIL(RD_ERR_NOTFOUND); }
    s = (STUB __far *)MK_FP(seg, 0);
    s->top = STUBP * 16; s->psp = d21(0x6200, 0, 0, 0), s->psp = kr.bx; s->mode = mode; s->xent = k_xmsent;
    cpy(s->rdp, k_home, 66);
    return seg;
}
/* Ende als Shell (29.10.2026, Karsten): statt "RetroDesk ist beendet" COMMAND.COM starten, EXIT holt RD2 zurueck.
 * Sucht COMSPEC, Startlaufwerk \ und \DOS, das RD2-Verzeichnis. 0xD5 = im Rest eingetragen, 0 = keins gefunden. */
static int gibt(const char *p) { d21(0x4300, 0, 0, (WORD)p); return !CARRY; }
WORD k_dosshell(void)
{
    static char p[80]; STUB __far *s; WORD i, k, lw, psp; const char __far *e;   /* static: DS, nicht Stapel */
    if (!k_wseg || (s = (STUB __far *)MK_FP(k_wseg, 0))->mode != 2) return 0;
    psp = d21(0x6200, 0, 0, 0), psp = kr.bx; p[0] = 0;
    if ((i = *(WORD __far *)MK_FP(psp, 0x2C)) != 0)
        for (e = (const char __far *)MK_FP(i, 0); *e; e += k + 1) {
            for (k = 0; e[k]; k++) ;
            if (e[0] == 'C' && e[1] == 'O' && e[2] == 'M' && e[3] == 'S' && e[4] == 'P' && e[5] == 'E' && e[6] == 'C' && e[7] == '=') { cpy((char __far *)p, e + 8, 80); break; }
        }
    for (i = 0; p[i]; i++) ;
    if (i < 11 || ((p[i - 11] | 0x20) != 'c' || (p[i - 4] != '.') || (p[i - 1] | 0x20) != 'm' || (p[i - 5] | 0x20) != 'd')) p[0] = 0;   /* nur ...COMMAND.COM, nicht RD.EXE */
    d21(0x3305, 0, 0, 0); lw = (BYTE)kr.dx; if (lw < 1 || lw > 26) lw = 3;
    for (k = 0; k < 4 && !(p[0] && gibt(p)); k++) {
        if (k == 3) { cpy((char __far *)p, k_home, 66); for (i = 0; p[i]; i++) ; if (i && p[i - 1] != '\\') p[i++] = '\\'; cpy((char __far *)p + i, "COMMAND.COM", 14); continue; }
        if (k == 0 && p[0]) continue;
        p[0] = (char)('@' + lw); cpy((char __far *)p + 1, k == 2 ? ":\\DOS\\COMMAND.COM" : ":\\COMMAND.COM", 20);
    }
    if (!p[0] || !gibt(p)) return 0;
    cpy(s->path, p, 80);
    for (i = k = 0; p[i]; i++) if (p[i] == '\\') k = i + 1;
    p[k] = 0; cpy(s->tail + 2, p, 124);          /* COMMAND X:\DIR\ setzt COMSPEC */
    s->flags = 0;
    d21(0x0900, 0, 0, (WORD)"RetroDesk 2 ist beendet. EXIT startet RetroDesk 2 wieder.\r\n$");
    return 0xD5;
}
WORD RDK RunDos(LPCSTR prog, LPCSTR cmd, WORD flags)
{
    STUB __far *s; WORD seg, n, top, r = 0, neu = 0, a[2], psp;
    profile_flush();
    psp = d21(0x6200, 0, 0, 0), psp = kr.bx;
    top = mem_arena((WORD __far *)a);
    n = (WORD)(((DWORD)(top - psp - 16) + 63) / 64);          /* KB zum Auslagern */
    if ((flags & RD_DOS_NEU) || (!(k_xmsent && rd_xms(0x0800, 0, 0) > n)   /* + 1 KB fuer die Vektorkopie */ && (k_home[0] | 0x20) < 'c')) {
        if ((flags & (RD_DOS_KEEP | RD_DOS_NEU)) == RD_DOS_KEEP) { r = RD_ERR_NOTSUP; goto fehl; }  /* zurueck geht nur mit XMS oder Festplatte */
        neu = 1;
        if (k_wseg) { seg = k_wseg; goto rein; }    /* laeuft unter einem Rest: dort eintragen, enden */
    }
    if (!(seg = k_stub(neu))) { r = k_lasterr; goto fehl; }
    s = (STUB __far *)MK_FP(seg, 0);
    s->kpar = *(WORD __far *)MK_FP(psp - 1, 3);
    s->aseg = a[0]; s->apar = a[1];
    s->swseg = psp + 16; s->swpar = top - s->swseg;
rein:
    s = (STUB __far *)MK_FP(seg, 0);
    s->flags = flags;
    cpy(s->path, prog, 80); cpy(s->tail + 2, cmd, 124);
    if (neu) { k_rseg = seg; return neustart(); }
    k_pausen(0);                                  /* Klang still, Netz gibt den Paketreiber frei */
    if (k_gdipause) k_gdipause(0);
    rd_timer_off();
    rd_swaprun(seg);
    rd_timer_on(16384);
    if (k_gdipause) k_gdipause(1);
    k_pausen(1);
    r = s->err == 1 ? RD_ERR_NOMEM : s->err == 0x102 || s->err == 0x103 ? RD_ERR_NOTFOUND : s->err ? RD_ERR_NOTSUP : 0;
    n = s->exitc & 0xFF;
    kr.ax = 0x4900; kr.es = seg; rd_int(0x21, &kr);
fehl:
    if (r) { k_lasterr = r; return 0xFFFF; }
    return n;
}
