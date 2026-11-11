/* RDAUSL.C - Auslagern bei Speichermangel (Entscheidung Karsten 23.10.2026, Checkliste Punkt 1).
 *
 * Reicht der Speicher nach Verdichten und Verwerfen nicht, schreibt der Kern ueber dieses Modul den
 * aeltesten verschiebbaren, ungesperrten Datenblock weg: bevorzugt nach XMS (ein XMS-Block je Platz),
 * sonst in RD2.SWP im RD2-Verzeichnis, aber nur, wenn das auf einer Festplatte liegt (C: oder hoeher).
 * Auf Disketten wird nicht ausgelagert. Der naechste MemLock holt den Block zurueck.
 * Nicht ausgelagert werden: verwerfbare Bloecke (die verwirft der Kern), Modulbilder, Kernbloecke,
 * gesperrte Bloecke und Bloecke mit MEM_NOEMS (DMA-Puffer).
 * RD2.INI [System] Auslagern=0 schaltet es ab. Das Modul bleibt geladen (es muss da sein, wenn der
 * Speicher ausgeht), deshalb laedt der Kern es nur, wenn es ein Ziel gibt.
 * Dateizugriffe gehen direkt ueber INT 21h: Dateien der RD2-API gehoeren dem aufrufenden Programm und
 * wuerden mit ihm geschlossen.
 * Export 1: Ende (RD2.SWP loeschen, XMS frei). Export 2: DWORD Sw(art, seg, n), siehe MEM.C. */
#include <i86.h>
#include "RD2.H"

typedef struct { WORD seg, paras, flags, h, owner; } BLK;        /* wie MEM.C */
typedef struct { WORD seg, flags; BYTE owner, lock; DWORD size; DWORD stamp; } HND;
typedef struct { WORD b, n, h, home, stamp; } SWI;               /* MEM.C mem_swi (nahe Zeiger im Kern-DS) */
#define B_FREE   0x8000
#define B_INTERN 0x1000
#define B_MOD    0x0800
#define HA       0x1000
#define NS       32                               /* Plaetze */
#define CH       0x0800                           /* Paragraphen je Dateizugriff (32 KB) */

static struct { BYTE art; WORD xh, paras; DWORD off; } sl[NS];   /* art 0 frei, 1 XMS, 2 Datei, 3 Dateiluecke */
static BLK FAR *blk; static WORD FAR *nblk; static HND FAR *hnd;
static DWORD xent;                                /* XMS-Einsprung, 0 = keiner */
static WORD fh = 0xFFFF, platte; static DWORD fend;
static WORD swerr, maxkb, reservekb; static DWORD fsize;
#define SWERR(e) (swerr = (e))
static char pfad[80];
#pragma pack(push, 1)
static struct { DWORD len; WORD sh; DWORD so; WORD dh; DWORD dof; } mv;
#pragma pack(pop)

extern DWORD d21(WORD ax, WORD bx, WORD cx, WORD dx, WORD ds);   /* DX:AX = -Carry : AX */
#pragma aux d21 = "push ds" "mov ds, si" "int 21h" "pop ds" "sbb dx, dx" parm [ax] [bx] [cx] [dx] [si] value [dx ax] modify [bx cx si di es]
extern DWORD xc(WORD ax, WORD dx, WORD si, WORD di);             /* XMS: AX Ergebnis, DX */
#pragma aux xc = "call dword ptr [di]" parm [ax] [dx] [si] [di] value [dx ax] modify [bx cx]
extern WORD x43(void);
#pragma aux x43 = "mov ax, 4300h" "int 2Fh" "xor ah, ah" value [ax]
extern DWORD x4310(void);
#pragma aux x4310 = "mov ax, 4310h" "int 2Fh" "mov dx, es" "mov ax, bx" value [dx ax] modify [bx es]

/* DOS 36h: physisch freie Cluster, keine RD2-Allokation im Speichermangelpfad. */
static struct { WORD spc, frei, bps; } platz;
extern void dplatz(WORD lw, void *p);
#pragma aux dplatz = "mov ax,3600h" "int 21h" "mov [si],ax" "mov [si+2],bx" "mov [si+4],cx" \
                    parm [dx] [si] modify [ax bx cx dx]

