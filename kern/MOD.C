/* MOD.C - Module (.MOD, .DRV): laden, ueber Sprungstuecke aufrufen, verwerfen, nachladen
 *
 * Ein Modul ist eine MZ-EXE mit RD2-Anhang "RD2M". Sein Einsprung (RD2MOD.OBJ)
 * wird mit AX=5232h, ES:BX=API gerufen und liefert in DX:AX einen Zeiger auf
 *   WORD anzahl; FARPROC f[anzahl];      (Export 1 = f[0])
 * ModProc liefert nie die echte Adresse, sondern ein Sprungstueck im Kern
 * (8 Bytes: MOV AX,nr / JMP FAR modgate). modgate (KASM.ASM) ruft mod_enter:
 * Modul laden falls verworfen (mit Diskettenabfrage), sperren, Ruecksprung
 * merken; nach der Rueckkehr mod_leave: entsperren, zurueck zum Aufrufer.
 * Ein gesperrtes Modul wird nie verworfen. Verwerfbar sind nur Module mit
 * Flag PF_DISCARD; ihr Zustand (Daten) ist nach dem Nachladen neu.
 *
 * Mit XMS (HIMEM): beim ersten Laden kommt das unberuehrte Bild (vor der Initialisierung)
 * samt Verschiebetabelle in einen XMS-Block. Nachladen holt es von dort, rechnet die
 * Segmente auf die neue Adresse um und braucht keine Diskette. RD2.INI [System] XMS=KB
 * (Vorgabe 256, 0 = aus). Reicht der Block nicht, wird wie ohne XMS nachgeladen. */
#include "KERN.H"

#define MAXMOD 12
#define MAXEXP 250                                /* Nummer passt in ein Byte des Sprungstuecks */
#define MAXTHUNK 160
#define PF_DISCARD 0x0080

typedef struct {
    BYTE  used, loaded, drive;
    WORD  refs, lock, flags, base, paras;
    DWORD serial, stamp;
    char  label[12];
    char  path[66];
    char  name[16];
    WORD  cs, ip;                                 /* Einsprung relativ zum Bild */
    DWORD xo; WORD xp, xr, xseg;                  /* XMS: Bild ab xo, xp Paragraphen, xr Verschiebungen, geladen fuer xseg */
    WORD  nexp;
    void __far * __far *tab;                      /* Exporttabelle im Modul selbst (nach jedem Laden neu) */
} MODUL;
static MODUL md[MAXMOD + 1];                      /* HMOD 1..MAXMOD */
static struct { BYTE mod, nr; } th[MAXTHUNK];
static BYTE thcode[MAXTHUNK][8];
static WORD nth;
extern DWORD k_stamp;

