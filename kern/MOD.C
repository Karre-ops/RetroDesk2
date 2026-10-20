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
#define MAXEXP 32
#define MAXTHUNK 96
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

#pragma pack(push, 1)
typedef struct { WORD sig, cblp, cp, crlc, cparhdr, minalloc, maxalloc, ss, sp, csum, ip, cs, lfarlc, ovno; } MZ;
typedef struct { char sig[4]; DWORD blk, len; WORD sum, res; } TRAILER;
typedef struct { char sig[4]; WORD api, flags, stack, kb; char name[16]; WORD ver, nexp, nres, oexp, ores, res[5]; } RD2BLK;
#pragma pack(pop)
static MZ mz; static TRAILER tr; static RD2BLK rb;
static struct { WORD seg, reloc; } pb;

static int rdat(int f, DWORD pos, void *buf, WORD n)
{
    kr.ax = 0x4200; kr.bx = f; kr.cx = (WORD)(pos >> 16); kr.dx = (WORD)pos; rd_int(0x21, &kr);
    if (CARRY) return 0;
    kr.ax = 0x3F00; kr.bx = f; kr.cx = n; kr.dx = (WORD)buf; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    return !CARRY && kr.ax == n;
}

/* Bild laden und initialisieren. 0 = ok */
static WORD load(WORD m)
{
    MODUL *x = &md[m]; int f; WORD i, sum = 0, seg, paras; DWORD img, sz, tab;
    WORD __far *t; void __far * __far *fp;
    kr.ax = 0x3D00; kr.dx = (WORD)x->path; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    if (CARRY) return RD_ERR_NOTFOUND;
    f = kr.ax;
    kr.ax = 0x4202; kr.bx = f; kr.cx = 0; kr.dx = 0; rd_int(0x21, &kr);
    sz = ((DWORD)kr.dx << 16) | kr.ax;
    i = rdat(f, 0, &mz, sizeof mz) && mz.sig == 0x5A4D && rdat(f, sz - 16, &tr, sizeof tr) &&
        tr.sig[0] == 'R' && tr.sig[3] == '!' && rdat(f, tr.blk, &rb, sizeof rb) && (rb.sig[3] == 'M' || rb.sig[3] == 'D');
    kr.ax = 0x3E00; kr.bx = f; rd_int(0x21, &kr);
    if (!i) return RD_ERR_FORMAT;
    for (i = 0; i < sizeof rb; i++) sum += ((BYTE *)&rb)[i];
    if (sum != tr.sum) return RD_ERR_FORMAT;
    if (rb.api > RD2_API_VERSION) return RD_ERR_VERSION;
    img = (DWORD)mz.cp * 512 - (mz.cblp ? 512 - mz.cblp : 0) - (DWORD)mz.cparhdr * 16;
    paras = (WORD)((img + 15) >> 4) + mz.minalloc;
    if (!(seg = mem_get(paras, 0, 0))) return RD_ERR_NOMEM;
    pb.seg = seg; pb.reloc = seg;
    kr.ax = 0x4B03; kr.dx = (WORD)x->path; kr.ds = FP_SEG((void __far *)&kr); kr.bx = (WORD)&pb; kr.es = kr.ds;
    rd_int(0x21, &kr);
    if (CARRY) { mem_put(seg); return RD_ERR_DISK; }
    tab = rd_enter(seg + mz.cs, mz.ip, (void __far *)rd_api, m, 0, (char __far *)x->path);
    t = (WORD __far *)MK_FP((WORD)(tab >> 16), (WORD)tab);
    if (!tab || t[0] > MAXEXP) { mem_put(seg); return RD_ERR_FORMAT; }
    x->nexp = t[0];
    fp = (void __far * __far *)(t + 1);
    for (i = 0; i < x->nexp; i++) { x->ex[i].off = FP_OFF(fp[i]); x->ex[i].seg = FP_SEG(fp[i]) - seg; }
    x->base = seg; x->paras = paras; x->flags = rb.flags; x->loaded = 1; x->stamp = ++k_stamp;
    for (i = 0; i < 15 && rb.name[i]; i++) x->name[i] = rb.name[i];
    x->name[i] = 0;
    mem_mod(seg, m);
    if (!(x->flags & PF_DISCARD)) x->lock = 0x8000;           /* nie verwerfen */
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
        /* sonst (Diskette gewechselt?) noch einmal fragen */
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
void RDK ModFree(HMOD m)
{
    MODUL *x;
    if (m < 1 || m > MAXMOD || !md[m].used) return;
    x = &md[m];
    if (--x->refs) return;
    if (x->loaded) mem_put(x->base);
    x->used = x->loaded = 0;
}
FARPROC RDK ModProc(HMOD m, WORD nr)
{
    WORD i; BYTE *c; WORD __far *g = (WORD __far *)&rd_gateptr;
    if (m < 1 || m > MAXMOD || !md[m].used || nr < 1) return (FARPROC)FAIL(RD_ERR_HANDLE);
    if (md[m].loaded && nr > md[m].nexp) return (FARPROC)FAIL(RD_ERR_NOTFOUND);
    for (i = 0; i < nth; i++) if (th[i].mod == m && th[i].nr == nr) break;
    if (i == nth) {
        if (nth >= MAXTHUNK) return (FARPROC)FAIL(RD_ERR_FULL);
        nth++;
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
    if (!x->used || t->rsp >= 12) { k_out("RD: Modulaufruf ungueltig, Programm wird beendet", 0); TaskKill(k_cur, 0xFFFE); }
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
    TASK *t = &k_task[k_cur];
    t->rsp--;
    md[t->rs[t->rsp].mod].lock--;
    return ((DWORD)t->rs[t->rsp].cs << 16) | t->rs[t->rsp].ip;
}

void mod_api(void)
{
    mem_mod_discard = mdiscard; mem_mod_stamp = mstamp;
    KAPI(ModLoad, ModLoad); KAPI(ModFree, ModFree); KAPI(ModProc, ModProc);
}
