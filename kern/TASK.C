/* TASK.C - Programme laden, Tasks, Umschalten, Nachrichten, Zeitgeber
 *
 * Task 0 ist der Kern selbst. Jeder andere Task hat
 *   - sein Programmbild (fester Block),
 *   - einen kleinen Startstack im Kern-Speicher (rd_enter wird darauf gerufen;
 *     RD2START schaltet dann auf den Stack des Programms um),
 *   - eine Warteschlange mit QLEN Nachrichten,
 *   - Platz fuer den 8087-Zustand.
 * Umschalten kooperativ in GetMessage/WaitMessage/TaskYield/PeekMessage und
 * praeemptiv aus dem Zeitgeber (task_tick), wenn der unterbrochene Code im
 * eigenen Programmbild liegt und DOS nicht aktiv ist. */
#include "KERN.H"

#define STK_PARAS 32                              /* 512 Bytes Startstack */
#define SLICE 4                                   /* Zeitscheibe in PIT-Takten (4 x 13,7 ms) */

TASK k_task[MAXTASK];
WORD k_cur;
static BYTE __far *indos;

#pragma pack(push, 1)
typedef struct { WORD sig, cblp, cp, crlc, cparhdr, minalloc, maxalloc, ss, sp, csum, ip, cs, lfarlc, ovno; } MZ;
typedef struct { char sig[4]; DWORD blk, len; WORD sum, res; } TRAILER;
typedef struct { char sig[4]; WORD api, flags, stack, kb; char name[16]; WORD ver, nexp, nres, oexp, ores, res[5]; } RD2BLK;
#pragma pack(pop)
static MZ mz; static TRAILER tr; static RD2BLK rb;
static char path[80];

static DWORD ms_now(void) { DWORD t = rd_ticks; return t * 13 + ((t * 47) >> 6); }

/* ---- Umschalten ----------------------------------------------------------- */
static void reap(void)
{
    WORD i;
    for (i = 1; i < MAXTASK; i++)
        if (k_task[i].state == T_ZOMBIE && i != k_cur) {
            if (k_task[i].stk) mem_put(k_task[i].stk);
            k_task[i].stk = 0;
            k_task[i].state = T_FREE;
        }
}
static int has_work(TASK *t)
{
    WORD j; DWORD now;
    if (t->qn || t->quit) return 1;
    now = ms_now();
    for (j = 0; j < NTIMER; j++) if (t->tm[j].ms && (long)(now - t->tm[j].next) >= 0) return 1;
    return 0;
}
static void switch_to(WORD n)
{
    WORD o = k_cur;
    if (n == o) return;
    if (k_fpu) rd_fsave(k_task[o].fpu);
    k_cur = n;
    k_task[n].slice = SLICE;
    rd_swtch(&k_task[o].ss, k_task[n].ss, k_task[n].sp);
    /* hier geht es weiter, wenn Task o wieder an der Reihe ist */
    if (k_fpu) rd_frstor(k_task[k_cur].fpu);
    reap();
}
static int alive(void)
{
    WORD i;
    for (i = 1; i < MAXTASK; i++) if (k_task[i].state == T_READY || k_task[i].state == T_WAIT) return 1;
    return 0;
}
/* Naechsten lauffaehigen Task suchen (reihum, der aktuelle zuletzt).
 * wait=1: aktueller Task wartet auf Arbeit (Nachricht, Zeitgeber, Ende). */
static void schedule(int wait)
{
    WORD i, n;
    if (wait && k_cur && k_task[k_cur].state == T_READY) k_task[k_cur].state = T_WAIT;
    for (;;) {
        for (i = 1; i <= MAXTASK; i++) {
            n = (k_cur + i) % MAXTASK;
            if (n == 0) continue;
            if (k_task[n].state == T_READY || (k_task[n].state == T_WAIT && has_work(&k_task[n]))) {
                k_task[n].state = T_READY;
                switch_to(n);
                return;
            }
        }
        if (!wait) return;                        /* niemand sonst bereit: weiterlaufen */
        if (!alive()) { if (k_cur) switch_to(0); return; }   /* alle fertig: zurueck zum Kern */
        rd_idle();                                /* bis zum naechsten Interrupt schlafen */
    }
}