/* ---- XMS-Ablage ---- */
static WORD xh; static DWORD xtop, xsize;
#pragma pack(push, 1)
static struct { DWORD len; WORD sh; DWORD so; WORD dh; DWORD dofs; } xmv;
#pragma pack(pop)
static WORD xrb[64];                              /* 32 Verschiebungen (Offset, Segment) */
static int xmove(WORD sh, DWORD so, WORD dh, DWORD dofs, DWORD len)
{
    xmv.len = (len + 1) & ~1UL; xmv.sh = sh; xmv.so = so; xmv.dh = dh; xmv.dofs = dofs;
    return rd_xms(0x0B00, 0, (WORD)&xmv) == 1;
}
#define KONV(seg) (((DWORD)(seg)) << 16)          /* Handle 0: Adresse Segment:0 */
void mod_xinit(void)
{
    WORD kb = (WORD)ProfileGetInt("RD2.INI", "System", "XMS", 256), fr;
    kr.ax = 0x4300; rd_int(0x2F, &kr);
    if (!kb || (kr.ax & 0xFF) != 0x80) return;
    kr.ax = 0x4310; rd_int(0x2F, &kr);
    k_xmsent = MK_FP(kr.es, kr.bx);
    fr = rd_xms(0x0800, 0, 0);                    /* groesster freier Block in KB */
    if (fr < kb) kb = fr;
    if (kb < 16 || rd_xms(0x0900, kb, 0) != 1) { k_xmsent = 0; return; }
    xh = k_xdx; xsize = (DWORD)kb << 10;
    k_hex("RD: XMS-Ablage", xsize, 1);
}
void mod_xdone(void)
{
    if (xh) { rd_xms(0x0A00, xh, 0); xh = 0; }
}
/* gerade geladenes, noch nicht initialisiertes Bild ablegen */
static void xsave(MODUL *x, WORD seg, WORD paras)
{
    WORD imgp = paras - k_mz.minalloc, n, i, f; DWORD need = ((DWORD)imgp << 4) + 4UL * k_mz.crlc, o = xtop;
    if (!xh || x->xp || xtop + need > xsize) return;
    if (!xmove(0, KONV(seg), xh, o, (DWORD)imgp << 4)) return;
    o += (DWORD)imgp << 4;
    kr.ax = 0x3D00; kr.dx = (WORD)x->path; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    if (CARRY) return;
    f = kr.ax;
    kr.ax = 0x4200; kr.bx = f; kr.cx = 0; kr.dx = k_mz.lfarlc; rd_int(0x21, &kr);
    for (i = 0; i < k_mz.crlc && !CARRY; i += n) {
        n = k_mz.crlc - i > 32 ? 32 : k_mz.crlc - i;
        kr.ax = 0x3F00; kr.bx = f; kr.cx = n * 4; kr.dx = (WORD)xrb; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
        if (CARRY || kr.ax != n * 4 || !xmove(0, (DWORD)(void __far *)xrb, xh, o, n * 4)) break;
        o += n * 4;
    }
    kr.ax = 0x3E00; kr.bx = f; rd_int(0x21, &kr);
    if (i < k_mz.crlc) return;
    x->xo = xtop; x->xp = imgp; x->xr = k_mz.crlc; x->xseg = seg;
    xtop = (o + 15) & ~15UL;
}
/* Bild aus XMS holen und umrechnen. 0 = ok */
static WORD xload(MODUL *x, WORD *seg)
{
    WORD i, n, k, d; DWORD o = x->xo + ((DWORD)x->xp << 4);
    if (!(*seg = mem_get(x->paras, B_HIGH, 0))) return RD_ERR_NOMEM;
    if (!xmove(xh, x->xo, 0, KONV(*seg), (DWORD)x->xp << 4)) { mem_put(*seg); return RD_ERR_FORMAT; }
    d = *seg - x->xseg;
    for (i = 0; i < x->xr; i += n, o += n * 4) {
        n = x->xr - i > 32 ? 32 : x->xr - i;
        if (!xmove(xh, o, 0, (DWORD)(void __far *)xrb, n * 4)) { mem_put(*seg); return RD_ERR_FORMAT; }
        for (k = 0; k < n; k++) *(WORD __far *)MK_FP(*seg + xrb[2 * k + 1], xrb[2 * k]) += d;
    }
    return 0;
}

/* Bild laden (Datei oder XMS) und initialisieren. 0 = ok */
static WORD load2(WORD m)
{
    MODUL *x = &md[m]; WORD i, e; static WORD seg, paras; DWORD tab;
    WORD __far *t;
    if (x->xp) { if ((e = xload(x, &seg)) != 0) return e; paras = x->paras; k_out("RD: Modul aus XMS", 0); }
    else {
        if ((e = k_imgload(x->path, "MD", &seg, &paras)) != 0) return e;
        x->cs = k_mz.cs; x->ip = k_mz.ip; x->flags = k_rb.flags; x->paras = paras;
        for (i = 0; i < 15 && k_rb.name[i]; i++) x->name[i] = k_rb.name[i];
        x->name[i] = 0;
        if (x->flags & PF_DISCARD) xsave(x, seg, paras);
    }
    tab = rd_enter(seg + x->cs, x->ip, (void __far *)rd_api, m, 0, (char __far *)x->path);
    t = (WORD __far *)MK_FP((WORD)(tab >> 16), (WORD)tab);
    if (!tab || t[0] > MAXEXP) { mem_put(seg); return RD_ERR_FORMAT; }
    x->nexp = t[0];
    x->tab = (void __far * __far *)(t + 1);       /* 0 = Export fehlt */
    k_hex(x->path, seg, 0);                       /* nur Protokoll: Ladeadresse */
    x->base = seg; x->loaded = 1; x->stamp = ++k_stamp;
    mem_mod(seg, m);
    x->lock = (x->flags & PF_DISCARD) ? 0 : 0x8000;           /* 0x8000: nie verwerfen */
    return 0;
}

/* Laden darf die Reserve nutzen; danach waechst sie auf das groesste verwerfbare Modul */
static WORD load(WORD m)
{
    WORD r = mem_resv, e;
    mem_resv = 0;
    if (!(e = load2(m)) && (md[m].flags & PF_DISCARD) && md[m].paras > r) r = md[m].paras;
    mem_resv = r;
    return e;
}

