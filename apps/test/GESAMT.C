/* GESAMT.C - Gesamttest PLAN_API (Luecke 3): Vollbild 13h, Maus (ScreenMouse), Tasten (KeyState), Systemtakt (TaktSet),
 * Klang (SndMixPlay in Schleife, sonst SndNote) zugleich, dazu Alt+Tab weg und zurueck. Dieselbe Quelle als GESAMT.EXE
 * (16 Bit) und GESAMT32.EXE (RD2/32, apps/test32/GESAMT32.C). Befehlszeile K (nur 32 Bit): vorher neunmal ein Kind mit
 * TaktSet starten und per TaskKill beenden (RD32-Plaetze werden frei). C: Kind (TaktSet, Schleife ohne API).
 * Ablauf und Pruefungen: test/gesamt.sh. Ausgaben "GESAMT: ..." ueber DbgPrint/DbgHex. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#ifdef __386__
#include "RD2_32.H"
#define LOWB BYTE __far *
#define TAKTF static void RDAPI
static BYTE __far *bios(void) { return BiosPtr(); }
#else
#include <i86.h>
#include "RD2.H"
#define LOWB BYTE FAR *
#define TAKTF static void RDAPI __loadds
static BYTE FAR *bios(void) { return (BYTE FAR *)0L; }
#endif

static SCREENINFO si; static HMEM ton; static WORD xmax, ymax, beide, tk, kl, weg, zur, maus2, phase;
static HWND haupt; static HTASK zweit; static volatile DWORD takte; static DWORD tweg, tzur, t0;
TAKTF takt(void) { takte++; }
static void klang(void)
{
    if (ton && SndMixPlay(0, ton, 8000, SM_LOOP)) { if (!kl) DbgPrint("GESAMT: Mischer klingt"); kl = 1; }
    else { SndNote(0, 69, 100); if (!kl) DbgPrint("GESAMT: Note klingt (kein Mischer)"); kl = 2; }
}
static void bild(void)
{
    WORD x, y;
#ifdef __386__
    BYTE *v = ScreenPtr();
    for (y = 0; y < 200; y++) for (x = 0; x < 320; x++) v[y * 320 + x] = (BYTE)(x ^ y);
    ScreenUpdate();
#else
    BYTE FAR *v = (BYTE FAR *)MK_FP(si.seg, 0);
    for (y = 0; y < 200; y++) for (x = 0; x < 320; x++) v[y * 320 + x] = (BYTE)(x ^ y);
#endif
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_MOUSEMOVE:
        if (w != haupt) return 0;
        if (LOWORD(p2) > xmax) xmax = LOWORD(p2);
        if (HIWORD(p2) > ymax) ymax = HIWORD(p2);
        if (zur && !maus2) { maus2 = 1; DbgHex("GESAMT: Maus nach Alt+Tab bei", p2); }
        return 0;
    case M_BUTTONDOWN: if (w == haupt) DbgHex("GESAMT: Klick bei", p2); return 0;
    case M_TIMER:
        if (KeyState(KS_SCAN + TS_LINKS) && KeyState(KS_SCAN + TS_RECHTS) && !beide) {
            beide = 1; DbgPrint("GESAMT: links und rechts zugleich");
            DbgHex("GESAMT: Mischkanaele dabei", SndMixState(0));
        }
        if (!tk && TickCount() - t0 >= 1000) { tk = 1; DbgHex("GESAMT: Takte in 1 s (145)", takte); }
        if (zur == 1 && TickCount() - tzur >= 500) { zur = 2; DbgHex("GESAMT: Takte 0,5 s nach Alt+Tab (72)", takte - tweg); }
        return 0;
    case M_SCREENLOST: weg++; DbgPrint("GESAMT: SCREENLOST"); return 0;
    case M_SCREENBACK:
        ScreenRestore(); ScreenMouse(w, 320, 200); bild();
        DbgHex("GESAMT: SCREENBACK, BIOS-Modus", bios()[0x449]);
        if (!SndMixState(0)) klang();             /* Klang nach dem Anhalten neu, falls weg */
        DbgHex("GESAMT: Mischkanaele nach Alt+Tab", SndMixState(0));
        tweg = takte; tzur = TickCount(); zur = 1; return 0;
    case M_KEY: if (p1 == K_ESC) {
        DbgHex("GESAMT: xmax", xmax); DbgHex("GESAMT: ymax", ymax); DbgHex("GESAMT: Alt+Tab weg", weg);
        SndMixStop(0); SndNoteOff(0); ScreenRelease(w); haupt = 0; DestroyWindow(w); return 0; } break;
    case M_DESTROY: if (w == haupt || !haupt) PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
