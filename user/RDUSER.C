/* RDUSER.C - Oberflaeche: Fenster, Zeichnen, Eingabe, Menueleiste (SPEC.md 4, 5)
 *
 * Fenster stehen in einer festen Tabelle. HWND = Nummer | Generation << 8 (alte Griffe werden ungueltig).
 * Baum: parent, child (oberstes Kind), next (naechstes darunter). Hauptfenster haengen an Wurzel 0.
 * Lage r: Hauptfenster in Bildschirmkoordinaten, Kinder relativ zum Client des Elternfensters.
 * Fensterfunktionen laufen immer im Task, dem das Fenster gehoert, mit DS = SS des Programms
 * (Programme laufen mit SS = DGROUP). Andere Tasks bekommen Nachrichten nur eingestellt
 * (SendMessage ueber Taskgrenzen folgt mit dem Kern-Umschalter).
 * Neu zeichnen: ungueltiger Bereich je Fenster, M_PAINT wird einmal eingestellt. Rahmen, Titel,
 * Schreibtisch und Menueleiste zeichnet RDUSER selbst, gleich beim Aufdecken.
 */
#include "RD2.H"
#include "../gdi/GDII.H"

#define MAXW   40
#define MAXCL  16
#define NSLOT  6                                  /* gleichzeitig offene Fenster-DCs */
#define F_PAINTQ 0x01                             /* M_PAINT eingestellt */
#define F_ERASE  0x02
#define F_INTERN 0x04                             /* Fensterfunktion in RDUSER */
#define M_NCBUTTONDOWN 0x00A1                     /* p1 = HT_*, p2 = Bildschirm */
#define HT_TITLE 2
#define HT_CLOSE 3
#define HT_MENU  1
#define HT_VSCROLL 4
#define HT_HSCROLL 5

typedef struct {
    BYTE  gen, used, vis, cls, task, flags, parent, child, next, dis;
    WORD  id; HINST inst;
    DWORD stil;
    RECT  r, inv;
    WNDPROC proc;
    HMENU menu;
    DWORD data[4];
    char  text[32];
} WND;
typedef struct { char name[16]; WNDPROC proc; WORD stil, inst; COLOR hinter; BYTE task, intern; } KLASSE;

static WND w_[MAXW + 1];                          /* 0 = Wurzel (Schreibtisch) */
static KLASSE kl[MAXCL];
static BYTE act, foc, cap;                        /* Nummern, 0 = keines */
static short sw, sh, mh, th;                      /* Schirm, Menueleiste, Titelzeile */
static DCX FAR *dcs; static GK FAR *gk;
static HDC (FAR __pascal *gdc)(HWND);
static struct { HDC dc; WORD n; RECT r[MAXCLIP]; } sl[NSLOT];
static RECT lim; static BYTE limon;
#define NSB 12                                    /* Fenster mit Rollbalken-Werten */
static struct { BYTE w; short v[2][3]; } sbt[NSB]; /* v: pos, max, seite */
static short bx[14]; static BYTE bn;              /* Menueleiste: Anfang je Titel, bx[bn] = Ende */               /* Aufdecken: nur hier zeichnen */

void RDEXPORT u_ShowWindow(HWND h, WORD art);
HWND RDEXPORT u_SetActive(HWND h);
static void desk(void);
static void leiste(void);
static int schirm_(void);
static void leistemenue(HDC d);
static void sbmal(WORD i, WORD bar, HDC d);
static void menulauf(WORD i, int bar, int maus);
static void sbklick(WORD i, WORD bar, short x, short y);
static void aufdecken(const RECT *scr);
static void inval(WORD i, const RECT *r, BYTE erase);

/* ---- Hilfen ------------------------------------------------------------------- */
static WORD idx(HWND h)
{
    WORD i = h & 0xFF;
    if (!i || i > MAXW || !w_[i].used || w_[i].gen != (BYTE)(h >> 8)) return 0;
    return i;
}
static HWND hw(WORD i) { return i ? (HWND)(((WORD)w_[i].gen << 8) | i) : 0; }
static WORD bad(void) { SetLastError(RD_ERR_HANDLE); return 0; }
static void isect(RECT *a, const RECT *b)
{
    if (b->l > a->l) a->l = b->l; if (b->t > a->t) a->t = b->t;
    if (b->r < a->r) a->r = b->r; if (b->b < a->b) a->b = b->b;
}
static int leer(const RECT *a) { return a->l >= a->r || a->t >= a->b; }
static void off(RECT *a, short x, short y) { a->l += x; a->r += x; a->t += y; a->b += y; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && s && *s) *d++ = *s++; *d = 0; }

static short rand_(WORD i) { return (w_[i].stil & WS_BORDER) ? 1 : 0; }
static short titel(WORD i) { return (w_[i].stil & WS_TITLE) ? th : 0; }
static void org(WORD i, short *x, short *y)       /* Client-Ursprung in Bildschirmkoordinaten */
{
    *x = 0; *y = 0;
    for (; i; i = w_[i].parent) { *x += w_[i].r.l + rand_(i); *y += w_[i].r.t + rand_(i) + titel(i); }
}
static void wrect(WORD i, RECT *r)                /* Fenster, Bildschirm */
{
    short x = 0, y = 0;
    if (w_[i].parent) org(w_[i].parent, &x, &y);
    *r = w_[i].r; off(r, x, y);
}
static void crect(WORD i, RECT *r)                /* Client, Bildschirm */
{
    short b = rand_(i);
    wrect(i, r); r->l += b; r->r -= b; r->b -= b; r->t += b + titel(i);
    if (w_[i].stil & WS_VSCROLL) r->r -= 16;       /* SBW */
    if (w_[i].stil & WS_HSCROLL) r->b -= th;
}
static WORD top(WORD i) { while (i && w_[i].parent) i = w_[i].parent; return i; }
static int sichtbar(WORD i) { for (; i; i = w_[i].parent) if (!w_[i].vis) return 0; return 1; }

/* ---- sichtbare Flaeche ----------------------------------------------------------- */
static RECT vt[MAXCLIP * 2];
static WORD minus(RECT *v, WORD n, const RECT *b)  /* b aus der Liste schneiden */
{
    WORD i, k = 0; RECT a, s;
    for (i = 0; i < n; i++) {
        a = v[i];
        if (b->l >= a.r || b->r <= a.l || b->t >= a.b || b->b <= a.t) { vt[k++] = a; continue; }
        if (k + 4 > MAXCLIP * 2) { vt[k++] = a; continue; }   /* voll: lieber zu viel zeichnen */
        if (b->t > a.t) { s = a; s.b = b->t; vt[k++] = s; a.t = b->t; }
        if (b->b < a.b) { s = a; s.t = b->b; vt[k++] = s; a.b = b->b; }
        if (b->l > a.l) { s = a; s.r = b->l; vt[k++] = s; }
        if (b->r < a.r) { s = a; s.l = b->r; vt[k++] = s; }
    }
    if (k > MAXCLIP) k = MAXCLIP;
    for (i = 0; i < k; i++) v[i] = vt[i];
    return k;
}
/* client = 1: nur Client ohne eigene Kinder. i = 0: Schreibtisch */
static WORD vis(WORD i, int client, RECT *v)
{
    RECT b, c; WORD n = 1, j, p;
    if (!i) { b.l = 0; b.t = mh; b.r = sw; b.b = sh; }
    else {
        if (!sichtbar(i)) return 0;
        if (client) crect(i, &b); else wrect(i, &b);
        for (p = w_[i].parent; p; p = w_[p].parent) { crect(p, &c); isect(&b, &c); }
        c.l = 0; c.t = mh; c.r = sw; c.b = sh; isect(&b, &c);
    }
    if (limon) isect(&b, &lim);
    if (leer(&b)) return 0;
    v[0] = b;
    if (i) for (p = i; ; p = w_[p].parent) {      /* Geschwister darueber, auf jeder Ebene */
        for (j = w_[w_[p].parent].child; j && j != p; j = w_[j].next)
            if (w_[j].vis) { wrect(j, &c); if (!(n = minus(v, n, &c))) return 0; }
        if (!w_[p].parent) break;
    }
    if (!i || client)
        for (j = w_[i].child; j; j = w_[j].next)
            if (w_[j].vis) { wrect(j, &c); if (!(n = minus(v, n, &c))) return 0; }
    return n;
}

/* ---- DCs ----------------------------------------------------------------------- */
static HDC ndc(WORD i, int client, const RECT *nur)   /* nur: zusaetzlich begrenzen (Bildschirm) */
{
    WORD s, k, n; HDC h; DCX FAR *d; short x, y; RECT r;
    for (s = 0; s < NSLOT && sl[s].dc; s++) ;
    if (s == NSLOT) { SetLastError(RD_ERR_FULL); return 0; }
    if (!(h = gdc(0))) return 0;
    n = vis(i, client, sl[s].r);
    if (nur) for (k = 0; k < n; ) {
        r = sl[s].r[k]; isect(&r, nur);
        if (leer(&r)) sl[s].r[k] = sl[s].r[--n]; else sl[s].r[k++] = r;
    }
    if (!n) { sl[s].r[0].l = sl[s].r[0].r = 0; sl[s].r[0].t = sl[s].r[0].b = 0; n = 1; }   /* nichts zeichnen */
    sl[s].dc = h; sl[s].n = n;
    d = &dcs[h - 1];
    if (client && i) org(i, &x, &y);
    else if (i) { RECT w; wrect(i, &w); x = w.l; y = w.t; }
    else { x = 0; y = 0; }
    d->ox = x; d->oy = y; d->clip = (const RECT FAR *)sl[s].r; d->nclip = n;
    gk->kclip = 1;
    return h;
}
static void fdc(HDC h)
{
    WORD s;
    for (s = 0; s < NSLOT; s++) if (sl[s].dc == h) { sl[s].dc = 0; dcs[h - 1].nclip = 0; gk->kclip = 1; }
    DCFree(h);
}