static DWORD mstamp(WORD m)
{
    if (m < 1 || m > MAXMOD || !md[m].loaded || md[m].lock) return 0xFFFFFFFFUL;
    return md[m].stamp;
}
static int mdiscard(WORD m)
{
    if (m < 1 || m > MAXMOD || !md[m].loaded || md[m].lock) return 0;
    md[m].loaded = 0;
    k_txt("RD: Modul verworfen: ", 0); k_out(md[m].name, 0);
    return 1;
}

/* Verworfenes Modul zurueckholen: richtige Diskette verlangen, neu laden */
static WORD reload(WORD m)
{
    MODUL *x = &md[m]; WORD e;
    if (x->xp) { if (!(e = load(m))) return 0; if (e == RD_ERR_NOMEM) return e; x->xp = 0; }   /* XMS kaputt: von Diskette */
    for (;;) {
        if (x->drive && DiskRequest(x->drive, x->serial, x->label)) return RD_ERR_NODISK;
        if (!(e = load(m))) { k_out("RD: Modul nachgeladen", 0); return 0; }
        if (e == RD_ERR_NOMEM) return e;
        if (!x->drive) return e;
        /* Diskette passte, Laden ging trotzdem schief: ausdruecklich fragen, nie still im Kreis */
        if (!k_askdisk(x->drive - 1, x->label)) return RD_ERR_NODISK;
    }
}

HMOD RDK ModLoad(LPCSTR name)
{
    WORD m, n = 0, i, e; LPCSTR s; MODUL *x;
    for (s = name; *s; s++) if (*s == ':' || *s == '\\') break;
    for (m = 1; m <= MAXMOD && !md[m].used; m++) ;
    /* schon geladen? (gleicher Pfad) */
    for (i = 1; i <= MAXMOD; i++) if (md[i].used) {
        const char *a = md[i].path; LPCSTR b = name; WORD j = 0;
        if (!*s) { const char *h = k_home; while (*h && *a == *h) a++, h++; if (*h) continue; }
        while (*a && *b && (*a | 0x20) == (*b | 0x20)) a++, b++, j++;
        if (!*a && !*b) { md[i].refs++; return i; }
    }
    for (m = 1; m <= MAXMOD && md[m].used; m++) ;
    if (m > MAXMOD) return FAIL(RD_ERR_FULL);
    x = &md[m];
    if (!*s) { const char *h = k_home; while (*h && n < 50) x->path[n++] = *h++; }
    while (*name && n < 65) x->path[n++] = *name++;
    x->path[n] = 0;
    x->used = 1; x->refs = 1; x->lock = 0; x->loaded = 0; x->drive = 0; x->xp = 0;
    if (x->path[1] == ':') {
        WORD d = (WORD)((x->path[0] | 0x20) - 'a');
        if (d < 2 && !k_media(d, &x->serial, x->label)) x->drive = (BYTE)(d + 1);
    }
    if ((e = load(m)) != 0) { x->used = 0; return FAIL(e); }
    return m;
}
/* Modul wirklich entfernen, wenn niemand es mehr braucht und kein Aufruf darin laeuft */
static void unload(WORD m)
{
    MODUL *x = &md[m]; WORD i;
    if (x->refs || (x->lock & 0x7FFF)) return;
    if (x->loaded) mem_put(x->base);
    x->used = x->loaded = 0;
    for (i = 0; i < nth; i++) if (th[i].mod == m) th[i].mod = 0;   /* alte Sprungstuecke ungueltig */
}
void RDK ModFree(HMOD m)
{
    if (m < 1 || m > MAXMOD || !md[m].used || !md[m].refs) return;
    md[m].refs--;
    unload(m);
}
FARPROC RDK ModProc(HMOD m, WORD nr)
{
    WORD i; BYTE *c; WORD __far *g = (WORD __far *)&rd_gateptr;
    if (m < 1 || m > MAXMOD || !md[m].used || nr < 1) return (FARPROC)FAIL(RD_ERR_HANDLE);
    if (md[m].loaded && nr > md[m].nexp) return (FARPROC)FAIL(RD_ERR_NOTFOUND);
    for (i = 0; i < nth; i++) if (th[i].mod == m && th[i].nr == nr) break;
    if (i == nth) for (i = 0; i < nth && th[i].mod; i++) ;          /* freies Sprungstueck? */
    if (i == nth) {
        if (nth >= MAXTHUNK) return (FARPROC)FAIL(RD_ERR_FULL);
        nth++;
    }
    if (th[i].mod != m || th[i].nr != nr) {
        th[i].mod = (BYTE)m; th[i].nr = (BYTE)nr;
        c = thcode[i];
        c[0] = 0xB8; c[1] = (BYTE)i; c[2] = (BYTE)(i >> 8);  /* MOV AX, i */
        c[3] = 0xEA;                                        /* JMP FAR modgate */
        c[4] = (BYTE)g[0]; c[5] = (BYTE)(g[0] >> 8); c[6] = (BYTE)g[1]; c[7] = (BYTE)(g[1] >> 8);
    }
    return (FARPROC)(void __far *)thcode[i];
}