static int wachsen(DWORD ende)
{
    DWORD cluster, frei, alt, neu, bedarf, reserve = (DWORD)reservekb << 10;
    if (ende <= fsize) return 1;                 /* belegte Dateilaenge / passende Luecke weiter nutzbar */
    if (ende > 0x7FFFFFFFUL || (maxkb && ende > ((DWORD)maxkb << 10))) { swerr = MEMERR_SWAPLIMIT; return 0; }
    dplatz((WORD)((pfad[0] | 0x20) - 'a' + 1), &platz);
    if (platz.spc == 0xFFFF || !platz.spc || !platz.bps) { swerr = MEMERR_DISK; return 0; }
    cluster = (DWORD)platz.spc * platz.bps;
    alt = fsize / cluster + (fsize % cluster != 0);
    neu = ende / cluster + (ende % cluster != 0);
    bedarf = (neu - alt) * cluster;
    if (!bedarf) return 1;                       /* Wachstum innerhalb des schon belegten letzten Clusters */
    frei = platz.frei && cluster > 0xFFFFFFFFUL / platz.frei ? 0xFFFFFFFFUL : cluster * platz.frei;
    if (frei < reserve || frei - reserve < bedarf) { swerr = MEMERR_RESERVE; return 0; }
    return 1;
}


