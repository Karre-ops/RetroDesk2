/* MEM.C - Speicherverwaltung des RD2-Kerns (Modus R)
 *
 * Der Kern holt sich beim Start fast den ganzen freien DOS-Speicher (Arena)
 * und verteilt ihn in Paragraphen. Bloecke stehen nach Adresse sortiert in
 * blk[], freie Bereiche sind Bloecke mit B_FREE.
 *   fest       (Programmbilder, MemFixed, Kern-intern) von unten
 *   verschiebbar (MEM_MOVABLE) von oben
 * Gibt DOS hohen Speicher (UMB, DOS=UMB), kommt er als weitere freie Bereiche dazu,
 * durch nicht freie Luecken (Bildschirm, ROM) vom Rest getrennt. Module und Treiber
 * kommen bevorzugt dorthin (B_HIGH), damit die 640 KB den Programmen bleiben.
 * Reicht der Platz nicht: 1. verschiebbare, nicht gesperrte Bloecke nach oben
 * schieben (Luecken schliessen), 2. verwerfbare, nicht gesperrte Bloecke
 * verwerfen (aeltester Zugriff zuerst), 3. noch einmal verdichten. */
#include "KERN.H"

#define MAXB 160
#define MAXH 200
typedef struct { WORD seg, paras, flags, h, owner; } BLK;
typedef struct { WORD seg, flags; BYTE owner, lock; DWORD size; DWORD stamp; } HND;   /* 14 Bytes; belegt = HU in flags */
#define HU 0x8000
#define HA 0x1000                                 /* ausgelagert: seg = 0, stamp = Platz im Auslagermodul */
static BLK blk[MAXB];
static WORD nblk;
static HND hnd[MAXH + 1];                         /* Handle 1..MAXH */
static WORD arena, arena_paras, minseg;
static WORD umb[4], numb;
WORD k_umb_paras;
DWORD k_stamp;


static WORD find_seg(WORD seg)
{
    WORD i;
    for (i = 0; i < nblk; i++) if (blk[i].seg == seg && !(blk[i].flags & B_FREE)) return i;
    return 0xFFFF;
}
static void ins(WORD i)                           /* Platz an Stelle i schaffen */
{
    WORD j;
    for (j = nblk; j > i; j--) blk[j] = blk[j - 1];
    nblk++;
}
static void del(WORD i)
{
    for (; i + 1 < nblk; i++) blk[i] = blk[i + 1];
    nblk--;
}
static void merge(void)
{
    WORD i = 0;
    while (i + 1 < nblk) {
        if ((blk[i].flags & B_FREE) && (blk[i + 1].flags & B_FREE)) { blk[i].paras += blk[i + 1].paras; del(i + 1); }
        else i++;
    }
}

