/* RECHNER.C - Zubehoer Rechner (RD1 1.2: Grundrechenarten, Wurzel, Prozent, Vorzeichen, Rueckschritt, Tastatur oder Maus).
 * Rechenkern RKERN.C aus RD1: Dezimal mit 18 Stellen ohne Gleitkomma (genau, kein 8087-Emulator noetig);
 * mit 8087 steht das wie in RD1 im Titel. Befehlszeile "T": meldet die Anzeige nach jeder Taste (test/rechner.sh). */
#include "RD2.H"
#include "RKERN.C"

#define BW 40
#define BG 6
static HINST hi; static HWND fe; static short kh, bh; static BYTE test;
static const char * const lbl[20] = { "7","8","9","/","C",  "4","5","6","*","Wz",
                                      "1","2","3","-","%",  "0",",","+-","+","=" };
static const char code[20] = { '7','8','9','/','C',  '4','5','6','*','S',
                               '1','2','3','-','%',  '0','.','N','+','=' };
static void anzeige(void)
{
    RECT r; GetClientRect(fe, &r); r.t = BG; r.b = (short)(BG + kh + 8); r.l = 8; r.r -= 8;
    InvalidateRect(fe, &r, FALSE);
}
static void taste(char k)
{
    calc_key(k); anzeige();
    if (test) { char t[32] = "RECHNER: "; strcat(t, calc_disp); DbgPrint(t); }
}
static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; HDC d; RECT r; char s[24]; WORD i;
    switch (msg) {
    case M_PAINT:
        d = BeginPaint(w, &ps); GetClientRect(w, &r);
        r.t = BG; r.b = (short)(BG + kh + 8); r.l = 8; r.r -= 8;
        SetBrush(d, C_WHITE, PAT_SOLID); FillRect(d, &r); SetPen(d, C_BLACK); Frame(d, &r);
        SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
        for (i = 0; calc_disp[i]; i++) s[i] = calc_disp[i] == '.' ? ',' : calc_disp[i];   /* deutsche Schreibweise */
        s[i] = 0;
        TextOut(d, (short)(r.r - 6 - TextWidth(d, s, -1)), (short)(r.t + 4), s, -1);
        if (calc_op) { s[0] = calc_op == '*' ? 'x' : calc_op; s[1] = 0; TextOut(d, (short)(r.l + 6), (short)(r.t + 4), s, 1); }
        EndPaint(w, &ps);
        return 0;
    case M_COMMAND:
        if (p1 >= 10 && p1 < 30) taste(code[p1 - 10]);
        SetFocus(w);                              /* Tastatur bleibt beim Rechner */
        return 0;
    case M_KEY:
        if (p1 == K_ESC) PostMessage(w, M_CLOSE, 0, 0);
        else if (p1 == K_ENTER) taste('=');
        else if (p1 == K_BACK) taste('\b');
        else if (p1 == K_DEL) taste('C');
        else break;
        return 0;
    case M_CHAR:
        if (p1 == ',') p1 = '.';
        else if (p1 == 'c' || p1 == 'C') p1 = 'C';
        else if (p1 == 'w' || p1 == 'W') p1 = 'S';
        else if (p1 == 'n' || p1 == 'N') p1 = 'N';
        else if (!(p1 >= '0' && p1 <= '9') && !strchr(".+-*/=%", (char)p1) || !p1) return 0;
        taste((char)p1);
        return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H, fw, fh, y0, i; SYSINFO si; RECT r, c;
    (void)show; hi = inst; test = cmd && cmd[0] == 'T';
    wc.name = "RdRechner"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); kh = GetCaps(d, CAPS_TEXTH); ReleaseDC(0, d);
    bh = (short)(kh + 8); y0 = (short)(BG + kh + 8 + BG + 4);
    KernInfo(&si); calc_reset();
    fe = CreateWindow("RdRechner", StrGet(inst, si.fpu ? 2 : 1), WS_TITLE | WS_CLOSE | WS_MIN | WS_BORDER | WS_TOOL | WS_VISIBLE,
                      0, 0, 100, 100, 0, 1, inst);
    if (!fe) return 1;
    GetWindowRect(fe, &r); GetClientRect(fe, &c);           /* Rahmen ausrechnen, dann passend gross */
    fw = (short)(5 * BW + 4 * BG + 16 + (r.r - r.l - c.r)); fh = (short)(y0 + 4 * (bh + BG) + (r.b - r.t - c.b));
    MoveWindow(fe, (short)(W - fw - W / 16), (short)(H / 6), fw, fh, TRUE);
    for (i = 0; i < 20; i++)
        CreateWindow(CL_BUTTON, lbl[i], WS_CHILD | WS_VISIBLE | (i == 19 ? ES_DEFAULT : 0),
                     (short)(8 + i % 5 * (BW + BG)), (short)(y0 + i / 5 * (bh + BG)), BW, bh, fe, (WORD)(10 + i), inst);
    SetFocus(fe);
    if (test) DbgPrint("RECHNER: bereit");
    while (GetMessage(&m)) DispatchMessage(&m);
    return 0;
}
