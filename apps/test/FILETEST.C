/* FILETEST.C - Datei-API, Profile, Laufwerksinfo, Zwischenablage */
#include "RD2.H"
static char buf[64];
static int same(LPCSTR a, LPCSTR b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static void ok(int c, LPCSTR t) { DbgPrint(t); DbgPrint(c ? "  -> ok" : "  -> FEHLER"); }
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    short f; WORD n; FINDDATA fd; DRIVEINFO di; HMEM h; char FAR *p; long pos;
    (void)inst; (void)cmd; (void)show;
    /* Dateien */
    f = FileCreate("A:\\TEST.DAT", 0);
    n = FileWrite(f, "RetroDesk 2 Dateitest", 21);
    FileClose(f);
    ok(f >= 0 && n == 21, "FILETEST: Datei schreiben");
    f = FileOpen("A:\\TEST.DAT", FO_READ);
    pos = FileSeek(f, 12, FS_SET);
    n = FileRead(f, buf, 10); buf[n] = 0;
    FileClose(f);
    ok(pos == 12 && same(buf, "Dateitest"), "FILETEST: Datei lesen mit Seek");
    ok(FileOpen("A:\\GIBTSNET.XYZ", FO_READ) == -1 && LastError() == RD_ERR_NOTFOUND, "FILETEST: fehlende Datei -> RD_ERR_NOTFOUND");
    ok(FileSaveSafe("A:\\TEST.DAT", "neu", 3) == 0, "FILETEST: FileSaveSafe");
    f = FileOpen("A:\\TEST.DAT", FO_READ); n = FileRead(f, buf, 20); FileClose(f); buf[n] = 0;
    ok(same(buf, "neu"), "FILETEST: Inhalt nach FileSaveSafe");
    n = 0;
    if (!FileFindFirst("A:\\RD2\\*.EXE", 0, &fd)) do n++; while (!FileFindNext(&fd));
    DbgHex("FILETEST: EXE-Dateien in A:\\RD2", n);
    ok(n >= 8, "FILETEST: FileFindFirst/Next");
    ok(DriveInfo(1, &di) == 0 && di.typ == DT_FLOPPY && di.format_kb == 720, "FILETEST: DriveInfo A: = 720-KB-Diskette");
    DbgHex("FILETEST: A: frei", di.frei);
    DbgHex("FILETEST: A: Seriennummer", di.serial);
    /* Profile */
    ProfileSetStr("A:\\TEST.INI", "Benutzer", "Name", "Karsten");
    ProfileSetStr("A:\\TEST.INI", "Benutzer", "Zahl", "42");
    ProfileSetStr("A:\\TEST.INI", "Rechner", "CPU", "8088");
    ProfileSetStr("A:\\TEST.INI", "Benutzer", "Name", "PC10-III");
    ProfileSetStr("A:\\TEST.INI", "Rechner", "Weg", "x");
    ProfileSetStr("A:\\TEST.INI", "Rechner", "Weg", 0);
    ProfileGetStr("A:\\TEST.INI", "benutzer", "NAME", "?", buf, sizeof buf);
    ok(same(buf, "PC10-III"), "FILETEST: Profil ueberschreiben, Gross/Klein egal");
    ok(ProfileGetInt("A:\\TEST.INI", "Benutzer", "Zahl", 0) == 42, "FILETEST: ProfileGetInt");
    ok(ProfileGetInt("A:\\TEST.INI", "Benutzer", "Fehlt", -5) == -5, "FILETEST: Vorgabewert");
    ProfileGetStr("A:\\TEST.INI", "Rechner", "Weg", "geloescht", buf, sizeof buf);
    ok(same(buf, "geloescht"), "FILETEST: Schluessel loeschen");
    /* Zwischenablage */
    h = MemAlloc(16, MEM_MOVABLE);
    p = (char FAR *)MemLock(h); p[0] = 'X'; p[1] = 'T'; p[2] = 0; MemUnlock(h);
    ClipOpen(0); ClipEmpty(); ClipSet(CF_TEXT, h); ClipClose();
    p = (char FAR *)MemLock(ClipGet(CF_TEXT));
    ok(p && same(p, "XT"), "FILETEST: Zwischenablage");
    MemUnlock(ClipGet(CF_TEXT));
    DbgPrint("FILETEST: Ende");
    return 0;
}
