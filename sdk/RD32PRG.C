/* RD32PRG.C - nur im RD2/32-Programm (nicht in Modulen): DPMI-Speicher, Einsprung, Rueckruf-Verteiler, Vollbild,
 * 32-Bit-Module laden (rd32_modload/rd32_modproc/rd32_modfree, Format .M32 aus tools/rd32mod.py, PLAN_32BIT).
 * Module laufen im Adressraum des Programms; was dort Zustand hat, rufen sie ueber die Tabelle im Modulkopf hier auf. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"
#include "RD32INT.H"

void *rd32_lin(DWORD lin);
static void *mods[8]; static WORD nmod;
void (*rd32_modende)(void);                       /* setzt rd32_modload: so zieht rd32_main den Lader nicht in jedes Programm */

/* ---- DPMI-Speicher -------------------------------------------------------------------------------------- */
#define NBLK 32
static struct { DWORD lin, h; } blk[NBLK];
void *rd32_alloc(DWORD n)
{
    R32 r; WORD i;
    for (i = 0; i < NBLK && blk[i].lin; i++) ;
    if (i == NBLK || !n) return 0;
    r.eax = 0x0501; r.ebx = n >> 16; r.ecx = n & 0xFFFF; rd32_int31(&r);
    if (r.cf) return 0;
    blk[i].lin = ((r.ebx & 0xFFFF) << 16) | (r.ecx & 0xFFFF); blk[i].h = ((r.esi & 0xFFFF) << 16) | (r.edi & 0xFFFF);
    return rd32_lin(blk[i].lin);
}
void rd32_free(void *p)
{
    R32 r; WORD i; DWORD l = (DWORD)p + rd32_base;
    for (i = 0; i < NBLK; i++) if (blk[i].lin && blk[i].lin == l) {
        r.eax = 0x0502; r.esi = blk[i].h >> 16; r.edi = blk[i].h & 0xFFFF; rd32_int31(&r); blk[i].lin = 0;
    }
}

/* ---- Einsprung und Rueckrufe -------------------------------------------------------------------------------- */
unsigned long rd32_jb[6]; int rd32_ecode; void (*rd32_beiende)(void);   /* exit (RD32C): zurueck hierher */
int setjmp(unsigned long *b);
int rd32_main(void)
{
    WORD i; int r;
    rd32_xp = 0; for (i = 0; i < 8; i++) PROCS[i] = 0;
    if (!setjmp(rd32_jb)) r = RdMain(TW(TB_INST), (LPCSTR)(TB + TB_CMD), TW(TB_SHOW)); else r = rd32_ecode;
    if (rd32_beiende) rd32_beiende();             /* atexit, Dateien schliessen, stdout leeren */
    if (rd32_modende) rd32_modende();             /* geladene Module: ihr Ende (atexit, Dateien), dann Speicher */
    for (i = 0; i < NBLK; i++) if (blk[i].lin) rd32_free(rd32_lin(blk[i].lin));
    return r;
}
DWORD rd32_cb(void)
{
    WORD s = TW(TB_CBSLOT);
    if (s > 7 || !PROCS[s]) return 0;
    if (s >= 4) return (WORD)((DLGPROC)PROCS[s])(TW(TB_CBW), TW(TB_CBMSG), TW(TB_CBP1), TD(TB_CBP2));
    return ((WNDPROC)PROCS[s])(TW(TB_CBW), TW(TB_CBMSG), TW(TB_CBP1), TD(TB_CBP2));
}

