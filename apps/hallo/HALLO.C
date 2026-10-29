/* HALLO.C - Beispiel fuer RetroDesk 2 in C (Open Watcom): Fenster mit Text und Menue,
 * zaehlt Mausklicks, Esc oder Datei > Beenden schliesst. Exitcode = Zahl der Klicks.
 * Bauen (Open Watcom, DOS oder Cross):
 *   wcc -0 -mm -os -s -zl -bt=dos HALLO.C
 *   wasm RD2START.ASM
 *   wlink format dos name HALLO file RD2START,HALLO library RD2.LIB,clibm.lib option nodefaultlibs
 *   RDRC HALLO.RC HALLO.EXE "Hallo"
 * Befehlszeile "T": meldet ueber DbgPrint (test/pascal.sh). */
#include "RD2.H"

#define CM_ENDE 100
static HINST hi; static WORD klicks; static BYTE test;

static void zahl(char *d, WORD v)                 /* ohne sprintf: kein DOS aus der clib */
{
    char t[6]; int i = 5; t[i] = 0;
    do t[--i] = (char)('0' + v % 10); while (v /= 10);
    while ((*d++ = t[i++]) != 0) ;
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; char s[24];
    switch (msg) {
    case M_PAINT:
        BeginPaint(w, &ps);
        SetBkMode(ps.dc, BK_TRANSPARENT); SetTextColor(ps.dc, C_BLACK);
        TextOut(ps.dc, 10, 10, StrGet(hi, 2), -1);        /* "Hallo aus C!" (Ressource, DE/EN) */
        { LPCSTR k = StrGet(hi, 3); int i = 0; while (k[i] && i < 12) { s[i] = k[i]; i++; } zahl(s + i, klicks); }
        TextOut(ps.dc, 10, 30, s, -1);
        EndPaint(w, &ps);
        if (test) { DbgPrint("CHALLO: Paint"); DbgPrint(s); }
        return 0;
    case M_BUTTONDOWN: klicks++; InvalidateRect(w, 0, TRUE); return 0;
    case M_KEY: if (p1 == K_ESC) { DestroyWindow(w); return 0; } break;
    case M_COMMAND: if (p1 == CM_ENDE) { DestroyWindow(w); return 0; } break;
    case M_DESTROY: PostQuit(klicks); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HWND fe;
    (void)show; hi = inst; test = (BYTE)(cmd && cmd[0] == 'T');
    wc.name = "Hallo"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) return 1;
    fe = CreateWindow("Hallo", StrGet(inst, 1), WS_TITLE | WS_CLOSE | WS_BORDER | WS_VISIBLE, 100, 100, 440, 200, 0, 0, inst);
    if (!fe) return 2;
    MenuSet(fe, MenuLoad(inst, 1));
    if (test) DbgPrint("CHALLO: Fenster da");
    while (GetMessage(&m)) DispatchMessage(&m);
    return (short)klicks;
}
