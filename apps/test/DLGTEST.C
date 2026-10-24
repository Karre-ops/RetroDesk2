/* DLGTEST.C - Phase 4: Steuerelemente und Dialoge (RDCTL). test/dlg.sh bedient per Tastatur:
 *  1. Dialog 10: Name ergaenzen, Ton aus (Leertaste), Gross waehlen (Pfeil, Leertaste), Eingabe
 *  2. MsgBox OK/Abbruch: Esc
 *  3. Dialog 10 (neu, Ton an): Alt+T schaltet Ton aus, Esc bricht ab
 *  4. InputBox mit "Retro": X anhaengen, Eingabe
 *  5. Dialog 11 (Liste, 10 Rechner, 4 sichtbar): c, c, Ende, Pfeil hoch, Eingabe
 *  6. FileDialog Oeffnen (*.EXE): rd2 + Eingabe, Tab, d, Eingabe -> A:\RD2\DLGTEST.EXE
 *  7. FileDialog Speichern RD.EXE: Eingabe -> Rueckfrage Ersetzen, Esc, Esc -> abgebrochen
 *  8. Dialog 12: TEXT a b Eingabe c d, hoch, X; Tab SCROLL Bild ab + runter; Tab Tab Tab COMBO:
 *     runter, F4 (auf), runter, Eingabe (zu), Eingabe -> Text "abX\ncd", Stand 11, VGA
 * Am Ende: "DLGTEST: Fehler n" (0 = alles wie erwartet). */
#include "RD2.H"

static WORD fehler, lauf;
static void pruef(LPCSTR was, int ok) { DbgPrint(was); if (!ok) { fehler++; DbgPrint("DLGTEST: ^^^ FALSCH"); } }
static int gleich(LPCSTR a, LPCSTR b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static const char * const rech[] = { "Amiga", "Atari ST", "C64", "CPC 464", "Dragon 32", "IBM PC", "KC 85", "MSX", "Oric", "ZX81" };
static BOOL RDCALLBACK lproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    HWND l = DlgItem(d, 40); WORD i; char b[40];
    (void)p2;
    if (msg == M_INITDIALOG) {
        for (i = 0; i < 10; i++) SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)rech[i]);
        SendMessage(DlgItem(d, 41), C_SETPOS, 0, 60);
        pruef("DLGTEST: Liste 10 Eintraege, keine Auswahl", SendMessage(l, L_COUNT, 0, 0) == 10 && SendMessage(l, L_GETSEL, 0, 0) == 0xFFFF);
        DbgPrint("DLGTEST: Liste bereit");
    }
    if (msg == M_COMMAND && p1 == 40 && HIWORD(p2) == N_SELECT) DbgPrint("DLGTEST: N_SELECT");
    if (msg == M_COMMAND && p1 == ID_OK) {
        DlgGetText(d, 40, b, sizeof b); DbgPrint(b);
        pruef("DLGTEST: Auswahl 8 = Oric", SendMessage(l, L_GETSEL, 0, 0) == 8 && gleich(b, "Oric"));
        SendMessage(l, L_DELETE, 0, 0); DlgGetText(d, 40, b, sizeof b);
        pruef("DLGTEST: nach L_DELETE 0: 9 Eintraege, Auswahl 7 = Oric", SendMessage(l, L_COUNT, 0, 0) == 9 && SendMessage(l, L_GETSEL, 0, 0) == 7 && gleich(b, "Oric"));
        SendMessage(l, L_GETTEXT, 0, (DWORD)(LPSTR)b); pruef("DLGTEST: L_GETTEXT 0 = Atari ST", gleich(b, "Atari ST"));
    }
    return FALSE;
}
static BOOL RDCALLBACK nproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    HWND c = DlgItem(d, 52); char b[40];
    (void)p2;
    if (msg == M_INITDIALOG) {
        SendMessage(c, L_ADD, 0, (DWORD)(LPCSTR)"CGA"); SendMessage(c, L_ADD, 0, (DWORD)(LPCSTR)"HGC"); SendMessage(c, L_ADD, 0, (DWORD)(LPCSTR)"VGA");
        SendMessage(c, L_SETSEL, 0, 0);
        DlgGetText(d, 52, b, sizeof b); pruef("DLGTEST: COMBO vorbelegt CGA", gleich(b, "CGA"));
        DbgPrint("DLGTEST: Notiz bereit");
    }
    if (msg == M_COMMAND && p1 == 52 && HIWORD(p2) == N_SELECT) DbgPrint("DLGTEST: COMBO N_SELECT");
    if (msg == M_COMMAND && p1 == ID_OK) {
        DlgGetText(d, 50, b, sizeof b); pruef("DLGTEST: TEXT = abX\\ncd", gleich(b, "abX\ncd"));
        DbgHex("DLGTEST: SCROLL", (WORD)SendMessage(DlgItem(d, 51), C_GETPOS, 0, 0));
        pruef("DLGTEST: SCROLL Stand 11", SendMessage(DlgItem(d, 51), C_GETPOS, 0, 0) == 11);
        DlgGetText(d, 52, b, sizeof b); DbgPrint(b);
        pruef("DLGTEST: COMBO = VGA, Auswahl 2", gleich(b, "VGA") && SendMessage(c, L_GETSEL, 0, 0) == 2);
    }
    return FALSE;
}
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
    { char b[20] = "Retro"; BOOL r;
      DbgPrint("DLGTEST: Eingabe");
      r = InputBox(w, "Wie heisst der Rechner?", "Frage", b, sizeof b);
      DbgPrint(b); pruef("DLGTEST: InputBox OK, Retro + X", r && gleich(b, "RetroX")); }
    e = DialogRun(inst, 11, w, lproc, 0);
    DbgHex("DLGTEST: Dialog 3 ->", (WORD)e); pruef("DLGTEST: Dialog 3 endet mit OK", e == ID_OK);
    { FILEDLG fd; char b[80]; BOOL r;
      fd.groesse = sizeof fd; fd.flags = FD_OPEN | FD_MUSTEXIST; fd.titel = 0; fd.filter = "Programme|*.EXE|Alle|*.*|"; fd.datei = b; fd.len = sizeof b; b[0] = 0;
      DbgPrint("DLGTEST: Datei"); r = FileDialog(w, &fd); DbgPrint(b);
      pruef("DLGTEST: FileDialog -> A:\\RD2\\DLGTEST.EXE", r && gleich(b, "A:\\RD2\\DLGTEST.EXE"));
      fd.flags = FD_SAVE | FD_ASKOVER; gleich(b, b); { LPCSTR q = "A:\\RD2\\RD.EXE"; WORD i; for (i = 0; q[i]; i++) b[i] = q[i]; b[i] = 0; }
      DbgPrint("DLGTEST: Speichern"); r = FileDialog(w, &fd);
      pruef("DLGTEST: Speichern abgebrochen, Name unveraendert", !r && gleich(b, "A:\\RD2\\RD.EXE")); }
    e = DialogRun(inst, 12, w, nproc, 0);
    DbgHex("DLGTEST: Dialog 4 ->", (WORD)e); pruef("DLGTEST: Dialog 4 endet mit OK", e == ID_OK);
    pruef("DLGTEST: CreateWindow BUTTON direkt", (e = (short)CreateWindow(CL_BUTTON, "Knopf", WS_CHILD | WS_VISIBLE, 200, 10, 60, 20, w, 5, inst)) != 0);
    DestroyWindow(w);
    DbgHex("DLGTEST: Fehler", fehler);
    return 0;
}
