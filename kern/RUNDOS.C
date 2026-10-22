/* RUNDOS.C - RunDos (SPEC 14): DOS-Programm mit fast vollem Speicher starten.
 * Der Kern und der belegte Teil der Arena gehen in den XMS-Speicher oder, ohne XMS, in eine
 * Datei auf der Festplatte (RD2SWAP.$$$ im RD2-Verzeichnis). Ein kleiner Rest (SWAP.ASM)
 * ganz oben im Speicher startet das Programm und holt danach alles an dieselbe Adresse
 * zurueck. Der Rest liegt als RDSWAP.BIN neben RD.EXE und kostet so keinen Kernspeicher.
 * Ohne XMS und ohne Festplatte: RD_ERR_NOTSUP (dafuer kommt der Neustart mit RD2.STA). */
#include "KERN.H"

extern void __cdecl rd_swaprun(WORD seg);
void (__far __pascal *k_gdipause)(WORD an);

#pragma pack(1)
typedef struct {
    void __far *xent; WORD xh, fh, psp, kpar, aseg, apar, swseg, swpar;
    void __far *ret; WORD ss, sp, exitc, err, flags;
    WORD env; char __far *cmd; BYTE __far *f1, __far *f2;
    BYTE mv[16]; WORD cur, rest, dir, xo[2], top;
    char path[80]; char tail[128]; BYTE fcb[32];
} STUB;
#pragma pack()
#define STUBP 80                                  /* Paragraphen, mit Stack: passt in die Reserve von mem_init */

static WORD d21(WORD ax, WORD bx, WORD cx, WORD dx)   /* INT 21h, DS = Kern; CARRY gilt danach */
{
    kr.ax = ax; kr.bx = bx; kr.cx = cx; kr.dx = dx; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    return kr.ax;
}
WORD RDK RunDos(LPCSTR prog, LPCSTR cmd, WORD flags)
{
    STUB __far *s; WORD seg, strat, i, n, top, r = 0, h; char *p;
    strat = d21(0x5800, 0, 0, 0);
    d21(0x5801, 2, 0, 0);                         /* zuletzt passend: ganz oben */
    seg = d21(0x4800, STUBP, 0, 0); if (CARRY) seg = 0;
    d21(0x5801, strat, 0, 0);
    if (!seg) return FAIL(RD_ERR_NOMEM);
    h = d21(0x3D00, 0, 0, (WORD)k_hpath("RDSWAP.BIN"));
    n = 0;
    if (!CARRY) { kr.ax = 0x3F00; kr.bx = h; kr.cx = STUBP * 16 - 384; kr.dx = 0; kr.ds = seg; rd_int(0x21, &kr); n = CARRY ? 0 : kr.ax; d21(0x3E00, h, 0, 0); }
    if (n < 400) { r = RD_ERR_NOTFOUND; goto frei; }
    s = (STUB __far *)MK_FP(seg, 0);
    s->top = STUBP * 16; s->flags = flags;
    s->psp = d21(0x6200, 0, 0, 0), s->psp = kr.bx;
    s->kpar = *(WORD __far *)MK_FP(s->psp - 1, 3);
    top = mem_arena(&s->aseg);
    s->swseg = s->psp + 16; s->swpar = top - s->swseg;
    for (i = 0; i < 79 && prog[i]; i++) s->path[i] = prog[i];
    for (n = 0; cmd && cmd[n] && n < 124; n++) s->tail[n + 2] = cmd[n];
    s->tail[0] = (char)(n + 1); s->tail[1] = ' '; s->tail[n + 2] = 13;
    s->cmd = (char __far *)s->tail; s->f1 = s->fcb; s->f2 = s->fcb + 16;
    if (k_xmsent && rd_xms(0x0900, (WORD)(((DWORD)s->swpar + 63) / 64), 0) == 1) { s->xh = k_xdx; s->xent = k_xmsent; }
    else {                                        /* Datei, nur auf Festplatte */
        p = k_hpath("RD2SWAP.$$$");
        if ((*p | 0x20) < 'c') { r = RD_ERR_NOTSUP; goto frei; }
        s->fh = d21(0x3C00, 0, 0, (WORD)p);
        if (CARRY) { r = RD_ERR_NOMEM; goto frei; }
    }
    if (k_gdipause) k_gdipause(0);
    rd_timer_off();
    rd_swaprun(seg);
    rd_timer_on(16384);
    if (k_gdipause) k_gdipause(1);
    if (s->xent) rd_xms(0x0A00, s->xh, 0);
    else { d21(0x3E00, s->fh, 0, 0); d21(0x4100, 0, 0, (WORD)k_hpath("RD2SWAP.$$$")); }
    r = s->err == 1 ? RD_ERR_NOMEM : s->err == 0x102 || s->err == 0x103 ? RD_ERR_NOTFOUND : s->err ? RD_ERR_NOTSUP : 0;
    n = s->exitc & 0xFF;
frei:
    kr.ax = 0x4900; kr.es = seg; rd_int(0x21, &kr);
    if (r) { k_lasterr = r; return 0xFFFF; }
    return n;
}