/* Hohen Speicher von DOS holen: Verkettung an, Strategie "nur oben", alles nehmen */
static void umb_grab(void)
{
    WORD strat, link, i, j, sz[4], end;
    kr.ax = 0x5800; rd_int(0x21, &kr); strat = kr.ax & 0xFF;
    kr.ax = 0x5802; rd_int(0x21, &kr); link = kr.ax & 0xFF;
    kr.ax = 0x5803; kr.bx = 1; rd_int(0x21, &kr);
    if (CARRY) return;                            /* DOS ohne UMB-Verwaltung */
    kr.ax = 0x5801; kr.bx = 0x40; rd_int(0x21, &kr);
    while (numb < 4) {
        kr.ax = 0x4800; kr.bx = 0xFFFF; rd_int(0x21, &kr);
        if (kr.bx < 64) break;                    /* unter 1 KB lohnt nicht */
        sz[numb] = kr.bx; kr.ax = 0x4800; rd_int(0x21, &kr);
        if (CARRY) break;
        if (kr.ax < 0xA000) { kr.es = kr.ax; kr.ax = 0x4900; rd_int(0x21, &kr); break; }
        umb[numb++] = kr.ax;
    }
    kr.ax = 0x5801; kr.bx = strat; rd_int(0x21, &kr);
    kr.ax = 0x5803; kr.bx = link; rd_int(0x21, &kr);
    for (i = 1; i < numb; i++)                    /* nach Adresse ordnen */
        for (j = i; j > 0 && umb[j - 1] > umb[j]; j--) { WORD t = umb[j]; umb[j] = umb[j - 1]; umb[j - 1] = t; t = sz[j]; sz[j] = sz[j - 1]; sz[j - 1] = t; }
    for (i = 0, end = arena + arena_paras; i < numb; i++) {
        blk[nblk].seg = end; blk[nblk].paras = umb[i] - end; blk[nblk].flags = B_INTERN; blk[nblk].h = 0; blk[nblk].owner = 0xFFFF; nblk++;
        blk[nblk].seg = umb[i]; blk[nblk].paras = sz[i]; blk[nblk].flags = B_FREE; blk[nblk].h = 0; blk[nblk].owner = 0; nblk++;
        end = umb[i] + sz[i]; k_umb_paras += sz[i];
    }
}
void mem_init(WORD reserve)
{
    kr.ax = 0x4800; kr.bx = 0xFFFF; rd_int(0x21, &kr);
    arena_paras = kr.bx > reserve ? kr.bx - reserve : 0;
    kr.ax = 0x4800; kr.bx = arena_paras; rd_int(0x21, &kr);
    arena = CARRY ? 0 : kr.ax;
    if (!arena) arena_paras = 0;
    nblk = 1; blk[0].seg = arena; blk[0].paras = arena_paras; blk[0].flags = B_FREE; blk[0].h = 0; blk[0].owner = 0;
    if (arena) umb_grab();
}
void mem_tsr(WORD paras)                         /* oben Platz fuer einen spaeter geladenen Treiber (RDNET live) */
{
    WORD i;
    for (i = 0; i < nblk; i++)
        if ((blk[i].flags & B_FREE) && blk[i].seg + blk[i].paras == arena + arena_paras && blk[i].paras > paras + 4096) {
            kr.ax = 0x4A00; kr.es = arena; kr.bx = arena_paras - paras; rd_int(0x21, &kr);
            if (!CARRY) { arena_paras -= paras; blk[i].paras -= paras; }
            return;
        }
}
WORD mem_arena(WORD __far *a)                   /* RunDos: a[0] Arena, a[1] Groesse; liefert Ende des belegten Teils */
{
    WORD i, t = arena;
    for (i = 0; i < nblk; i++)
        if (!(blk[i].flags & B_FREE) && blk[i].seg >= arena && blk[i].seg < arena + arena_paras && blk[i].seg + blk[i].paras > t)
            t = blk[i].seg + blk[i].paras;
    a[0] = arena; a[1] = arena_paras;
    return t;
}
void mem_done(void)
{
    mod_xdone();
    if (arena) { kr.ax = 0x4900; kr.es = arena; rd_int(0x21, &kr); arena = 0; }
    while (numb) { kr.ax = 0x4900; kr.es = umb[--numb]; rd_int(0x21, &kr); }
}

/* Freien Bereich suchen: fest von unten, verschiebbar von oben */
static WORD take(WORD paras, WORD flags, WORD h, WORD owner)
{
    WORD i, seg; int up = (flags & MEM_MOVABLE) != 0;
    if (nblk >= MAXB - 1) return 0;
    for (i = up ? nblk - 1 : 0; i < nblk; i = up ? i - 1 : i + 1) {
        if (!(blk[i].flags & B_FREE) || blk[i].paras < paras || (!up && blk[i].seg < minseg)) continue;
        if (blk[i].paras == paras) {
            blk[i].flags = flags; blk[i].h = h; blk[i].owner = owner;
            return blk[i].seg;
        }
        if (up) {                                 /* oberes Ende des freien Bereichs */
            blk[i].paras -= paras;
            ins(i + 1);
            seg = blk[i].seg + blk[i].paras;
            blk[i + 1].seg = seg; blk[i + 1].paras = paras;
            blk[i + 1].flags = flags; blk[i + 1].h = h; blk[i + 1].owner = owner;
        } else {
            ins(i);
            seg = blk[i + 1].seg;
            blk[i].seg = seg; blk[i].paras = paras; blk[i].flags = flags; blk[i].h = h; blk[i].owner = owner;
            blk[i + 1].seg += paras; blk[i + 1].paras -= paras;
        }
        return seg;
    }
    return 0;
}

/* Verschiebbare Bloecke nach oben in direkt darueber liegende Luecken schieben */
static void compact(void)
{
    WORD i, moved = 1, gap;
    while (moved) {
        moved = 0;
        for (i = nblk - 1; i > 0; i--) {
            BLK *f = &blk[i], *b = &blk[i - 1];
            if (!(f->flags & B_FREE) || (b->flags & (B_FREE | B_INTERN)) || !(b->flags & MEM_MOVABLE)) continue;
            if (b->h && hnd[b->h].lock) continue;
            gap = f->paras;
            rd_pmove(b->seg + gap, b->seg, b->paras);
            f->seg = b->seg; b->seg += gap;
            { BLK t = *f; *f = *b; *b = t; }      /* Reihenfolge nach Adresse */
            if (blk[i].h) hnd[blk[i].h].seg = blk[i].seg;
            moved = 1;
        }
        merge();
    }
}

