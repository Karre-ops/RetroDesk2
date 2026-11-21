/* RD32LIB.C - Bibliothek fuer RD2/32-Programme (32 Bit), PLAN_32BIT Stufe 4b.
 * API-Aufrufe: Woerter in den Taskblock, rd32_call (RD32C0) wechselt in den Realmodus, RD32.MOD ruft den Kern.
 * Die Aufrufstuecke aller einfachen Funktionen erzeugt tools/mk32api.py (RD32API.C); hier stehen Kern der Bruecke,
 * Funktionen mit Rueckrufen oder Zeigern in Strukturen, DPMI-Speicher, Puffer-Bitmap und Vollbild. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"
#include "RD32INT.H"

extern DWORD rd32_tlin, rd32_base;
#pragma aux rd32_tlin "_*";
#pragma aux rd32_base "_*";
void rd32_call(void);
typedef struct { DWORD eax, ebx, ecx, edx, esi, edi, cf; } R32;
void rd32_int31(R32 *r);
#define TB ((BYTE *)(rd32_tlin - rd32_base))
#define TW(o) (*(volatile WORD *)(TB + (o)))
#define TD(o) (*(volatile DWORD *)(TB + (o)))
#define TB_NR 8
#define TB_NW 10
#define TB_RES 12
#define TB_CBSLOT 56
#define TB_CBW 58
#define TB_CBMSG 60
#define TB_CBP1 62
#define TB_CBP2 64
#define TB_THUNK 80
#define TB_THUNK2 104
#define TB_INST 96
#define TB_SHOW 98
#define TB_ARGS 128
#define TB_CMD 192
#define TB_XBUF 320
#define XBUF 3072
#define TB_KLASSE (TB_XBUF + XBUF - 64)           /* 4 Klassennamen zu 16 Zeichen, bleiben liegen */
#define XFREI (XBUF - 64)

WORD rd32_xp;                                     /* belegt im Austauschpuffer (Stapel, Rueckrufe arbeiten darueber) */
static void *procs[8];                            /* 0-3 Fensterklassen, 4-7 Dialoge */
static WORD a[24], na;

static void kopie(void *z, const void *q, DWORD n) { BYTE *d = z; const BYTE *s = q; while (n--) *d++ = *s++; }
WORD rd32_slen(const char *s) { WORD n = 0; while (s[n]) n++; return n; }
void *rd32_lin(DWORD lin) { return (void *)(lin - rd32_base); }
void *rd32_fp2p(DWORD sf) { return sf ? rd32_lin(((sf >> 16) << 4) + (WORD)sf) : 0; }
static DWORD p2fp(const void *p) { DWORD l = (DWORD)p + rd32_base; return ((l >> 4) << 16) | (l & 15); }
void rd32_w1(WORD v) { a[na++] = v; }
void rd32_w2(DWORD v) { a[na++] = (WORD)(v >> 16); a[na++] = (WORD)v; }   /* pascal: hohes Wort zuerst */
DWORD rd32_ruf(WORD nr)
{
    WORD i;
    for (i = 0; i < na; i++) TW(TB_ARGS + 2 * i) = a[i];
    TW(TB_NW) = na; TW(TB_NR) = nr; na = 0;
    rd32_call();
    return TD(TB_RES);
}
DWORD rd32_pin(PIN *b, const void *p, DWORD n, int aus)
{
    DWORD l;
    b->p = (void *)p; b->n = n; b->aus = 0; b->tmp = 0; b->sf = 0;
    if (!p) return 0;
    l = (DWORD)p + rd32_base;
    if (l + n <= 0x100000UL) return b->sf = p2fp(p);   /* liegt schon unter 1 MB (MemLock, Taskblock) */
    if (n <= (DWORD)(XFREI - rd32_xp)) {
        b->sf = ((rd32_tlin << 12) & 0xFFFF0000UL) | (TB_XBUF + rd32_xp);
        kopie(TB + TB_XBUF + rd32_xp, p, n); rd32_xp += (WORD)((n + 1) & ~1UL);
    } else {                                      /* gross: fester Block unter 1 MB, nur fuer diesen Aufruf */
        BYTE *q; if (n > 65520UL || !(b->tmp = MemAlloc(n, MEM_FIXED)) || !(q = MemLock(b->tmp))) { b->tmp = 0; return 0; }
        kopie(q, p, n); b->sf = p2fp(q);
    }
    b->aus = (BYTE)aus;
    return b->sf;
}
void rd32_unpin(PIN *b, WORD nb)
{
    WORD i;
    for (i = 0; i < nb; i++, b++) {
        if (b->aus == 1) kopie(b->p, rd32_fp2p(b->sf), b->n);
        else if (b->aus == 2) { char *q = rd32_fp2p(b->sf), *z = b->p; DWORD k; for (k = 0; k < b->n && (z[k] = q[k]) != 0; k++) ; }
        if (b->tmp) { MemUnlock(b->tmp); MemFree(b->tmp); }
    }
}