#ifdef __386__
static void warte(WORD ms) { DWORD t = TickCount(); while (TickCount() - t < ms) TaskYield(); }
#endif
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HWND w; WORD i; LOWB p; WORD nr;
    (void)show;
    if (cmd && cmd[0] == 'C') {                   /* Kind: Takt anmelden, dann ohne API rechnen bis TaskKill */
        volatile DWORD x = 0;
        if (TaktSet(takt, 1)) DbgPrint("GESAMT: Kind Takt ok"); else DbgHex("GESAMT: Kind Takt FEHLER", LastError());
        for (;;) x++;
    }
#ifdef __386__
    if (cmd && cmd[0] == 'K') {                   /* neunmal Kind starten und abschiessen: mehr als die 8 RD32-Plaetze */
        char pf[80]; HTASK t; WORD n;
        InstRes(inst, pf, sizeof pf);
        for (i = 0, n = 0; i < 9; i++) { t = TaskExec(pf, "C", SW_SHOW); if (t) n++; warte(400); if (t) TaskKill(t, 0); warte(200); }
        DbgHex("GESAMT: Kinder gestartet und beendet", n);
    }
#endif
    if ((ton = MemAlloc(4000, MEM_MOVABLE)) != 0) {   /* 0,5 s Rechteck 500 Hz bei 8 kHz */
        p = (LOWB)MemLock(ton); for (i = 0; i < 4000; i++) p[i] = (BYTE)(i & 8 ? 0xA0 : 0x60); MemUnlock(ton);
    }
    wc.name = cmd && cmd[0] == 'W' ? "GesamtW" : "Gesamt"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) return 1;
    if (cmd && cmd[0] == 'W') {                   /* zweites Programm: nur ein Fenster als Ziel fuer Alt+Tab */
        w = CreateWindow("GesamtW", "Zweites Programm", WS_TITLE | WS_BORDER | WS_VISIBLE, 300, 200, 200, 60, 0, 0, inst);
        haupt = w; while (GetMessage(&m)) DispatchMessage(&m); return 0;
    }
    { char pf[80]; InstRes(inst, pf, sizeof pf); zweit = TaskExec(pf, "W", SW_SHOW); TaskYield(); TaskYield(); }
    w = CreateWindow("Gesamt", "Gesamttest", WS_TITLE | WS_BORDER | WS_VISIBLE, 0, 0, 200, 60, 0, 0, inst);
    if (!w || !ScreenAcquire(w) || !ScreenMode(SM_13H, &si)) { DbgPrint("GESAMT: Vollbild FEHLER"); return 2; }
    haupt = w; ScreenMouse(w, 320, 200); bild();
    nr = TaktSet(takt, 1); DbgHex("GESAMT: TaktSet", nr);
    klang(); t0 = TickCount();
    TimerSet(w, 1, 20);
    DbgPrint("GESAMT: Vollbild");
    while (GetMessage(&m)) DispatchMessage(&m);
    TaktKill(nr); if (ton) MemFree(ton); if (zweit) TaskKill(zweit, 0);
    DbgPrint("GESAMT: Ende");
    (void)phase;
    return 0;
}