int (*mem_mod_discard)(WORD mod);
DWORD (*mem_mod_stamp)(WORD mod);

/* Verwerfbar? Liefert den Zeitstempel des letzten Zugriffs, 0xFFFFFFFF = nein (frei, fest, gesperrt) */
static DWORD vstamp(WORD i)
{
    WORD h = blk[i].h, f = blk[i].flags;
    if (f & B_FREE) return 0xFFFFFFFFUL;
    if (f & B_MOD) return mem_mod_stamp ? mem_mod_stamp(blk[i].owner) : 0xFFFFFFFFUL;
    if (!(f & MEM_DISCARDABLE) || !h || hnd[h].lock) return 0xFFFFFFFFUL;
    return hnd[h].stamp;
}
/* Aeltesten verwerfbaren, nicht gesperrten Block verwerfen (Daten oder Modul). 1 = einer verworfen */
static int discard_one(void)
{
    WORD i, best = 0xFFFF; DWORD bs = 0xFFFFFFFFUL, st;
    for (i = 0; i < nblk; i++) if ((st = vstamp(i)) < bs) { bs = st; best = i; }
    if (best == 0xFFFF) return 0;
    if (blk[best].flags & B_MOD) mem_mod_discard(blk[best].owner);   /* Modul merkt sich: nicht geladen */
    else hnd[blk[best].h].seg = 0;                /* Handle bleibt, Inhalt ist weg */
    blk[best].flags = B_FREE; blk[best].h = 0; blk[best].owner = 0;
    merge();
    return 1;
}

/* Reserve zum Nachladen: so viele Paragraphen bleiben fuer das groesste verwerfbare Modul
   erreichbar (frei oder verwerfbar). Nur Modulbilder selbst duerfen sie nutzen (MOD.C setzt
   mem_resv waehrend des Ladens auf 0). Sonst koennte ein Programm alles belegen und der
   naechste Aufruf eines verworfenen Moduls scheitern. */
extern DWORD (__far __pascal *mem_sw)(WORD art, WORD seg, WORD n);
WORD mem_resv;
static WORD erreichbar(void)
{
    WORD i, n = 0;
    for (i = 0; i < nblk; i++)                    /* frei, verwerfbar oder (mit RDAUSL) auslagerbar */
        if ((blk[i].flags & B_FREE) || vstamp(i) != 0xFFFFFFFFUL || (mem_sw && (blk[i].flags & (MEM_MOVABLE | B_INTERN | B_MOD | MEM_NOEMS)) == MEM_MOVABLE && !hnd[blk[i].h].lock))
            n += blk[i].paras;
    return n;
}

static int aus(void);
static void put_index(WORD i);
static WORD get(WORD paras, WORD flags, WORD h, WORD owner)
{
    WORD seg;
    if (!paras) paras = 1;
    if (mem_resv && (DWORD)paras + mem_resv > erreichbar()) return 0;
    if (flags & B_HIGH) {                         /* erst nur im hohen Speicher */
        flags &= ~B_HIGH;
        if (numb) { minseg = 0xA000; seg = take(paras, flags, h, owner); minseg = 0; if (seg) return seg; }
    }
    if ((seg = take(paras, flags, h, owner)) != 0) return seg;
    if (profile_flush(), (seg = take(paras, flags, h, owner)) != 0) return seg;   /* INI-Zwischenspeicher frei */
    compact();
    if ((seg = take(paras, flags, h, owner)) != 0) return seg;
    while (discard_one() || aus()) {
        compact();
        if ((seg = take(paras, flags, h, owner)) != 0) return seg;
    }
    return 0;
}

/* Auslagern (RDAUSL.MOD, Entscheidung 23.10.2026). Das Modul waehlt den aeltesten verschiebbaren, ungesperrten
   Datenblock (nicht verwerfbar, kein Modul, nicht Kern, nicht MEM_NOEMS), schreibt ihn nach XMS oder RD2.SWP,
   setzt im Handle seg = 0, HA und stamp = Platz und gibt den Block frei (B_FREE). Art 0 aus, 1 ein (n = Platz),
   2 Platz frei, 3 Anmeldung (seg:n = mem_swi). */
DWORD (__far __pascal *mem_sw)(WORD art, WORD seg, WORD n);
static BYTE swbusy;
static DWORD sw(WORD art, WORD seg, WORD n) { DWORD r; swbusy = 1; r = mem_sw(art, seg, n); swbusy = 0; return r; }
static int aus(void) { if (!mem_sw || swbusy || !sw(0, 0, 0)) return 0; merge(); return 1; }
static void swfrei(HND *x) { if (x->flags & HA) sw(2, 0, (WORD)x->stamp); }
struct mem_swi_t mem_swi = { blk, &nblk, hnd, k_home, &k_stamp };