/* ---- Fensterfunktion aufrufen --------------------------------------------------- */
static DWORD callp(WNDPROC f, HWND w, WORD m, WORD p1, DWORD p2)
{
    WORD lo = 0, hi = 0;
    _asm {
        push ds
        push w
        push m
        push p1
        push word ptr p2+2
        push word ptr p2
        mov  ax, ss                ; Programm: DS = SS
        mov  ds, ax
        call dword ptr f
        pop  ds
        mov  lo, ax
        mov  hi, dx
    }
    return MAKELONG(lo, hi);
}
static DWORD sendi(WORD i, WORD m, WORD p1, DWORD p2)
{
    WND *x = &w_[i];
    if (!i || !x->proc) return 0;
    if (x->flags & F_INTERN) return x->proc(hw(i), m, p1, p2);
    if (x->task != (BYTE)TaskSelf()) { PostMessage(hw(i), m, p1, p2); return 0; }
    return callp(x->proc, hw(i), m, p1, p2);
}

/* ---- Zeichnen: Schreibtisch, Leiste, Rahmen --------------------------------------- */
static void desk(void)
{
    HDC d = ndc(0, 1, 0); RECT r;
    if (!d) return;
    r.l = 0; r.t = mh; r.r = sw; r.b = sh;
    SetPen(d, C_BLACK); SetBrush(d, C_BLACK, PAT_GRAY50); SetBkColor(d, C_WHITE); FillRect(d, &r);
    fdc(d);
}
static void leiste(void)
{
    HDC d = gdc(0); RECT r;
    if (!d) return;
    if (limon && lim.t >= mh) { DCFree(d); return; }
    r.l = 0; r.t = 0; r.r = sw; r.b = mh - 1;
    SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &r);
    SetPen(d, C_BLACK); Line(d, 0, mh - 1, sw - 1, mh - 1);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
    TextOut(d, 8, 1, "RD", 2);
    leistemenue(d);
    DCFree(d);
}
static void rahmen(WORD i)
{
    HDC d; RECT r, c; short b = rand_(i), t = titel(i), wd;
    if (!b && !t && !(w_[i].stil & (WS_VSCROLL | WS_HSCROLL))) return;
    if (!(d = ndc(i, 0, limon ? &lim : 0))) return;
    r = w_[i].r; off(&r, (short)-r.l, (short)-r.t); wd = r.r;
    SetPen(d, C_BLACK);
    if (b) Frame(d, &r);
    if (t) {
        c.l = b; c.t = b; c.r = wd - b; c.b = b + t - 1;
        SetBrush(d, i == act ? C_BLACK : C_WHITE, PAT_SOLID); FillRect(d, &c);
        Line(d, b, b + t - 1, wd - b - 1, b + t - 1);
        SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, i == act ? C_WHITE : C_BLACK);
        TextOut(d, (short)((wd - TextWidth(d, w_[i].text, -1)) / 2), b + 1, w_[i].text, -1);
        if (w_[i].stil & WS_CLOSE) {              /* Schliessfeld links (wie GEM) */
            c.l = b + 3; c.t = b + 2; c.r = c.l + t - 5; c.b = b + t - 3;
            SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &c); SetPen(d, C_BLACK); Frame(d, &c);
        }
    }
    if (w_[i].stil & WS_VSCROLL) sbmal(i, SB_VERT, d);
    if (w_[i].stil & WS_HSCROLL) sbmal(i, SB_HORZ, d);
    fdc(d);
}
static void inval(WORD i, const RECT *r, BYTE erase)   /* r: Client-Koordinaten, 0 = alles */
{
    WND *x = &w_[i]; RECT c, a;
    crect(i, &c); off(&c, (short)-c.l, (short)-c.t);
    a = r ? *r : c; isect(&a, &c);
    if (leer(&a)) return;
    if (leer(&x->inv)) x->inv = a;
    else {
        if (a.l < x->inv.l) x->inv.l = a.l; if (a.t < x->inv.t) x->inv.t = a.t;
        if (a.r > x->inv.r) x->inv.r = a.r; if (a.b > x->inv.b) x->inv.b = a.b;
    }
    if (erase) x->flags |= F_ERASE;
    if (!(x->flags & F_PAINTQ)) { x->flags |= F_PAINTQ; PostMessage(hw(i), M_PAINT, 0, 0); }
}
static void neuz(WORD i, const RECT *scr)         /* Fenster samt Kindern im Bereich scr neu */
{
    WORD j; RECT c, a;
    if (!w_[i].vis) return;
    wrect(i, &a);
    if (scr) { isect(&a, scr); if (leer(&a)) return; }
    limon = 1; lim = a; rahmen(i); limon = 0;
    crect(i, &c); isect(&a, &c);
    if (!leer(&a)) { off(&a, (short)-c.l, (short)-c.t); inval(i, &a, 1); }
    for (j = w_[i].child; j; j = w_[j].next) neuz(j, scr);
}
static void aufdecken(const RECT *scr)            /* Bereich wurde frei: alles darunter neu */
{
    WORD j;
    limon = 1; lim = *scr; desk(); leiste(); limon = 0;
    for (j = w_[0].child; j; j = w_[j].next) neuz(j, scr);
}