/* ---- DPMI-Speicher -------------------------------------------------------------------------------------- */
static struct { DWORD lin, h; } blk[16];
void *rd32_alloc(DWORD n)
{
    R32 r; WORD i;
    for (i = 0; i < 16 && blk[i].lin; i++) ;
    if (i == 16 || !n) return 0;
    r.eax = 0x0501; r.ebx = n >> 16; r.ecx = n & 0xFFFF; rd32_int31(&r);
    if (r.cf) return 0;
    blk[i].lin = ((r.ebx & 0xFFFF) << 16) | (r.ecx & 0xFFFF); blk[i].h = ((r.esi & 0xFFFF) << 16) | (r.edi & 0xFFFF);
    return rd32_lin(blk[i].lin);
}
void rd32_free(void *p)
{
    R32 r; WORD i; DWORD l = (DWORD)p + rd32_base;
    for (i = 0; i < 16; i++) if (blk[i].lin && blk[i].lin == l) {
        r.eax = 0x0502; r.esi = blk[i].h >> 16; r.edi = blk[i].h & 0xFFFF; rd32_int31(&r); blk[i].lin = 0;
    }
}

/* ---- Einsprung und Rueckrufe -------------------------------------------------------------------------------- */
unsigned long rd32_jb[6]; int rd32_ecode; void (*rd32_beiende)(void);   /* exit (RD32C): zurueck hierher */
int setjmp(unsigned long *b);
int rd32_main(void)
{
    WORD i; int r;
    if (!setjmp(rd32_jb)) r = RdMain(TW(TB_INST), (LPCSTR)(TB + TB_CMD), TW(TB_SHOW)); else r = rd32_ecode;
    if (rd32_beiende) rd32_beiende();             /* atexit, Dateien schliessen, stdout leeren */
    for (i = 0; i < 16; i++) if (blk[i].lin) rd32_free(rd32_lin(blk[i].lin));
    return r;
}
DWORD rd32_cb(void)
{
    WORD s = TW(TB_CBSLOT);
    if (s > 7 || !procs[s]) return 0;
    if (s >= 4) return (WORD)((DLGPROC)procs[s])(TW(TB_CBW), TW(TB_CBMSG), TW(TB_CBP1), TD(TB_CBP2));
    return ((WNDPROC)procs[s])(TW(TB_CBW), TW(TB_CBMSG), TW(TB_CBP1), TD(TB_CBP2));
}
static int platz(void *p, WORD von)               /* Rueckruf-Platz fuer p (gleiche Prozedur: gleicher Platz) */
{
    WORD i;
    for (i = von; i < von + 4; i++) if (procs[i] == p) return i;
    for (i = von; i < von + 4; i++) if (!procs[i]) { procs[i] = p; return i; }
    return -1;
}
static DWORD thunk(int i) { return TD(i < 4 ? TB_THUNK + 4 * i : TB_THUNK2 + 4 * (i - 4)); }