WORD mem_get(WORD paras, WORD flags, WORD owner) { return get(paras, (flags & ~MEM_MOVABLE) | B_INTERN, 0, owner); }

static void put_index(WORD i)
{
    blk[i].flags = B_FREE; blk[i].h = 0; blk[i].owner = 0;
    merge();
}
void mem_put(WORD seg) { WORD i = find_seg(seg); if (i != 0xFFFF) put_index(i); }

void mem_owner_free(WORD owner)
{
    WORD i, h;
    for (h = 1; h <= MAXH; h++)
        if ((hnd[h].flags & HU) && hnd[h].owner == owner && !(hnd[h].flags & MEM_SHARE)) {
            if (hnd[h].seg) { i = find_seg(hnd[h].seg); if (i != 0xFFFF) put_index(i); }
            swfrei(&hnd[h]); hnd[h].flags = 0;
        }
    for (i = 0; i < nblk; i++)
        if (!(blk[i].flags & (B_FREE | B_MOD | MEM_SHARE)) && blk[i].owner == owner) { put_index(i); i = (WORD)-1; }
}

DWORD mem_free_bytes(int groesster)
{
    WORD i; DWORD n = 0;
    for (i = 0; i < nblk; i++)
        if (blk[i].flags & B_FREE) {
            if (groesster) { if (blk[i].paras > n) n = blk[i].paras; }
            else n += blk[i].paras;
        }
    return n << 4;
}

/* ---- API ------------------------------------------------------------------ */
static WORD paras_of(DWORD size) { return size > 0x9FFF0UL ? 0 : (WORD)((size + 15) >> 4); }

HMEM RDK MemAlloc(DWORD size, WORD flags)
{
    WORD h, p = paras_of(size), seg, f;
    if (!p && size) return FAIL(RD_ERR_NOMEM);
    for (h = 1; h <= MAXH && (hnd[h].flags & HU); h++) ;
    if (h > MAXH) return FAIL(RD_ERR_NOMEM);
    f = flags & (MEM_MOVABLE | MEM_DISCARDABLE | MEM_SHARE | MEM_NOEMS);
    if (f & MEM_DISCARDABLE) f |= MEM_MOVABLE;
    seg = get(p, f, h, k_cur);
    if (!seg) return FAIL(RD_ERR_NOMEM);
    hnd[h].seg = seg; hnd[h].flags = f | HU; hnd[h].owner = (BYTE)k_cur;
    hnd[h].lock = (f & MEM_MOVABLE) ? 0 : 1; hnd[h].size = size; hnd[h].stamp = ++k_stamp;
    if (flags & MEM_ZEROINIT) rd_pfill(seg, p ? p : 1, 0);
    return h;
}
static HND *hp(HMEM h) { return (h && h <= MAXH && (hnd[h].flags & HU)) ? &hnd[h] : 0; }
static WORD paras_of(DWORD size);
static int ein(WORD h)                            /* ausgelagerten Block zurueckholen, 1 = ok */
{
    HND *x = &hnd[h]; WORD p = paras_of(x->size), seg;
    if (!(x->flags & HA)) return 0;
    if (!(seg = get(p ? p : 1, x->flags & ~(HU | HA), h, x->owner))) return 0;
    if (!sw(1, seg, (WORD)x->stamp)) { mem_put(seg); return 0; }
    x->seg = seg; x->flags &= ~HA;
    return 1;
}