/* ---- Vollbild im Programm: 13h, Text, VESA 2.0 linear; INT 10h ueber den DPMI-Host, DAC ueber Ports ---------- */
typedef struct { DWORD edi, esi, ebp, res, ebx, edx, ecx, eax; WORD fl, es, ds, fs, gs, ip, cs, sp, ss; } RMR;
static WORD akt = 0xFFFF; static BYTE pal[768]; static WORD paln; static BYTE *lfb; static WORD lfbz;
static void outb(WORD p, BYTE v);
#pragma aux outb = "out dx, al" parm [dx] [al];
static int rmint(BYTE nr, RMR *m)
{
    R32 r; m->fl = 0; m->sp = m->ss = 0; m->fs = m->gs = 0;
    r.eax = 0x0300; r.ebx = nr; r.ecx = 0; r.edi = (DWORD)m; rd32_int31(&r);
    return !r.cf;
}
static int vesa(void)                             /* Modus 101h mit linearem Bildspeicher, einmal abbilden */
{
    RMR m; R32 r; BYTE *mi = TB + TB_XBUF + rd32_xp; DWORD phys, gr;
    if (!lfb) {
        { BYTE *z = (BYTE *)&m; WORD i; for (i = 0; i < sizeof m; i++) z[i] = 0; }
        m.eax = 0x4F01; m.ecx = 0x101; m.es = (WORD)(rd32_tlin >> 4); m.edi = TB_XBUF + rd32_xp;
        if (!rmint(0x10, &m) || (WORD)m.eax != 0x004F || !(mi[0] & 0x80)) return 0;
        phys = *(DWORD *)(mi + 0x28); lfbz = *(WORD *)(mi + 0x10); gr = ((DWORD)lfbz * 480 + 4095) & ~4095UL;
        r.eax = 0x0800; r.ebx = phys >> 16; r.ecx = phys & 0xFFFF; r.esi = gr >> 16; r.edi = gr & 0xFFFF; rd32_int31(&r);
        if (r.cf) return 0;
        lfb = rd32_lin(((r.ebx & 0xFFFF) << 16) | (r.ecx & 0xFFFF));
    }
    { BYTE *z = (BYTE *)&m; WORD i; for (i = 0; i < sizeof m; i++) z[i] = 0; }
    m.eax = 0x4F02; m.ebx = 0x4101;
    return rmint(0x10, &m) && (WORD)m.eax == 0x004F;
}
WORD RDAPI ScreenMode(WORD modus, SCREENINFO FAR *si)
{
    RMR m; WORD i;
    if (modus == SM_LFB) { if (!vesa()) return 0; }
    else if (modus == SM_13H || modus == SM_TEXT) {
        BYTE *z = (BYTE *)&m; for (i = 0; i < sizeof m; i++) z[i] = 0;
        m.eax = modus == SM_13H ? 0x13 : 0x03; rmint(0x10, &m);
    } else return 0;
    akt = modus;
    if (si) {
        si->modus = modus; si->w = modus == SM_TEXT ? 80 : modus == SM_13H ? 320 : 640; si->h = modus == SM_TEXT ? 25 : modus == SM_13H ? 200 : 480;
        si->bpp = modus == SM_TEXT ? 4 : 8; si->zeilenbytes = modus == SM_TEXT ? 160 : modus == SM_13H ? 320 : lfbz;
        si->seg = modus == SM_TEXT ? 0xB800 : modus == SM_13H ? 0xA000 : 0; si->ebenen = 1;
    }
    return 1;
}
BYTE *ScreenPtr(void) { return akt == SM_LFB ? lfb : akt == SM_13H ? rd32_lin(0xA0000) : akt == SM_TEXT ? rd32_lin(0xB8000) : 0; }
void RDAPI ScreenPalette(WORD erste, WORD n, const BYTE FAR *rgb)
{
    WORD i;
    if (erste > 255) return;
    if (erste + n > 256) n = 256 - erste;
    outb(0x3C8, (BYTE)erste);
    for (i = 0; i < n * 3; i++) { outb(0x3C9, rgb[i]); pal[erste * 3 + i] = rgb[i]; }
    if (erste + n > paln) paln = erste + n;
}
void RDAPI ScreenRestore(void) { if (akt != 0xFFFF) { ScreenMode(akt, 0); if (paln) ScreenPalette(0, paln, pal); } }

