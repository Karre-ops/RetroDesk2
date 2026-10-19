/* KERN.H - interne Schnittstellen des RD2-Kerns (Modell Small, -zu)
 *
 * Wichtig: API-Funktionen (RDK) laufen mit dem Stack des aufrufenden Programms
 * (SS != DS). Darin keine Funktionen der C-Bibliothek benutzen und keine
 * Adressen lokaler Variablen an Funktionen mit nahen Zeigern geben.
 * DOS/BIOS nur ueber rd_int() mit der statischen Struktur kr. */
#ifndef KERN_H
#define KERN_H
#define RD2_KERNEL
#include "RD2.H"
#include "RD2ORD.H"
#include <i86.h>
#include <conio.h>
#pragma intrinsic(inp, outp)

#define RDK __far __pascal __loadds
#define KAPI(n, f) rd_api[RDO_##n] = (FARPROC)(f)

/* ---- KASM.ASM ----------------------------------------------------------- */
typedef struct { WORD ax, bx, cx, dx, si, di, ds, es, flags; } KREGS;
extern KREGS kr;                                  /* in RD.C */
extern FARPROC rd_api[RDO_MAX];
extern void  __cdecl rd_nsinit(void);           /* NOTSUP.ASM: Tabelle mit "nicht vorhanden" fuellen */
extern short __cdecl rd_enter(WORD cs, WORD ip, void __far *api, WORD hinst, WORD show, char __far *cmd);
extern void  __cdecl rd_int(WORD nr, KREGS *r);   /* r->flags & 1 = Carry */
extern void  __cdecl rd_swtch(WORD *save_sssp, WORD ss, WORD sp);
extern WORD  __cdecl rd_newstack(WORD ss, WORD sp, void (*start)(void)); /* liefert SP des vorbereiteten Stacks */
extern void  __cdecl rd_pmove(WORD dst, WORD src, WORD paras);
extern void  __cdecl rd_pfill(WORD seg, WORD paras, WORD wort);
extern void  __cdecl rd_timer_on(WORD divisor);
extern void  __cdecl rd_timer_off(void);
extern void  __cdecl rd_idle(void);               /* STI, HLT */
extern WORD  __cdecl rd_fpudet(void);
extern void  __cdecl rd_fsave(BYTE *p);
extern void  __cdecl rd_frstor(BYTE *p);
extern volatile DWORD rd_ticks;                   /* 72,8 Hz */
extern volatile WORD  rd_preempt;                 /* 1 = Zeitscheiben an */
#define CARRY (kr.flags & 1)

/* ---- RD.C ------------------------------------------------------------------- */
void k_out(LPCSTR s, int scr);
void k_hex(LPCSTR s, DWORD v, int scr);
extern WORD k_lasterr;
extern char k_home[66];
extern BYTE k_fpu;
#define FAIL(e) (k_lasterr = (e), 0)

/* ---- MEM.C ------------------------------------------------------------------- */
#define B_FREE   0x8000                           /* intern: freier Bereich */
#define B_INTERN 0x1000                           /* intern: Kern (Stack, Schlange), kein Handle */
void  mem_init(WORD reserve_paras);
void  mem_done(void);
WORD  mem_get(WORD paras, WORD flags, WORD owner); /* liefert Segment, 0 = kein Platz */
void  mem_put(WORD seg);
void  mem_owner_free(WORD owner);
DWORD mem_free_bytes(int groesster);
void  mem_api(void);

/* ---- TASK.C ------------------------------------------------------------------ */
#define MAXTASK 12
#define QLEN    16
#define NTIMER  4
enum { T_FREE, T_READY, T_WAIT, T_ZOMBIE, T_KERNEL };
typedef struct {
    WORD  state;
    WORD  ss, sp;                                 /* gesichert beim Umschalten */
    WORD  base, paras;                            /* Programmbild */
    WORD  stk;                                    /* Startstack (Segment) */
    WORD  q;                                      /* Warteschlange (Segment), QLEN x MSG */
    BYTE  qh, qn, quit, slice;
    WORD  quitcode;
    WORD  ecs, eip, show;
    struct { WORD id, ms; HWND w; DWORD next; } tm[NTIMER];
    DWORD lastget;
    char  name[16];
    char  cmd[80];
    BYTE  fpu[94];
} TASK;
extern TASK k_task[MAXTASK];
extern WORD k_cur;
void  task_init(void);
void  task_api(void);
WORD  task_load(LPCSTR path, LPCSTR cmd, WORD show);   /* HTASK oder 0 */
void  task_run_all(void);                         /* Kern wartet, bis alle Programme beendet sind */
void  __cdecl task_tick(WORD cs);                 /* aus dem Zeitgeber-Interrupt */
#endif
