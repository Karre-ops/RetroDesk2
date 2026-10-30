/* BENCH2.C - Leistungsvergleich, Seite RetroDesk 2: dieselben Aufgaben wie BENCH1 (RD1)
 * ueber die RD2-API (RDGDI, Nachrichten). Gleiche Messung: so oft wie moeglich in 36 BIOS-
 * Ticks (ca. 2 s), Mikrosekunden je Aufruf. Ausgabe auf COM1/Debug ("BENCH2: ..."). */
#include "RD2.H"

static volatile DWORD __far *bt = (volatile DWORD __far *)0x0040006CL;
static HDC dc, bdc, gdc;
static short W, H;
static RECT r, ri, rr;
static MSG m;

static void miss(LPCSTR name, void (*fn)(void))
{
    DWORD t, n = 0;
    t = *bt; while (*bt == t) ;
    t = *bt;
    do { fn(); n++; } while (*bt - t < 36);
    DbgPrint(name);
    DbgHex("BENCH2: us", (36UL * 54925UL) / n);
}
/* "Text 20 Zeichen" wie RD1: durchsichtig (nur die Schrift). "deckend": mit Hintergrund */
static void a_text13(void) { TextOut(dc, 13, 40, "RDEDIT.EXE     51110", -1); }   /* Modus durchsichtig vorher gesetzt (wie RD1: kein Umschalten je Aufruf), 25.10. */
/* x=16: RD1 gfx_text schreibt auf Bytegrenze die ganze Zelle (mit Hintergrund), also deckend vergleichen */
static void a_text16(void) { TextOut(dc, 16, 40, "RDEDIT.EXE     51110", -1); }
static void a_dtext13(void) { TextOut(dc, 13, 40, "RDEDIT.EXE     51110", -1); }
static void a_dtext16(void) { TextOut(dc, 16, 40, "RDEDIT.EXE     51110", -1); }
static void a_fill(void)   { SetBrush(dc, C_WHITE, PAT_SOLID); FillRect(dc, &r); }
static void a_pat(void)    { SetBrush(dc, C_BLACK, PAT_GRAY50); FillRect(dc, &r); }
static void a_inv(void)    { InvertRect(dc, &ri); }
static void a_rect(void)   { Frame(dc, &rr); }
static void a_blk(void)    { BitBlt(bdc, 0, 0, 208, 150, dc, 8, 20, ROP_COPY); BitBlt(dc, 8, 20, 208, 150, bdc, 0, 0, ROP_COPY); }
static void a_gblk(void)   { BitBlt(gdc, 0, 0, 208, 150, dc, 8, 20, ROP_COPY); BitBlt(dc, 8, 20, 208, 150, gdc, 0, 0, ROP_COPY); }   /* Geraeteformat wie Menue-Hintergrund */
static volatile unsigned rz;
static void a_calc(void)   { unsigned i; for (i = 0; i < 1000; i++) rz += i; }
static void a_inp(void)    { PeekMessage(&m, 0); }
static RECT rk = { 400, 150, 408, 151 };
static void a_klein(void)  { InvertRect(dc, &rk); }
static void a_pen(void)    { SetPen(dc, C_BLACK); }
static RECT rl = { 400, 150, 400, 151 };
static void a_null(void)   { InvertRect(dc, &rl); }
static void a_text1(void)  { TextOut(dc, 16, 40, "R", -1); }
static HMEM hm;
static void a_lock(void)   { MemLock(hm); MemUnlock(hm); }
static void a_text0(void)  { TextOut(dc, 16, 40, "R", 0); }
static void a_text1b(void) { TextOut(dc, 400, 150, "R", 1); }
static void a_leer(void)   { TickCount(); }

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HBMP bm, gm;
    (void)inst; (void)cmd; (void)show;
    dc = GetDC(0);
    if (!dc) { DbgHex("BENCH2: kein Schirm", LastError()); return 1; }
    W = GetCaps(dc, CAPS_W); H = GetCaps(dc, CAPS_H);
    r.l = 0; r.t = 0; r.r = W; r.b = H;
    ri.l = 40; ri.t = 40; ri.r = 240; ri.b = 140;
    rr = ri;
    SetPen(dc, C_BLACK); SetTextColor(dc, C_BLACK); SetBkColor(dc, C_WHITE);
    bm = BmpCreate(208, 150, 1); bdc = bm ? BmpDC(bm) : 0;
    if (!bdc) { DbgHex("BENCH2: keine Bitmap", LastError()); return 2; }
    SetBkMode(dc, BK_TRANSPARENT); miss("BENCH2: Text 20 Zeichen, x=13", a_text13); SetBkMode(dc, BK_OPAQUE);
    miss("BENCH2: Text 20 Zeichen, x=16", a_text16);
    miss("BENCH2: Text deckend, x=13", a_dtext13);
    miss("BENCH2: Text deckend, x=16", a_dtext16);
    miss("BENCH2: Schirm fuellen", a_fill);
    miss("BENCH2: Muster 50% voll", a_pat);
    miss("BENCH2: Rechteck 200x100 invertieren", a_inv);
    miss("BENCH2: Rahmen 200x100", a_rect);
    miss("BENCH2: Block 208x150 sichern+zurueck", a_blk);
    gm = BmpCreate(208, 150, 0); gdc = gm ? BmpDC(gm) : 0;   /* 0 = Geraeteformat (30.10.2026) */
    if (gdc) { miss("BENCH2: Block 208x150 Geraeteformat", a_gblk); DCFree(gdc); BmpFree(gm); }
    miss("BENCH2: Rechenschleife 1000",  a_calc);
    miss("BENCH2: Eingabe abfragen", a_inp);
    miss("BENCH2: leerer API-Aufruf", a_leer);
    miss("BENCH2: InvertRect 8x1 (Grundlast GDI)", a_klein);
    miss("BENCH2: SetPen (API + DC pruefen)", a_pen);
    miss("BENCH2: InvertRect leer", a_null);
    miss("BENCH2: Text 1 Zeichen", a_text1);
    miss("BENCH2: Text 0 Zeichen", a_text0);
    miss("BENCH2: Text 1 Zeichen bei 400,150", a_text1b);
    hm = MemAlloc(256, 0); miss("BENCH2: MemLock+MemUnlock", a_lock);
    DCFree(bdc); BmpFree(bm); ReleaseDC(0, dc);
    DbgPrint("BENCH2: fertig");
    return 0;
}