/* aus modgate: Ziel liefern, Ruecksprung merken. Laeuft mit dem Stack des Aufrufers. */
DWORD __cdecl mod_enter(WORD idx, WORD ip, WORD cs)
{
    TASK *t = &k_task[k_cur]; WORD m = th[idx].mod, nr = th[idx].nr; MODUL *x = &md[m];
    /* ungueltig, Stapel voll, nicht nachladbar oder Funktion fehlt (Grund: RD /D, Modulliste) */
    if (!m || !x->used || t->rsp >= 12 || (!x->loaded && reload(m)) || nr > x->nexp || !x->tab[nr - 1]) {
        k_out("RD: Modulaufruf scheitert", 0);
        TaskKill(k_cur, 0xFFFE);
    }
    t->rs[t->rsp].ip = ip; t->rs[t->rsp].cs = cs; t->rs[t->rsp].mod = m; t->rsp++;
    x->lock++; x->stamp = ++k_stamp;
    return (DWORD)x->tab[nr - 1];
}
DWORD __cdecl mod_leave(void)
{
    TASK *t = &k_task[k_cur]; WORD m;
    t->rsp--;
    m = t->rs[t->rsp].mod;
    md[m].lock--;
    if (!md[m].refs) unload(m);                   /* ModFree waehrend des Aufrufs */
    return ((DWORD)t->rs[t->rsp].cs << 16) | t->rs[t->rsp].ip;
}
void mod_unwind(TASK *t)
{
    WORD m;
    while (t->rsp) { t->rsp--; m = t->rs[t->rsp].mod; md[m].lock--; if (!md[m].refs) unload(m); }
}

/* Systemmodul (RDGDI, spaeter RDUSER ...): Export k+2 wird API base+k, extra: Paare
   (Export, API), mit 0 beendet. Export 1 ist der Abschluss beim Beenden von RD.
   Nicht verwerfbare Systemmodule bleiben bis zum Ende fest im Speicher: ihre Exporte kommen
   direkt in die API-Tabelle (ohne Sprungstueck, spart je Aufruf mod_enter/mod_leave). */
static FARPROC sysp(HMOD m, WORD nr) { return (md[m].flags & PF_DISCARD) ? ModProc(m, nr) : (FARPROC)md[m].tab[nr - 1]; }
HMOD k_sysmod(const char *name, WORD base, WORD n, const WORD *extra)
{
    HMOD m = ModLoad(name); WORD k;
    if (!m) return 0;
    for (k = 0; k < n && k + 2 <= md[m].nexp; k++)
        if (md[m].tab[k + 1]) rd_api[base + k] = sysp(m, k + 2);
    for (; extra && extra[0]; extra += 2)
        if (extra[0] <= md[m].nexp && md[m].tab[extra[0] - 1]) rd_api[extra[1]] = sysp(m, extra[0]);
    if (md[m].flags & PF_DISCARD) { mem_put(md[m].base); md[m].loaded = 0; }   /* verwerfbar: erst beim ersten Aufruf laden */
    return m;
}
void k_modrest(void)                              /* Ende von RD: von Programmen geladene Module (RDTCP ueber RDKX) */
{
    WORD m;
    for (m = 1; m <= MAXMOD; m++) if (md[m].used && md[m].refs) { md[m].refs = 1; k_sysend(m); }
}
void __far *k_modraw(HMOD m, WORD nr)
{
    MODUL *x = &md[m];
    if (m < 1 || m > MAXMOD || !x->used || !x->loaded || nr < 1 || nr > x->nexp) return 0;
    return x->tab[nr - 1];
}
void k_sysend(HMOD m)
{
    MODUL *x;
    if (m < 1 || m > MAXMOD || !md[m].used) return;
    x = &md[m];
    if (x->loaded && x->nexp && x->tab[0])
        ((void (__far __pascal *)(void))x->tab[0])();
    ModFree(m);
}

void mod_api(void)
{
    mem_mod_discard = mdiscard; mem_mod_stamp = mstamp;
    KAPI(ModLoad, ModLoad); KAPI(ModFree, ModFree); KAPI(ModProc, ModProc);
}
