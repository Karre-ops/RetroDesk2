/* SCHUTZ32.C - RD2/32 Zeitscheiben und Absturzschutz (PLAN_32BIT, 21.11.2026). Befehlszeile:
 *   (leer)  Hauptprogramm: startet "S" (Endlosschleife ohne API-Aufruf), zaehlt 8 Zeitgebertakte (beweist die Zeitscheiben),
 *           beendet S mit TaskKill, startet dann "D" (Division durch 0) und "P" (Seitenfehler), wartet je auf deren Ende
 *           und zaehlt weiter (beweist: RD2 und dieses Programm laufen nach dem Absturz weiter). */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"

static HINST hi; static WORD takte, stufe; static HTASK ts; static volatile int null = 0;
static void log(const char *t) { DbgPrint(t); }
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    char pf[80];
    if (msg == M_TIMER) {
        takte++;
        if (stufe == 0 && takte == 1) { InstRes(hi, pf, sizeof pf); ts = TaskExec(pf, "S", SW_SHOW); log(ts ? "SCHUTZ32: S gestartet" : "SCHUTZ32: S FEHLER"); }
        if (stufe == 0 && takte == 9) { log("SCHUTZ32: 8 Takte waehrend S rechnet"); TaskKill(ts, 0); stufe = 1; takte = 0; }
        if (stufe == 1 && takte == 2) { InstRes(hi, pf, sizeof pf); TaskExec(pf, "D", SW_SHOW); log("SCHUTZ32: D gestartet"); }
        if (stufe == 1 && takte == 8) { log("SCHUTZ32: nach D noch am Leben"); stufe = 2; takte = 0; }
        if (stufe == 2 && takte == 2) { InstRes(hi, pf, sizeof pf); TaskExec(pf, "P", SW_SHOW); log("SCHUTZ32: P gestartet"); }
        if (stufe == 2 && takte == 8) { log("SCHUTZ32: nach P noch am Leben"); DestroyWindow(w); }
        return 0;
    }
    if (msg == M_DESTROY) { PostQuit(0); return 0; }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HWND w; volatile DWORD x = 0; DWORD *p;
    (void)show; hi = inst;
    if (cmd[0] == 'S') { for (;;) x++; }                     /* nie ein API-Aufruf */
    if (cmd[0] == 'D') { log("SCHUTZ32: D teilt durch 0"); x = 5 / null; log("SCHUTZ32: D FEHLER (kein Absturz)"); return 0; }
    if (cmd[0] == 'P') { log("SCHUTZ32: P schreibt ins Leere"); p = (DWORD *)0x7FFFFFF0UL; *p = 1; log("SCHUTZ32: P FEHLER (kein Absturz)"); return 0; }
    wc.name = "Schutz32"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) return 1;
    if (!(w = CreateWindow("Schutz32", "Schutz", WS_TITLE | WS_BORDER | WS_VISIBLE, 20, 60, 200, 60, 0, 0, inst))) return 2;
    TimerSet(w, 1, 250);
    while (GetMessage(&m)) DispatchMessage(&m);
    log("SCHUTZ32: Ende");
    return 0;
}