/* ---- Klassen und Fenster -------------------------------------------------------- */
static int gleich(const char *a, LPCSTR b)
{
    while (*a && (*a | 0x20) == (*b | 0x20)) a++, b++;
    return !*a && !*b;
}
static WORD klasse(LPCSTR n)
{
    WORD k;
    for (k = 0; k < MAXCL; k++) if (kl[k].name[0] && gleich(kl[k].name, n)) return k + 1;
    return 0;
}
WORD RDEXPORT u_RegisterClass(const WNDCLASS FAR *wc)
{
    WORD k;
    if (!wc || !wc->name || !wc->proc) return bad();
    if ((k = klasse(wc->name)) != 0) { if (kl[k - 1].task != (BYTE)TaskSelf()) { SetLastError(RD_ERR_BUSY); return 0; } k--; }
    else { for (k = 0; k < MAXCL && kl[k].name[0]; k++) ; if (k == MAXCL) { SetLastError(RD_ERR_FULL); return 0; } }
    scpy(kl[k].name, wc->name, 16); kl[k].proc = wc->proc; kl[k].stil = wc->stil; kl[k].inst = wc->inst;
    kl[k].hinter = wc->hinter; kl[k].task = (BYTE)TaskSelf(); kl[k].intern = 0;
    return k + 1;
}
static void link(WORD i, WORD p)                  /* ganz oben einhaengen */
{
    w_[i].parent = (BYTE)p; w_[i].next = w_[p].child; w_[p].child = (BYTE)i;
}
static void unlink(WORD i)
{
    BYTE *q = &w_[w_[i].parent].child;
    while (*q && *q != i) q = &w_[*q].next;
    if (*q) *q = w_[i].next;
    w_[i].next = 0;
}
HWND RDEXPORT u_CreateWindow(LPCSTR kn, LPCSTR titel_, DWORD stil, short x, short y, short b, short h, HWND eltern, WORD id, HINST inst)
{
    WORD k = klasse(kn), i, p = 0; WND *w;
    if (!k) { SetLastError(RD_ERR_NOTFOUND); return 0; }
    if (!schirm_()) return 0;
    if (eltern && !(p = idx(eltern))) return bad();
    for (i = 1; i <= MAXW && w_[i].used; i++) ;
    if (i > MAXW) { SetLastError(RD_ERR_FULL); return 0; }
    w = &w_[i];
    w->gen++; if (!w->gen) w->gen = 1;
    w->used = 1; w->vis = 0; w->cls = (BYTE)k; w->task = (BYTE)TaskSelf(); w->flags = kl[k - 1].intern ? F_INTERN : 0;
    w->child = 0; w->dis = (stil & WS_DISABLED) ? 1 : 0; w->id = id; w->inst = inst; w->stil = stil; w->menu = 0;
    w->proc = kl[k - 1].proc; w->data[0] = w->data[1] = w->data[2] = w->data[3] = 0;
    if (p) w->stil |= WS_CHILD;
    if (x == CW_DEFAULT) { x = (short)(16 + (i & 7) * 16); y = (short)(mh + 8 + (i & 7) * 12); }
    if (b == CW_DEFAULT) { b = sw / 2; h = (sh - mh) / 2; }
    w->r.l = x; w->r.t = y; w->r.r = x + b; w->r.b = y + h;
    w->inv.l = w->inv.r = 0; w->inv.t = w->inv.b = 0;
    scpy(w->text, titel_, sizeof w->text);
    link(i, p);
    if (sendi(i, M_CREATE, 0, 0) == 0xFFFFFFFFUL) { unlink(i); w->used = 0; SetLastError(RD_ERR_ABORT); return 0; }
    if (idx(hw(i))) sendi(i, M_SIZE, SIZE_NORMAL, MAKELONG(b - 2 * rand_(i), h - 2 * rand_(i) - titel(i)));
    if ((stil & WS_VISIBLE) && idx(hw(i))) u_ShowWindow(hw(i), SW_SHOW);
    return hw(i);
}
static void weg(WORD i, int melden)               /* samt Kindern, ohne Neuzeichnen */
{
    WORD j, n;
    for (j = w_[i].child; j; j = n) { n = w_[j].next; weg(j, melden); }
    if (melden) sendi(i, M_DESTROY, 0, 0);
    if (!w_[i].used) return;
    if (cap == i) cap = 0;
    if (foc == i) foc = 0;
    unlink(i); w_[i].used = 0; w_[i].child = 0;
    { WORD k; for (k = 0; k < NSB; k++) if (sbt[k].w == (BYTE)i) sbt[k].w = 0; }
}
static void neuaktiv(void)
{
    WORD j;
    for (j = w_[0].child; j && !w_[j].vis; j = w_[j].next) ;
    act = 0;
    if (j) u_SetActive(hw(j)); else leiste();
}
BOOL RDEXPORT u_DestroyWindow(HWND h)
{
    WORD i = idx(h), war; RECT r;
    if (!i) return bad();
    war = w_[i].vis && sichtbar(i); wrect(i, &r);
    weg(i, 1);
    if (war) aufdecken(&r);
    if (!w_[act].used) neuaktiv();
    return TRUE;
}
void RDEXPORT u_ShowWindow(HWND h, WORD art)
{
    WORD i = idx(h); RECT r;
    if (!i) { bad(); return; }
    if (art == SW_HIDE) {
        if (!w_[i].vis) return;
        wrect(i, &r); w_[i].vis = 0; aufdecken(&r);
        if (act == i) neuaktiv();
        return;
    }
    if (art == SW_MAX && !w_[i].parent) { w_[i].r.l = 0; w_[i].r.t = mh; w_[i].r.r = sw; w_[i].r.b = sh; }
    if (!w_[i].vis) { w_[i].vis = 1; sendi(i, M_SHOW, 1, 0); }
    if (!w_[i].parent) u_SetActive(h); else neuz(i, 0);
}
void RDEXPORT u_MoveWindow(HWND h, short x, short y, short b, short hh, BOOL neu)
{
    WORD i = idx(h); RECT alt; short ab, ah;
    if (!i) { bad(); return; }
    wrect(i, &alt); ab = w_[i].r.r - w_[i].r.l; ah = w_[i].r.b - w_[i].r.t;
    w_[i].r.l = x; w_[i].r.t = y; w_[i].r.r = x + b; w_[i].r.b = y + hh;
    if (x != alt.l || y != alt.t) sendi(i, M_MOVE, 0, MAKELONG(x, y));
    if (b != ab || hh != ah) sendi(i, M_SIZE, SIZE_NORMAL, MAKELONG(b - 2 * rand_(i), hh - 2 * rand_(i) - titel(i)));
    if (!idx(h) || !sichtbar(i)) return;
    aufdecken(&alt);
    if (neu) neuz(i, 0);
}
void RDEXPORT u_SetWindowText(HWND h, LPCSTR s)
{
    WORD i = idx(h);
    if (!i) { bad(); return; }
    scpy(w_[i].text, s, sizeof w_[i].text);
    if (sichtbar(i)) { rahmen(i); if (i == act) leiste(); }
}
WORD RDEXPORT u_GetWindowText(HWND h, LPSTR buf, WORD len)
{
    WORD i = idx(h), n = 0;
    if (!i) return bad();
    if (!len) return 0;
    while (n + 1 < len && w_[i].text[n]) { buf[n] = w_[i].text[n]; n++; }
    buf[n] = 0;
    return n;
}
void RDEXPORT u_GetClientRect(HWND h, RECT FAR *r)
{
    WORD i = idx(h); RECT c;
    if (!i) { bad(); return; }
    crect(i, &c); r->l = 0; r->t = 0; r->r = c.r - c.l; r->b = c.b - c.t;
}
void RDEXPORT u_GetWindowRect(HWND h, RECT FAR *r)
{
    WORD i = idx(h); RECT c;
    if (!i) { bad(); return; }
    wrect(i, &c); *r = c;
}
void RDEXPORT u_InvalidateRect(HWND h, const RECT FAR *r, BOOL loeschen)
{
    WORD i = idx(h); RECT a;
    if (!i) { bad(); return; }
    if (!sichtbar(i)) return;
    if (r) a = *r;
    inval(i, r ? &a : 0, (BYTE)(loeschen ? 1 : 0));
}
void RDEXPORT u_ValidateRect(HWND h, const RECT FAR *r)
{
    WORD i = idx(h);
    (void)r;                                      /* Teilbereiche: alles gilt als gezeichnet */
    if (!i) { bad(); return; }
    w_[i].inv.l = w_[i].inv.r = 0; w_[i].inv.t = w_[i].inv.b = 0; w_[i].flags &= ~F_ERASE;
}
void RDEXPORT u_UpdateWindow(HWND h)
{
    WORD i = idx(h);
    if (i && !leer(&w_[i].inv)) sendi(i, M_PAINT, 0, 0);
}
static void hinter(WORD i, HDC d, const RECT *r)
{
    COLOR c = kl[w_[i].cls - 1].hinter;
    SetBrush(d, c == RD_DEFAULT ? C_WHITE : c, PAT_SOLID); FillRect(d, r);
}
HDC RDEXPORT u_BeginPaint(HWND h, PAINT FAR *ps)
{
    WORD i = idx(h); RECT a; HDC d;
    if (!i) return bad();
    w_[i].flags &= ~F_PAINTQ;
    a = w_[i].inv; ps->neu = a; ps->loeschen = TRUE;
    { short x, y; org(i, &x, &y); off(&a, x, y); }
    d = ndc(i, 1, &a);
    ps->dc = d;
    if (d && (w_[i].flags & F_ERASE)) { a = w_[i].inv; hinter(i, d, &a); }
    w_[i].inv.l = w_[i].inv.r = 0; w_[i].inv.t = w_[i].inv.b = 0; w_[i].flags &= ~F_ERASE;
    return d;
}
void RDEXPORT u_EndPaint(HWND h, PAINT FAR *ps) { (void)h; if (ps->dc) fdc(ps->dc); ps->dc = 0; }
HDC RDEXPORT u_GetDC(HWND h)
{
    WORD i = 0;
    if (h && !(i = idx(h))) return bad();
    return i ? ndc(i, 1, 0) : gdc(0);
}
void RDEXPORT u_ReleaseDC(HWND h, HDC d) { (void)h; fdc(d); }
HWND RDEXPORT u_SetFocus(HWND h)
{
    WORD i = idx(h), alt = foc;
    if (h && !i) return bad();
    if (i == foc) return hw(alt);
    foc = (BYTE)i;
    if (alt && w_[alt].used) sendi(alt, M_KILLFOCUS, 0, 0);
    if (i) sendi(i, M_SETFOCUS, 0, 0);
    return hw(alt);
}
HWND RDEXPORT u_GetFocus(void) { return hw(foc); }
HWND RDEXPORT u_SetActive(HWND h)
{
    WORD i = top(idx(h)), alt = act;
    if (!i) return bad();
    if (w_[0].child != i) { unlink(i); link(i, 0); }   /* nach oben */
    act = (BYTE)i;
    if (alt != i && alt && w_[alt].used) { if (w_[alt].vis) rahmen(alt); sendi(alt, M_ACTIVATE, 0, 0); }
    if (w_[i].vis) neuz(i, 0);
    leiste();
    if (alt != i) { sendi(i, M_ACTIVATE, 1, 0); if (!foc || top(foc) != i) u_SetFocus(h); }
    return hw(alt);
}
HWND RDEXPORT u_GetActive(void) { return hw(act); }
DWORD RDEXPORT u_SetWindowData(HWND h, short n, DWORD v)
{
    WORD i = idx(h); DWORD alt;
    if (!i) return bad();
    switch (n) {
    case WD_PROC: alt = (DWORD)w_[i].proc; w_[i].proc = (WNDPROC)v; w_[i].flags &= ~F_INTERN; return alt;
    case WD_ID: alt = w_[i].id; w_[i].id = (WORD)v; return alt;
    case WD_STIL: alt = w_[i].stil; w_[i].stil = v; return alt;
    }
    if (n < 0 || n > 3) return bad();
    alt = w_[i].data[n]; w_[i].data[n] = v; return alt;
}
DWORD RDEXPORT u_GetWindowData(HWND h, short n)
{
    WORD i = idx(h);
    if (!i) return bad();
    switch (n) {
    case WD_PROC: return (DWORD)w_[i].proc;
    case WD_INST: return w_[i].inst;
    case WD_ID: return w_[i].id;
    case WD_STIL: return w_[i].stil;
    }
    if (n < 0 || n > 3) return bad();
    return w_[i].data[n];
}
HWND RDEXPORT u_GetParent(HWND h) { WORD i = idx(h); return i ? hw(w_[i].parent) : bad(); }
static WORD bei(WORD p, short x, short y)          /* tiefstes sichtbares Fenster am Punkt */
{
    WORD j; RECT r;
    for (j = w_[p].child; j; j = w_[j].next) {
        if (!w_[j].vis) continue;
        wrect(j, &r);
        if (x >= r.l && x < r.r && y >= r.t && y < r.b) {
            crect(j, &r);
            if (x >= r.l && x < r.r && y >= r.t && y < r.b) { WORD k = bei(j, x, y); return k ? k : j; }
            return j;
        }
    }
    return 0;
}
HWND RDEXPORT u_WindowFromPoint(short x, short y) { return y < mh ? 0 : hw(bei(0, x, y)); }
void RDEXPORT u_ClientToScreen(HWND h, POINT FAR *p) { short x, y; WORD i = idx(h); if (!i) { bad(); return; } org(i, &x, &y); p->x += x; p->y += y; }
void RDEXPORT u_ScreenToClient(HWND h, POINT FAR *p) { short x, y; WORD i = idx(h); if (!i) { bad(); return; } org(i, &x, &y); p->x -= x; p->y -= y; }
BOOL RDEXPORT u_IsWindow(HWND h) { return idx(h) != 0; }
void RDEXPORT u_EnableWindow(HWND h, BOOL an)
{
    WORD i = idx(h);
    if (!i) { bad(); return; }
    if (w_[i].dis == !an) return;
    w_[i].dis = (BYTE)!an;
    if (an) w_[i].stil &= ~WS_DISABLED; else w_[i].stil |= WS_DISABLED;
    sendi(i, M_ENABLE, an ? 1 : 0, 0);
    if (sichtbar(i)) inval(i, 0, 1);
}
HWND RDEXPORT u_CaptureMouse(HWND h) { WORD alt = cap; cap = (BYTE)idx(h); return hw(alt); }
void RDEXPORT u_ReleaseMouse(void) { cap = 0; }

