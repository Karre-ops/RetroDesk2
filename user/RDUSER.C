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

#define MAXW   48
#define MAXCL  16
#define NSLOT  6                                  /* gleichzeitig offene Fenster-DCs */
#define F_PAINTQ 0x01                             /* M_PAINT eingestellt */
#define F_ERASE  0x02
#define F_INTERN 0x04                             /* Fensterfunktion in RDUSER */
#define M_NCBUTTONDOWN 0x00A1                     /* p1 = HT_*, p2 = Bildschirm */
#define HT_TITLE 2
#define HT_CLOSE 3

typedef struct {
    BYTE  gen, used, vis, cls, task, flags, parent, child, next, dis;
    WORD  id; HINST inst;
    DWORD stil;
    RECT  r, inv;
    WNDPROC proc;
    HMENU menu;
    DWORD data[4];
    char  text[40];
} WND;
typedef struct { char name[16]; WNDPROC proc; WORD stil, inst; COLOR hinter; BYTE task, intern; } KLASSE;

static WND w_[MAXW + 1];                          /* 0 = Wurzel (Schreibtisch) */
static KLASSE kl[MAXCL];
static BYTE act, foc, cap;                        /* Nummern, 0 = keines */
static short sw, sh, mh, th;                      /* Schirm, Menueleiste, Titelzeile */
static DCX FAR *dcs; static BYTE FAR *kclip;
static HDC (FAR __pascal *gdc)(HWND);
static struct { HDC dc; WORD n; RECT r[MAXCLIP]; } sl[NSLOT];
static RECT lim; static BYTE limon;               /* Aufdecken: nur hier zeichnen */

void RDEXPORT u_ShowWindow(HWND h, WORD art);
HWND RDEXPORT u_SetActive(HWND h);
static void desk(void);
static void leiste(void);
static int schirm_(void);

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
    *kclip = 1;
    return h;
}
static void fdc(HDC h)
{
    WORD s;
    for (s = 0; s < NSLOT; s++) if (sl[s].dc == h) { sl[s].dc = 0; dcs[h - 1].nclip = 0; *kclip = 1; }
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
    if (act) TextOut(d, 40, 1, w_[act].text, -1);
    DCFree(d);
}
static void rahmen(WORD i)
{
    HDC d; RECT r, c; short b = rand_(i), t = titel(i), wd;
    if (!b && !t) return;
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
        if (p1 == HT_CLOSE) PostMessage(h, M_CLOSE, 0, 0);
        else if (p1 == HT_TITLE) ziehen(i, (short)LOWORD(p2), (short)HIWORD(p2));
        return 0;
    case M_CLOSE: u_DestroyWindow(h); return 0;
    case M_PAINT: u_BeginPaint(h, &ps); u_EndPaint(h, &ps); return 0;
    case M_KEY:
        if (p1 == 0x100 + 0x3E && (HIWORD(p2) & KS_ALT)) PostMessage(hw(top(i)), M_CLOSE, 0, 0);   /* Alt+F4 */
        return 0;
    }
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
        if (n && !w_[act].used) neuaktiv();
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
    dcs = (DCX FAR *)d; kclip = (BYTE FAR *)k; gdc = (HDC (FAR __pascal *)(HWND))g;
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
    U(GetParent), U(WindowFromPoint), U(ClientToScreen), U(ScreenToClient), U(IsWindow), U(EnableWindow), 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, U(CaptureMouse), U(ReleaseMouse), 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, U(hook), (FARPROC)u_init, U(SendMessage), U(DispatchMessage), U(MsgRegister),
} };
void FAR * RDAPI RdModInit(void) { return &ext; }
