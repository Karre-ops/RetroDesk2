/* RD32LIB.C - Bibliothek fuer RD2/32-Programme (32 Bit), PLAN_32BIT Schritt 4a.
 * API-Aufrufe: Woerter in den Taskblock, rd32_call (RD32C0) wechselt in den Realmodus, RD32.MOD ruft den Kern.
 * Zeiger fuer den Kern: Daten in den Austauschpuffer des Taskblocks (Stapel, damit Rueckrufe darin weiterarbeiten). */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"

extern DWORD rd32_tlin, rd32_base;
#pragma aux rd32_tlin "_*";
#pragma aux rd32_base "_*";
void rd32_call(void);
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
#define TB_INST 96
#define TB_SHOW 98
#define TB_ARGS 128
#define TB_CMD 192
#define TB_XBUF 320
#define XBUF 3072
#define TB_KLASSE (TB_XBUF + XBUF - 64)           /* 4 Klassennamen zu 16 Zeichen, bleiben liegen */

static WORD xp;                                   /* belegt im Austauschpuffer */
static WNDPROC procs[4];
static WORD a[24], na;

void *rd32_lin(DWORD lin) { return (void *)(lin - rd32_base); }
static DWORD fp(const void *p, WORD n)            /* Daten unter 1 MB, liefert seg:off fuer den Kern */
{
    BYTE *z = TB + TB_XBUF + xp; const BYTE *q = p; WORD i; DWORD r = (rd32_tlin << 12 & 0xFFFF0000UL) | (TB_XBUF + xp);
    if (xp + n > XBUF - 64) { n = 0; }
    for (i = 0; i < n; i++) z[i] = q ? q[i] : 0;
    xp += (n + 1) & ~1;
    return r;
}
static WORD slen(const char *s) { WORD n = 0; while (s[n]) n++; return n; }
static DWORD fs(const char *s) { return s ? fp(s, slen(s) + 1) : 0; }
static void w1(WORD v) { a[na++] = v; }
static void w2(DWORD v) { a[na++] = (WORD)(v >> 16); a[na++] = (WORD)v; }   /* pascal: hohes Wort zuerst */
static DWORD ruf(WORD nr)
{
    WORD i;
    for (i = 0; i < na; i++) TW(TB_ARGS + 2 * i) = a[i];
    TW(TB_NW) = na; TW(TB_NR) = nr; na = 0;
    rd32_call();
    return TD(TB_RES);
}
static void *tbp(DWORD sf) { return TB + (WORD)sf; }   /* seg:off im Taskblock -> Zeiger */
DWORD rd32_api(WORD nr, WORD nw, const WORD *w) { WORD i; for (i = 0; i < nw; i++) w1(w[i]); return ruf(nr); }

int rd32_main(void) { return RdMain(TW(TB_INST), (const char *)(TB + TB_CMD), TW(TB_SHOW)); }
DWORD rd32_cb(void)
{
    WORD s = TW(TB_CBSLOT);
    if (s > 3 || !procs[s]) return 0;
    return procs[s](TW(TB_CBW), TW(TB_CBMSG), TW(TB_CBP1), TD(TB_CBP2));
}