/* ---- Nachrichten ---------------------------------------------------------------- */
DWORD RDEXPORT u_SendMessage(HWND h, WORD m, WORD p1, DWORD p2)
{
    WORD i = idx(h);
    if (!i) return bad();
    return sendi(i, m, p1, p2);
}
DWORD RDEXPORT u_DispatchMessage(const MSG FAR *m)
{
    WORD i = idx(m->w), t;
    if (!i) return 0;
    if ((m->msg == M_BUTTONDOWN || m->msg == M_NCBUTTONDOWN) && (t = top(i)) != act) u_SetActive(hw(t));
    if (m->msg == M_PAINT && leer(&w_[i].inv)) { w_[i].flags &= ~F_PAINTQ; return 0; }   /* schon gezeichnet */
    return sendi(i, m->msg, m->p1, m->p2);
}
static void ziehen(WORD i, short mx, short my)    /* Titel gezogen: Umriss folgen lassen */
{
    MSG m; RECT r, a; HDC d = gdc(0); short dx = 0, dy = 0;
    if (!d) return;
    wrect(i, &r); a = r;
    SetRop(d, ROP_XOR); SetPen(d, C_WHITE); SetBrush(d, C_WHITE, PAT_NONE);
    cap = (BYTE)i;
    Frame(d, &a);
    for (;;) {
        if (!GetMessage(&m)) { PostQuit(0); break; }
        if (m.msg == M_MOUSEMOVE || m.msg == M_BUTTONUP) {
            Frame(d, &a);
            dx = m.pt.x - mx; dy = m.pt.y - my;
            if (r.t + dy < mh) dy = mh - r.t;
            a = r; off(&a, dx, dy);
            if (m.msg == M_BUTTONUP) break;
            Frame(d, &a);
        } else if (m.msg == M_KEY && m.p1 == K_ESC) { Frame(d, &a); dx = dy = 0; break; }
        else if (m.msg != M_BUTTONDOWN) u_DispatchMessage(&m);
    }
    cap = 0;
    DCFree(d);
    if (dx || dy) u_MoveWindow(hw(i), r.l + dx, r.t + dy, r.r - r.l, r.b - r.t, TRUE);
}
DWORD RDEXPORT u_DefWindowProc(HWND h, WORD m, WORD p1, DWORD p2)
{
    WORD i = idx(h); PAINT ps;
    if (!i) return 0;
    switch (m) {
    case M_NCBUTTONDOWN:
        if (p1 == HT_MENU) { short x = (short)LOWORD(p2); WORD k; for (k = 0; k < bn && !(x >= bx[k] && x < bx[k + 1]); k++) ; if (k < bn) menulauf(top(i), k, 1); }
        else if (p1 == HT_VSCROLL || p1 == HT_HSCROLL) sbklick(i, p1 == HT_VSCROLL ? SB_VERT : SB_HORZ, (short)LOWORD(p2), (short)HIWORD(p2));
        else if (p1 == HT_CLOSE) PostMessage(h, M_CLOSE, 0, 0);
        else if (p1 == HT_TITLE) ziehen(i, (short)LOWORD(p2), (short)HIWORD(p2));
        return 0;
    case M_CLOSE: u_DestroyWindow(h); return 0;
    case M_PAINT: u_BeginPaint(h, &ps); u_EndPaint(h, &ps); return 0;
    case M_KEY:
        if (p1 == 0x100 + 0x3E && (HIWORD(p2) & KS_ALT)) PostMessage(hw(top(i)), M_CLOSE, 0, 0);   /* Alt+F4 */
        else if (p1 == K_F10 && top(i) == act) menulauf(act, 0, 0);
        return 0;
    }
    return 0;
}
/* ---- Rollbalken (SPEC.md 5) -------------------------------------------------------- */
#define SBW 16                                    /* senkrechter Balken: Breite; waagrechter: Hoehe th */
static short sb0[3];
static short *sbw(WORD i, WORD bar, int neu)      /* Werte des Balkens, neu: Eintrag anlegen */
{
    WORD k, f = NSB;
    for (k = 0; k < NSB; k++) { if (sbt[k].w == (BYTE)i) return sbt[k].v[bar]; if (!sbt[k].w && f == NSB) f = k; }
    if (!neu || f == NSB) { sb0[0] = sb0[1] = sb0[2] = 0; return sb0; }
    sbt[f].w = (BYTE)i;
    for (k = 0; k < 6; k++) (&sbt[f].v[0][0])[k] = 0;
    return sbt[f].v[bar];
}
static void sbrect(WORD i, WORD bar, RECT *r)     /* Fensterkoordinaten */
{
    short b = rand_(i), t = titel(i), wd = w_[i].r.r - w_[i].r.l, ht = w_[i].r.b - w_[i].r.t;
    short hv = (w_[i].stil & WS_VSCROLL) ? SBW : 0, hh = (w_[i].stil & WS_HSCROLL) ? th : 0;
    if (bar == SB_VERT) { r->l = wd - b - hv; r->r = wd - b; r->t = b + t; r->b = ht - b - hh; }
    else { r->l = b; r->r = wd - b - hv; r->t = ht - b - hh; r->b = ht - b; }
}
/* Teile entlang des Balkens: a = Pfeillaenge, Daumen von d0 bis d1 (relativ zum Anfang), len gesamt */
static void sbteile(WORD i, WORD bar, short *a, short *len, short *d0, short *d1)
{
    RECT r; short *s = sbw(i, bar, 0), spur, dl;
    sbrect(i, bar, &r);
    *len = bar == SB_VERT ? r.b - r.t : r.r - r.l;
    *a = bar == SB_VERT ? (th < SBW ? th : SBW) : SBW;
    spur = *len - 2 * *a;
    if (s[1] <= 0 || spur < 8) { *d0 = *d1 = 0; return; }
    dl = (short)((long)spur * (s[2] > 0 ? s[2] : 1) / (s[1] + (s[2] > 0 ? s[2] : 1)));
    if (dl < 8) dl = 8; if (dl > spur) dl = spur;
    *d0 = *a + (short)((long)(spur - dl) * s[0] / s[1]); *d1 = *d0 + dl;
}
static void pfeil(HDC d, RECT *r, int richt)      /* 0 hoch, 1 runter, 2 links, 3 rechts */
{
    POINT p[3]; short mx = (r->l + r->r) / 2, my = (r->t + r->b) / 2, k = (r->b - r->t) / 4, j = (r->r - r->l) / 4;
    if (k < 2) k = 2; if (j < 2) j = 2; if (k > j) k = j; else j = k;
    SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, r); SetPen(d, C_BLACK); Frame(d, r);
    if (richt < 2) { p[0].x = mx - 2 * j; p[1].x = mx + 2 * j; p[2].x = mx; p[0].y = p[1].y = my + (richt ? -k : k); p[2].y = my + (richt ? k : -k); }
    else { p[0].y = my - 2 * k; p[1].y = my + 2 * k; p[2].y = my; p[0].x = p[1].x = mx + (richt == 3 ? -j : j); p[2].x = mx + (richt == 3 ? j : -j); }
    SetBrush(d, C_BLACK, PAT_SOLID); Polygon(d, p, 3, 1);
}
static void sbmal(WORD i, WORD bar, HDC d)        /* d: Fenster-DC */
{
    RECT r, q; short a, len, d0, d1;
    sbrect(i, bar, &r);
    if (leer(&r)) return;
    sbteile(i, bar, &a, &len, &d0, &d1);
    q = r;
    if (bar == SB_VERT) { q.b = q.t + a; pfeil(d, &q, 0); q.t = r.b - a; q.b = r.b; pfeil(d, &q, 1); q.t = r.t + a - 1; q.b = r.b - a + 1; }
    else { q.r = q.l + a; pfeil(d, &q, 2); q.l = r.r - a; q.r = r.r; pfeil(d, &q, 3); q.l = r.l + a - 1; q.r = r.r - a + 1; }
    SetBrush(d, C_BLACK, PAT_GRAY50); SetBkColor(d, C_WHITE); FillRect(d, &q); SetPen(d, C_BLACK); Frame(d, &q);
    if (d1 > d0) {
        q = r;
        if (bar == SB_VERT) { q.t = r.t + d0; q.b = r.t + d1; } else { q.l = r.l + d0; q.r = r.l + d1; }
        SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &q); Frame(d, &q);
    }
}
static void sbneu(WORD i, WORD bar)
{
    HDC d; DWORD f = bar == SB_VERT ? WS_VSCROLL : WS_HSCROLL;
    if (!(w_[i].stil & f) || !sichtbar(i) || !(d = ndc(i, 0, 0))) return;
    sbmal(i, bar, d); fdc(d);
}
void RDEXPORT u_ScrollSet(HWND h, WORD bar, short pos, short max, short seite)
{
    WORD i = idx(h); short *s;
    if (!i || bar > SB_VERT) { bad(); return; }
    if (max < 0) max = 0;
    if (pos > max) pos = max; if (pos < 0) pos = 0;
    s = sbw(i, bar, 1);
    if (s == sb0) { SetLastError(RD_ERR_FULL); return; }
    if (s[0] == pos && s[1] == max && s[2] == seite) return;
    s[0] = pos; s[1] = max; s[2] = seite;
    sbneu(i, bar);
}
short RDEXPORT u_ScrollGet(HWND h, WORD bar) { WORD i = idx(h); if (!i || bar > SB_VERT) return (short)bad(); return sbw(i, bar, 0)[0]; }
static void sbklick(WORD i, WORD bar, short x, short y)   /* Bildschirmkoordinaten */
{
    RECT r, w; short a, len, d0, d1, p, code, alt, neu, start; MSG m; DWORD t0, warte = 400; HDC d;
    wrect(i, &w); sbrect(i, bar, &r); off(&r, w.l, w.t);
    sbteile(i, bar, &a, &len, &d0, &d1);
    p = bar == SB_VERT ? y - r.t : x - r.l;
    if (p >= d0 && p < d1 && d1 > d0) {           /* Daumen ziehen: Umriss folgt, Wert beim Loslassen */
        RECT q = r; short pos = sbw(i, bar, 0)[0], spur = len - 2 * a - (d1 - d0);
        if (!(d = gdc(0))) return;
        SetRop(d, ROP_XOR); SetPen(d, C_WHITE); start = p; alt = d0;
        if (bar == SB_VERT) { q.t = r.t + d0; q.b = r.t + d1; } else { q.l = r.l + d0; q.r = r.l + d1; }
        cap = (BYTE)i; Frame(d, &q);
        for (;;) {
            if (!GetMessage(&m)) { PostQuit(0); break; }
            if (m.msg == M_MOUSEMOVE || m.msg == M_BUTTONUP) {
                neu = d0 + (bar == SB_VERT ? m.pt.y - r.t : m.pt.x - r.l) - start;
                if (neu < a) neu = a; if (neu > a + spur) neu = a + spur;
                if (neu != alt || m.msg == M_BUTTONUP) {
                    Frame(d, &q);
                    if (bar == SB_VERT) { q.t = r.t + neu; q.b = q.t + d1 - d0; } else { q.l = r.l + neu; q.r = q.l + d1 - d0; }
                    alt = neu;
                    if (m.msg == M_BUTTONUP) break;
                    Frame(d, &q);
                }
            } else if (m.msg != M_BUTTONDOWN && m.msg != M_KEY) u_DispatchMessage(&m);
        }
        cap = 0; DCFree(d);
        if (spur > 0) pos = (short)(((long)(alt - a) * sbw(i, bar, 0)[1] + spur / 2) / spur);
        sendi(i, M_SCROLL, SB_THUMB, MAKELONG(pos, bar));
        return;
    }
    code = p < a ? SB_LINEUP : p >= len - a ? SB_LINEDOWN : p < d0 ? SB_PAGEUP : SB_PAGEDOWN;
    cap = (BYTE)i;
    sendi(i, M_SCROLL, code, MAKELONG(sbw(i, bar, 0)[0], bar));
    for (t0 = TickCount(); idx(hw(i)); ) {        /* gedrueckt halten: wiederholen */
        if (PeekMessage(&m, TRUE)) {
            if (m.msg == M_BUTTONUP) break;
            if (m.msg != M_MOUSEMOVE && m.msg != M_BUTTONDOWN && m.msg != M_KEY) u_DispatchMessage(&m);
            if (m.msg == M_QUIT) { PostQuit(0); break; }
            continue;
        }
        if (TickCount() - t0 >= warte) { t0 = TickCount(); warte = 80; sendi(i, M_SCROLL, code, MAKELONG(sbw(i, bar, 0)[0], bar)); }
        else TaskYield();
    }
    cap = 0;
}
void RDEXPORT u_ScrollWindow(HWND h, short dx, short dy, const RECT FAR *rr)
{
    WORD i = idx(h), n; RECT c, a, v[MAXCLIP], f; HDC d;
    if (!i) { bad(); return; }
    if (!sichtbar(i)) return;
    u_GetClientRect(h, &c);
    if (rr) { a = *rr; isect(&a, &c); } else a = c;
    if (leer(&a)) return;
    n = vis(i, 1, v); crect(i, &f);
    if (n != 1 || v[0].l != f.l || v[0].t != f.t || v[0].r != f.r || v[0].b != f.b ||
        dx >= a.r - a.l || -dx >= a.r - a.l || dy >= a.b - a.t || -dy >= a.b - a.t) { inval(i, &a, 1); return; }
    if (!(d = ndc(i, 1, 0))) { inval(i, &a, 1); return; }
    BitBlt(d, (short)(a.l + (dx > 0 ? dx : 0)), (short)(a.t + (dy > 0 ? dy : 0)), (short)(a.r - a.l - (dx < 0 ? -dx : dx)),
           (short)(a.b - a.t - (dy < 0 ? -dy : dy)), d, (short)(a.l - (dx < 0 ? dx : 0)), (short)(a.t - (dy < 0 ? dy : 0)), ROP_COPY);
    fdc(d);
    f = a;                                        /* frei gewordene Streifen neu */
    if (dy > 0) { f.b = f.t + dy; inval(i, &f, 1); } else if (dy < 0) { f.t = f.b + dy; inval(i, &f, 1); }
    f = a;
    if (dx > 0) { f.r = f.l + dx; inval(i, &f, 1); } else if (dx < 0) { f.l = f.r + dx; inval(i, &f, 1); }
}

