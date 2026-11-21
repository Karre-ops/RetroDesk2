/* FEN32.C - RD2/32 Stufe 4b: Fensterprogramm in 32 Bit. Zeichnen (Rechteck, Linie, Text), Puffer-Bitmap mit
 * Farbverlauf, Zeitgeber, Dialog aus der .RC mit 32-Bit-Dialogprozedur, Datei 100 KB schreiben und lesen,
 * RD2.INI lesen/schreiben. Ohne Befehlszeile startet es sich nach dem Dialog ein zweites Mal ("2"): zwei 32-Bit-Tasks zugleich. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"

static HINST hi; static char nr = '1'; static WORD takte; static RDBUF buf; static BYTE *daten;
static void log(const char *t) { char z[48]; WORD i = 0; const char *q = "FEN32 x: "; while (*q) z[i++] = *q++; z[6] = nr; while (*t && i < 46) z[i++] = *t++; z[i] = 0; DbgPrint(z); }
static void loghex(const char *t, DWORD v) { char z[48]; WORD i = 0; const char *q = "FEN32 x: "; while (*q) z[i++] = *q++; z[6] = nr; while (*t && i < 46) z[i++] = *t++; z[i] = 0; DbgHex(z, v); }

static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    (void)p2;
    if (msg == M_INITDIALOG) { log("Dialog offen"); DlgSetText(d, 10, "Text aus 32 Bit"); return TRUE; }
    if (msg == M_COMMAND && (p1 == ID_OK || p1 == ID_CANCEL)) {
        char t[32]; DlgGetText(d, 10, t, sizeof t); log(t[0] == 'T' ? "Dialog Text zurueck ok" : "Dialog Text FEHLER");
        DialogEnd(d, (short)p1); return TRUE;
    }
    return FALSE;
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_PAINT: {
        PAINT ps; RECT r = { 4, 4, 196, 30 }; HDC dc = BeginPaint(w, &ps);
        SetPen(dc, C_BLACK); SetBrush(dc, RGB(255, 255, 0), PAT_SOLID); FillRect(dc, &r);
        MoveTo(dc, 4, 34); LineTo(dc, 196, 34);
        SetTextColor(dc, RGB(0, 0, 255)); SetBkMode(dc, BK_TRANSPARENT); TextOut(dc, 10, 10, "RD2/32 Fenster", 14);
        BufShow(&buf, dc, 4, 40);
        EndPaint(w, &ps);
        log("gezeichnet");
        return 0; }
    case M_TIMER:
        takte++;
        if (takte == 3) loghex("Takt", takte);
        if (takte == 12) { loghex("Takt", takte); DestroyWindow(w); }
        return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HWND w; WORD x, y; DWORD i, bad = 0; short f; char pf[80], t[16];
    (void)show; hi = inst;
    if (cmd[0] == '2') nr = '2';
    log("Start");
    if (nr == '1') {                              /* Datei 100 KB aus dem 32-Bit-Speicher, gross: ueber feste Bloecke */
        daten = rd32_alloc(102400UL);
        for (i = 0; i < 102400UL; i++) daten[i] = (BYTE)(i * 7 + (i >> 9));
        if ((f = FileCreate("FEN32.DAT", 0)) >= 0) { FileWrite(f, daten, 51200); FileWrite(f, daten + 51200, 51200); FileClose(f); }
        for (i = 0; i < 102400UL; i++) daten[i] = 0;
        if ((f = FileOpen("FEN32.DAT", FO_READ)) >= 0) { FileRead(f, daten, 60000); FileRead(f, daten + 60000, 42400); FileClose(f); }
        for (i = 0; i < 102400UL; i++) if (daten[i] != (BYTE)(i * 7 + (i >> 9))) bad++;
        loghex("Datei 100 KB, Abweichungen", bad); FileDelete("FEN32.DAT");
        ProfileSetStr("FEN32.INI", "Test", "Wert", "zweiunddreissig");
        ProfileGetStr("FEN32.INI", "Test", "Wert", "", t, sizeof t);
        log(t[0] == 'z' && t[14] == 'g' ? "Profil ok" : "Profil FEHLER"); FileDelete("FEN32.INI");
    }
    wc.name = nr == '1' ? "Fen32" : "Fen32b";   /* Klassen gehoeren einem Task */ wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) { loghex("RegisterClass Fehler", LastError()); return 1; }
    if (!BufCreate(&buf, 192, 64)) log("Puffer FEHLER");
    for (y = 0; y < 64; y++) for (x = 0; x < 192; x++) buf.pix[y * 192 + x] = (BYTE)(x / 12);   /* 16 Farbstreifen */
    w = CreateWindow(wc.name, nr == '1' ? "RD2/32 eins" : "RD2/32 zwei", WS_TITLE | WS_BORDER | WS_VISIBLE,
                     nr == '1' ? 20 : 260, 60, 210, 140, 0, 0, inst);
    if (!w) { loghex("CreateWindow Fehler", LastError()); return 2; }
    TimerSet(w, 1, 250);
    if (nr == '1') {
        loghex("Dialog Ergebnis", (WORD)DialogRun(inst, 20, w, dproc, 0));
        takte = 0;                                /* zweiter Task erst nach dem Dialog (Fokus), Takte neu zaehlen */
        InstRes(inst, pf, sizeof pf);
        if (TaskExec(pf, "2", SW_SHOW)) log("zweiter Task gestartet"); else loghex("TaskExec Fehler", LastError());
    }
    while (GetMessage(&m)) DispatchMessage(&m);
    BufFree(&buf);
    log("Ende");
    return 0;
}
