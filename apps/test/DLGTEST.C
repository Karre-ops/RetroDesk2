/* DLGTEST.C - Phase 4: Steuerelemente und Dialoge (RDCTL). test/dlg.sh bedient per Tastatur:
 *  1. Dialog 10: Name ergaenzen, Ton aus (Leertaste), Gross waehlen (Pfeil, Leertaste), Eingabe
 *  2. MsgBox OK/Abbruch: Esc
 *  3. Dialog 10 (neu, Ton an): Alt+T schaltet Ton aus, Esc bricht ab
 * Am Ende: "DLGTEST: Fehler n" (0 = alles wie erwartet). */
#include "RD2.H"

static WORD fehler, lauf;
static void pruef(LPCSTR was, int ok) { DbgPrint(was); if (!ok) { fehler++; DbgPrint("DLGTEST: ^^^ FALSCH"); } }
static int gleich(LPCSTR a, LPCSTR b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    char b[40]; BOOL ok;
    (void)p2;
    if (msg == M_INITDIALOG) {
        if (lauf == 1) { DlgSetText(d, 20, "Karsten"); DlgSetInt(d, 99, 5); }
        pruef("DLGTEST: Ton vorbelegt an", DlgChecked(d, 21) == 1);
        pruef("DLGTEST: Klein vorbelegt", DlgChecked(d, 31) == 1 && DlgChecked(d, 32) == 0);
        pruef("DLGTEST: DlgItem findet OK", DlgItem(d, 1) != 0 && DlgItem(d, 77) == 0);
        DbgPrint("DLGTEST: bereit");
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK && lauf == 1) {
        DlgGetText(d, 20, b, sizeof b); DbgPrint(b);
        pruef("DLGTEST: Name = Karsten!", gleich(b, "Karsten!"));
        pruef("DLGTEST: Ton aus", DlgChecked(d, 21) == 0);
        pruef("DLGTEST: Gross gewaehlt, Klein nicht", DlgChecked(d, 32) == 1 && DlgChecked(d, 31) == 0);
        DlgSetText(d, 20, "-42"); pruef("DLGTEST: DlgGetInt -42", DlgGetInt(d, 20, &ok) == -42 && ok);
        DlgSetText(d, 20, "4x"); DlgGetInt(d, 20, &ok); pruef("DLGTEST: DlgGetInt 4x ungueltig", !ok);
    }
    if (msg == M_COMMAND && p1 == ID_CANCEL && lauf == 2)
        pruef("DLGTEST: Alt+T schaltet Ton aus", DlgChecked(d, 21) == 0);
    return FALSE;
}
static DWORD RDCALLBACK wproc(HWND h, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps;
    if (msg == M_PAINT) { BeginPaint(h, &ps); TextOut(ps.dc, 8, 8, "Hauptfenster", -1); EndPaint(h, &ps); return 0; }
    return DefWindowProc(h, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cl, WORD show)
{
    WNDCLASS wc; HWND w; short e;
    (void)cl; (void)show;
    wc.name = "DlgTest"; wc.proc = wproc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    w = CreateWindow("DlgTest", "Dialogtest", WS_NORMAL | WS_VISIBLE, 4, 30, 300, 120, 0, 1, inst);
    lauf = 1; e = DialogRun(inst, 10, w, dproc, 0);
    DbgHex("DLGTEST: Dialog 1 ->", (WORD)e); pruef("DLGTEST: Dialog 1 endet mit OK", e == ID_OK);
    pruef("DLGTEST: Hauptfenster wieder bedienbar", !(GetWindowData(w, WD_STIL) & WS_DISABLED));
    DbgPrint("DLGTEST: Meldung");
    e = MsgBox(w, "Alles gespeichert. Soll das Programm weiterlaufen?", 0, MB_OKCANCEL | MB_INFO);
    DbgHex("DLGTEST: MsgBox ->", (WORD)e); pruef("DLGTEST: MsgBox Esc = Abbruch", e == ID_CANCEL);
    lauf = 2; e = DialogRun(inst, 10, w, dproc, 0);
    DbgHex("DLGTEST: Dialog 2 ->", (WORD)e); pruef("DLGTEST: Dialog 2 Esc = Abbruch", e == ID_CANCEL);
    pruef("DLGTEST: CreateWindow BUTTON direkt", (e = (short)CreateWindow(CL_BUTTON, "Knopf", WS_CHILD | WS_VISIBLE, 200, 10, 60, 20, w, 5, inst)) != 0);
    DestroyWindow(w);
    DbgHex("DLGTEST: Fehler", fehler);
    return 0;
}