void DbgPrint(const char *t) { WORD s = xp; w2(fs(t)); ruf(0x02C); xp = s; }
void DbgHex(const char *t, DWORD v) { WORD s = xp; w2(fs(t)); w2(v); ruf(0x02D); xp = s; }
WORD LastError(void) { return (WORD)ruf(0x00C); }
DWORD TickCount(void) { return ruf(0x01B); }
void TaskYield(void) { ruf(0x003); }
short MsgBox(HWND o, const char *text, const char *titel, WORD fl)
{
    WORD s = xp; short r; w1(o); w2(fs(text)); w2(fs(titel)); w1(fl); r = (short)ruf(0x103); xp = s; return r;
}
WORD RegisterClass(const WNDCLASS *wc)
{
    #pragma pack(push, 1)
    struct { DWORD name, proc; WORD stil, daten, inst, symbol; DWORD hinter; } k;
    #pragma pack(pop)
    WORD s = xp, i, n, r; char *kn;
    for (i = 0; i < 4 && procs[i] && procs[i] != wc->proc; i++) ;
    if (i == 4) return 0;
    kn = (char *)TB + TB_KLASSE + 16 * i;
    for (n = 0; n < 15 && wc->name[n]; n++) kn[n] = wc->name[n];
    kn[n] = 0;
    k.name = (rd32_tlin << 12 & 0xFFFF0000UL) | (TB_KLASSE + 16 * i); k.proc = TD(TB_THUNK + 4 * i);
    k.stil = wc->stil; k.daten = wc->daten; k.inst = wc->inst; k.symbol = wc->symbol; k.hinter = wc->hinter;
    procs[i] = wc->proc;
    w2(fp(&k, sizeof k)); r = (WORD)ruf(0x0A9); xp = s;
    if (!r) procs[i] = 0;
    return r;
}
HWND CreateWindow(const char *kl, const char *ti, DWORD st, short x, short y, short w, short h, HWND el, WORD id, HINST in)
{
    WORD s = xp; HWND r; w2(fs(kl)); w2(fs(ti)); w2(st); w1(x); w1(y); w1(w); w1(h); w1(el); w1(id); w1(in);
    r = (HWND)ruf(0x0AA); xp = s; return r;
}
BOOL DestroyWindow(HWND w) { w1(w); return (BOOL)ruf(0x0AB); }
DWORD DefWindowProc(HWND w, WORD m, WORD p1, DWORD p2) { w1(w); w1(m); w1(p1); w2(p2); return ruf(0x0BD); }
BOOL GetMessage(MSG *m)
{
    WORD s = xp; DWORD p = fp(0, sizeof *m); BOOL r; w2(p); r = (BOOL)ruf(0x010);
    { BYTE *q = tbp(p), *z = (BYTE *)m; WORD i; for (i = 0; i < sizeof *m; i++) z[i] = q[i]; }
    xp = s; return r;
}
DWORD DispatchMessage(const MSG *m) { WORD s = xp; DWORD r; w2(fp(m, sizeof *m)); r = ruf(0x014); xp = s; return r; }
void PostQuit(WORD c) { w1(c); ruf(0x015); }
static void back(void *z, DWORD p, WORD n) { BYTE *q = tbp(p), *d = z; WORD i; for (i = 0; i < n; i++) d[i] = q[i]; }
HDC BeginPaint(HWND w, PAINT *ps) { WORD s = xp; DWORD p = fp(ps, sizeof *ps); HDC r; w1(w); w2(p); r = (HDC)ruf(0x0B5); back(ps, p, sizeof *ps); xp = s; return r; }
void EndPaint(HWND w, PAINT *ps) { WORD s = xp; w1(w); w2(fp(ps, sizeof *ps)); ruf(0x0B6); xp = s; }
WORD ScreenAcquire(HWND w) { w1(w); return (WORD)ruf(0x0E0); }
void ScreenRelease(HWND w) { w1(w); ruf(0x0E1); }

/* ---- Vollbild im Programm (wie VOLL.C): INT 10h reicht der DPMI-Host an das BIOS weiter, DAC ueber Ports ---- */
static WORD akt = 0xFFFF; static BYTE pal[768]; static WORD paln;
static void bios(WORD ax);
#pragma aux bios = "int 10h" parm [eax] modify [eax ebx ecx edx esi edi];
static void outb(WORD p, BYTE v);
#pragma aux outb = "out dx, al" parm [dx] [al];
WORD ScreenMode(WORD modus, SCREENINFO *si)
{
    if (modus != SM_13H && modus != SM_TEXT) return 0;
    bios(modus == SM_13H ? 0x13 : 0x03);
    akt = modus;
    if (si) {
        si->modus = modus; si->w = modus == SM_13H ? 320 : 80; si->h = modus == SM_13H ? 200 : 25;
        si->bpp = modus == SM_13H ? 8 : 4; si->zeilenbytes = modus == SM_13H ? 320 : 160;
        si->seg = modus == SM_13H ? 0xA000 : 0xB800; si->ebenen = 1; si->bild = rd32_lin((DWORD)si->seg << 4);
    }
    return 1;
}
void ScreenPalette(WORD erste, WORD n, const BYTE *rgb)
{
    WORD i;
    if (erste > 255) return;
    if (erste + n > 256) n = 256 - erste;
    outb(0x3C8, (BYTE)erste);
    for (i = 0; i < n * 3; i++) { outb(0x3C9, rgb[i]); pal[erste * 3 + i] = rgb[i]; }
    if (erste + n > paln) paln = erste + n;
}
void ScreenRestore(void) { if (akt != 0xFFFF) { ScreenMode(akt, 0); if (paln) ScreenPalette(0, paln, pal); } }
