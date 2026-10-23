/* TEXTTEST.C - Testbild Schriften (Phase 3): SYS8/14/16, fett/kursiv/unterstrichen,
 * deckend/durchsichtig/XOR, TextBox (umbrechen, zentriert, rechts, senkrecht mittig),
 * Abschneiden am Clip-Rechteck, Umlaute (Codepage 437). Ergebnis auf COM1, Bild in SHOT.BIN. */
#include "RD2.H"

static HDC dc;
static short W, H;
static RECT r;
static FONTINFO fi;
static WORD fehler;

static void pruefe(const char *was, long ist, long soll)
{
    if (ist != soll) { DbgHex(was, (DWORD)ist); fehler++; }
}
static void rc(short l, short t, short rr, short b) { r.l = l; r.t = t; r.r = rr; r.b = b; }
static void box(LPCSTR s, WORD fl) { Frame(dc, &r); r.l += 2; r.t += 2; r.r -= 2; r.b -= 2; TextBox(dc, &r, s, fl); }

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HFONT f8, f14, f16, fb, fk, fu, fbku;
    short y, z;
    (void)inst; (void)cmd; (void)show;
    if (!(dc = GetDC(0))) { DbgPrint("TEXTTEST: kein Schirm"); return 1; }
    W = GetCaps(dc, CAPS_W); H = GetCaps(dc, CAPS_H);
    DbgHex("TEXTTEST: Systemschrift Hoehe", (DWORD)GetCaps(dc, CAPS_TEXTH));
    SetBrush(dc, C_WHITE, PAT_SOLID); rc(0, 0, W, H); FillRect(dc, &r);
    f8 = FontLoad("SYS", 8, FS_NORMAL);
    {   /* Tempo: 100 Zeilen zu 40 Zeichen SYS8, danach wieder geloescht */
        WORD i; DWORD t0 = TickCount();
        SetFont(dc, f8);
        for (i = 0; i < 100; i++) TextOut(dc, 8, (short)(H - 8), "Tempo Tempo Tempo Tempo Tempo Tempo Temp", 40);
        DbgHex("TEXTTEST: 100 x 40 Zeichen, ms", TickCount() - t0);
        rc(0, (short)(H - 8), W, H); SetBrush(dc, C_WHITE, PAT_SOLID); FillRect(dc, &r);
    }

    f14 = FontLoad("SYS", 14, FS_NORMAL); f16 = FontLoad("SYS16.FNT", 0, FS_NORMAL);
    fb = FontLoad("SYS", 8, FS_BOLD); fk = FontLoad("SYS", 8, FS_ITALIC); fu = FontLoad("SYS", 8, FS_UNDER);
    fbku = FontLoad("SYS", 8, FS_BOLD | FS_ITALIC | FS_UNDER);
    pruefe("TEXTTEST: FontLoad Fehler", (long)(!f8) + !f14 + !f16 + !fb + !fk + !fu + !fbku, 0);
    pruefe("TEXTTEST: gleiche Schrift doppelt geladen", FontLoad("SYS8.FNT", 0, 0) == f8, 1);
    FontFree(f8);
    pruefe("TEXTTEST: unbekannte Schrift", FontLoad("FEHLT", 9, 0), 0);
    FontInfo(f14, &fi);
    pruefe("TEXTTEST: SYS14 Hoehe", fi.hoehe, 14); pruefe("TEXTTEST: SYS14 Oberlaenge", fi.oberlaenge, 11);
    pruefe("TEXTTEST: SYS14 fest", fi.fest, 1);
    SetFont(dc, f8);
    pruefe("TEXTTEST: Breite Hallo", TextWidth(dc, "Hallo", -1), 40);
    SetFont(dc, fb);
    pruefe("TEXTTEST: Breite Hallo fett", TextWidth(dc, "Hallo", 5), 45);
    SetFont(dc, fk);
    pruefe("TEXTTEST: Breite Hallo kursiv", TextWidth(dc, "Hallo", -1), 41);

    /* Zeilen in drei Schriften, Umlaute (CP437: 84 94 81 8E 99 9A E1) */
    y = 4;
    SetFont(dc, f8);  TextOut(dc, 8, y, "SYS8: RetroDesk 2 \x84\x94\x81 \x8E\x99\x9A \xE1  0123456789 !?()[]{}", -1); y += 10;
    SetFont(dc, f14); TextOut(dc, 8, y, "SYS14: Gr\x94\xE1" "e \x84\x94\x81\x8E\x99\x9A  Hercules 720x348", -1); y += 16;
    SetFont(dc, f16); TextOut(dc, 8, y, "SYS16: VGA 640x480 \x84\x94\x81", -1); y += 18;
    SetFont(dc, 0);   TextOut(dc, 8, y, "Systemschrift (0)", -1); y = (short)(y + GetCaps(dc, CAPS_TEXTH) + 2);
    SetFont(dc, fb);  TextOut(dc, 8, y, "fett", -1);
    SetFont(dc, fk);  TextOut(dc, 64, y, "kursiv", -1);
    SetFont(dc, fu);  TextOut(dc, 136, y, "unterstrichen", -1);
    SetFont(dc, fbku); TextOut(dc, 256, y, "alles zusammen", -1); y += 12;

    /* deckend auf Muster, durchsichtig auf Muster, XOR auf Schwarz */
    SetBrush(dc, C_GRAY, PAT_SOLID); rc(8, y, 360, (short)(y + 30)); FillRect(dc, &r);
    SetFont(dc, f8);
    SetBkMode(dc, BK_OPAQUE); TextOut(dc, 16, (short)(y + 3), "deckend", -1);
    SetBkMode(dc, BK_TRANSPARENT); TextOut(dc, 120, (short)(y + 3), "durchsichtig", -1);
    SetTextColor(dc, C_WHITE); TextOut(dc, 240, (short)(y + 3), "weiss", -1);
    SetBrush(dc, C_BLACK, PAT_SOLID); rc(16, (short)(y + 14), 200, (short)(y + 26)); FillRect(dc, &r);
    SetRop(dc, ROP_XOR); TextOut(dc, 20, (short)(y + 16), "XOR auf Schwarz", -1);
    SetTextColor(dc, C_BLACK); TextOut(dc, 150, (short)(y + 16), "XOR", -1);   /* schwarz XOR = nichts */
    SetRop(dc, ROP_COPY); SetBkMode(dc, BK_OPAQUE); SetTextColor(dc, C_BLACK);
    TextOut(dc, 213, (short)(y + 16), "deckend x=213", -1);           /* deckend verschoben (Treiber-Glyphen) */
    SetTextColor(dc, C_WHITE); SetBkColor(dc, C_BLACK); TextOut(dc, 323, (short)(y + 16), "weiss/schwarz", -1);
    SetTextColor(dc, C_BLACK); SetBkColor(dc, C_WHITE);
    y += 36;

    /* TextBox: links umbrochen, zentriert, rechts, senkrecht mittig; mit Rahmen */
    z = (short)(W / 4 - 8);
    SetPen(dc, C_BLACK); SetBrush(dc, C_WHITE, PAT_SOLID);
    rc(8, y, (short)(8 + z), (short)(y + 50)); box("Dieser Text wird an Wortgrenzen umbrochen und unten abgeschnitten, weil er zu lang ist.", TB_LEFT | TB_WRAP);
    rc((short)(16 + z), y, (short)(16 + 2 * z), (short)(y + 50)); box("zentriert\nzwei Zeilen", TB_CENTER);
    rc((short)(24 + 2 * z), y, (short)(24 + 3 * z), (short)(y + 50)); box("rechts\nb\x81ndig", TB_RIGHT);
    rc((short)(32 + 3 * z), y, (short)(32 + 4 * z - 8), (short)(y + 50)); box("mittig", TB_CENTER | TB_VCENTER);
    y += 56;

    /* Clipping: Text laeuft ueber das Clip-Rechteck und den rechten Rand */
    rc(8, y, 120, (short)(y + 6)); SetClip(dc, &r);
    TextOut(dc, 4, y, "abgeschnitten oben und seitlich", -1);
    SetClip(dc, 0);
    TextOut(dc, (short)(W - 60), y, "Rand rechts weg", -1);
    y += 12;
    SetFont(dc, f14);
    TextOut(dc, 8, y, "Zeichen 32..127:", -1); y += 15;
    {
        static char z2[100]; WORD i, k;
        for (k = 0; k < 3; k++) {
            for (i = 0; i < 64 && 32 + k * 64 + i < 256; i++) z2[i] = (char)(32 + k * 64 + i);
            z2[i] = 0;
            SetFont(dc, H >= 348 ? f14 : f8);
            TextOut(dc, 8, y, z2, (short)i); y = (short)(y + (H >= 348 ? 15 : 9));
        }
        for (i = 0; i < 32; i++) z2[i] = (char)(224 + i);
        TextOut(dc, 8, y, z2, 32);
    }
    /* ein Punkt im Strich von 'H' (SYS8, Zeile 1): x = 8, y = 5 schwarz, daneben weiss */
    pruefe("TEXTTEST: Punkt im H", GetPixel(dc, 8, 6) == C_BLACK, 1);

    FontFree(f14); FontFree(f16); FontFree(fb); FontFree(fk); FontFree(fu); FontFree(fbku);
    {
        short f = FileCreate("SHOT.BIN", 0);
        WORD seg = H == 348 ? 0xB000 : H == 480 ? 0xA000 : 0xB800, len = H == 348 ? 0x8000 : H == 480 ? 38400U : 0x4000;
        if (f >= 0) { FileWrite(f, (const void FAR *)((DWORD)seg << 16), len); FileClose(f); }
    }
    DbgHex("TEXTTEST: Fehler", fehler);
    ReleaseDC(0, dc);
    return 0;
}