/* ---- von Hand: Rueckrufe, Zeiger in Strukturen ---------------------------------------------------------------- */
WORD RDAPI RegisterClass(const WNDCLASS FAR *wc)
{
    #pragma pack(push, 1)
    struct { DWORD name, proc; WORD stil, daten, inst, symbol; DWORD hinter; } k;
    #pragma pack(pop)
    WORD s = rd32_xp, n, r; char *kn; int i; PIN b;
    if (!wc || !wc->name || (i = platz((void *)wc->proc, 0)) < 0) { SetLastError(RD_ERR_FULL); return 0; }
    kn = (char *)TB + TB_KLASSE + 16 * i;
    for (n = 0; n < 15 && wc->name[n]; n++) kn[n] = wc->name[n];
    kn[n] = 0;
    k.name = p2fp(kn); k.proc = thunk(i);
    k.stil = wc->stil; k.daten = wc->daten; k.inst = wc->inst; k.symbol = wc->symbol; k.hinter = wc->hinter;
    rd32_w2(rd32_pin(&b, &k, sizeof k, 0)); r = (WORD)rd32_ruf(0x0A9); rd32_xp = s;
    return r;
}
static short dlg(WORD nr, HINST in, WORD id, const void *vorlage, DWORD vn, HWND owner, DLGPROC proc, DWORD param)
{
    WORD s = rd32_xp; short r; int i = proc ? platz((void *)proc, 4) : -1; PIN b;
    if (proc && i < 0) { SetLastError(RD_ERR_FULL); return -1; }
    rd32_w1(in);
    if (nr == 0x0F3) rd32_w1(id); else rd32_w2(rd32_pin(&b, vorlage, vn, 0));
    rd32_w1(owner); rd32_w2(proc ? thunk(i) : 0); rd32_w2(param);
    r = (short)rd32_ruf(nr);
    if (nr != 0x0F3) rd32_unpin(&b, 1);
    rd32_xp = s;
    return r;
}
short RDAPI DialogRun(HINST i, WORD id, HWND owner, DLGPROC proc, DWORD param) { return dlg(0x0F3, i, id, 0, 0, owner, proc, param); }
short RDAPI DialogIndirect(HINST i, const void FAR *v, HWND owner, DLGPROC proc, DWORD param) { return dlg(0x0FC, i, 0, v, 2048, owner, proc, param); }
BOOL RDAPI FileDialog(HWND owner, FILEDLG FAR *fd)
{
    WORD s = rd32_xp; BOOL r; FILEDLG k; PIN b[4];
    k = *fd;
    k.titel = (LPCSTR)rd32_pin(&b[0], fd->titel, fd->titel ? rd32_slen(fd->titel) + 1 : 0, 0);
    k.filter = (LPCSTR)rd32_pin(&b[1], fd->filter, fd->filter ? rd32_slen(fd->filter) + 1 : 0, 0);
    k.datei = (LPSTR)rd32_pin(&b[2], fd->datei, fd->len, 2);
    rd32_w1(owner); rd32_w2(rd32_pin(&b[3], &k, sizeof k, 0));
    r = (BOOL)rd32_ruf(0x104);
    rd32_unpin(b, 4); rd32_xp = s;
    return r;
}
HDC RDAPI PrnOpen(HWND owner, PRNJOB FAR *job)
{
    WORD s = rd32_xp; HDC r; PRNJOB k; PIN b[2];
    k = *job; k.name = (LPCSTR)rd32_pin(&b[0], job->name, job->name ? rd32_slen(job->name) + 1 : 0, 0);
    rd32_w1(owner); rd32_w2(rd32_pin(&b[1], &k, sizeof k, 1));
    r = (HDC)rd32_ruf(0x14C);
    rd32_unpin(&b[1], 1); job->kopien = k.kopien; job->von = k.von; job->bis = k.bis;
    job->dpi_x = k.dpi_x; job->dpi_y = k.dpi_y; job->breite = k.breite; job->hoehe = k.hoehe;
    rd32_xp = s;
    return r;
}
void RDAPI MemFixedFree(void FAR *p) { rd32_w2(p2fp(p)); rd32_ruf(0x041); }
FARPROC RDAPI ModProc(HMOD m, WORD nummer) { (void)m; (void)nummer; SetLastError(RD_ERR_NOTSUP); return 0; }   /* 16-Bit-Code: nicht aus 32 Bit */

/* ---- Puffer-Bitmap ------------------------------------------------------------------------------------------ */
BOOL BufCreate(RDBUF *b, WORD w, WORD h)
{
    WORD bpr, z;
    b->w = w; b->h = h; b->bmp = 0;
    if (!w || !h || !(b->pix = rd32_alloc((DWORD)w * h))) return FALSE;
    for (z = h; z; z--) {                         /* Streifen: so viele Zeilen, wie in 64 KB passen */
        if ((b->bmp = BmpCreate(w, z, 0)) != 0) break;
        if (z > 8) z = z / 2 + 1;
    }
    if (!b->bmp) { rd32_free(b->pix); b->pix = 0; return FALSE; }
    { WORD *bh = MemLock(b->bmp); b->streifen = bh[1]; bpr = bh[3]; MemUnlock(b->bmp); (void)bpr; }
    return TRUE;
}
void BufShow(RDBUF *b, HDC dc, short x, short y)
{
    WORD y0, n, yy, xx, bpp, bpr; BYTE *q, *d; WORD *bh; HDC s;
    if (!b->bmp) return;
    for (y0 = 0; y0 < b->h; y0 += n) {
        n = b->h - y0 < b->streifen ? b->h - y0 : b->streifen;
        bh = MemLock(b->bmp); bpp = bh[2] & 0xFF; bpr = bh[3]; d = (BYTE *)bh + 8;
        for (yy = 0; yy < n; yy++, d += bpr) {
            q = b->pix + (DWORD)(y0 + yy) * b->w;
            if (bpp == 8) kopie(d, q, b->w);
            else if (bpp == 4) { for (xx = 0; xx < b->w; xx += 2) d[xx >> 1] = (BYTE)((q[xx] << 4) | (xx + 1 < b->w ? q[xx + 1] & 15 : 0)); }
            else { for (xx = 0; xx < bpr; xx++) d[xx] = 0;
                   for (xx = 0; xx < b->w; xx++) if (q[xx] >= 8) d[xx >> 3] |= (BYTE)(0x80 >> (xx & 7)); }
        }
        MemUnlock(b->bmp);
        if ((s = BmpDC(b->bmp)) != 0) { BitBlt(dc, x, (short)(y + y0), b->w, n, s, 0, 0, ROP_COPY); DCFree(s); }
    }
}
void BufFree(RDBUF *b) { if (b->bmp) BmpFree(b->bmp); if (b->pix) rd32_free(b->pix); b->bmp = 0; b->pix = 0; }

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