/* ---- 32-Bit-Module (.M32): Bild ab Offset 0, dahinter Exporte und Verschiebungen; Kopf siehe RD32M0.ASM ---------- */
typedef struct { char sig[4]; DWORD init, isize, msize, ver, tlin, base, f_call, f_alloc, f_free, f_smode, f_sptr, f_spal, f_srest, f_int31, p_ende, p_jb, p_8087; } M32KOPF;
extern char _8087, _real87;
static DWORD lies(short f, BYTE *z, DWORD n)
{
    DWORD r = 0; WORD w, k;
    while (r < n) { k = (WORD)(n - r > 32768UL ? 32768UL : n - r); w = FileRead(f, z + r, k); if (!w || w == 0xFFFF) break; r += w; if (w < k) break; }
    return r;
}
static void alle_mods(void);
void *rd32_modload(const char *datei)
{
    char pf[96]; const char *n = datei; short f; long gr; DWORD tab, i, *rel, nrel; BYTE *b, *t; M32KOPF k, *m; WORD j;
    for (j = 0; datei[j] && datei[j] != ':' && datei[j] != '\\'; j++) ;
    if (!datei[j] && InstRes(TW(TB_INST), pf, sizeof pf)) {   /* ohne Pfad: Verzeichnis des Programms */
        WORD e = 0, q; for (q = 0; pf[q]; q++) if (pf[q] == '\\' || pf[q] == ':') e = q + 1;
        for (q = 0; datei[q] && e + q < sizeof pf - 1; q++) pf[e + q] = datei[q];
        pf[e + q] = 0; n = pf;
    }
    if (nmod == 8 || (f = FileOpen(n, FO_READ)) < 0) { SetLastError(RD_ERR_NOTFOUND); return 0; }
    gr = FileSeek(f, 0, FS_END); FileSeek(f, 0, FS_SET);
    if (gr < (long)sizeof k || lies(f, (BYTE *)&k, sizeof k) != sizeof k || k.sig[0] != 'R' || k.sig[1] != '3' || k.sig[2] != '2' || k.sig[3] != 'M'
        || k.ver != 1 || k.isize > (DWORD)gr || k.msize < k.isize) { FileClose(f); SetLastError(RD_ERR_FORMAT); return 0; }
    tab = (DWORD)gr - k.isize;
    if (!(b = rd32_alloc(k.msize + tab + 4))) { FileClose(f); SetLastError(RD_ERR_NOMEM); return 0; }
    FileSeek(f, 0, FS_SET);
    if (lies(f, b, (DWORD)gr) != (DWORD)gr) { FileClose(f); rd32_free(b); SetLastError(RD_ERR_FORMAT); return 0; }
    FileClose(f);
    for (i = tab; i; i--) b[k.msize + i - 1] = b[k.isize + i - 1];   /* Tabellen hinter das BSS */
    for (i = k.isize; i < k.msize; i++) b[i] = 0;
    t = b + k.msize; i = *(DWORD *)t; t += 4;     /* Exporte ueberspringen: je DWORD Offset, Laenge, Name */
    while (i--) t += 5 + t[4];
    nrel = *(DWORD *)t; rel = (DWORD *)(t + 4);
    for (i = 0; i < nrel; i++) *(DWORD *)(b + rel[i]) += (DWORD)b;
    m = (M32KOPF *)b; m->msize = k.msize;          /* +12 ist eine Adresse (Ende BSS) und wurde mit verschoben */
    m->tlin = rd32_tlin; m->base = rd32_base; m->f_call = (DWORD)rd32_call; m->f_alloc = (DWORD)rd32_alloc; m->f_free = (DWORD)rd32_free;
    m->f_smode = (DWORD)ScreenMode; m->f_sptr = (DWORD)ScreenPtr; m->f_spal = (DWORD)ScreenPalette; m->f_srest = (DWORD)ScreenRestore; m->f_int31 = (DWORD)rd32_int31;
    for (j = 0; j < 24; j++) ((BYTE *)m->p_jb)[j] = ((BYTE *)rd32_jb)[j];   /* exit im Modul beendet das Programm */
    ((char *)m->p_8087)[0] = _8087; ((char *)m->p_8087)[1] = _real87;
    if (((int (*)(void))m->init)() != 0) { rd32_free(b); SetLastError(RD_ERR_NOTSUP); return 0; }
    mods[nmod++] = b; rd32_modende = alle_mods;
    return b;
}
void *rd32_modproc(void *mod, const char *name)
{
    M32KOPF *m = mod; BYTE *t; DWORD i; WORD j;
    if (!m) return 0;
    t = (BYTE *)m + m->msize; i = *(DWORD *)t; t += 4;
    for (; i; i--, t += 5 + t[4]) {
        for (j = 0; j < t[4] && name[j] == (char)t[5 + j]; j++) ;
        if (j == t[4] && !name[j]) return (BYTE *)m + *(DWORD *)t;
    }
    SetLastError(RD_ERR_NOTFOUND);
    return 0;
}
void rd32_modfree(void *mod)
{
    M32KOPF *m = mod; WORD i; void (*ende)(void);
    for (i = 0; i < nmod && mods[i] != mod; i++) ;
    if (i == nmod) return;
    mods[i] = mods[--nmod];
    if ((ende = *(void (**)(void))m->p_ende) != 0) ende();   /* atexit des Moduls, seine Dateien */
    rd32_free(mod);
}
static void alle_mods(void) { while (nmod) rd32_modfree(mods[nmod - 1]); }