void __cdecl task_tick(WORD cs)
{
    TASK *t = &k_task[k_cur];
    WORD i, n;
    if (!k_cur || t->state != T_READY) return;
    if (t->slice) t->slice--;
    if (t->slice) return;
    if (cs < t->base || cs >= t->base + t->paras) return;   /* nicht im eigenen Code */
    if (indos[0] || indos[-1]) return;                      /* DOS aktiv / kritischer Fehler */
    for (i = 1; i < MAXTASK; i++) {
        n = (k_cur + i) % MAXTASK;
        if (n && (k_task[n].state == T_READY || (k_task[n].state == T_WAIT && has_work(&k_task[n])))) {
            k_task[n].state = T_READY;
            switch_to(n);                         /* Interruptrahmen bleibt auf dem Stack */
            return;
        }
    }
    t->slice = SLICE;
}

/* ---- Laden und Start ----------------------------------------------------- */
static int rdat(int f, DWORD pos, void *buf, WORD n)
{
    kr.ax = 0x4200; kr.bx = f; kr.cx = (WORD)(pos >> 16); kr.dx = (WORD)pos; rd_int(0x21, &kr);
    if (CARRY) return 0;
    kr.ax = 0x3F00; kr.bx = f; kr.cx = n; kr.dx = (WORD)buf; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    return !CARRY && kr.ax == n;
}
static DWORD fsize(int f)
{
    kr.ax = 0x4202; kr.bx = f; kr.cx = 0; kr.dx = 0; rd_int(0x21, &kr);
    return CARRY ? 0 : ((DWORD)kr.dx << 16) | kr.ax;
}
static void mkpath(LPCSTR app)
{
    WORD n = 0; LPCSTR s;
    for (s = app; *s; s++) if (*s == ':' || *s == '\\') break;
    if (!*s) { const char *h = k_home; while (*h && n < 60) path[n++] = *h++; }
    while (*app && n < 79) path[n++] = *app++;
    path[n] = 0;
}

static void task_start(void)                      /* laeuft auf dem Startstack des neuen Tasks */
{
    TASK *t = &k_task[k_cur];
    short res;
    t->lastget = ms_now();
    res = rd_enter(t->base + t->ecs, t->eip, (void __far *)rd_api, k_cur, t->show, (char __far *)t->cmd);
    /* Programm ist zurueckgekehrt */
    t = &k_task[k_cur];
    t->quitcode = res;
    k_hex("RD: Task beendet:", ((DWORD)k_cur << 16) | (WORD)res, 0);
    mem_owner_free(k_cur);
    t->base = t->paras = t->q = 0;
    t->state = T_ZOMBIE;                          /* Startstack gibt reap() im naechsten Task frei */
    schedule(1);
}

