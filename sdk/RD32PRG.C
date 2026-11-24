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
/* Speicherschutz (PLAN_API 3b): Kopf +12 Bit 0 = Schutz-Format, Bit 1 = Vollzugriff (RD32C0V). Geschuetzt: DS/SS-Grenze
   = Ende des hoechsten eigenen Blocks (wird hier nachgezogen); alles unter dem Bild liegt ausserhalb (Offsets laufen
   ueber 4 GB), Zugriff dorthin = Ausnahme 0Dh, nur dieses Programm endet. */
extern BYTE rd32_schutz;
#pragma aux rd32_schutz "_*";
#define GESCHUETZT ((rd32_schutz & 3) == 1)
static DWORD lsl_ds(void);
#pragma aux lsl_ds = "mov dx, ds" "lsl eax, edx" value [eax] modify [edx];
static WORD get_ds(void);
#pragma aux get_ds = "mov ax, ds" value [ax];
static void seg_neu(void);
#pragma aux seg_neu = "mov ax, ds" "mov ds, ax" "mov es, ax" "mov ss, ax" modify [eax];
int rd32_grenze(DWORD lin, DWORD n)               /* [lin, lin+n) fuer nahe Zeiger erreichbar machen; 0 = geht nicht */
{
    R32 r; DWORD bis;
    if (!GESCHUETZT) return 1;
    if (lin < rd32_base || !n || lin - rd32_base + n - 1 < lin - rd32_base) return 0;
    bis = (lin - rd32_base + n - 1) | 0xFFF;
    if (bis <= lsl_ds()) return 1;
    r.eax = 0x0008; r.ebx = get_ds(); r.ecx = bis >> 16; r.edx = bis & 0xFFFF; rd32_int31(&r);
    if (r.cf) return 0;
    seg_neu();                                    /* neue Grenze in DS/ES/SS laden */
    return 1;
}
static void dpmi_frei(DWORD h) { R32 r; r.eax = 0x0502; r.esi = h >> 16; r.edi = h & 0xFFFF; rd32_int31(&r); }
void *rd32_alloc(DWORD n)
{
    R32 r; WORD i, k, nw = 0; DWORD weg[4], lin;
    for (i = 0; i < NBLK && blk[i].lin; i++) ;
    if (i == NBLK || !n) return 0;
    for (;;) {
        r.eax = 0x0501; r.ebx = n >> 16; r.ecx = n & 0xFFFF; rd32_int31(&r);
        if (r.cf) { lin = 0; break; }
        lin = ((r.ebx & 0xFFFF) << 16) | (r.ecx & 0xFFFF);
        if (rd32_grenze(lin, n)) break;
        if (nw == 4) { dpmi_frei(((r.esi & 0xFFFF) << 16) | (r.edi & 0xFFFF)); lin = 0; break; }
        weg[nw++] = ((r.esi & 0xFFFF) << 16) | (r.edi & 0xFFFF);   /* Block unter dem Bild: festhalten, neu anfordern */
    }
    for (k = 0; k < nw; k++) dpmi_frei(weg[k]);
    if (!lin) return 0;
    blk[i].lin = lin; blk[i].h = ((r.esi & 0xFFFF) << 16) | (r.edi & 0xFFFF);
    return rd32_lin(blk[i].lin);
}
BYTE __far *BiosPtr(void) { return LOWP(0); }      /* BIOS-Daten, Vektoren (0-4FFh) und ROM (F0000h): nur ueber LOW */
BYTE __far *LowPtr(DWORD lin) { return lin < 0x110000UL ? LOWP(lin) : (BYTE __far *)0; }
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
    TD(68) = 0;                                   /* Systemtakt: noch kein TaktSet (PLAN_API 2b) */
    { static char cmd[128]; for (i = 0; i < 127 && (cmd[i] = (char)TB[TB_CMD + i]) != 0; i++) ; cmd[i] = 0;   /* Befehlszeile nah kopieren */
      if (!setjmp(rd32_jb)) r = RdMain(TW(TB_INST), cmd, TW(TB_SHOW)); else r = rd32_ecode; }
    TD(68) = 0;                                   /* ab hier keine Takt-Rueckrufe mehr */
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
static WORD vw = 640, vh = 480, vb = 8;          /* zuletzt gesetzter VESA-Modus (ScreenRestore) */
static DWORD lfbphys, lfbgr;
static BYTE bank, *schat, *schat13; static DWORD schgr; static WORD wseg, wgran, wgr;   /* Bankumschaltung: Schattenpuffer, Fenster A */
static void null(void *p, WORD n) { BYTE *z = p; while (n--) *z++ = 0; }
static WORD modliste(WORD *l, WORD max)            /* VBE 4F00: Modusnummern (die Liste kann im Puffer selbst liegen) */
{
    RMR m; BYTE __far *vi = TB + TB_XBUF + rd32_xp; WORD __far *q; WORD n = 0;
    null(&m, sizeof m); vi[0] = 'V'; vi[1] = 'B'; vi[2] = 'E'; vi[3] = '2';
    m.eax = 0x4F00; m.es = (WORD)(rd32_tlin >> 4); m.edi = TB_XBUF + rd32_xp;
    if (!rmint(0x10, &m) || (WORD)m.eax != 0x004F || vi[0] != 'V' || vi[1] != 'E' || vi[5] < 1) return 0;   /* ab VBE 1.x (Baenke), linear ab 2.0 */
    q = (WORD __far *)LOWP(((DWORD)*(WORD __far *)(vi + 16) << 4) + *(WORD __far *)(vi + 14));
    while (n < max && *q != 0xFFFF) l[n++] = *q++;
    return n;
}
static BYTE __far *modinfo(WORD nr)                    /* VBE 4F01 in den Austauschpuffer, 0 = keiner */
{
    RMR m; BYTE __far *mi = TB + TB_XBUF + rd32_xp;
    null(&m, sizeof m); m.eax = 0x4F01; m.ecx = nr; m.es = (WORD)(rd32_tlin >> 4); m.edi = TB_XBUF + rd32_xp;
    return rmint(0x10, &m) && (WORD)m.eax == 0x004F ? mi : 0;
}
static WORD tiefe2(BYTE __far *mi, BYTE lin)           /* 8, 15, 16, 24, 32; 0 = passt nicht. lin: linear, sonst Fenster A beschreibbar */
{
    if (lin ? (mi[0] & 0x99) != 0x99 : (mi[0] & 0x1B) != 0x1B || (mi[2] & 5) != 5 || !*(WORD __far *)(mi + 4) || *(WORD __far *)(mi + 6) < 64 || !*(WORD __far *)(mi + 8)) return 0;   /* unterstuetzt, Zusatzangaben, Farbe, Grafik */
    if (mi[0x19] == 8 && mi[0x1B] == 4) return 8;
    if (mi[0x1B] != 6) return 0;
    if (mi[0x19] == 16 && mi[0x21] == 5) return 15;
    return mi[0x19] == 15 || mi[0x19] == 16 || mi[0x19] == 24 || mi[0x19] == 32 ? mi[0x19] : 0;
}
static WORD tiefe(BYTE __far *mi) { return tiefe2(mi, 1); }
WORD ScreenVesaList(VESAMODE *l, WORD max)
{
    WORD nr[128], n = modliste(nr, 128), i, k = 0, b; BYTE __far *mi;
    for (i = 0; i < n && k < max; i++)
        if ((mi = modinfo(nr[i])) != 0 && (b = tiefe(mi)) != 0) { l[k].nr = nr[i]; l[k].w = *(WORD __far *)(mi + 0x12); l[k].h = *(WORD __far *)(mi + 0x14); l[k].bpp = b; k++; }
    return k;
}
static int vesa(WORD w, WORD h, WORD bpp)         /* VESA-Modus nach Groesse und Farbtiefe: linear, sonst mit Baenken */
{
    RMR m; R32 r; WORD nr[128], n = modliste(nr, 128), i = n; BYTE __far *mi = 0; DWORD phys, gr; BYTE zw = (BYTE)(bpp >> 15);
    bpp &= 0xFF;
    if (!zw) for (i = 0; i < n; i++)
        if ((mi = modinfo(nr[i])) != 0 && *(WORD __far *)(mi + 0x12) == w && *(WORD __far *)(mi + 0x14) == h && tiefe(mi) == bpp) break;
    if (i == n) {                                 /* kein linearer Modus (VBE 1.2): Fenster A, Schattenpuffer, ScreenUpdate */
        for (i = 0; i < n; i++)
            if ((mi = modinfo(nr[i])) != 0 && *(WORD __far *)(mi + 0x12) == w && *(WORD __far *)(mi + 0x14) == h && tiefe2(mi, 0) == bpp) break;
        if (i == n) { SetLastError(RD_ERR_NOTSUP); return 0; }
        lfbz = *(WORD __far *)(mi + 0x10); wgran = *(WORD __far *)(mi + 4); wgr = *(WORD __far *)(mi + 6); wseg = *(WORD __far *)(mi + 8);
        if (wgr > 64) wgr = 64;
        gr = (DWORD)lfbz * h;
        if (!schat || gr > schgr) {               /* neu: leer; sonst bleibt der Inhalt (ScreenRestore) */
            DWORD *z; if (schat) rd32_free(schat);
            if (!(schat = rd32_alloc(gr))) { schgr = 0; SetLastError(RD_ERR_NOMEM); return 0; }
            schgr = gr; for (z = (DWORD *)schat, gr /= 4; gr; gr--) *z++ = 0;
        }
        null(&m, sizeof m); m.eax = 0x4F02; m.ebx = nr[i];
        if (!rmint(0x10, &m) || (WORD)m.eax != 0x004F) { SetLastError(RD_ERR_NOTSUP); return 0; }
        vw = w; vh = h; vb = bpp; bank = 1;
        return 1;
    }
    phys = *(DWORD __far *)(mi + 0x28); lfbz = *(WORD __far *)(mi + 0x10);
    if (*(WORD __far *)mi & 0x0400 && *(WORD __far *)(mi + 0x32)) lfbz = *(WORD __far *)(mi + 0x32);   /* VBE 3: Zeilenbytes linear */
    gr = ((DWORD)lfbz * h + 4095) & ~4095UL;
    if (!lfb || phys != lfbphys || gr > lfbgr) {  /* abbilden (DPMI 0.9 kennt kein Freigeben): nur wenn noetig, dann reichlich */
        if (gr < 0x300000UL) gr = 0x300000UL;
        r.eax = 0x0800; r.ebx = phys >> 16; r.ecx = phys & 0xFFFF; r.esi = gr >> 16; r.edi = gr & 0xFFFF; rd32_int31(&r);
        if (r.cf) { SetLastError(RD_ERR_NOMEM); return 0; }
        { DWORD l = ((r.ebx & 0xFFFF) << 16) | (r.ecx & 0xFFFF);
          if (!rd32_grenze(l, gr)) { SetLastError(RD_ERR_NOTSUP); return 0; }   /* LFB unter dem Bild: nahe nicht erreichbar */
          lfb = rd32_lin(l); lfbphys = phys; lfbgr = gr; }   /* geschuetzt: Grenze reicht dann bis zum LFB-Ende */
    }
    null(&m, sizeof m); m.eax = 0x4F02; m.ebx = 0x4000 | nr[i];
    if (!rmint(0x10, &m) || (WORD)m.eax != 0x004F) { SetLastError(RD_ERR_NOTSUP); return 0; }
    vw = w; vh = h; vb = bpp; bank = 0;
    return 1;
}
void ScreenUpdate(void)                           /* Schattenpuffer ueber Fenster A (64 KB) auf den Schirm */
{
    RMR m; DWORD n, off, k, i, *q; DWORD __far *z; WORD b = 0xFFFF, nb;
    if ((akt == SM_13H || akt == SM_TEXT) && schat13) {   /* geschuetzt: Schatten nach A0000h/B8000h (LOW) */
        n = akt == SM_13H ? 16000 : 1000; z = (DWORD __far *)LOWP(akt == SM_13H ? 0xA0000UL : 0xB8000UL); q = (DWORD *)schat13;
        for (i = 0; i < n; i++) z[i] = q[i];
        return;
    }
    if (akt != SM_LFB || !bank || !schat) return;
    n = (DWORD)lfbz * vh; z = (DWORD __far *)LOWP((DWORD)wseg << 4);
    for (off = 0; off < n; off += (DWORD)wgr << 10) {
        nb = (WORD)(off / ((DWORD)wgran << 10));
        if (nb != b) { null(&m, sizeof m); m.eax = 0x4F05; m.ebx = 0; m.edx = nb; rmint(0x10, &m); b = nb; }
        k = n - off < ((DWORD)wgr << 10) ? n - off : (DWORD)wgr << 10;
        q = (DWORD *)(schat + off);
        for (i = 0; i < k / 4; i++) z[i] = q[i];
    }
}
WORD ScreenModeVesa(WORD w, WORD h, WORD bpp, SCREENINFO *si)
{
    if (!vesa(w, h, bpp)) return 0;
    akt = SM_LFB;
    if (si) { si->modus = SM_LFB; si->w = w; si->h = h; si->bpp = bpp & 0xFF; si->zeilenbytes = lfbz; si->seg = 0; si->ebenen = 1; }
    return 1;
}
WORD RDAPI ScreenMode(WORD modus, SCREENINFO FAR *si)
{
    RMR m; WORD i;
    if (modus == SM_LFB) return ScreenModeVesa(640, 480, 8, si);
    if (modus == SM_13H || modus == SM_TEXT) {
        BYTE *z = (BYTE *)&m; for (i = 0; i < sizeof m; i++) z[i] = 0;
        if (GESCHUETZT && !schat13) {             /* 13h/Text: Schatten, ScreenUpdate zeigt ihn */
            DWORD *q; if (!(schat13 = rd32_alloc(64000UL))) { SetLastError(RD_ERR_NOMEM); return 0; }
            for (q = (DWORD *)schat13, i = 0; i < 16000; i++) q[i] = modus == SM_TEXT ? 0x07200720UL : 0;
        }
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
BYTE *ScreenPtr(void) { return akt == SM_LFB ? (bank ? schat : lfb) : akt == SM_13H || akt == SM_TEXT ? (schat13 ? schat13 : rd32_lin(akt == SM_13H ? 0xA0000UL : 0xB8000UL)) : 0; }   /* geschuetzt: Schatten, ScreenUpdate */
void RDAPI ScreenPalette(WORD erste, WORD n, const BYTE FAR *rgb)
{
    WORD i;
    if (erste > 255) return;
    if (erste + n > 256) n = 256 - erste;
    outb(0x3C8, (BYTE)erste);
    for (i = 0; i < n * 3; i++) { outb(0x3C9, rgb[i]); pal[erste * 3 + i] = rgb[i]; }
    if (erste + n > paln) paln = erste + n;
}
void RDAPI ScreenRestore(void)
{
    if (akt == SM_LFB) { if (!ScreenModeVesa(vw, vh, (WORD)(vb | (bank ? VESA_BANK : 0)), 0)) return; ScreenUpdate(); }   /* Baenke: Schatten bleibt, neu zeigen */
    else if (akt != 0xFFFF) { ScreenMode(akt, 0); ScreenUpdate(); }   /* geschuetzt: Schatten bleibt, neu zeigen */
    if (paln && (akt != SM_LFB || vb == 8)) ScreenPalette(0, paln, pal);
}

/* ---- 32-Bit-Module (.M32): Bild ab Offset 0, dahinter Exporte und Verschiebungen; Kopf siehe RD32M0.ASM ---------- */
typedef struct { char sig[4]; DWORD init, isize, msize, ver, tlin, base, f_call, f_alloc, f_free, f_smode, f_sptr, f_spal, f_srest, f_int31, p_ende, p_jb, p_8087, f_svesa, f_slist, f_supd; WORD lowsel, frei; } M32KOPF;
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
        || k.ver < 1 || k.ver > 3 || (GESCHUETZT && k.ver < 3) || k.isize > (DWORD)gr || k.msize < k.isize) { FileClose(f); SetLastError(RD_ERR_FORMAT); return 0; }
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
    m->f_svesa = (DWORD)ScreenModeVesa; m->f_slist = (DWORD)ScreenVesaList;
    if (k.ver >= 2) m->f_supd = (DWORD)ScreenUpdate;
    if (k.ver >= 3) m->lowsel = rd32_lowsel;       /* Fassung 3: Taskblock ueber LOW (Speicherschutz); aeltere nur ohne Schutz */
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
