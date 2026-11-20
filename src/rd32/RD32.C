/* RD32.MOD - RD2/32-Programme (PLAN_32BIT Schritt 4a, ab 386): DPMI-Anmeldung, 32-Bit-Bild laden, starten.
 * Der 16-Bit-Vorspann eines RD2/32-Programms (sdk/RD32STUB.C) ruft Run32 (Export 2). RD2 bleibt im Realmodus;
 * der 32-Bit-Code laeuft im geschuetzten Modus mit eigenem Code-/Datenselektor (Basis = Bild, Grenze 4 GB)
 * und ruft die RD2-API ueber die Bruecke in RD32A.ASM. Speicher fuer das Bild kommt vom DPMI-Host (XMS),
 * die 640 KB bleiben frei bis auf den Taskblock (3,4 KB je Programm). */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"
#include <i86.h>

#define RT_CODE32 11
#define TB_P2R 20
#define TB_R2P 28
#define TB_API 16
#define TB_PMESP 4
#define TB_CS32 32
#define TB_DS32 34
#define TB_RMCS 36
#define TB_RMDS 38
#define TB_RMAPI 40
#define TB_RMCB 42
#define TB_PMCB 44
#define TB_CBRES 52
#define TB_BASE 68
#define TB_LIN 72
#define TB_PMSTART 76
#define TB_THUNK 80
#define TB_INST 96
#define TB_SHOW 98
#define TB_HTASK 100
#define TB_CMD 192
#define TB_XBUF 320
#define XBUF 3072
#define TBSIZE (TB_XBUF + XBUF)

typedef struct { DWORD entry, r2p, p2r; WORD p2rsel, pcs, pds, flat, rmss, rmsp, rmds, rmcs, svcnr, err, stsize, svcsp;
                 DWORD a, b, c, d, api; WORD tab[16]; } GLOB;
extern GLOB g; extern BYTE g_stktop;
extern DWORD _rd2api;
extern void __far dpmi_enter(WORD seg);
extern void __far svc_call(WORD nr);
extern void __far rm_start(WORD tb);
extern void __far rm_api(void); extern void __far rm_cbret(void);
extern void __far wp0(void); extern void __far wp1(void); extern void __far wp2(void); extern void __far wp3(void);
#pragma aux dpmi_enter "*_" parm [ax];
#pragma aux svc_call "*_" parm [ax];
#pragma aux rm_start "*_" parm [ax];

typedef struct { WORD typ, nr, lang, flags; DWORD off, len; } RESDIR;
static BYTE bereit;                               /* 0 = noch nicht versucht, 1 = angemeldet, 2 = kein Host */

static WORD dpmi(void)
{
    WORD paras = 0, eo = 0, es_ = 0, ok = 0, seg; BYTE fl = 0; HMEM h; BYTE FAR *p;
    if (bereit) return bereit == 1;
    bereit = 2;
    _asm {
        push es
        mov  ax, 1687h
        int  2Fh
        or   ax, ax
        jnz  kein
        mov  fl, bl
        mov  paras, si
        mov  eo, di
        mov  es_, es
        mov  ok, 1
    kein:
        pop  es
    }
    if (!ok) { DbgPrint("RD32: kein DPMI-Host"); return 0; }
    if (!(fl & 1)) { DbgPrint("RD32: DPMI-Host ohne 32 Bit"); return 0; }
    g.entry = ((DWORD)es_ << 16) | eo;
    seg = 0;
    if (paras) {                                  /* Host-Daten: bleibt fest, solange RD2 laeuft */
        if (!(h = MemAlloc((DWORD)paras * 16 + 16, MEM_FIXED)) || !(p = (BYTE FAR *)MemLock(h))) return 0;
        seg = FP_SEG(p) + ((FP_OFF(p) + 15) >> 4);
    }
    g.api = _rd2api; g.svcsp = (WORD)&g_stktop;
    dpmi_enter(seg);
    if (g.err) { DbgHex("RD32: DPMI-Anmeldung Fehler", g.err); return 0; }
    DbgHex("RD32: DPMI angemeldet, Zustand", g.stsize);
    bereit = 1;
    return 1;
}
#define W(o) (*(WORD FAR *)(tb + (o)))
#define D(o) (*(DWORD FAR *)(tb + (o)))
static DWORD fp(void (__far *f)(void)) { return ((DWORD)FP_SEG(f) << 16) | FP_OFF(f); }