/* ---- Menues (SPEC.md 5): Ressource RT_MENU, eigene Kopie je Menue ------------------- */
#define MAXM 12
#define MAXI 24
static struct { HMEM h; BYTE task; } mt[MAXM];
static BYTE FAR *mlock(HMENU m) { return m && m <= MAXM && mt[m - 1].h ? (BYTE FAR *)MemLock(mt[m - 1].h) : 0; }
static void munlock(HMENU m) { MemUnlock(mt[m - 1].h); }
/* eine Ebene ab p durchgehen; liefert den Eintrag mit id (auch tiefer), *end = hinter der Ebene */
static WORD FAR *mfind(BYTE FAR *p, WORD id, BYTE FAR **end)
{
    WORD f; WORD FAR *r = 0, FAR *q; BYTE FAR *e;
    do {
        f = *(WORD FAR *)p;
        if (!r && !(f & (MF_POPUP | MF_SEP)) && ((WORD FAR *)p)[1] == id) r = (WORD FAR *)p;
        p += 4; while (*p++) ;
        if (f & MF_POPUP) { q = mfind(p, id, &e); p = e; if (!r) r = q; }
    } while (!(f & MF_END));
    *end = p;
    return r;
}
static BYTE FAR *mnext(BYTE FAR *p)               /* naechster Eintrag derselben Ebene, 0 = Ende */
{
    WORD f = *(WORD FAR *)p; BYTE FAR *e;
    if (f & MF_END) return 0;
    p += 4; while (*p++) ;
    if (f & MF_POPUP) { mfind(p, 0xFFFF, &e); p = e; }
    return p;
}
#define MTEXT(p) ((LPCSTR)((p) + 4))
/* Menuetext: & unterstreicht den naechsten Buchstaben, \t trennt die Tastenangabe (rechtsbuendig).
   zeichnen = 0: nur messen. Liefert Breite Haupttext, *acc = Breite Tastenangabe */
