/* INPTEST.C - Phase 3: Tastatur und Maus. Meldet jede Eingabe auf COM1, schreibt Zeichen
 * in eine Zeile, zeichnet bei Linksklick ein Kaestchen an die Mausposition (unter dem
 * Zeiger), bei Doppelklick einen Rahmen, rechts loescht. ESC: Bild nach SHOT.BIN, Ende. */
#include "RD2.H"

static HDC dc;
static short W, H, tx = 8;
static RECT r;
static char z[2];

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    MSG m; short x, y;
    (void)inst; (void)cmd; (void)show;
    if (!(dc = GetDC(0))) return 1;
    W = GetCaps(dc, CAPS_W); H = GetCaps(dc, CAPS_H);
    SetBrush(dc, C_WHITE, PAT_SOLID); r.l = 0; r.t = 0; r.r = W; r.b = H; FillRect(dc, &r);
    TextOut(dc, 8, 4, "INPTEST: tippen, klicken, ESC beendet", -1);
    DbgPrint("INPTEST: bereit");
    while (GetMessage(&m)) {
        x = m.pt.x; y = m.pt.y;
        switch (m.msg) {
        case M_KEY:
            DbgHex("INPTEST: KEY", ((DWORD)HIWORD(m.p2) << 16) | m.p1);
            if (m.p1 == K_ESC) PostQuit(0);
            break;
        case M_CHAR:
            DbgHex("INPTEST: CHAR", m.p1);
            if (m.p1 >= 32) { z[0] = (char)m.p1; TextOut(dc, tx, 20, z, 1); tx += 8; }
            break;
        case M_MOUSEMOVE: DbgHex("INPTEST: MOVE", m.p2); break;
        case M_BUTTONDOWN:
            DbgHex("INPTEST: DOWN", ((DWORD)m.p1 << 16) | (WORD)(x + (y << 0) * 0));
            DbgHex("INPTEST: DOWN bei", m.p2);
            if ((m.p1 & 0xFF) == 1) { SetBrush(dc, C_BLACK, PAT_SOLID); r.l = x - 4; r.t = y - 4; r.r = x + 12; r.b = y + 12; FillRect(dc, &r); }
            if ((m.p1 & 0xFF) == 2) { SetBrush(dc, C_WHITE, PAT_SOLID); r.l = 0; r.t = 30; r.r = W; r.b = H; FillRect(dc, &r); }
            break;
        case M_BUTTONUP: DbgHex("INPTEST: UP", m.p1); break;
        case M_DBLCLK:
            DbgHex("INPTEST: DBLCLK", m.p1);
            SetPen(dc, C_BLACK); r.l = x - 20; r.t = y - 20; r.r = x + 20; r.b = y + 20; Frame(dc, &r);
            break;
        }
    }
    {
        short f = FileCreate("SHOT.BIN", 0);
        WORD seg = H == 348 ? 0xB000 : H == 480 ? 0xA000 : 0xB800, len = H == 348 ? 0x8000 : H == 480 ? 38400U : 0x4000;
        if (f >= 0) { FileWrite(f, (const void FAR *)((DWORD)seg << 16), len); FileClose(f); }
    }
    DbgPrint("INPTEST: fertig");
    ReleaseDC(0, dc);
    return 0;
}