WORD task_load(LPCSTR app, LPCSTR cmd, WORD show)
{
    int f; WORD i, n, sum = 0, seg, paras; DWORD img;
    static struct { WORD seg, reloc; } pb;
    TASK *t;
    reap();
    for (n = 1; n < MAXTASK && k_task[n].state != T_FREE; n++) ;
    if (n == MAXTASK) return FAIL(RD_ERR_FULL);
    mkpath(app);
    kr.ax = 0x3D00; kr.dx = (WORD)path; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    if (CARRY) return FAIL(RD_ERR_NOTFOUND);
    f = kr.ax;
    if (!rdat(f, 0, &mz, sizeof mz) || mz.sig != 0x5A4D ||
        !rdat(f, fsize(f) - 16, &tr, sizeof tr) || tr.sig[0] != 'R' || tr.sig[1] != 'D' || tr.sig[2] != '2' || tr.sig[3] != '!' ||
        !rdat(f, tr.blk, &rb, sizeof rb) || rb.sig[3] != 'P') {
        kr.ax = 0x3E00; kr.bx = f; rd_int(0x21, &kr);
        return FAIL(RD_ERR_FORMAT);
    }
    kr.ax = 0x3E00; kr.bx = f; rd_int(0x21, &kr);
    for (i = 0; i < sizeof rb; i++) sum += ((BYTE *)&rb)[i];
    if (sum != tr.sum) return FAIL(RD_ERR_FORMAT);
    if (rb.api > RD2_API_VERSION) return FAIL(RD_ERR_VERSION);
    img = (DWORD)mz.cp * 512 - (mz.cblp ? 512 - mz.cblp : 0) - (DWORD)mz.cparhdr * 16;
    paras = (WORD)((img + 15) >> 4) + mz.minalloc;
    t = &k_task[n];
    if (!(seg = mem_get(paras, 0, n))) return FAIL(RD_ERR_NOMEM);
    pb.seg = seg; pb.reloc = seg;
    kr.ax = 0x4B03; kr.dx = (WORD)path; kr.ds = FP_SEG((void __far *)&kr); kr.bx = (WORD)&pb; kr.es = kr.ds;
    rd_int(0x21, &kr);
    if (CARRY) { mem_owner_free(n); return FAIL(RD_ERR_DISK); }
    t->stk = mem_get(STK_PARAS, 0, 0);
    t->q = mem_get((QLEN * sizeof(MSG) + 15) >> 4, 0, n);
    if (!t->stk || !t->q) { if (t->stk) mem_put(t->stk); mem_owner_free(n); return FAIL(RD_ERR_NOMEM); }
    t->base = seg; t->paras = paras; t->ecs = mz.cs; t->eip = mz.ip; t->show = show;
    t->qh = t->qn = t->quit = 0; t->quitcode = 0;
    for (i = 0; i < NTIMER; i++) t->tm[i].ms = 0;
    for (i = 0; i < 15 && rb.name[i]; i++) t->name[i] = rb.name[i];
    t->name[i] = 0;
    for (i = 0; i < 79 && cmd && cmd[i]; i++) t->cmd[i] = cmd[i];
    t->cmd[i] = 0;
    if (k_fpu) rd_fsave(t->fpu), rd_frstor(t->fpu);   /* Startzustand = aktueller (initialisierter) 8087 */
    t->ss = t->stk;
    t->sp = rd_newstack(t->stk, STK_PARAS * 16, task_start);
    t->state = T_READY;
    return n;
}

/* ---- Nachrichten ------------------------------------------------------------ */
static int enqueue(WORD n, HWND w, WORD msg, WORD p1, DWORD p2)
{
    TASK *t = &k_task[n]; MSG __far *m;
    if (n == 0 || n >= MAXTASK || (t->state != T_READY && t->state != T_WAIT)) return FAIL(RD_ERR_HANDLE);
    if (t->qn >= QLEN) return FAIL(RD_ERR_FULL);
    m = (MSG __far *)MK_FP(t->q, 0) + (t->qh + t->qn) % QLEN;
    m->w = w; m->msg = msg; m->p1 = p1; m->p2 = p2; m->time = ms_now(); m->pt.x = m->pt.y = 0;
    t->qn++;
    return 1;
}
/* Naechste Nachricht holen (Schlange, dann Zeitgeber, dann Ende). 0 = nichts da */
static int fetch(MSG FAR *m, int remove)
{
    TASK *t = &k_task[k_cur]; MSG __far *q; WORD j; DWORD now;
    t->lastget = ms_now();
    if (t->qn) {
        q = (MSG __far *)MK_FP(t->q, 0) + t->qh;
        *m = *q;
        if (remove) { t->qh = (t->qh + 1) % QLEN; t->qn--; }
        return 1;
    }
    now = ms_now();
    for (j = 0; j < NTIMER; j++)
        if (t->tm[j].ms && (long)(now - t->tm[j].next) >= 0) {
            m->w = t->tm[j].w; m->msg = M_TIMER; m->p1 = t->tm[j].id; m->p2 = 0; m->time = now; m->pt.x = m->pt.y = 0;
            if (remove) t->tm[j].next = now + t->tm[j].ms;
            return 1;
        }
    if (t->quit) {
        m->w = 0; m->msg = M_QUIT; m->p1 = t->quitcode; m->p2 = 0; m->time = now; m->pt.x = m->pt.y = 0;
        return 1;
    }
    return 0;
}