static char mb[40];
static short mtxt(HDC d, short x, short y, LPCSTR s, short rechts, short *acc, int zeichnen)
{
    WORD n = 0, u = 0xFFFF; short w, ux;
    while (*s && *s != '\t' && n < sizeof mb - 1) { if (*s == '&' && s[1] && s[1] != '&' && u == 0xFFFF) { u = n; s++; continue; } if (*s == '&') s++; mb[n++] = *s++; }
    w = TextWidth(d, mb, n);
    if (zeichnen && n) {
        TextOut(d, x, y, mb, n);
        if (u != 0xFFFF) { ux = x + TextWidth(d, mb, u); Line(d, ux, y + th - 4, ux + TextWidth(d, mb + u, 1) - 1, y + th - 4); }
    }
    *acc = 0;
    if (*s == '\t') { s++; *acc = TextWidth(d, s, -1); if (zeichnen) TextOut(d, rechts - *acc, y, s, -1); }
    return w;
}
static char mkey(LPCSTR s)                        /* Buchstabe hinter &, gross */
{
    for (; *s && *s != '\t'; s++) if (*s == '&' && s[1] && s[1] != '&') return (char)(s[1] >= 'a' && s[1] <= 'z' ? s[1] - 32 : s[1]);
    return 0;
}
HMENU RDEXPORT u_MenuLoad(HINST inst, WORD id)
{
    WORD k; HMEM h;
    for (k = 0; k < MAXM && mt[k].h; k++) ;
    if (k == MAXM) { SetLastError(RD_ERR_FULL); return 0; }
    if (!(h = ResLoad(inst, RT_MENU, id))) return 0;
    mt[k].h = h; mt[k].task = (BYTE)TaskSelf();
    return k + 1;
}
void RDEXPORT u_MenuFree(HMENU m)
{
    WORD i;
    if (!m || m > MAXM || !mt[m - 1].h) { bad(); return; }
    ResFree(mt[m - 1].h); mt[m - 1].h = 0;
    for (i = 1; i <= MAXW; i++) if (w_[i].used && w_[i].menu == m) { w_[i].menu = 0; if (i == act) leiste(); }
}
void RDEXPORT u_MenuSet(HWND h, HMENU m)
{
    WORD i = top(idx(h));
    if (!i) { bad(); return; }
    w_[i].menu = m;
    if (i == act) leiste();
}
static WORD mflag(HMENU m, WORD id, WORD bit, BOOL an)
{
    BYTE FAR *p = mlock(m), FAR *e; WORD FAR *x; WORD alt = 0;
    if (!p) return bad();
    if ((x = mfind(p, id, &e)) != 0) { alt = *x & bit; if (an) *x |= bit; else *x &= ~bit; }
    else SetLastError(RD_ERR_NOTFOUND);
    munlock(m);
    return alt;
}
void RDEXPORT u_MenuCheck(HMENU m, WORD id, BOOL an) { mflag(m, id, MF_CHECK, an); }
void RDEXPORT u_MenuEnable(HMENU m, WORD id, BOOL an) { mflag(m, id, MF_GRAY, !an); }
WORD RDEXPORT u_MenuText(HMENU m, WORD id, LPCSTR s)   /* nur gleich lang oder kuerzer (Kopie hat feste Laenge) */
{
    BYTE FAR *p = mlock(m), FAR *e; WORD FAR *x; LPSTR t; WORD n = 0, k;
    if (!p) return bad();
    if ((x = mfind(p, id, &e)) != 0) {
        t = (LPSTR)(x + 2);
        for (k = 0; t[k]; k++) ;
        while (s && s[n] && n < k) { t[n] = s[n]; n++; }
        while (n < k) t[n++] = ' ';               /* Rest mit Leerzeichen: Laenge bleibt */
    } else SetLastError(RD_ERR_NOTFOUND);
    munlock(m);
    return x ? 1 : 0;
}
static void leistemenue(HDC d)                    /* Titel des aktiven Menues, Positionen merken */
{
    BYTE FAR *p; short x = 40;
    bn = 0; bx[0] = x;
    if (!act || !(p = mlock(w_[act].menu))) { if (act) TextOut(d, x, 1, w_[act].text, -1); return; }
    for (; p && bn < 13; p = mnext(p)) {
        short a;
        x += mtxt(d, (short)(x + 6), 1, MTEXT(p), 0, &a, 1) + 12;
        bx[++bn] = x;
    }
    munlock(w_[act].menu);
}
/* Klappmenue: Eintraege ab q (erste Ebene der Kinder), links oben x, y. Liefert ID oder 0.
   bar: Index in der Leiste, *weiter = neuer Index (Pfeil links/rechts, anderer Titel), -1 = Ende */
static WORD klapp(WORD i, BYTE FAR *q, short x, short y, int bar, int *weiter, int maus)
{
    BYTE FAR *it[MAXI]; short iy[MAXI + 1], wd = 0, aw = 0, k, n = 0, sel = -1, s2, ww, a; WORD id = 0, f;
    HDC d = gdc(0), bd = 0; HBMP bm = 0; RECT r, z; MSG m; BYTE FAR *p;
    *weiter = -1;
    if (!d) return 0;
    for (p = q; p && n < MAXI; p = mnext(p)) {
        it[n] = p; iy[n] = n ? iy[n - 1] + ((*(WORD FAR *)it[n - 1] & MF_SEP) ? th / 2 : th) : 1;
        if ((ww = mtxt(d, 0, 0, MTEXT(p), 0, &a, 0)) > wd) wd = ww;
        if (a > aw) aw = a;
        n++;
    }
    if (!n) { DCFree(d); return 0; }
    iy[n] = iy[n - 1] + ((*(WORD FAR *)it[n - 1] & MF_SEP) ? th / 2 : th);
    r.l = x; r.t = y; r.r = x + wd + (aw ? aw + 16 : 0) + 28; r.b = y + iy[n] + 2;
    if (r.r > sw) off(&r, sw - r.r, 0);
    if (r.b > sh) off(&r, 0, sh - r.b);
    if (r.l < 0) off(&r, (short)-r.l, 0);
    if ((bm = BmpCreate(r.r - r.l, r.b - r.t, 1)) != 0 && (bd = BmpDC(bm)) != 0)   /* Hintergrund retten */
        BitBlt(bd, 0, 0, r.r - r.l, r.b - r.t, d, r.l, r.t, ROP_COPY);
    SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &r); SetPen(d, C_BLACK); Frame(d, &r);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
    for (k = 0; k < n; k++) {
        f = *(WORD FAR *)it[k];
        if (f & MF_SEP) { SetBrush(d, C_BLACK, PAT_GRAY50); z.l = r.l + 2; z.r = r.r - 2; z.t = r.t + iy[k] + th / 4 - 1; z.b = z.t + 1; FillRect(d, &z); continue; }
        mtxt(d, (short)(r.l + 16), (short)(r.t + iy[k] + 1), MTEXT(it[k]), (short)(r.r - 12), &a, 1);
        if (f & MF_CHECK) TextOut(d, (short)(r.l + 4), (short)(r.t + iy[k] + 1), "\xFB", 1);
        if (f & MF_POPUP) TextOut(d, (short)(r.r - 10), (short)(r.t + iy[k] + 1), "\x10", 1);
        if (f & MF_GRAY) {                        /* grau: jedes zweite Pixel weiss */
            z.l = r.l + 1; z.r = r.r - 1; z.t = r.t + iy[k]; z.b = r.t + iy[k + 1];
            SetRop(d, ROP_OR); SetBkColor(d, C_BLACK); SetBrush(d, C_WHITE, PAT_GRAY50); FillRect(d, &z); SetRop(d, ROP_COPY);
        }
    }
