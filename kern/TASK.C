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

#define STK_PARAS 64                              /* 1 KB Startstack (BIOS-Aufrufe in Abfragen) */
#define SLICE 4                                   /* Zeitscheibe in PIT-Takten (4 x 13,7 ms) */

TASK k_task[MAXTASK];
WORD k_fpuseg;                                    /* 8087-Zustaende aller Tasks, nur mit 8087 belegt */
#define FPU(n) k_fpuseg, (WORD)((n) * 94)
WORD k_cur;
static BYTE __far *indos;

KMZ k_mz; KRD2 k_rb;
static struct { char sig[4]; DWORD blk, len; WORD sum, res; } tr;
static char path[80];

WORD RDK TaskKill(HTASK n, WORD code);

/* ---- Umschalten ----------------------------------------------------------- */
/* Beendete Tasks aufraeumen. Laeuft nie im Kontext des beendeten Tasks selbst
   (dessen Stack liegt evtl. in seinem Programmbild), sondern im naechsten. */
static void reap(void)
{
    WORD i;
    for (i = 1; i < MAXTASK; i++)
        if (k_task[i].state == T_ZOMBIE && i != k_cur) {
            if (k_wnd) k_wnd(3, i, 0, 0, 0);       /* RDUSER: Fenster des Tasks weg */
            mod_unwind(&k_task[i]);
            file_owner_close(i);
            mem_owner_free(i);
            if (k_task[i].stk) mem_put(k_task[i].stk);
            k_task[i].stk = k_task[i].base = k_task[i].paras = k_task[i].q = k_task[i].xs = 0;
            k_task[i].state = T_FREE;
        }
}
#if NTIMER != 4
#error TAKTIV anpassen
#endif
#define TAKTIV(t) ((t)->tm[0].ms | (t)->tm[1].ms | (t)->tm[2].ms | (t)->tm[3].ms)   /* ein Zeitgeber laeuft */
static int has_work(TASK *t)
{
    WORD j; DWORD now;
    if (t->qn || t->quit) return 1;
    if (!TAKTIV(t)) return 0;
    now = ms_now();
    for (j = 0; j < NTIMER; j++) if (t->tm[j].ms && (long)(now - t->tm[j].next) >= 0) return 1;
    return 0;
}
static WORD k_nsw;                                /* Zahl der Umschaltungen (PeekMessage) */
static void switch_to(WORD n)
{
    WORD o = k_cur;
    if (n == o) return;
    if (k_fpu) rd_fsave(FPU(o));
    k_cur = n;
    k_task[n].slice = SLICE;
    k_nsw++;
    rd_swtch(&k_task[o].ss, k_task[n].ss, k_task[n].sp);
    /* hier geht es weiter, wenn Task o wieder an der Reihe ist */
    if (k_fpu) rd_frstor(FPU(k_cur));
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
/* ---- Eingabe: Tastatur (BIOS INT 16h) und Maus (ueber RDGDI) ----------------------
 * Abgefragt bei GetMessage/PeekMessage, im Leerlauf und im Zeitgeber, wenn ein Task
 * gerade eigenen Code rechnet. Alles geht an k_focus (Phase 4: Fenster mit Fokus). */
WORD (__far __pascal *k_mausget)(WORD __far *e);
WORD k_focus;
static int enqueue(WORD n, HWND w, WORD msg, WORD p1, DWORD p2);
static BYTE inpoll, kbext = 0xFF;
static DWORD lastdown; static short ldx, ldy; static WORD ldb;
static MSG im;                                    /* Eingabe auf dem Weg zum Fenster */
DWORD (__far __pascal *k_wnd)(WORD art, HWND w, WORD msg, WORD p1, DWORD p2);
static void post_in(WORD msg, WORD p1, DWORD p2, short x, short y)
{
    TASK *t; MSG __far *m; WORD n = k_focus;
    im.w = 0; im.msg = msg; im.p1 = p1; im.p2 = p2; im.pt.x = x; im.pt.y = y;
    if (k_wnd) {                                  /* RDUSER: Fenster und Task, 0xFFFF = noch keine Fenster */
        if (!(n = (WORD)k_wnd(2, 0, msg, p1, (DWORD)(MSG __far *)&im))) return;
        if (n == 0xFFFF) n = k_focus;
    }
    t = &k_task[n];
    if (t->qn && msg == M_MOUSEMOVE) {            /* letzte Bewegung ersetzen statt anhaengen */
        m = (MSG __far *)MK_FP(t->q, 0) + (t->qh + t->qn - 1) % QLEN;
        if (m->msg == M_MOUSEMOVE) { m->w = im.w; m->p1 = p1; m->p2 = im.p2; m->pt.x = x; m->pt.y = y; m->time = ms_now(); return; }
    }
    if (enqueue(n, im.w, msg, im.p1, im.p2) > 0) {
        m = (MSG __far *)MK_FP(t->q, 0) + (t->qh + t->qn - 1) % QLEN;
        m->pt.x = x; m->pt.y = y;
    }
}
void input_poll(void)
{
    static WORD e[4]; WORD i, ks, k; BYTE a, sc; BYTE __far *bda = (BYTE __far *)MK_FP(0x40, 0);
    if (inpoll) return;
    inpoll = 1;
    if (kbext == 0xFF) kbext = (BYTE)((bda[0x96] & 0x10) ? 0x10 : 0);   /* 101-Tasten-BIOS */
    if (!k_focus || (k_task[k_focus].state != T_READY && k_task[k_focus].state != T_WAIT))
        for (k_focus = 0, i = MAXTASK - 1; i > 0; i--)
            if (k_task[i].state == T_READY || k_task[i].state == T_WAIT) { k_focus = i; break; }
    for (i = 0; i < 8 && *(WORD __far *)(bda + 0x1A) != *(WORD __far *)(bda + 0x1C); i++) {   /* Puffer leer: kein INT 16h */
        kr.ax = (WORD)(kbext + 1) << 8; rd_int(0x16, &kr);
        if (kr.flags & 0x40) break;               /* ZF: keine Taste */
        kr.ax = (WORD)kbext << 8; rd_int(0x16, &kr);
        if (!k_focus) continue;
        a = (BYTE)kr.ax; sc = (BYTE)(kr.ax >> 8);
        if (a == 0xE0 && sc) a = 0;               /* graue Tasten der 101er-Tastatur */
        ks = (bda[0x17] & 3 ? KS_SHIFT : 0) | (bda[0x17] & (KS_CTRL | KS_ALT));
        k = a ? a : 0x100 + sc;
        post_in(M_KEY, k, ((DWORD)ks << 16) | 1, 0, 0);
        if (a) post_in(M_CHAR, a, ((DWORD)ks << 16) | 1, 0, 0);
    }
    if (k_mausget) while (k_mausget((WORD __far *)e)) {
        short x = (short)e[2], y = (short)e[3]; DWORD now = ms_now(), xy = ((DWORD)e[3] << 16) | e[2];
        if (!k_focus) continue;
        ks = (bda[0x17] & 3 ? KS_SHIFT : 0) | (bda[0x17] & (KS_CTRL | KS_ALT));
        if (e[0] == 1) post_in(M_MOUSEMOVE, e[1], xy, x, y);
        else if (e[0] == 2) {
            if (e[1] == ldb && now - lastdown < 500 && x - ldx < 5 && ldx - x < 5 && y - ldy < 5 && ldy - y < 5) {
                post_in(M_DBLCLK, e[1] | (ks << 8), xy, x, y); lastdown = 0;
            } else {
                post_in(M_BUTTONDOWN, e[1] | (ks << 8), xy, x, y);
                lastdown = now; ldx = x; ldy = y; ldb = e[1];
            }
        } else post_in(M_BUTTONUP, e[1] | (ks << 8), xy, x, y);
    }
    inpoll = 0;
}

static void schedule(int wait)
{
    WORD i, n; TASK *t;
    if (wait && k_cur && k_task[k_cur].state == T_READY) k_task[k_cur].state = T_WAIT;
    for (;;) {
        for (i = 1, n = k_cur, t = &k_task[n]; i <= MAXTASK; i++) {   /* reihum ohne Division */
            if (++n == MAXTASK) { n = 1; t = &k_task[1]; i++; } else t++;
            if (t->state == T_READY || (t->state == T_WAIT && has_work(t))) {
                t->state = T_READY;
                switch_to(n);
                return;
            }
        }
        if (!wait) return;                        /* niemand sonst bereit: weiterlaufen */
        if (!alive()) { if (k_cur) switch_to(0); return; }   /* alle fertig: zurueck zum Kern */
        input_poll();
        if (k_focus && has_work(&k_task[k_focus])) continue;
        rd_idle();                                /* bis zum naechsten Interrupt schlafen */
    }
}

/* Strg+Alt+Rueckschritt: Task beenden, der seit mehr als 5 s keine Nachricht abgeholt hat
   und nicht wartet (also rechnet oder haengt). Laeuft im Zeitgeber-Interrupt. */
static void notbeenden(WORD cs)
{
    WORD i, best = 0; DWORD now = ms_now(), alt = 5000;
    if (indos[0] || indos[-1]) return;            /* DOS aktiv: beim naechsten Takt noch einmal */
    for (i = 1; i < MAXTASK; i++)
        if (k_task[i].state == T_READY && now - k_task[i].lastget > alt) { alt = now - k_task[i].lastget; best = i; }
    if (best && best == k_cur && (cs < k_task[best].base || cs >= k_task[best].base + k_task[best].paras))
        return;                                   /* steckt gerade im Kern/BIOS: Kernzustand nicht zerstoeren, spaeter */
    rd_killreq = 0;
    if (!best) { k_out("RD: Notbeenden: keins", 0); return; }
    k_hex("RD: Notbeenden Task", best, 0);
    TaskKill(best, 0xFFFF);                       /* bei k_cur: schaltet um, kehrt nicht zurueck */
}

void __cdecl task_tick(WORD cs)
{
    TASK *t = &k_task[k_cur];
    WORD i, n;
    if (rd_killreq) notbeenden(cs);
    if (!k_cur || t->state != T_READY) return;
    if (t->slice) t->slice--;
    if (cs < t->base || cs >= t->base + t->paras) return;   /* nicht im eigenen Code */
    if (indos[0] || indos[-1]) return;                      /* DOS aktiv / kritischer Fehler */
    input_poll();                                           /* Task rechnet: Eingabe trotzdem annehmen */
    if (t->slice) return;
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
    kr.ax = 0x4200; kr.bx = f; kr.cx = (WORD)(pos >> 16); kr.dx = (WORD)pos;
    if (!k_dos()) return 0;
    kr.ax = 0x3F00; kr.bx = f; kr.cx = n; kr.dx = (WORD)buf; kr.ds = FP_SEG((void __far *)&kr);
    return k_dos() && kr.ax == n;
}
/* Datei pruefen und laden. typen: erlaubte Typbuchstaben ("P" oder "MD"). Ergebnis in k_mz, k_rb. */
WORD k_imgload(const char *path, const char *typen, WORD *seg, WORD *paras)
{
    int f, ok; WORD i, sum = 0; DWORD img, sz; const char *t;
    static struct { WORD seg, reloc; } pb;
    kr.ax = 0x3D00; kr.dx = (WORD)path; kr.ds = FP_SEG((void __far *)&kr);
    if (!k_dos()) return k_lasterr;
    f = kr.ax;
    kr.ax = 0x4202; kr.bx = f; kr.cx = 0; kr.dx = 0;
    ok = k_dos();
    sz = ((DWORD)kr.dx << 16) | kr.ax;
    ok = ok && sz > sizeof k_mz + 16 && rdat(f, 0, &k_mz, sizeof k_mz) && k_mz.sig == 0x5A4D &&
         rdat(f, sz - 16, &tr, sizeof tr) && tr.sig[0] == 'R' && tr.sig[1] == 'D' && tr.sig[2] == '2' && tr.sig[3] == '!' &&
         tr.blk < sz && rdat(f, tr.blk, &k_rb, sizeof k_rb) && k_rb.sig[0] == 'R' && k_rb.sig[1] == 'D' && k_rb.sig[2] == '2';
    kr.ax = 0x3E00; kr.bx = f; rd_int(0x21, &kr);
    if (!ok) return RD_ERR_FORMAT;
    for (t = typen; *t && *t != k_rb.sig[3]; t++) ;
    if (!*t) return RD_ERR_FORMAT;
    for (i = 0; i < sizeof k_rb; i++) sum += ((BYTE *)&k_rb)[i];
    if (sum != tr.sum) return RD_ERR_FORMAT;
    if (k_rb.api > RD2_API_VERSION) return RD_ERR_VERSION;
    img = (DWORD)k_mz.cp * 512 - (k_mz.cblp ? 512 - k_mz.cblp : 0) - (DWORD)k_mz.cparhdr * 16;
    if (img > 0x9FFF0UL) return RD_ERR_FORMAT;
    *paras = (WORD)((img + 15) >> 4) + k_mz.minalloc;
    if (*paras < k_mz.minalloc) return RD_ERR_FORMAT;   /* G12: Ueberlauf ueber 64K Paragraphen */
    if (!(*seg = mem_get(*paras, typen[0] == 'M' ? B_HIGH : 0, 0))) return RD_ERR_NOMEM;
    pb.seg = *seg; pb.reloc = *seg;
    kr.ax = 0x4B03; kr.dx = (WORD)path; kr.ds = FP_SEG((void __far *)&kr); kr.bx = (WORD)&pb; kr.es = kr.ds;
    if (!k_dos()) { mem_put(*seg); return k_lasterr; }
    return 0;
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
    res = (short)rd_enter(t->base + t->ecs, t->eip, (void __far *)rd_api, k_cur, t->show, TX(t)->cmd);
    /* Programm ist zurueckgekehrt. Aufraeumen erledigt reap() im naechsten Task. */
    t = &k_task[k_cur];
    t->quitcode = res;
    k_hex("RD: Task beendet:", ((DWORD)k_cur << 16) | (WORD)res, 0);
    t->state = T_ZOMBIE;
    schedule(1);                                  /* kehrt nie zurueck */
}

static void fcpy(char __far *d, const char __far *s, WORD n)   /* hoechstens n - 1 Zeichen und 0 */
{
    while (--n && *s) *d++ = *s++;
    *d = 0;
}
WORD task_load(LPCSTR app, LPCSTR cmd, WORD show)
{
    WORD i, n, e; static WORD seg, paras;
    TASK *t; TASKX __far *x;
    reap();
    for (n = 1; n < MAXTASK && k_task[n].state != T_FREE; n++) ;
    if (n == MAXTASK) return FAIL(RD_ERR_FULL);
    mkpath(app);
    if ((e = k_imgload(path, "P", &seg, &paras)) != 0) return FAIL(e);
    t = &k_task[n];
    t->stk = mem_get(STK_PARAS, 0, 0);
    t->q = mem_get((QLEN * sizeof(MSG) + 15) >> 4, 0, n);
    t->xs = mem_get((sizeof(TASKX) + 15) >> 4, 0, n);
    if (!t->stk || !t->q || !t->xs) {
        if (t->stk) mem_put(t->stk);
        if (t->q) mem_put(t->q);
        if (t->xs) mem_put(t->xs);
        mem_put(seg); t->stk = t->q = t->xs = 0;
        return FAIL(RD_ERR_NOMEM);
    }
    mem_chown(seg, n);                            /* Programmbild gehoert dem Task */
    t->base = seg; t->paras = paras; t->ecs = k_mz.cs; t->eip = k_mz.ip; t->show = show;
    t->qh = t->qn = t->quit = 0; t->quitcode = 0; t->rsp = 0;
    for (i = 0; i < NTIMER; i++) t->tm[i].ms = 0;
    fcpy(t->name, k_rb.name, 16);
    x = TX(t); fcpy(x->cmd, cmd ? cmd : "", 80); fcpy(x->path, path, 80); x->blk = k_rb.nres ? tr.blk : 0;
    if (k_fpu) { rd_fsave(FPU(n)); rd_frstor(FPU(n)); }  /* Startzustand = aktueller 8087 */
    t->ss = t->stk;
    t->sp = rd_newstack(t->stk, STK_PARAS * 16, task_start);
    t->state = T_READY;
    k_focus = n;                                  /* das neueste Programm bekommt die Eingabe */
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
    TASK *t = &k_task[k_cur]; MSG __far *q; WORD j; DWORD now = ms_now();
    t->lastget = now;
    if (t->qn) {
        q = (MSG __far *)MK_FP(t->q, 0) + t->qh;
        *m = *q;
        if (remove) { t->qh = (t->qh + 1) % QLEN; t->qn--; }
        return 1;
    }
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
    for (;;) { input_poll(); if (fetch(m, 1)) break; schedule(1); }
    return m->msg != M_QUIT;
}
BOOL RDK PeekMessage(MSG FAR *m, WORD remove)
{
    WORD n;
    input_poll();
    n = k_nsw;
    if (fetch(m, remove)) return TRUE;
    schedule(0);                                  /* kurz anderen Tasks Zeit geben */
    return n != k_nsw && fetch(m, remove) ? TRUE : FALSE;   /* nur nach Umschalten neu nachsehen */
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
    t->state = T_ZOMBIE;
    k_hex("RD: Task beendet (Kill):", ((DWORD)n << 16) | code, 0);
    if (n == k_cur) schedule(1);                  /* kehrt nie zurueck; reap() raeumt im naechsten Task auf */
    reap();
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
    if (k_fpu && !(k_fpuseg = mem_get((MAXTASK * 94 + 15) / 16, 0, 0))) k_fpu = 0;
}

/* Fensternachrichten: Fenster verwaltet RDUSER (k_wnd Art 0: Task des Fensters, 0 = ungueltig;
   Art 2: Eingabe zuordnen; Art 3: Task beendet). SendMessage traegt RDUSER selbst ein.
   Ohne RDUSER geht nur Fenster 0 = der eigene Task. */
BOOL RDK PostMessage(HWND w, WORD msg, WORD p1, DWORD p2)
{
    WORD n = k_cur;
    if (w && (!k_wnd || !(n = (WORD)k_wnd(0, w, msg, p1, p2)))) return FAIL(k_wnd ? RD_ERR_HANDLE : RD_ERR_NOTSUP);
    return enqueue(n, w, msg, p1, p2) > 0;
}
/* Shell: alle Programme bekommen M_QUIT, danach endet RD */
void RDK ExitRD(WORD flags)
{
    WORD i;
    for (i = 1; i < MAXTASK; i++) if (k_task[i].state == T_READY || k_task[i].state == T_WAIT) { k_task[i].quit = 1; k_task[i].quitcode = flags; }
}
DWORD RDK InstRes(HINST i, LPSTR pfad, WORD len)
{
    TASK *t = &k_task[i];
    if (i < 1 || i >= MAXTASK || !t->xs || t->state == T_FREE || t->state == T_ZOMBIE) return FAIL(RD_ERR_HANDLE);
    if (pfad && len) fcpy(pfad, TX(t)->path, len);
    return TX(t)->blk;
}

void task_api(void)
{
    KAPI(PostMessage, PostMessage);
    KAPI(ExitRD, ExitRD); KAPI(InstRes, InstRes);
    KAPI(GetMessage, GetMessage); KAPI(PeekMessage, PeekMessage); KAPI(WaitMessage, WaitMessage);
    KAPI(TaskYield, TaskYield); KAPI(PostTask, PostTask); KAPI(PostQuit, PostQuit);
    KAPI(DispatchMessage, DispatchMessage); KAPI(TimerSet, TimerSet); KAPI(TimerKill, TimerKill);
    KAPI(TickCount, TickCount); KAPI(TaskSelf, TaskSelf); KAPI(TaskExec, TaskExec); KAPI(TaskKill, TaskKill);
    KAPI(TaskFirst, TaskFirst); KAPI(TaskNext, TaskNext);
}
