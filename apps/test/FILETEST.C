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
    ok(DriveInfo(1, &di) == 0 && di.typ == DT_FLOPPY && di.format_kb == 360, "FILETEST: DriveInfo A: = 360-KB-Diskette");
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
    /* Fehlerfaelle (Audit 24.10.2026) */
    f = FileCreate("A:\\NURW.DAT", 0); FileWrite(f, "abc", 3); FileClose(f);
    f = FileOpen("A:\\NURW.DAT", FO_WRITE); SetLastError(0); n = FileRead(f, buf, 3); FileClose(f);
    ok(n == 0 && LastError() != 0, "FILETEST: Lesefehler -> 0 Bytes und LastError gesetzt");
    FileDelete("A:\\NURW.DAT");
    DirMake("A:\\INIDIR");
    n = ProfileSetStr("A:\\INIDIR", "A", "B", "C");
    ok(n != 0 && FileAttr("A:\\INIDIR.$$$", 0, 0) == 0xFFFF && (FileAttr("A:\\INIDIR", 0, 0) & FA_DIR), "FILETEST: Profil nicht lesbar -> Fehler, nichts angelegt");
    DirRemove("A:\\INIDIR");
    { static char gross[800]; WORD i; DWORD vor, nach;
      f = FileCreate("A:\\GROSS.INI", 0);
      for (i = 0; i < 253; i++) gross[i] = (char)(i ? '-' : ';'); gross[252] = 13; gross[253] = 10;
      for (i = 0; i < 64; i++) FileWrite(f, gross, 254);
      FileWrite(f, "[S]\r\nK=1\r\n", 10); vor = FileSeek(f, 0, FS_END); FileClose(f);
      for (i = 0; i < sizeof gross - 1; i++) gross[i] = 'x'; gross[i] = 0;
      n = ProfileSetStr("A:\\GROSS.INI", "S", "Neu", gross);
      f = FileOpen("A:\\GROSS.INI", FO_READ); nach = FileSeek(f, 0, FS_END); FileClose(f);
      ok(n == RD_ERR_FULL && vor == nach, "FILETEST: Profil zu gross -> RD_ERR_FULL, Datei unveraendert");
      ProfileGetStr("A:\\GROSS.INI", "S", "K", "?", buf, sizeof buf);
      ok(same(buf, "1"), "FILETEST: grosses Profil noch lesbar");
      FileDelete("A:\\GROSS.INI"); }
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
