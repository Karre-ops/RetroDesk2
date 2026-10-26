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
/* Prototypen aller API-Funktionen, wie der Kern sie definiert (mit __loadds) */
#define RDF(n, ret, name, args) ret RDK name args;
#include "RD2API.H"
#undef RDF

/* ---- KASM.ASM ----------------------------------------------------------- */
typedef struct { WORD ax, bx, cx, dx, si, di, ds, es, flags; } KREGS;
extern KREGS kr;                                  /* in RD.C */
extern FARPROC rd_api[RDO_MAX];
extern void  __cdecl rd_nsinit(void);           /* NOTSUP.ASM: Tabelle mit "nicht vorhanden" fuellen */
extern DWORD __cdecl rd_enter(WORD cs, WORD ip, void __far *api, WORD hinst, WORD show, char __far *cmd); /* DX:AX */
extern void  __cdecl rd_int(WORD nr, KREGS *r);   /* r->flags & 1 = Carry */
extern WORD  __cdecl rd_xms(WORD ax, WORD dx, WORD si);   /* XMS-Treiber, DX danach in k_xdx */
extern void __far *k_xmsent; extern WORD k_xdx;
void  mod_xinit(void);                            /* XMS als Ablage fuer verworfene Module */
void  mod_xdone(void);
extern void  __cdecl rd_swtch(WORD *save_sssp, WORD ss, WORD sp);
extern WORD  __cdecl rd_newstack(WORD ss, WORD sp, void (*start)(void)); /* liefert SP des vorbereiteten Stacks */
extern void  __cdecl rd_pmove(WORD dst, WORD src, WORD paras);
extern void  __cdecl rd_pfill(WORD seg, WORD paras, WORD wort);
extern void  __cdecl rd_timer_on(WORD divisor);   /* auch INT 9, 23h, 24h */
extern void  __cdecl rd_timer_off(void);
extern WORD  __cdecl rd_absread(WORD drive, void *buf);   /* INT 25h, Sektor 0 */
extern volatile WORD rd_killreq;                 /* INT 9: Strg+Alt+Rueckschritt */
extern volatile WORD k_crit, k_critdrv;          /* INT 24h: Fehlercode+1, AX */
extern void __far *rd_gateptr;                   /* Einsprung der Modul-Sprungstuecke */
extern DWORD __cdecl ms_now(void);               /* Millisekunden seit Start, gesperrt gelesen (M9) */
extern void  __cdecl rd_idle(void);               /* STI, HLT */
extern WORD  __cdecl rd_fpudet(void);
extern WORD  __cdecl rd_cpudet(void);              /* CPU_* */
extern void  __cdecl rd_fsave(WORD seg, WORD off);   /* 94 Bytes je Task, nur mit 8087 */
extern void  __cdecl rd_frstor(WORD seg, WORD off);
extern WORD  k_fpuseg;
extern volatile DWORD rd_ticks;                   /* 72,8 Hz */
extern volatile WORD  rd_preempt;                 /* 1 = Zeitscheiben an */
extern void (__far __pascal * volatile k_sndtick)(void);   /* KASM: RDSND-Takt im INT 8 */
extern void (__far __pascal *k_sndpause)(WORD an);
extern void (__far __pascal *k_netpause)(WORD an);
void k_pausen(WORD an);
#define RDO_NETHAKEN 0x3BF                        /* kein Aufruf: Zeiger auf k_netpause fuer RDTCP.MOD */  /* RDSND: 0 still und Treiber aus, 1 wieder an (RunDos), 2 Abfrage (input_poll) */
#define CARRY (kr.flags & 1)

/* ---- RD.C ------------------------------------------------------------------- */
void k_txt(LPCSTR s, int scr);
void k_out(LPCSTR s, int scr);
void k_hex(LPCSTR s, DWORD v, int scr);
extern WORD k_lasterr;
extern char k_home[66];
extern BYTE k_fpu;
#define FAIL(e) (k_lasterr = (e), 0)