#define WAEHLBAR(k) (!(*(WORD FAR *)it[k] & (MF_SEP | MF_GRAY | MF_POPUP)))
#define ZEILE(k) (z.l = r.l + 1, z.r = r.r - 1, z.t = r.t + iy[k], z.b = r.t + iy[(k) + 1], &z)
    if (!maus) for (sel = 0; sel < n && !WAEHLBAR(sel); sel++) ;
    if (sel >= n) sel = -1;
    if (sel >= 0) InvertRect(d, ZEILE(sel));
    cap = (BYTE)i;
    for (;;) {
        if (!GetMessage(&m)) { PostQuit(0); break; }
        if (m.msg == M_MOUSEMOVE || m.msg == M_BUTTONDOWN || m.msg == M_BUTTONUP) {
            s2 = -1;
            if (m.pt.x >= r.l && m.pt.x < r.r && m.pt.y >= r.t && m.pt.y < r.b)
                for (k = 0; k < n; k++) if (m.pt.y < r.t + iy[k + 1]) { if (WAEHLBAR(k)) s2 = k; break; }
            if (s2 != sel) { if (sel >= 0) InvertRect(d, ZEILE(sel)); sel = s2; if (sel >= 0) InvertRect(d, ZEILE(sel)); }
            if (bar >= 0 && m.pt.y < mh && m.msg != M_BUTTONUP) {   /* anderer Titel */
                for (k = 0; k < bn && !(m.pt.x >= bx[k] && m.pt.x < bx[k + 1]); k++) ;
                if (k < bn && k != bar) { *weiter = k; break; }
                if (k == bar && m.msg == M_BUTTONDOWN) break;     /* derselbe Titel nochmal: zu */
            }
            if (m.msg == M_BUTTONUP && sel >= 0) { id = ((WORD FAR *)it[sel])[1]; break; }
            if (m.msg == M_BUTTONDOWN && s2 < 0 && !(m.pt.x >= r.l && m.pt.x < r.r && m.pt.y >= r.t && m.pt.y < r.b) && !(bar >= 0 && m.pt.y < mh)) break;
        } else if (m.msg == M_KEY) {
            k = sel;
            if (m.p1 == K_ESC) break;
            if (m.p1 == K_ENTER) { if (sel >= 0) id = ((WORD FAR *)it[sel])[1]; break; }
            if (m.p1 < 0x100) {                   /* Buchstabe: Eintrag mit & */
                char c = (char)(m.p1 >= 'a' && m.p1 <= 'z' ? m.p1 - 32 : m.p1);
                for (k = 0; k < n && !(WAEHLBAR(k) && mkey(MTEXT(it[k])) == c); k++) ;
                if (k < n) { id = ((WORD FAR *)it[k])[1]; break; }
            }
            if (m.p1 == K_DOWN || m.p1 == K_UP) {
                for (s2 = 0; s2 < n; s2++) {
                    k = m.p1 == K_DOWN ? (k + 1) % n : (k <= 0 ? n - 1 : k - 1);
                    if (WAEHLBAR(k)) break;
                }
                if (s2 < n && k != sel) { if (sel >= 0) InvertRect(d, ZEILE(sel)); sel = k; InvertRect(d, ZEILE(sel)); }
            }
            if (bar >= 0 && bn && (m.p1 == K_LEFT || m.p1 == K_RIGHT)) { *weiter = m.p1 == K_RIGHT ? (bar + 1) % bn : (bar + bn - 1) % bn; break; }
        } else if (m.msg == M_PAINT) { WORD j = idx(m.w); if (j) w_[j].flags &= ~F_PAINTQ; }   /* spaeter */
        else if (m.msg == M_QUIT) { PostQuit(m.p1); break; }
        else u_DispatchMessage(&m);
    }
    cap = 0;
    if (bd) { BitBlt(d, r.l, r.t, r.r - r.l, r.b - r.t, bd, 0, 0, ROP_COPY); DCFree(bd); }
    if (bm) BmpFree(bm); else { DCFree(d); d = 0; aufdecken(&r); }
    if (d) DCFree(d);
    for (k = 1; k <= MAXW; k++)                   /* zurueckgestellte Zeichenwuensche */
        if (w_[k].used && !leer(&w_[k].inv) && !(w_[k].flags & F_PAINTQ)) { w_[k].flags |= F_PAINTQ; PostMessage(hw(k), M_PAINT, 0, 0); }
    return id;
}
static BYTE FAR *ntes(BYTE FAR *p, int k) { while (p && k--) p = mnext(p); return p; }
static void menulauf(WORD i, int bar, int maus)   /* Menueleiste des Fensters i, Titel bar */
{
    HMENU hm = w_[i].menu; BYTE FAR *p, FAR *q; HDC d; RECT r; int w; WORD id = 0;
    if (!hm || !bn) return;
    while (bar >= 0 && bar < bn) {
        if (!(p = mlock(hm))) return;
        sendi(i, M_MENUOPEN, (WORD)bar, hm);
        q = ntes(p, bar);
        r.l = bx[bar]; r.r = bx[bar + 1]; r.t = 0; r.b = mh - 1;
        if ((d = gdc(0)) != 0) { InvertRect(d, &r); DCFree(d); }
        id = 0; w = -1;
        if (q && (*(WORD FAR *)q & MF_POPUP)) { q += 4; while (*q++) ; id = klapp(i, q, r.l, mh - 1, bar, &w, maus); }
        else if (q) { id = ((WORD FAR *)q)[1]; }  /* Titel ohne Klappmenue: gleich Befehl */
        if ((d = gdc(0)) != 0) { InvertRect(d, &r); DCFree(d); }
        munlock(hm);
        bar = w; maus = 0;
    }
    if (id) PostMessage(hw(i), M_COMMAND, id, 0);
}
WORD RDEXPORT u_MenuPopup(HWND h, HMENU hm, WORD sub, short x, short y)   /* x, y: Client von h */
{
    WORD i = idx(h), id = 0; BYTE FAR *p, FAR *q; POINT pt; int w;
    if (!i || !(p = mlock(hm))) return bad();
    pt.x = x; pt.y = y; u_ClientToScreen(h, &pt);
    sendi(i, M_MENUOPEN, sub, hm);
    q = ntes(p, sub);
    if (q && (*(WORD FAR *)q & MF_POPUP)) { q += 4; while (*q++) ; id = klapp(i, q, pt.x, pt.y, -1, &w, 1); }
    munlock(hm);
    return id;
}
WORD RDEXPORT u_KeyState(WORD taste)              /* 0: KS_* der Umschalttasten */
{
    BYTE b = *(BYTE FAR *)0x00400017UL;
    WORD ks = (b & 3 ? KS_SHIFT : 0) | (b & (KS_CTRL | KS_ALT));
    if (!taste) return ks;
    if (taste == KS_SHIFT || taste == KS_CTRL || taste == KS_ALT) return ks & taste;
    return 0;
}

static WORD my[16][12];
WORD RDEXPORT u_MsgRegister(LPCSTR name)          /* ab 0xC000, gleicher Name = gleiche Nummer */
{
    WORD i, j; char *s;
    if (!name || !*name) { SetLastError(RD_ERR_FORMAT); return 0; }
    for (i = 0; i < 16 && my[i][0]; i++) {
        s = (char *)my[i];
        for (j = 0; j < 23 && name[j] && (s[j] | 0x20) == (name[j] | 0x20); j++) ;
        if (j == 23 || (!name[j] && !s[j])) return 0xC000 + i;
    }
    if (i == 16) { SetLastError(RD_ERR_FULL); return 0; }
    scpy((char *)my[i], name, 24);
    return 0xC000 + i;
}

/* ---- Kern-Haken ----------------------------------------------------------------- */
/* Art 0: Task des Fensters. Art 2: Eingabe zuordnen (p2 = MSG FAR *), 0xFFFF = wie ohne RDUSER.
   Art 3: Task w ist beendet. */