HMEM RDK MemFree(HMEM h)
{
    HND *x = hp(h); WORD i;
    if (!x) return (HMEM)FAIL(RD_ERR_HANDLE) | h;
    if (x->seg && (i = find_seg(x->seg)) != 0xFFFF) put_index(i);
    swfrei(x); x->flags = 0;
    return 0;
}
void FAR * RDK MemLock(HMEM h)
{
    HND *x = hp(h);
    if (!x) { k_lasterr = RD_ERR_HANDLE; return 0; }
    if (!x->seg && !ein(h)) { k_lasterr = (x->flags & HA) ? RD_ERR_NOMEM : RD_ERR_NOTFOUND; return 0; }  /* verworfen oder ausgelagert */
    if (x->lock < 255) x->lock++;
    x->stamp = ++k_stamp;
    return MK_FP(x->seg, 0);
}
BOOL RDK MemUnlock(HMEM h)
{
    HND *x = hp(h);
    if (!x || !x->lock) return FALSE;
    if (x->flags & MEM_MOVABLE) x->lock--;
    return TRUE;
}
HMEM RDK MemRealloc(HMEM h, DWORD size, WORD flags)
{
    HND *x = hp(h); WORD p = paras_of(size), old, oldp, seg, i, lk;
    (void)flags;
    if (!x) return FAIL(RD_ERR_HANDLE);
    if (!p) return FAIL(RD_ERR_NOMEM);
    if (!x->seg && (x->flags & HA) && !ein(h)) return FAIL(RD_ERR_NOMEM);
    old = x->seg;
    if (!old) {                                   /* verworfen: neu anlegen */
        if (!(seg = get(p, x->flags & ~HU, h, x->owner))) return FAIL(RD_ERR_NOMEM);
        x->seg = seg; x->size = size; return h;
    }
    i = find_seg(old); oldp = blk[i].paras;
    if (p <= oldp) {                              /* verkleinern: Rest freigeben */
        if (p < oldp && nblk < MAXB - 1) {
            ins(i + 1);
            blk[i].paras = p;
            blk[i + 1].seg = old + p; blk[i + 1].paras = oldp - p; blk[i + 1].flags = B_FREE; blk[i + 1].h = 0; blk[i + 1].owner = 0;
            merge();
        }
        x->size = size; return h;
    }
    if (x->lock && !(x->flags & MEM_MOVABLE) ) return FAIL(RD_ERR_NOMEM);
    lk = x->lock; x->lock = 1;                    /* waehrend des Umzugs nicht verschieben */
    seg = get(p, x->flags & ~HU, h, x->owner);
    x->lock = lk;
    if (!seg) return FAIL(RD_ERR_NOMEM);
    old = x->seg;                                 /* kann durch Verdichten gewandert sein (nicht bei lock) */
    rd_pmove(seg, old, oldp);
    i = find_seg(old); if (i != 0xFFFF) put_index(i);
    x->seg = seg; x->size = size;
    return h;
}
DWORD RDK MemSize(HMEM h)  { HND *x = hp(h); return x ? x->size : 0; }
WORD  RDK MemFlags(HMEM h) { HND *x = hp(h); return x ? ((x->flags & ~(HU | HA)) | (x->seg || (x->flags & HA) ? 0 : 0x4000u) | x->lock) : 0; }
DWORD RDK MemAvail(WORD art)
{
    if (art == MEMA_KONV) return mem_free_bytes(0);
    if (art == MEMA_GROESSTER) return mem_free_bytes(1);
    return 0;                                     /* EMS/EXT folgen */
}
DWORD RDK MemCompact(DWORD wunsch)
{
    (void)wunsch;
    compact();
    if (mem_free_bytes(1) < wunsch) { while (mem_free_bytes(1) < wunsch && discard_one()) compact(); }
    return mem_free_bytes(1);
}
void FAR * RDK MemFixed(WORD size)
{
    WORD seg = get((WORD)(((DWORD)size + 15) >> 4), MEM_FIXED, 0, k_cur);
    if (!seg) { k_lasterr = RD_ERR_NOMEM; return 0; }
    return MK_FP(seg, 0);
}
void mem_share(HMEM h)
{
    WORD i;
    if (!h || h > MAXH || !(hnd[h].flags & HU)) return;
    hnd[h].owner = 0; hnd[h].flags |= MEM_SHARE;
    if (hnd[h].seg && (i = find_seg(hnd[h].seg)) != 0xFFFF) { blk[i].owner = 0; blk[i].flags |= MEM_SHARE; }
}
void mem_chown(WORD seg, WORD owner)
{
    WORD i = find_seg(seg);
    if (i != 0xFFFF) blk[i].owner = owner;
}
void mem_mod(WORD seg, WORD mod)
{
    WORD i = find_seg(seg);
    if (i != 0xFFFF) { blk[i].flags = (blk[i].flags & ~B_INTERN) | B_MOD; blk[i].owner = mod; }
}
void RDK MemFixedFree(void FAR *p) { if (p && !FP_OFF(p)) mem_put(FP_SEG(p)); }

void mem_api(void)
{
    KAPI(MemAlloc, MemAlloc); KAPI(MemFree, MemFree); KAPI(MemLock, MemLock); KAPI(MemUnlock, MemUnlock);
    KAPI(MemRealloc, MemRealloc); KAPI(MemSize, MemSize); KAPI(MemFlags, MemFlags); KAPI(MemAvail, MemAvail);
    KAPI(MemCompact, MemCompact); KAPI(MemFixed, MemFixed); KAPI(MemFixedFree, MemFixedFree);
}