/* ---- MEM.C ------------------------------------------------------------------- */
#define B_FREE   0x8000                           /* intern: freier Bereich */
#define B_INTERN 0x1000                           /* intern: Kern (Stack, Schlange), kein Handle */
#define B_HIGH   0x0400                           /* nur Wunsch an mem_get: hoher Speicher (UMB) zuerst */
extern WORD mem_resv;                             /* Reserve (Paragraphen) zum Nachladen verwerfbarer Module */
#define B_MOD    0x0800                           /* Modulbild: fest, aber verwerfbar wenn nicht in Benutzung */
void  mem_init(WORD reserve_paras);
void  mem_done(void);
WORD  mem_get(WORD paras, WORD flags, WORD owner); /* liefert Segment, 0 = kein Platz */
void  mem_put(WORD seg);
void  mem_owner_free(WORD owner);
DWORD mem_free_bytes(int groesster);
void  mem_api(void);
void  mem_share(HMEM h);                          /* gehoert danach keinem Task */
void  mem_mod(WORD seg, WORD mod);
void  mem_chown(WORD seg, WORD owner);                /* Block gehoert Modul mod (verwerfbar, wenn frei) */
extern DWORD (__far __pascal *mem_sw)(WORD art, WORD seg, WORD n);   /* MEM.C: Auslagermodul RDAUSL */
extern struct mem_swi_t { void *b; WORD *n; void *h; char *home; DWORD *stamp; } mem_swi;
extern int (*mem_mod_discard)(WORD mod);          /* MOD.C: 1 = Modul darf/wurde verworfen */
extern DWORD (*mem_mod_stamp)(WORD mod);

/* ---- FILE.C ------------------------------------------------------------------ */
void  file_api(void);
void  file_owner_close(WORD task);
void  clip_done(void);
int   k_askdisk(WORD drive, LPCSTR label);         /* 0 = Abbruch */
int   k_dos(void);
HMOD  k_sysmod(const char *name, WORD base, WORD n, const WORD *extra);
void  k_sysend(HMOD m);
void __far *k_modraw(HMOD m, WORD nr);
void k_modrest(void);          /* Exportadresse direkt (Systemmodule, ohne Sprungstueck) */
extern WORD (__far __pascal *k_mausget)(WORD __far *e);
extern void (__far __pascal *k_gdipause)(WORD an);  /* RDGDI: 0 Textmodus/Maus aus, 1 wieder an (RunDos) */
WORD mem_arena(WORD __far *a);
WORD k_stub(WORD mode);
extern WORD k_wseg, k_rseg;                       /* RunDos-Neustart (RUNDOS.C) */
extern void __cdecl rd_swaprun(WORD seg);
char *k_hpath(const char *name);                  /* RD2-Verzeichnis + Name (Puffer wird wiederverwendet) */
WORD RDK RunDos(LPCSTR prog, LPCSTR cmd, WORD flags);   /* RDGDI: Mausereignis abholen */
extern WORD k_focus;                              /* Task, der Tastatur und Maus bekommt */
void  input_poll(void);
extern WORD k_umb_paras;                          /* hoher Speicher, den RD bekommen hat */
extern WORD k_gfx;                                /* /CGA /HGC /VGA: GFX_*, 0 = automatisch */                                /* INT 21h ueber kr mit Diskettenabfrage, 1 = ok */
WORD  k_media(WORD drive, DWORD *serial, char *label);   /* drive 0 = A: */

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
    BYTE  rsp;                                    /* Ruecksprungstapel fuer Modulaufrufe */
    struct { WORD ip, cs, mod; } rs[12];
    char  name[16];
    WORD  xs;                                     /* Zusatzblock des Tasks: Befehlszeile, Pfad, RD2-Block */
} TASK;
typedef struct { char cmd[80], path[80]; DWORD blk; } TASKX;
#define TX(t) ((TASKX __far *)MK_FP((t)->xs, 0))
extern TASK k_task[MAXTASK];
extern WORD k_cur;
void  task_init(void);
/* gemeinsamer Lader fuer Programme und Module (MZ + RD2-Anhang), path im Kern-DS */
#pragma pack(push, 1)
typedef struct { WORD sig, cblp, cp, crlc, cparhdr, minalloc, maxalloc, ss, sp, csum, ip, cs, lfarlc, ovno; } KMZ;
typedef struct { char sig[4]; WORD api, flags, stack, kb; char name[16]; WORD ver, nexp, nres, oexp, ores, res[5]; } KRD2;
#pragma pack(pop)
extern KMZ k_mz; extern KRD2 k_rb;
WORD  k_imgload(const char *path, const char *typen, WORD *seg, WORD *paras);   /* 0 = ok, sonst RD_ERR_* */
void  task_api(void);
extern DWORD (__far __pascal *k_wnd)(WORD art, HWND w, WORD msg, WORD p1, DWORD p2);   /* von RDUSER */
WORD  task_load(LPCSTR path, LPCSTR cmd, WORD show);   /* HTASK oder 0 */
void  task_run_all(void);                         /* Kern wartet, bis alle Programme beendet sind */
void  __cdecl task_tick(WORD cs);                 /* aus dem Zeitgeber-Interrupt */
/* ---- MOD.C ------------------------------------------------------------------- */
void  mod_api(void);
DWORD __cdecl mod_enter(WORD idx, WORD ip, WORD cs);
DWORD __cdecl mod_leave(void);
void  mod_unwind(TASK *t);                        /* Task endet: offene Modulaufrufe entsperren */

#endif
