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
 * Flag PF_DISCARD; ihr Zustand (Daten) ist nach dem Nachladen neu. */
#include "KERN.H"

#define MAXMOD 12
#define MAXEXP 96                                 /* RDUSER braucht die meisten */
#define MAXTHUNK 160
#define PF_DISCARD 0x0080

typedef struct {
    BYTE  used, loaded, drive;
    WORD  refs, lock, flags, base, paras;
    DWORD serial, stamp;
    char  label[12];
    char  path[66];
    char  name[16];
    WORD  nexp;
    struct { WORD off, seg; } ex[MAXEXP];         /* seg relativ zur Ladeadresse */
} MODUL;
static MODUL md[MAXMOD + 1];                      /* HMOD 1..MAXMOD */
static struct { BYTE mod, nr; } th[MAXTHUNK];
static BYTE thcode[MAXTHUNK][8];
static WORD nth;
extern DWORD k_stamp;

/* Bild laden und initialisieren. 0 = ok */
static WORD load(WORD m)
{
    MODUL *x = &md[m]; WORD i, e; static WORD seg, paras; DWORD tab;
    WORD __far *t; void __far * __far *fp;
    if ((e = k_imgload(x->path, "MD", &seg, &paras)) != 0) return e;
    tab = rd_enter(seg + k_mz.cs, k_mz.ip, (void __far *)rd_api, m, 0, (char __far *)x->path);
    t = (WORD __far *)MK_FP((WORD)(tab >> 16), (WORD)tab);
    if (!tab || t[0] > MAXEXP) { mem_put(seg); return RD_ERR_FORMAT; }
    x->nexp = t[0];
    fp = (void __far * __far *)(t + 1);
    for (i = 0; i < x->nexp; i++) {
        x->ex[i].off = FP_OFF(fp[i]); x->ex[i].seg = fp[i] ? FP_SEG(fp[i]) - seg : 0xFFFF;   /* 0xFFFF: Export fehlt */
    }
    x->base = seg; x->paras = paras; x->flags = k_rb.flags; x->loaded = 1; x->stamp = ++k_stamp;
    for (i = 0; i < 15 && k_rb.name[i]; i++) x->name[i] = k_rb.name[i];
    x->name[i] = 0;
    mem_mod(seg, m);
    x->lock = (x->flags & PF_DISCARD) ? 0 : 0x8000;           /* 0x8000: nie verwerfen */
    return 0;
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
    { static char b[40]; WORD n = 0; LPCSTR s = "RD: Modul verworfen: "; while (*s) b[n++] = *s++; s = md[m].name; while (*s && n < 38) b[n++] = *s++; b[n] = 0; k_out(b, 0); }
    return 1;
}

/* Verworfenes Modul zurueckholen: richtige Diskette verlangen, neu laden */
static WORD reload(WORD m)
{
    MODUL *x = &md[m]; WORD e;
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
    x->used = 1; x->refs = 1; x->lock = 0; x->loaded = 0; x->drive = 0;
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
    if (!m || !x->used || t->rsp >= 12) { k_out("RD: Modulaufruf ungueltig, Programm wird beendet", 0); TaskKill(k_cur, 0xFFFE); }
    if (!x->loaded && reload(m)) {
        k_out("RD: Modul fehlt, Programm wird beendet", 0);
        TaskKill(k_cur, 0xFFFE);
    }
    if (nr > x->nexp) { k_out("RD: Modulfunktion fehlt, Programm wird beendet", 0); TaskKill(k_cur, 0xFFFE); }
    t->rs[t->rsp].ip = ip; t->rs[t->rsp].cs = cs; t->rs[t->rsp].mod = m; t->rsp++;
    x->lock++; x->stamp = ++k_stamp;
    return ((DWORD)(x->base + x->ex[nr - 1].seg) << 16) | x->ex[nr - 1].off;
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
   (Export, API), mit 0 beendet. Export 1 ist der Abschluss beim Beenden von RD. */
HMOD k_sysmod(const char *name, WORD base, WORD n, const WORD *extra)
{
    HMOD m = ModLoad(name); WORD k;
    if (!m) return 0;
    for (k = 0; k < n && k + 2 <= md[m].nexp; k++)
        if (md[m].ex[k + 1].seg != 0xFFFF) rd_api[base + k] = ModProc(m, k + 2);
    for (; extra && extra[0]; extra += 2)
        if (extra[0] <= md[m].nexp && md[m].ex[extra[0] - 1].seg != 0xFFFF) rd_api[extra[1]] = ModProc(m, extra[0]);
    return m;
}
void __far *k_modraw(HMOD m, WORD nr)
{
    MODUL *x = &md[m];
    if (m < 1 || m > MAXMOD || !x->used || !x->loaded || nr < 1 || nr > x->nexp || x->ex[nr - 1].seg == 0xFFFF) return 0;
    return MK_FP(x->base + x->ex[nr - 1].seg, x->ex[nr - 1].off);
}
void k_sysend(HMOD m)
{
    MODUL *x;
    if (m < 1 || m > MAXMOD || !md[m].used) return;
    x = &md[m];
    if (x->loaded && x->nexp && x->ex[0].seg != 0xFFFF)
        ((void (__far __pascal *)(void))MK_FP(x->base + x->ex[0].seg, x->ex[0].off))();
    ModFree(m);
}

void mod_api(void)
{
    mem_mod_discard = mdiscard; mem_mod_stamp = mstamp;
    KAPI(ModLoad, ModLoad); KAPI(ModFree, ModFree); KAPI(ModProc, ModProc);
}
