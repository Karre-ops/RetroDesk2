/* GDITEST.C - Testbild fuer Phase 3: alle Grundfunktionen von RDGDI.
 * Zeichnet auf den ganzen Schirm (GetDC(0)), meldet auf COM1 "GDITEST: fertig"
 * und wartet 4 s, damit das Testskript ein Bildschirmfoto machen kann.
 * Aufteilung relativ zur Schirmgroesse: laeuft auf CGA, Hercules und VGA. */
#include "RD2.H"

static HDC dc;
static short W, H;
static RECT r;
static POINT stern[5];

static void rc(short l, short t, short rr, short b) { r.l = l; r.t = t; r.r = rr; r.b = b; }
#define X(p) ((short)((long)(p) * W / 1000))      /* Promille der Breite */
#define Y(p) ((short)((long)(p) * H / 1000))

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HBMP bm; HDC bdc; MSG msg; short i, x, y; DWORD t0; WORD n, stufe = 9;
    (void)inst; (void)show;
    if (cmd && *cmd >= '0' && *cmd <= '8') stufe = (WORD)(*cmd - '0');   /* nur bis Feld n (Fehlersuche) */
    dc = GetDC(0);
    if (!dc) { DbgHex("GDITEST: kein Schirm, Fehler", LastError()); return 1; }
    W = GetCaps(dc, CAPS_W); H = GetCaps(dc, CAPS_H);
    DbgHex("GDITEST: Breite", (DWORD)W); DbgHex("GDITEST: Hoehe", (DWORD)H);
    t0 = TickCount();

    /* Hintergrund weiss, Rahmen */
    SetBrush(dc, C_WHITE, PAT_SOLID); rc(0, 0, W, H); FillRect(dc, &r);
    SetPen(dc, C_BLACK); Frame(dc, &r);

    if (stufe < 1) goto ende;
    /* Feld 1 oben links: Linienfaecher in alle Richtungen */
    x = X(125); y = Y(165);
    for (i = 0; i <= 16; i++) {
        Line(dc, x, y, (short)(x - X(110) + (long)i * X(220) / 16), Y(30));
        Line(dc, x, y, (short)(x - X(110) + (long)i * X(220) / 16), Y(300));
    }
    MoveTo(dc, X(10), Y(310)); LineTo(dc, X(240), Y(310));

    if (stufe < 2) goto ende;
    /* Feld 2: Grauwerte per Dithering und Muster */
    for (i = 0; i < 5; i++) {
        BYTE g = (BYTE)(i * 63);
        SetBrush(dc, RGB(g, g, g), PAT_SOLID);
        rc((short)(X(260) + i * X(45)), Y(30), (short)(X(300) + i * X(45)), Y(150)); FillRect(dc, &r); Frame(dc, &r);
    }
    for (i = 1; i <= 4; i++) {
        SetBrush(dc, C_BLACK, (WORD)i);
        rc((short)(X(260) + (i - 1) * X(56)), Y(170), (short)(X(310) + (i - 1) * X(56)), Y(300)); FillRect(dc, &r); Frame(dc, &r);
    }

    if (stufe < 3) goto ende;
    /* Feld 3: Ellipsen, Kreis, abgerundete Rechtecke */
    SetBrush(dc, C_GRAY, PAT_SOLID);
    rc(X(500), Y(30), X(640), Y(150)); Ellipse(dc, &r, 0);
    rc(X(660), Y(30), X(800), Y(150)); Ellipse(dc, &r, 1);
    rc(X(820), Y(30), X(980), Y(150)); RoundRect(dc, &r, 8, 0);
    rc(X(500), Y(170), X(740), Y(300)); RoundRect(dc, &r, 12, 1);
    rc(X(760), Y(170), X(980), Y(300)); Ellipse(dc, &r, 1);

    if (stufe < 4) goto ende;
    /* Feld 4: Stern (Polygon, gerade/ungerade gefuellt), Fuellen eines Bereichs */
    stern[0].x = X(120); stern[0].y = Y(340);
    stern[1].x = X(200); stern[1].y = Y(600);
    stern[2].x = X(10);  stern[2].y = Y(440);
    stern[3].x = X(230); stern[3].y = Y(440);
    stern[4].x = X(40);  stern[4].y = Y(600);
    SetBrush(dc, C_BLACK, PAT_GRAY50); SetBkColor(dc, C_WHITE);
    Polygon(dc, stern, 5, 1);
    rc(X(260), Y(340), X(480), Y(600)); Frame(dc, &r);
    Line(dc, X(260), Y(470), X(370), Y(340)); Line(dc, X(370), Y(600), X(480), Y(470));
    SetBrush(dc, C_BLACK, PAT_HATCH);
    FloodFill(dc, X(370), Y(470));

    if (stufe < 5) goto ende;
    /* Feld 5: Clipping und Ursprung */
    rc(X(520), Y(340), X(740), Y(600)); Frame(dc, &r);
    rc(X(540), Y(360), X(720), Y(580)); SetClip(dc, &r);
    for (i = 0; i < 12; i++) Line(dc, X(500), (short)(Y(330) + i * Y(25)), X(760), (short)(Y(400) + i * Y(25)));
    SetClip(dc, 0);
    SetOrigin(dc, X(760), Y(340));
    rc(0, 0, X(220), Y(260)); Frame(dc, &r);
    rc(X(20), Y(20), X(100), Y(120)); SetBrush(dc, C_BLACK, PAT_SOLID); FillRect(dc, &r);
    rc(X(60), Y(80), X(200), Y(240)); InvertRect(dc, &r);
    SetOrigin(dc, 0, 0);

    if (stufe < 6) goto ende;
    /* Feld 6: Bitmap im Speicher, BitBlt mit Rasteroperationen, StretchBlt, Schirm -> Schirm */
    bm = BmpCreate(64, 32, 1);
    bdc = bm ? BmpDC(bm) : 0;
    if (bdc) {
        SetBrush(bdc, C_WHITE, PAT_SOLID); rc(0, 0, 64, 32); FillRect(bdc, &r);
        SetPen(bdc, C_BLACK); Frame(bdc, &r);
        rc(4, 4, 36, 28); SetBrush(bdc, C_BLACK, PAT_SOLID); Ellipse(bdc, &r, 1);
        Line(bdc, 36, 4, 60, 28); Line(bdc, 36, 28, 60, 4);
        y = Y(640);
        BitBlt(dc, X(20), y, 64, 32, bdc, 0, 0, ROP_COPY);
        BitBlt(dc, (short)(X(20) + 80), y, 64, 32, bdc, 0, 0, ROP_COPY);
        BitBlt(dc, (short)(X(20) + 112), (short)(y + 16), 64, 32, bdc, 0, 0, ROP_XOR);
        rc((short)(X(20) + 200), y, (short)(X(20) + 328), (short)(y + 64));
        { RECT s; s.l = 0; s.t = 0; s.r = 64; s.b = 32; StretchBlt(dc, &r, bdc, &s, ROP_COPY); }
        BitBlt(dc, (short)(X(20) + 340), y, 180, 64, dc, X(20), y, ROP_COPY);     /* Schirm -> Schirm */
        BitBlt(dc, (short)(X(20) + 350), (short)(y + 8), 160, 40, dc, (short)(X(20) + 340), y, ROP_COPY);   /* ueberlappend */
        n = (WORD)(GetPixel(bdc, 20, 16) == C_BLACK) | (WORD)((GetPixel(bdc, 1, 1) == C_WHITE) << 1);
        DbgHex("GDITEST: Bitmap-Punkte (3 = ok)", n);
        DCFree(bdc);
        BmpFree(bm);
    } else DbgHex("GDITEST: Bitmap fehlt, Fehler", LastError());

    if (stufe < 7) goto ende;
    /* Feld 7: Rasteroperationen der Rechtecke und Punkte */
    SetBrush(dc, C_BLACK, PAT_SOLID);
    rc(X(600), Y(640), X(800), Y(760)); FillRect(dc, &r);
    SetBrush(dc, C_WHITE, PAT_SOLID); SetRop(dc, ROP_XOR); rc(X(650), Y(680), X(900), Y(800)); FillRect(dc, &r); SetRop(dc, ROP_COPY);
    SetBrush(dc, C_WHITE, PAT_HATCH); SetRop(dc, ROP_OR); rc(X(603), Y(643), X(641), Y(675)); FillRect(dc, &r);   /* ODER/UND, schraege Raender */
    rc(X(611), Y(700), X(613), Y(750)); FillRect(dc, &r);
    SetBrush(dc, C_BLACK, PAT_GRAY50); SetRop(dc, ROP_AND); rc(X(661), Y(691), X(797), Y(757)); FillRect(dc, &r);
    rc(X(662), Y(761), X(665), Y(797)); FillRect(dc, &r); SetRop(dc, ROP_COPY);
    for (i = 0; i < 40; i++) Pixel(dc, (short)(X(820) + i * 3), (short)(Y(900) + (i & 7)), C_BLACK);
    n = (WORD)(GetPixel(dc, X(620), Y(660)) == C_BLACK) | (WORD)((GetPixel(dc, X(700), Y(720)) == C_WHITE) << 1)
      | (WORD)((GetPixel(dc, X(850), Y(780)) == C_BLACK) << 2);
    DbgHex("GDITEST: Schirm-Punkte (7 = ok)", n);

    if (stufe < 8) goto ende;
ende:
    DbgHex("GDITEST: Zeit ms", TickCount() - t0);
    /* Bildspeicher nach SHOT.BIN (nur fuer die Testautomatik, wie SHOT in RetroDesk 1) */
    {
        short f = FileCreate("SHOT.BIN", 0);
        WORD seg = H == 348 ? 0xB000 : H == 480 ? 0xA000 : 0xB800, len = H == 348 ? 0x8000 : H == 480 ? 38400U : 0x4000;
        if (f >= 0) { FileWrite(f, (const void FAR *)((DWORD)seg << 16), len); FileClose(f); }
    }
    DbgPrint("GDITEST: fertig");
    TimerSet(0, 1, 4000);
    while (GetMessage(&msg) && msg.msg != M_TIMER) ;
    TimerKill(0, 1);
    ReleaseDC(0, dc);
    return 0;
}
