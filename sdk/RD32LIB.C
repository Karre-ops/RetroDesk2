/* RD32LIB.C - Bibliothek fuer RD2/32-Programme (32 Bit), PLAN_32BIT Stufe 4b.
 * API-Aufrufe: Woerter in den Taskblock, rd32_call (RD32C0) wechselt in den Realmodus, RD32.MOD ruft den Kern.
 * Die Aufrufstuecke aller einfachen Funktionen erzeugt tools/mk32api.py (RD32API.C); hier stehen Kern der Bruecke,
 * Funktionen mit Rueckrufen oder Zeigern in Strukturen, Puffer-Bitmap. Gilt fuer Programme und 32-Bit-Module (RD32M0);
 * nur im Programm (RD32PRG.C): DPMI-Speicher, Einsprung, Rueckruf-Verteiler, Vollbild, Module laden. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"
#include "RD32INT.H"



static WORD a[24], na;                            /* rd32_xp und die Rueckruf-Prozeduren liegen im Taskblock (RD32INT.H): Module teilen sie */

WORD rd32_slen(const char *s) { WORD n = 0; while (s[n]) n++; return n; }
void *rd32_lin(DWORD lin) { return (void *)(lin - rd32_base); }
void *rd32_fp2p(DWORD sf) { return sf ? rd32_lin(((sf >> 16) << 4) + (WORD)sf) : 0; }
static DWORD p2fp(const void *p) { DWORD l = (DWORD)p + rd32_base; return ((l >> 4) << 16) | (l & 15); }
/* Speicherschutz (PLAN_API 3b): Speicher unter 1 MB nur ueber LOW */
BYTE __far *rd32_fp2f(DWORD sf) { return sf ? LOWP(((sf >> 16) << 4) + (WORD)sf) : (BYTE __far *)0; }
static DWORD f2fp(const void __far *p) { DWORD l = FOFF(p); return ((l >> 4) << 16) | (l & 15); }
void rd32_n2f(BYTE __far *z, const void *q, DWORD n) { const BYTE *s = q; while (n--) *z++ = *s++; }
void rd32_f2n(void *z, const BYTE __far *q, DWORD n) { BYTE *d = z; while (n--) *d++ = *q++; }
LPCSTR rd32_strkopie(DWORD sf)                    /* Texte aus dem Kern (StrGet): Kopie in einen von 4 Ringpuffern */
{
    static char rp[4][256]; static BYTE nr; BYTE __far *q = rd32_fp2f(sf); char *d; WORD i;
    if (!q) return 0;
    d = rp[nr++ & 3];
    for (i = 0; i < 255 && (d[i] = (char)q[i]) != 0; i++) ;
    d[i] = 0; return d;
}
void rd32_w1(WORD v) { if (na < 24) a[na++] = v; }
void rd32_w2(DWORD v) { if (na < 23) { a[na++] = (WORD)(v >> 16); a[na++] = (WORD)v; } }   /* a[24]: nie darueber */   /* pascal: hohes Wort zuerst */
static BYTE pinfehl;                              /* ein Zeiger liess sich nicht unter 1 MB bringen: Aufruf nicht ausfuehren */
DWORD rd32_ruf(WORD nr)
{
    WORD i;
    if (pinfehl) { pinfehl = 0; na = 0; SetLastError(RD_ERR_NOMEM); return 0; }   /* Audit 24.11.: nie 0:0 an den Kern */
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
    if (l < 0x100000UL && n <= 0x100000UL - l) return b->sf = p2fp(p);   /* ohne Ueberlauf (Audit 24.11.) */   /* liegt schon unter 1 MB (MemLock, Taskblock) */
    if (n <= (DWORD)(XFREI - rd32_xp)) {
        b->sf = ((rd32_tlin << 12) & 0xFFFF0000UL) | (TB_XBUF + rd32_xp);
        rd32_n2f(TB + TB_XBUF + rd32_xp, p, n); rd32_xp += (WORD)((n + 1) & ~1UL);
    } else {                                      /* gross: fester Block unter 1 MB, nur fuer diesen Aufruf */
        BYTE __far *q; if (n > 65520UL || !(b->tmp = MemAlloc(n, MEM_FIXED))) { b->tmp = 0; pinfehl = 1; return 0; }
        if (!(q = MemLock(b->tmp))) { MemFree(b->tmp); b->tmp = 0; pinfehl = 1; return 0; }
        rd32_n2f(q, p, n); b->sf = f2fp(q);
    }
    b->aus = (BYTE)aus;
    return b->sf;
}
void rd32_unpin(PIN *b, WORD nb)
{
    WORD i;
    for (i = 0; i < nb; i++, b++) {
        if (b->aus == 1) rd32_f2n(b->p, rd32_fp2f(b->sf), b->n);
        else if (b->aus == 2) { BYTE __far *q = rd32_fp2f(b->sf); char *z = b->p; DWORD k; for (k = 0; k < b->n && (z[k] = q[k]) != 0; k++) ; }
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
    WORD s = rd32_xp, n, r; char __far *kn; int i; PIN b;
    if (!wc || !wc->name || (i = platz((void *)wc->proc, 0)) < 0) { SetLastError(RD_ERR_FULL); return 0; }
    kn = (char __far *)TB + TB_KLASSE + 16 * i;
    for (n = 0; n < 15 && wc->name[n]; n++) kn[n] = wc->name[n];
    kn[n] = 0;
    k.name = f2fp(kn); k.proc = thunk(i);
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
void RDAPI MemFixedFree(LPLOW p) { rd32_w2(p ? f2fp(p) : 0); rd32_ruf(0x041); }
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
    { WORD __far *bh = MemLock(b->bmp); if (!bh) { BmpFree(b->bmp); b->bmp = 0; rd32_free(b->pix); b->pix = 0; return FALSE; } b->streifen = bh[1]; bpr = bh[3]; MemUnlock(b->bmp); (void)bpr; }
    return TRUE;
}
void BufShow(RDBUF *b, HDC dc, short x, short y)
{
    WORD y0, n, yy, xx, bpp, bpr; BYTE *q; BYTE __far *d; WORD __far *bh; HDC s;
    if (!b->bmp) return;
    for (y0 = 0; y0 < b->h; y0 += n) {
        n = b->h - y0 < b->streifen ? b->h - y0 : b->streifen;
        if (!(bh = MemLock(b->bmp))) return;
        bpp = bh[2] & 0xFF; bpr = bh[3]; d = (BYTE __far *)bh + 8;
        for (yy = 0; yy < n; yy++, d += bpr) {
            q = b->pix + (DWORD)(y0 + yy) * b->w;
            if (bpp == 8) rd32_n2f(d, q, b->w);
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
