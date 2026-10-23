/* RESTEST.C - Kern-Zusatz (RDKX): Ressourcen, Texte, Sprache, Uhrzeit; Kern: MsgRegister, PostMessage */
#include "RD2.H"
static char buf[80];
static int same(LPCSTR a, LPCSTR b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static void ok(int c, LPCSTR t) { DbgPrint(t); DbgPrint(c ? "  -> ok" : "  -> FEHLER"); }
static int ende(LPCSTR a, LPCSTR b)                  /* a endet auf b */
{
    LPCSTR p = a, q = b;
    while (*p) p++;
    while (*q) q++;
    while (q > b && p > a && *--p == *--q) ;
    return q == b && *p == *q;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HMEM h; BYTE FAR *p; RDTIME t; MSG m; WORD a, b, n; LPCSTR s;
    (void)cmd; (void)show;
    ok(InstRes(inst, buf, sizeof buf) != 0 && ende(buf, "RESTEST.EXE"), "RESTEST: InstRes Pfad und RD2-Block");
    ok(LangGet() == RD_LANG_DE, "RESTEST: LangGet = Deutsch");
    n = StrLoad(inst, 1, buf, sizeof buf);
    ok(n == 21 && same(buf, "Hallo von RetroDesk 2"), "RESTEST: StrLoad deutsch vor englisch");
    ok(same(StrGet(inst, 2), "Datei nicht gefunden: %s"), "RESTEST: StrGet");
    s = StrGet(inst, 99);
    ok(s && !*s && LastError() == RD_ERR_NOTFOUND, "RESTEST: StrGet fehlt -> \"\" und RD_ERR_NOTFOUND");
    n = StrLoad(inst, 1, buf, 6);
    ok(n == 5 && same(buf, "Hallo"), "RESTEST: StrLoad kuerzt auf die Puffergroesse");
    ok(same(StrGet(0, 3), "Abbruch") && same(StrGet(0, 1), "RetroDesk 2"), "RESTEST: Systemtexte aus DE.LNG");

    h = ResLoad(inst, RT_MENU, 1);
    p = h ? (BYTE FAR *)MemLock(h) : 0;
    ok(p && ((WORD FAR *)p)[0] == MF_POPUP && same((LPCSTR)p + 4, "&Datei"), "RESTEST: ResLoad MENU 1 (deutsch)");
    if (p) {   /* zweiter Eintrag: ITEM "&Neu\tStrg+N" 101 */
        p += 4 + 7;
        ok(((WORD FAR *)p)[0] == 0 && ((WORD FAR *)p)[1] == 101 && same((LPCSTR)p + 4, "&Neu\tStrg+N"), "RESTEST: Menue-Eintrag 101");
        p += 4 + 12;
        ok(((WORD FAR *)p)[0] == MF_GRAY && ((WORD FAR *)p)[1] == 102, "RESTEST: Menue-Eintrag GRAU");
        MemUnlock(h);
    }
    ResFree(h);
    h = ResLoad(inst, RT_DIALOG, 10);
    p = h ? (BYTE FAR *)MemLock(h) : 0;
    ok(p && ((WORD FAR *)p)[4] == 220 && ((WORD FAR *)p)[6] == 5 && same((LPCSTR)p + 14, "Einstellungen"), "RESTEST: ResLoad DIALOG 10 (5 Elemente)");
    if (p) MemUnlock(h);
    ResFree(h);
    h = ResLoad(inst, RT_DATA, 5);
    p = h ? (BYTE FAR *)MemLock(h) : 0;
    ok(p && MemSize(h) >= 9 && p[0] == 'R' && p[8] == 'n', "RESTEST: ResLoad DATEN (Sprache 0)");
    if (p) MemUnlock(h);
    ResFree(h);
    ok(!ResLoad(inst, RT_MENU, 7) && LastError() == RD_ERR_NOTFOUND, "RESTEST: ResLoad fehlt -> RD_ERR_NOTFOUND");

    {   /* Bilder: 4 Bit "#g.#" -> 0110, Symbol auf eine schwarze Bitmap (CGA-Fassung bei <= 200 Zeilen) */
        HBMP bm = BmpLoad(inst, 2), bf = BmpLoadFile("A:\\RD2\\RESTEST.RDB"), z; HDC dc; WORD cga = GetCaps(0, CAPS_H) <= 200;
        p = bm ? (BYTE FAR *)MemLock(bm) : 0;
        ok(p && ((WORD FAR *)p)[0] == 4 && ((WORD FAR *)p)[1] == 1 && (p[8] & 0xF0) == 0x60, "RESTEST: BmpLoad 4 Bit -> 1 Bit");
        if (p) MemUnlock(bm);
        p = bf ? (BYTE FAR *)MemLock(bf) : 0;
        ok(p && (p[8] & 0xF0) == 0x60, "RESTEST: BmpLoadFile");
        if (p) MemUnlock(bf);
        BmpFree(bm); BmpFree(bf);
        z = BmpCreate(8, 2, 1); dc = z ? BmpDC(z) : 0;
        if (dc) { IconDraw(dc, 0, 0, inst, 1); DCFree(dc); }
        p = z ? (BYTE FAR *)MemLock(z) : 0;
        ok(p && (cga ? p[8] == 0x55 && p[9] == 0 : p[8] == 0x30 && p[9] == 0x4D), cga ? "RESTEST: IconDraw CGA-Fassung" : "RESTEST: IconDraw mit Maske");
        if (p) MemUnlock(z);
        BmpFree(z);
    }
    TimeGet(&t);
    ok(t.jahr >= 1980 && t.monat >= 1 && t.monat <= 12 && t.tag >= 1 && t.tag <= 31 && t.std < 24 && t.min < 60, "RESTEST: TimeGet");
    ok(TimeSet(&t, 0) == 0, "RESTEST: TimeSet (dieselbe Zeit)");
    t.monat = 13;
    ok(TimeSet(&t, 0) == RD_ERR_FORMAT, "RESTEST: TimeSet Monat 13 -> RD_ERR_FORMAT");

    a = MsgRegister("RD_TEST"); b = MsgRegister("rd_test");
    if (!a && LastError() == RD_ERR_NOTSUP) DbgPrint("RESTEST: MsgRegister uebersprungen (ohne RDUSER)");
    else ok(a >= 0xC000 && a == b && MsgRegister("ANDERS") == a + 1, "RESTEST: MsgRegister gleich/anders");
    ok(PostMessage(0, M_USER + 1, 5, 7) && PeekMessage(&m, 1) && m.msg == M_USER + 1 && m.p1 == 5 && m.p2 == 7, "RESTEST: PostMessage an Fenster 0");
    SetLastError(0);
    SetLastError(0);
    ok(SendMessage(3, M_USER, 0, 0) == 0 && (LastError() == RD_ERR_NOTSUP || LastError() == RD_ERR_HANDLE), "RESTEST: SendMessage an ungueltiges Fenster -> 0 und Fehler");
    DbgBreak();
    ok(1, "RESTEST: DbgBreak ohne Debugger");
    DbgPrint("RESTEST: Ende");
    return 0;
}