/* ---- Mauszeiger: Form je Programm (Bit 7 = beschaeftigt), gilt ueber dessen Fenstern ------- */
static const BYTE zbild[4][66] = {   /* CUR_WAIT, CUR_TEXT, CUR_CROSS, CUR_HAND: Hotspot x, y, UND, XOR */
    { 7, 7, 0x00,0x03,0x00,0x03,0x80,0x07,0x80,0x07,0xC0,0x0F,0xE0,0x1F,0xF0,0x3F,0xF8,0x7F,0xF8,0x7F,0xF0,0x3F,0xE0,0x1F,0xC0,0x0F,0x80,0x07,0x80,0x07,0x00,0x03,0x00,0x03,0x00,0x00,0x7F,0xF8,0x3F,0xF0,0x3F,0xF0,0x18,0x60,0x0C,0xC0,0x07,0x80,0x03,0x00,0x03,0x00,0x06,0x80,0x0F,0xC0,0x1C,0xE0,0x30,0x30,0x20,0x10,0x7F,0xF8,0x00,0x00 },   /* WAIT */
    { 4, 8, 0x11,0xFF,0x01,0xFF,0x83,0xFF,0xEF,0xFF,0xEF,0xFF,0xEF,0xFF,0xEF,0xFF,0xEF,0xFF,0xEF,0xFF,0xEF,0xFF,0xEF,0xFF,0xEF,0xFF,0xEF,0xFF,0x83,0xFF,0x01,0xFF,0x11,0xFF,0x00,0x00,0x6C,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x6C,0x00,0x00,0x00 },   /* TEXT */
    { 7, 7, 0xFC,0x7F,0xFC,0x7F,0xFC,0x7F,0xFC,0x7F,0xFC,0x7F,0xFC,0x7F,0x00,0x01,0x00,0x01,0x00,0x01,0xFC,0x7F,0xFC,0x7F,0xFC,0x7F,0xFC,0x7F,0xFC,0x7F,0xFC,0x7F,0xFF,0xFF,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x7F,0xFC,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00 },   /* CROSS */
    { 5, 0, 0xF3,0xFF,0xE1,0xFF,0xE1,0xFF,0xE0,0x7F,0xE0,0x0F,0xE0,0x03,0x80,0x01,0x00,0x01,0x00,0x01,0x80,0x01,0x80,0x03,0xC0,0x03,0xC0,0x07,0xE0,0x07,0xE0,0x07,0xE0,0x07,0x00,0x00,0x0C,0x00,0x0C,0x00,0x0C,0x00,0x0D,0x80,0x0D,0xB0,0x0D,0xB4,0x6F,0xFC,0x7F,0xFC,0x3F,0xFC,0x3F,0xF8,0x1F,0xF8,0x1F,0xF0,0x0F,0xF0,0x0F,0xF0,0x00,0x00 },   /* HAND */
};
static const BYTE FAR *pfeil0;                   /* Pfeil aus RDGDI */
static BYTE tz[16], zakt, zeig[66], zeigt;        /* zeig: eigene Form (Ressource RT_CURSOR), zeigt = Programm */
static short mpx, mpy;
static BYTE zwo(short x, short y)                 /* Form fuer den Punkt */
{
    WORD z = cap ? cap : y < mh ? 0 : bei(0, x, y); BYTE f; RECT r;
    if (!z || w_[z].task >= 16) return 0;
    f = tz[w_[z].task]; crect(z, &r);
    if (f & 0x80) return CUR_WAIT;
    return x < r.l || x >= r.r || y < r.t || y >= r.b ? 0 : f;
}
void cli_(void);
#pragma aux cli_ = "cli"
void sti_(void);
#pragma aux sti_ = "sti"
static int zform(BYTE k)                          /* 1 = geaendert */
{
    const BYTE FAR *b;
    if (k == zakt) return 0;
    zakt = k;
    cli_();
    if (!k) { gk->hx = gk->hy = 0; gk->zb = pfeil0; }
    else { b = k == 5 ? zeig : zbild[k - 1]; gk->hx = b[0]; gk->hy = b[1]; gk->zb = b + 2; }
    sti_();
    return 1;
}
static void zjetzt(void)                          /* aus einem API-Aufruf: gleich neu zeigen */
{
    HDC d;
    if (zform(zwo(mpx, mpy)) && (d = gdc(0)) != 0) { GetPixel(d, mpx, mpy); DCFree(d); }
}
void RDEXPORT u_CursorSet(HINST inst, WORD id)
{
    WORD t = TaskSelf(), k; BYTE f = 0; HMEM r; BYTE FAR *p;
    if (t >= 16) return;
    if (inst) {
        if (!(r = ResLoad(inst, RT_CURSOR, id))) return;
        if ((p = (BYTE FAR *)MemLock(r)) != 0 && MemSize(r) >= 66) {
            if (zakt == 5) zform(0);
            for (k = 0; k < 66; k++) zeig[k] = p[k];
            zeigt = (BYTE)t; f = 5;
        } else SetLastError(RD_ERR_FORMAT);
        if (p) MemUnlock(r);
        ResFree(r);
        if (!f) return;
    } else f = id <= CUR_HAND ? (BYTE)id : 0;
    tz[t] = (BYTE)((tz[t] & 0x80) | f);
    zakt = 0xFF; zjetzt();
}
void RDEXPORT u_CursorBusy(BOOL an)
{
    WORD t = TaskSelf();
    if (t >= 16) return;
    if (an) tz[t] |= 0x80; else tz[t] &= 0x7F;
    zjetzt();
}
void RDEXPORT u_CursorPos(POINT FAR *p) { if (p) { p->x = mpx; p->y = mpy; } }

DWORD RDEXPORT u_hook(WORD art, HWND h, WORD msg, WORD p1, DWORD p2)
{
    WORD i, j, n; MSG FAR *m; RECT r; short x, y, b, t;
    (void)p1;
    if (art == 0) return (i = idx(h)) != 0 ? w_[i].task : 0;
    if (art == 3) {
        for (n = 0, i = 1; i <= MAXW; i++)
            if (w_[i].used && w_[i].task == (BYTE)h && !w_[i].parent) { wrect(i, &r); weg(i, 0); aufdecken(&r); n = 1; }
        for (i = 1; i <= MAXW; i++) if (w_[i].used && w_[i].task == (BYTE)h) weg(i, 0);   /* Kinder in fremden Fenstern */
        for (j = 0; j < MAXCL; j++) if (kl[j].task == (BYTE)h && !kl[j].intern) kl[j].name[0] = 0;
        for (j = 0; j < MAXM; j++) if (mt[j].h && mt[j].task == (BYTE)h) mt[j].h = 0;   /* Speicher gibt der Kern frei */
        if (n && !w_[act].used) neuaktiv();
        if (h < 16) { tz[h] = 0; if (zeigt == (BYTE)h) zeigt = 0; }
        zjetzt();
        return 0;
    }
    if (art != 2) return 0;
    m = (MSG FAR *)p2;
    if (!w_[0].child) return 0xFFFF;              /* noch keine Fenster: Programm bekommt alles */
    if (msg == M_KEY || msg == M_KEYUP || msg == M_CHAR) {
        i = foc ? foc : act;
        if (!i) return 0;
        m->w = hw(i); return w_[i].task;
    }
    x = m->pt.x; y = m->pt.y;
    mpx = x; mpy = y; zform(zwo(x, y));           /* Form wechselt mit der naechsten Bewegung */
    i = cap;
    if (!i) {
        if (y < mh) {                             /* Menueleiste: gehoert dem aktiven Fenster */
            if (msg != M_BUTTONDOWN || !act) return 0;
            m->w = hw(act); m->msg = M_NCBUTTONDOWN; m->p1 = 1; return w_[act].task;
        }
        if (!(i = bei(0, x, y))) return 0;        /* Schreibtisch: spaeter die Shell */
        crect(i, &r);
        if (x < r.l || x >= r.r || y < r.t || y >= r.b) {
            if (msg != M_BUTTONDOWN) return 0;
            wrect(i, &r); b = rand_(i); t = titel(i);
            m->msg = M_NCBUTTONDOWN; m->p1 = 0;
            if (w_[i].stil & (WS_VSCROLL | WS_HSCROLL)) {
                RECT q; WORD bar;
                for (bar = 0; bar < 2; bar++) {
                    if (!(w_[i].stil & (bar == SB_VERT ? WS_VSCROLL : WS_HSCROLL))) continue;
                    sbrect(i, bar, &q); off(&q, r.l, r.t);
                    if (x >= q.l && x < q.r && y >= q.t && y < q.b) m->p1 = bar == SB_VERT ? HT_VSCROLL : HT_HSCROLL;
                }
            }
            if (t && y < r.t + b + t) m->p1 = (w_[i].stil & WS_CLOSE) && x < r.l + b + t ? HT_CLOSE : HT_TITLE;
            m->w = hw(i); return w_[i].task;
        }
        if (w_[i].dis) return 0;
    }
    org(i, &x, &y);
    m->p2 = MAKELONG(m->pt.x - x, m->pt.y - y);
    m->w = hw(i);
    return w_[i].task;
}
void RDEXPORT u_init(void FAR *d, void FAR *k, void FAR *g)
{
    dcs = (DCX FAR *)d; gk = (GK FAR *)k; pfeil0 = gk->zb; gdc = (HDC (FAR __pascal *)(HWND))g;
    w_[0].used = 1; w_[0].vis = 1;
}
static void RDEXPORT u_ende(void) { }
static int schirm_(void)                           /* erst beim ersten Fenster: Grafik an, Schreibtisch */
{
    HDC d;
    if (sw) return 1;
    if (!(d = gdc(0))) return 0;
    sw = GetCaps(d, CAPS_W); sh = GetCaps(d, CAPS_H); th = GetCaps(d, CAPS_TEXTH) + 3; mh = th + 1;
    DCFree(d);
    desk(); leiste();
    return 1;
}

#define U(n) (FARPROC)u_##n
/* Exporte: 1 = Ende, 2 + (API - 0x200) bis 0x241, dann 0x44 Kern-Haken, 0x45 Start, 0x46 SendMessage,
   0x47 DispatchMessage, 0x48 MsgRegister */
static struct { WORD n; FARPROC f[72]; } ext = { 72, {
    U(ende), U(RegisterClass), U(CreateWindow), U(DestroyWindow), U(ShowWindow), U(MoveWindow), U(SetWindowText), U(GetWindowText),
    U(GetClientRect), U(GetWindowRect), U(InvalidateRect), U(ValidateRect), U(UpdateWindow), U(BeginPaint), U(EndPaint), U(GetDC),
    U(ReleaseDC), U(SetFocus), U(GetFocus), U(SetActive), U(GetActive), U(DefWindowProc), U(SetWindowData), U(GetWindowData),
    U(GetParent), U(WindowFromPoint), U(ClientToScreen), U(ScreenToClient), U(IsWindow), U(EnableWindow), U(ScrollWindow), 0,
    0, U(ScrollSet), U(ScrollGet), 0, 0, 0, 0, 0,
    0, U(CaptureMouse), U(ReleaseMouse), U(CursorSet), U(CursorBusy), U(CursorPos), 0, 0,
    0, U(MenuLoad), U(MenuSet), U(MenuCheck), U(MenuEnable), U(MenuFree), U(MenuPopup), U(MenuText),
    0, 0, 0, 0, 0, 0, 0, 0,
    0, U(KeyState), 0, U(hook), (FARPROC)u_init, U(SendMessage), U(DispatchMessage), U(MsgRegister),
} };
void FAR * RDAPI RdModInit(void) { return &ext; }