BOOL RDK GetMessage(MSG FAR *m)
{
    while (!fetch(m, 1)) schedule(1);
    return m->msg != M_QUIT;
}
BOOL RDK PeekMessage(MSG FAR *m, WORD remove)
{
    if (fetch(m, remove)) return TRUE;
    schedule(0);                                  /* kurz anderen Tasks Zeit geben */
    return fetch(m, remove) ? TRUE : FALSE;
}
void RDK WaitMessage(void)       { while (!has_work(&k_task[k_cur])) schedule(1); }
void RDK TaskYield(void)         { schedule(0); }
BOOL RDK PostTask(HTASK t, WORD msg, WORD p1, DWORD p2) { return enqueue(t, 0, msg, p1, p2) ? TRUE : FALSE; }
void RDK PostQuit(WORD code)     { k_task[k_cur].quit = 1; k_task[k_cur].quitcode = code; }
DWORD RDK DispatchMessage(const MSG FAR *m) { (void)m; return 0; }   /* Fenster folgen in Phase 4 */

WORD RDK TimerSet(HWND w, WORD id, WORD ms)
{
    TASK *t = &k_task[k_cur]; WORD j, fr = NTIMER;
    if (!ms) ms = 1;
    for (j = 0; j < NTIMER; j++) {
        if (t->tm[j].ms && t->tm[j].id == id && t->tm[j].w == w) break;
        if (!t->tm[j].ms && fr == NTIMER) fr = j;
    }
    if (j == NTIMER) j = fr;
    if (j == NTIMER) return FAIL(RD_ERR_FULL);
    t->tm[j].id = id; t->tm[j].w = w; t->tm[j].ms = ms; t->tm[j].next = ms_now() + ms;
    return id ? id : 1;
}
void RDK TimerKill(HWND w, WORD id)
{
    TASK *t = &k_task[k_cur]; WORD j;
    for (j = 0; j < NTIMER; j++) if (t->tm[j].id == id && t->tm[j].w == w) t->tm[j].ms = 0;
}
DWORD RDK TickCount(void) { return ms_now(); }

/* ---- Tasks ------------------------------------------------------------------- */
HTASK RDK TaskSelf(void) { return k_cur; }
HTASK RDK TaskExec(LPCSTR app, LPCSTR cmd, WORD show) { return task_load(app, cmd, show); }
WORD RDK TaskKill(HTASK n, WORD code)
{
    TASK *t;
    if (!n || n >= MAXTASK || (k_task[n].state != T_READY && k_task[n].state != T_WAIT)) return FAIL(RD_ERR_HANDLE);
    t = &k_task[n];
    t->quitcode = code;
    mem_owner_free(n);
    t->base = t->paras = t->q = 0;
    t->state = T_ZOMBIE;
    k_hex("RD: Task beendet (Kill):", ((DWORD)n << 16) | code, 0);
    if (n == k_cur) schedule(1);                  /* kehrt nie zurueck */
    return 0;
}
static HTASK info(WORD n, TASKINFO FAR *ti)
{
    WORD i;
    for (; n < MAXTASK; n++)
        if (k_task[n].state == T_READY || k_task[n].state == T_WAIT) {
            ti->t = n; ti->i = n; ti->kb = (k_task[n].paras + 63) >> 6; ti->flags = 0;
            for (i = 0; i < 16; i++) ti->name[i] = k_task[n].name[i];
            return n;
        }
    return 0;
}
HTASK RDK TaskFirst(TASKINFO FAR *ti)          { return info(1, ti); }
HTASK RDK TaskNext(HTASK t, TASKINFO FAR *ti)  { return info(t + 1, ti); }

void task_run_all(void) { schedule(1); }

void task_init(void)
{
    kr.ax = 0x3400; rd_int(0x21, &kr);
    indos = (BYTE __far *)MK_FP(kr.es, kr.bx);
    k_cur = 0;
    k_task[0].state = T_KERNEL;
}

void task_api(void)
{
    KAPI(GetMessage, GetMessage); KAPI(PeekMessage, PeekMessage); KAPI(WaitMessage, WaitMessage);
    KAPI(TaskYield, TaskYield); KAPI(PostTask, PostTask); KAPI(PostQuit, PostQuit);
    KAPI(DispatchMessage, DispatchMessage); KAPI(TimerSet, TimerSet); KAPI(TimerKill, TimerKill);
    KAPI(TickCount, TickCount); KAPI(TaskSelf, TaskSelf); KAPI(TaskExec, TaskExec); KAPI(TaskKill, TaskKill);
    KAPI(TaskFirst, TaskFirst); KAPI(TaskNext, TaskNext);
}