#define OK(r) (!((r) >> 16))
static int xmove(WORD sh, DWORD so, WORD dh, DWORD dof, WORD paras)
{
    mv.len = (DWORD)paras << 4; mv.sh = sh; mv.so = so; mv.dh = dh; mv.dof = dof;
    return (WORD)xc(0x0B00, 0, (WORD)&mv, (WORD)&xent) == 1;
}
static int datei(WORD fn, WORD seg, DWORD off, WORD paras)   /* fn 3Fh lesen, 40h schreiben; 1 = ok */
{
    WORD k, n; DWORD r;
    r = d21(0x4200, fh, (WORD)(off >> 16), (WORD)off, 0);
    if (!OK(r)) { SWERR(MEMERR_DISK); return 0; }
    for (; paras; paras -= k, seg += k) {
        k = paras > CH ? CH : paras; n = k << 4;
        r = d21(fn << 8, fh, n, 0, seg);
        if (fn == 0x40 && OK(r) && off + (WORD)r > fsize) fsize = off + (WORD)r;
        if (!OK(r) || (WORD)r != n) { SWERR(MEMERR_DISK); return 0; }
        off += n;
    }
    return 1;
}
static void frei(WORD i)
{
    WORD k;
    if (sl[i].art == 1) xc(0x0A00, sl[i].xh, 0, (WORD)&xent);
    sl[i].art = sl[i].art == 2 ? 3 : 0;
    for (k = 0; k < NS && sl[k].art != 2; k++) ;  /* keine Datei-Plaetze mehr belegt: Datei von vorn */
    if (k == NS) { for (k = 0; k < NS; k++) if (sl[k].art == 3) sl[k].art = 0; fend = 0; }
}
static DWORD aus(void)
{
    WORD i, best = 0xFFFF, h, p, s, k; DWORD bs = 0xFFFFFFFFUL, r; BLK FAR *b;
    DWORD ende;
    SWERR(MEMERR_RAM);
    for (i = 0; i < *nblk; i++) {
        b = &blk[i]; h = b->h;
        if (!h || (b->flags & (B_FREE | B_INTERN | B_MOD | MEM_NOEMS | MEM_DISCARDABLE)) || !(b->flags & MEM_MOVABLE) || hnd[h].lock) continue;
        if (hnd[h].stamp < bs) { bs = hnd[h].stamp; best = i; }
    }
    if (best == 0xFFFF) return 0;
    b = &blk[best]; p = b->paras;
    for (s = 0; s < NS && ((sl[s].art + 1) & 2); s++) ;   /* art 0 oder 3: frei oder Dateiluecke */
    if (s == NS) { SWERR(MEMERR_SLOTS); return 0; }
    if (xent) {                                   /* XMS: eigener Block */
        r = xc(0x0900, (p + 63) >> 6, 0, (WORD)&xent);
        if ((WORD)r == 1) {
            sl[s].xh = (WORD)(r >> 16);
            if (xmove(0, (DWORD)b->seg << 16, sl[s].xh, 0, p)) { sl[s].art = 1; goto weg; }
            xc(0x0A00, sl[s].xh, 0, (WORD)&xent);
        }
    }
    if (!platte) return 0;
    if (fh == 0xFFFF) {
        r = d21(0x3C00, 0, 0, (WORD)pfad, FP_SEG((void FAR *)pfad));
        if (!OK(r)) { SWERR(MEMERR_DISK); return 0; }
        fh = (WORD)r; fend = 0;
        fsize = 0;
    }
    for (k = 0; k < NS && !(sl[k].art == 3 && sl[k].paras >= p); k++) ;   /* Luecke wiederverwenden */
    ende = (k < NS ? sl[k].off : fend) + ((DWORD)p << 4);
    if (ende < (k < NS ? sl[k].off : fend)) { SWERR(MEMERR_SWAPLIMIT); return 0; }
    if (!wachsen(ende)) return 0;
    if (k < NS) s = k; else { sl[s].off = fend; sl[s].paras = p; }
    sl[s].art = 0;                                /* auch zu kleine alte Luecke vor dem Schreiben entwerten */
    if (!datei(0x40, b->seg, sl[s].off, p)) { if (k < NS) sl[s].art = 3; return 0; }
    if (k == NS) fend += (DWORD)p << 4;
    sl[s].art = 2;
weg:
    SWERR(MEMERR_NONE);
    if (sl[s].art == 1) sl[s].paras = p;
    h = b->h;
    hnd[h].seg = 0; hnd[h].flags |= HA; hnd[h].stamp = s + 1;
    b->flags = B_FREE; b->h = 0; b->owner = 0;
    return 1;
}
static DWORD ein(WORD seg, WORD n)
{
    WORD i = n - 1; int ok;
    SWERR(MEMERR_DISK);
    if (n < 1 || n > NS || (sl[i].art != 1 && sl[i].art != 2)) return 0;
    ok = sl[i].art == 1 ? xmove(sl[i].xh, 0, 0, (DWORD)seg << 16, sl[i].paras) : datei(0x3F, seg, sl[i].off, sl[i].paras);
    if (ok) { frei(i); SWERR(MEMERR_NONE); }
    return ok;
}
static DWORD RDEXPORT Sw(WORD art, WORD seg, WORD n)
{
    if (art == 0) return aus();
    if (art == 1) return ein(seg, n);
    if (art == 2) { if (n >= 1 && n <= NS && (sl[n - 1].art == 1 || sl[n - 1].art == 2)) frei(n - 1); return 1; }
    if (art == 6) {
        if (n) { n--; return sl[n].art == 2 ? sl[n].paras : 0; }
        return xent ? xc(0x0800, 0, 0, (WORD)&xent) >> 16 : 0;
    }
    if (art == 5) return n ? ((DWORD)reservekb << 16) | maxkb : fsize;
    if (art == 4) return swerr;                   /* Zusatzgrund, 0 bleibt mit alten Modulen kompatibel */
    if (art == 3) {                               /* Anmeldung: Zeiger aus dem Kern, Ziel suchen */
        SWI FAR *w = (SWI FAR *)MK_FP(seg, n); char FAR *h = (char FAR *)MK_FP(seg, w->home); WORD k = 0;
        if (!ProfileGetInt("RD2.INI", "System", "Auslagern", 1)) return 0;
        { short v = ProfileGetInt("RD2.INI", "System", "SwapMaxKB", 0); maxkb = v < 0 ? 0 : (WORD)v;
          v = ProfileGetInt("RD2.INI", "System", "SwapReserveKB", 64); reservekb = v < 0 ? 64 : (WORD)v; }
        blk = (BLK FAR *)MK_FP(seg, w->b); nblk = (WORD FAR *)MK_FP(seg, w->n); hnd = (HND FAR *)MK_FP(seg, w->h);
        if (x43() == 0x80) xent = x4310();
        platte = (h[0] | 0x20) >= 'c' && h[1] == ':';
        while (h[k] && k < sizeof pfad - 8) { pfad[k] = h[k]; k++; }
        pfad[k++] = 'R'; pfad[k++] = 'D'; pfad[k++] = '2'; pfad[k++] = '.'; pfad[k++] = 'S'; pfad[k++] = 'W'; pfad[k++] = 'P'; pfad[k] = 0;
        if (!xent && !platte) return 0;
        DbgHex(xent ? "RD: Auslagern nach XMS, Platte" : "RD: Auslagern nach RD2.SWP, Platte", platte);
        return 1;
    }
    return 0;
}
static void RDEXPORT Ende(void)
{
    WORD i;
    for (i = 0; i < NS; i++) if (sl[i].art == 1) xc(0x0A00, sl[i].xh, 0, (WORD)&xent);
    if (fh != 0xFFFF) { d21(0x3E00, fh, 0, 0, 0); d21(0x4100, 0, 0, (WORD)pfad, FP_SEG((void FAR *)pfad)); fh = 0xFFFF; }
}
static struct { WORD n; FARPROC f[2]; } ex = { 2, { (FARPROC)Ende, (FARPROC)Sw } };
void FAR * RDAPI RdModInit(void) { return &ex; }