static short RDEXPORT run32(HINST inst, LPCSTR cmd, WORD show)
{
    static char pfad[80];
    static struct { char sig[4]; WORD api, flags, stack, kb; char name[16]; WORD ver, nexp, nres, oexp, ores; } rb;
    static struct { char sig[4]; DWORD start, cb, isize, msize, ssize, tlin, base; } h;
    static RESDIR d;
    DWORD blk, off = 0, len = 0, gesamt, lin, hdl, sels, pos; WORD k, n, tseg; short f, r = -1; HMEM htb; BYTE FAR *tb;
    if (!dpmi()) { MsgBox(0, "Kein DPMI-Host (32 Bit) gefunden.\nCWSDPMI -p vor RD2 laden.", "RD2/32", MB_OK | MB_ERROR); return -1; }
    if (!(blk = InstRes(inst, pfad, sizeof pfad))) return -1;
    if ((f = FileOpen(pfad, FO_READ | FO_DENYNONE)) < 0) return -1;
    if (FileSeek(f, (long)blk, 0) >= 0 && FileRead(f, &rb, sizeof rb) == sizeof rb && FileSeek(f, (long)(blk + rb.ores), 0) >= 0)
        for (k = 0; k < rb.nres && FileRead(f, &d, sizeof d) == sizeof d; k++)
            if (d.typ == RT_CODE32 && d.nr == 1) { off = blk + d.off; len = d.len; break; }
    if (!len || FileSeek(f, (long)off, 0) < 0 || FileRead(f, &h, sizeof h) != sizeof h || h.sig[0] != 'R' || h.sig[3] != '!') {
        DbgPrint("RD32: kein 32-Bit-Bild"); FileClose(f); return -1;
    }
    if (len > h.isize) len = h.isize;
    gesamt = (h.msize + h.ssize + 4095) & ~4095UL;
    if (!(htb = MemAlloc(TBSIZE + 16, MEM_FIXED | MEM_ZEROINIT)) || !(tb = (BYTE FAR *)MemLock(htb))) { FileClose(f); return -1; }
    tseg = FP_SEG(tb) + ((FP_OFF(tb) + 15) >> 4); tb = (BYTE FAR *)MK_FP(tseg, 0);
    for (n = 0; n < TBSIZE; n++) tb[n] = 0;
    g.a = gesamt; svc_call(1);
    if (g.err) { DbgHex("RD32: kein Speicher (DPMI)", g.err); FileClose(f); MemFree(htb); return -1; }
    lin = g.b; hdl = g.c; sels = g.d;
    DbgHex("RD32: Bild bei", lin); DbgHex("RD32: Bytes", gesamt);
    for (pos = 0; pos < len; pos += n) {          /* Bild in Stuecken ueber den Taskblock nach oben kopieren */
        n = (WORD)(len - pos > XBUF ? XBUF : len - pos);
        FileSeek(f, (long)(off + pos), 0);
        if (FileRead(f, tb + TB_XBUF, n) != n) break;
        g.a = ((DWORD)tseg << 4) + TB_XBUF; g.b = lin + pos; g.c = n; svc_call(2);
    }
    FileClose(f);
    if (pos < len) { DbgPrint("RD32: Lesefehler"); goto weg; }
    if (h.msize > len) { g.b = lin + len; g.c = h.msize - len; svc_call(4); }   /* BSS loeschen */
    D(TB_XBUF) = ((DWORD)tseg << 4); D(TB_XBUF + 4) = lin;   /* Kopf +24: Taskblock linear, Bildbasis */
    g.a = ((DWORD)tseg << 4) + TB_XBUF; g.b = lin + 24; g.c = 8; svc_call(2);
    W(TB_RMDS) = tseg; W(TB_RMCS) = FP_SEG(rm_api); W(TB_RMAPI) = FP_OFF(rm_api); W(TB_RMCB) = FP_OFF(rm_cbret);
    D(TB_API) = g.api; D(TB_P2R) = g.p2r; W(TB_P2R + 4) = g.p2rsel; D(TB_R2P) = g.r2p;
    W(TB_CS32) = (WORD)sels; W(TB_DS32) = (WORD)(sels >> 16);
    D(TB_PMCB) = h.cb; D(TB_PMSTART) = h.start; D(TB_PMESP) = gesamt - 16; D(TB_BASE) = lin; D(TB_LIN) = (DWORD)tseg << 4;
    D(TB_THUNK) = fp(wp0); D(TB_THUNK + 4) = fp(wp1); D(TB_THUNK + 8) = fp(wp2); D(TB_THUNK + 12) = fp(wp3);
    W(TB_INST) = inst; W(TB_SHOW) = show; W(TB_HTASK) = TaskSelf();
    for (n = 0; cmd && cmd[n] && n < 127; n++) tb[TB_CMD + n] = cmd[n];
    for (k = 0; k < 8 && g.tab[2 * k]; k++) ;
    if (k == 8) goto weg;
    g.tab[2 * k] = W(TB_HTASK); g.tab[2 * k + 1] = tseg;
    DbgPrint("RD32: Start");
    rm_start(tseg);
    r = (short)D(TB_CBRES);
    DbgHex("RD32: Ende", (WORD)r);
    g.tab[2 * k] = 0;
weg:
    g.c = hdl; g.d = sels; svc_call(3);
    MemFree(htb);
    return r;
}
static void RDEXPORT ende(void) { }
static struct { WORD n; FARPROC f[2]; } ext = { 2, { (FARPROC)ende, (FARPROC)run32 } };
void FAR * RDAPI RdModInit(void) { return &ext; }
