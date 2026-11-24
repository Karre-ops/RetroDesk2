/* RD32LIB.C - Bibliothek fuer RD2/32-Programme (32 Bit), PLAN_32BIT Stufe 4b.
 * API-Aufrufe: Woerter in den Taskblock, rd32_call (RD32C0) wechselt in den Realmodus, RD32.MOD ruft den Kern.
 * Die Aufrufstuecke aller einfachen Funktionen erzeugt tools/mk32api.py (RD32API.C); hier stehen Kern der Bruecke,
 * Funktionen mit Rueckrufen oder Zeigern in Strukturen, Puffer-Bitmap. Gilt fuer Programme und 32-Bit-Module (RD32M0);
 * nur im Programm (RD32PRG.C): DPMI-Speicher, Einsprung, Rueckruf-Verteiler, Vollbild, Module laden. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"
#include "RD32INT.H"



static WORD a[24], na;                            /* rd32_xp und die Rueckruf-Prozeduren liegen im Taskblock (RD32INT.H): Module teilen sie */

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

static int platz(void *p, WORD von)               /* Rueckruf-Platz fuer p (gleiche Prozedur: gleicher Platz) */
{
    WORD i;
    for (i = von; i < von + 4; i++) if (PROCS[i] == p) return i;
    for (i = von; i < von + 4; i++) if (!PROCS[i]) { PROCS[i] = p; return i; }
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
/* Systemtakt fuer RD2/32 (PLAN_API 2b): Eintraege im Programm, RD32 ruft im IRQ 0 rd32_takttramp (RD32C0), die ruft
   rd32_taktlauf auf eigenem Stapel. TB+68 = Einsprung, 0 = keiner angemeldet. */
typedef struct { TAKTPROC f; WORD teil, zahl; } TAKT32;
static TAKT32 t32[8];
extern void rd32_takttramp(void); extern DWORD rd32_hattakt;
void rd32_taktlauf(void)
{
    int i;
    for (i = 0; i < 8; i++) if (t32[i].f && ++t32[i].zahl >= t32[i].teil) { t32[i].zahl = 0; t32[i].f(); }
}
WORD TaktSet(TAKTPROC f, WORD teiler)
{
    int i;
    if (!f) { SetLastError(RD_ERR_HANDLE); return 0; }
    if (!rd32_hattakt) { SetLastError(RD_ERR_NOTSUP); return 0; }   /* im Modul: das Programm meldet an */
    for (i = 0; i < 8; i++) if (!t32[i].f) {
        t32[i].teil = teiler ? teiler : 1; t32[i].zahl = 0; t32[i].f = f;
        TD(72) = ~(DWORD)rd32_takttramp; TD(68) = (DWORD)rd32_takttramp;   /* RD32 prueft TB+72 = ~TB+68 */
        return (WORD)(i + 1);
    }
    SetLastError(RD_ERR_NOMEM); return 0;
}
void TaktKill(WORD nr)
{
    int i;
    if (!nr || nr > 8) return;
    t32[nr - 1].f = 0;
    for (i = 0; i < 8 && !t32[i].f; i++) ;
    if (i == 8) { TD(68) = 0; TD(72) = 0; }
}
