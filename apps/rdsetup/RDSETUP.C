/* RDSETUP.C - Pakete installieren und entfernen (PLAN_32BIT Schritt 1, 20.11.2026).
 *
 * RDSETUP [pfad\SETUP.INF | pfad\]     installieren (ohne Angabe: fragt nach SETUP.INF)
 * RDSETUP /D ziel\RDSETUP.LST          entfernen (die Shell ruft es beim Oeffnen von RDSETUP.LST)
 *
 * SETUP.INF (INI-Format, alles ausser [Dateien] freiwillig):
 *   [Paket]  Name=Burgenkampf  Version=1.2  Ziel=C:\SPIELE\BURGEN
 *            Programm=BURGEN.EXE  Programm386=BURG32.EXE (ab 386 statt Programm)  Schreibtisch=1
 *            Setup=EIGEN.EXE (eigenes Setup: wird mit dem Paketordner gestartet, sonst nichts)
 *   [Dateien] 1=BURGEN.EXE  2=BURG32.EXE 386  3=MUSIK.IMF KLANG  4=GROSS.DAT FARBE  5=GLIDE.DXE 3D  6=X.DAT 8087
 * Bedingungen: 386 (ab 386, sonst nicht mitkopiert: 32-Bit-Teile), 8087, FARBE (16 Farben und mehr), KLANG
 * (mehr als der Lautsprecher), 3D (3D-Karte laut RD2SETUP). Das Programm wird mit BedarfCheck geprueft.
 * Ergebnis: Dateien im Ziel, ziel\RDSETUP.LST (fuer das Entfernen), Eintrag in PROGRAMM.LST; die Shell liest
 * die Liste neu (Nachricht "RD2_PROGRAMME" an alle Tasks).
 */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"

static HINST hi;
static char inf[80], quelle[80], ziel[80], prog[16], name[32], sec[9], t[200], z[100], lst[80], wert[40];
static BYTE puf[2048];
static RDSYSDATA sd;
static const char pk[] = "Paket", da[] = "Dateien";

static WORD slen(const char *s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { WORD i = 0; while (i + 1 < n && s[i]) { d[i] = s[i]; i++; } d[i] = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); scpy(d + l, s, n - l); }
static void zahl(char *d, WORD v) { char b[6]; WORD i = 0; do b[i++] = (char)('0' + v % 10); while ((v /= 10) && i < 5); while (i) *d++ = b[--i]; *d = 0; }
static void fmt(char *d, LPCSTR f, LPCSTR a, LPCSTR b, WORD n)   /* %s nacheinander durch a, b */
{
    WORD i = 0; LPCSTR q;
    while (*f && i + 1 < n) {
        if (f[0] == '%' && f[1] == 's') { for (q = a ? a : ""; *q && i + 1 < n; q++) d[i++] = *q; a = b; b = 0; f += 2; }
        else d[i++] = *f++;
    }
    d[i] = 0;
}
static int gleich(LPCSTR a, LPCSTR b) { while (*a && (*a | 0x20) == (*b | 0x20)) a++, b++; return !*a && !*b; }
static short frage(WORD id, LPCSTR a, LPCSTR b, WORD flags) { fmt(t, StrGet(hi, id), a, b, sizeof t); return MsgBox(0, t, StrGet(hi, 1), flags); }
static void melden(void)                          /* Shell (und wer sonst will) liest PROGRAMM.LST neu */
{
    TASKINFO ti; HTASK k; WORD m = MsgRegister("RD2_PROGRAMME");
    for (k = TaskFirst(&ti); k; k = TaskNext(k, &ti)) PostTask(k, m, 0, 0);
}
static void plst(char *p)                         /* RD2-Ordner\PROGRAMM.LST */
{
    SYSINFO si; KernInfo(&si); scpy(p, si.home, 66);
    if (p[0] && p[slen(p) - 1] != '\\') scat(p, "\\", 80);
    scat(p, "PROGRAMM.LST", 80);
}
static int ordner(const char *p)                  /* Ordner samt Zwischenordnern anlegen */
{
    char b[80]; WORD i; FINDDATA fd;
    for (i = 0; p[i]; i++) {
        b[i] = p[i];
        if ((p[i] == '\\' && i > 2) || !p[i + 1]) {
            b[p[i] == '\\' ? i : i + 1] = 0;
            if (FileFindFirst(b, FA_DIR, &fd) || !(fd.attr & FA_DIR)) DirMake(b);
            b[i] = p[i];                          /* Trenner wieder einsetzen */
        }
    }
    return !FileFindFirst(p, FA_DIR, &fd) && (fd.attr & FA_DIR);
}
static int kopieren(const char *von, const char *nach)
{
    short a, b; WORD n; int ok = 1;
    if ((a = FileOpen(von, FO_READ)) < 0) return 0;
    if ((b = FileCreate(nach, 0)) < 0) { FileClose(a); return 0; }
    while ((n = FileRead(a, puf, sizeof puf)) != 0) if (FileWrite(b, puf, n) != n) { ok = 0; break; }
    FileClose(a); if (FileClose(b)) ok = 0;
    if (!ok) FileDelete(nach);
    return ok;
}
static int passt(const char *bed)                 /* Bedingung hinter dem Dateinamen */
{
    if (!*bed) return 1;
    if (gleich(bed, "386")) return sd.cpu >= CPU_386;
    if (gleich(bed, "8087")) return sd.fpu != 0;
    if (gleich(bed, "FARBE")) { WORD g = sd.grafik; return g == GFX_EGA || g == GFX_VGA16 || g == GFX_EGA320 || g == GFX_EGA200 || g == GFX_SVGA16 || g == GFX_VESA || g == GFX_MCGA || g == GFX_VESA800 || g == GFX_VESA1024; }
    if (gleich(bed, "KLANG")) return (sd.sound & ~SC_SPEAKER) != 0;
    if (gleich(bed, "3D")) return sd.d3 != 0;
    return 0;                                     /* unbekannt: lieber weglassen */
}
static void bedarfstext(WORD m, char *d, WORD n)
{
    WORD i; d[0] = 0;
    for (i = 0; i < 8; i++) if (m & (1 << i)) { if (d[0]) scat(d, ",\n", n); scat(d, StrGet(hi, 10 + i), n); }
}
static short installieren(void)
{
    WORD k, nd = 0, i, l; char key[4], nr[6], datei[16], bed[8]; static char p[80], liste[80];
    if (ProfileGetStr(inf, pk, "Name", "", name, sizeof name) == 0 && ProfileGetStr(inf, da, "1", "", wert, sizeof wert) == 0) {
        frage(3, inf, 0, MB_OK | MB_ERROR); return 1; }
    if (!name[0]) scpy(name, "Paket", sizeof name);
    DbgPrint("RDSETUP: Paket"); DbgPrint(name);
    if (ProfileGetStr(inf, pk, "Setup", "", wert, sizeof wert) && wert[0]) {   /* eigenes Setup des Pakets */
        scpy(p, quelle, sizeof p); scat(p, wert, sizeof p);
        return TaskExec(p, quelle, SW_SHOW) ? 0 : (frage(3, p, 0, MB_OK | MB_ERROR), 1);
    }
    prog[0] = 0;
    if (sd.cpu >= CPU_386) ProfileGetStr(inf, pk, "Programm386", "", prog, sizeof prog);
    if (!prog[0]) ProfileGetStr(inf, pk, "Programm", "", prog, sizeof prog);
    if (prog[0]) {
        WORD m; scpy(p, quelle, sizeof p); scat(p, prog, sizeof p);
        if ((m = BedarfCheck(p) & ~BD_RAM) != 0) { DbgHex("RDSETUP: Bedarf fehlt", m); bedarfstext(m, z, sizeof z); if (frage(5, name, z, MB_YESNO | MB_WARN) != ID_YES) return 1; }
    }
    scpy(ziel, "C:\\", sizeof ziel); scat(ziel, name, 12);
    ProfileGetStr(inf, pk, "Ziel", ziel, ziel, sizeof ziel);
    fmt(t, StrGet(hi, 4), name, 0, sizeof t);
    if (!InputBox(0, t, StrGet(hi, 1), ziel, sizeof ziel)) return 1;
    l = slen(ziel); if (l > 3 && ziel[l - 1] == '\\') ziel[--l] = 0;
    if (!ordner(ziel)) { frage(18, ziel, 0, MB_OK | MB_ERROR); return 1; }
    scat(ziel, "\\", sizeof ziel);
    scpy(lst, ziel, sizeof lst); scat(lst, "RDSETUP.LST", sizeof lst);
    FileDelete(lst);
    for (k = 1; k < 100; k++) {
        zahl(key, k);
        if (!ProfileGetStr(inf, da, key, "", wert, sizeof wert) || !wert[0]) break;
        for (i = 0; wert[i] && wert[i] != ' ' && i < 12; i++) datei[i] = wert[i];
        datei[i] = 0; while (wert[i] == ' ') i++;
        scpy(bed, wert + i, sizeof bed);
        if (!passt(bed)) { DbgPrint("RDSETUP: weggelassen"); DbgPrint(datei); continue; }
        scpy(p, quelle, sizeof p); scat(p, datei, sizeof p);
        scpy(liste, ziel, sizeof liste); scat(liste, datei, sizeof liste);
        if (!kopieren(p, liste)) { frage(7, datei, 0, MB_OK | MB_ERROR); return 1; }
        zahl(nr, ++nd); ProfileSetStr(lst, da, nr, datei);
    }
    ProfileSetStr(lst, pk, "Name", name);
    if (prog[0]) {
        for (i = 0; i < 8 && prog[i] && prog[i] != '.'; i++) sec[i] = prog[i];
        sec[i] = 0;
        ProfileSetStr(lst, pk, "Abschnitt", sec);
        plst(p); scpy(liste, ziel, sizeof liste); scat(liste, prog, sizeof liste);
        ProfileSetStr(p, sec, "Datei", liste);
        if (ProfileGetInt(inf, pk, "Schreibtisch", 0)) ProfileSetStr(p, sec, "Schreibtisch", "1");
        melden();
    }
    zahl(nr, nd); DbgHex("RDSETUP: installiert", nd);
    frage(6, name, nr, MB_OK | MB_INFO);
    return 0;
}
static short entfernen(void)
{
    WORD k, l; char key[4]; static char p[80];
    if (!ProfileGetStr(lst, pk, "Name", "", name, sizeof name)) { frage(3, lst, 0, MB_OK | MB_ERROR); return 1; }
    DbgPrint("RDSETUP: entfernen"); DbgPrint(name);
    if (frage(8, name, 0, MB_YESNO | MB_WARN) != ID_YES) return 1;
    scpy(ziel, lst, sizeof ziel); l = slen(ziel); while (l && ziel[l - 1] != '\\') l--; ziel[l] = 0;
    for (k = 1; k < 100; k++) {
        zahl(key, k);
        if (!ProfileGetStr(lst, da, key, "", wert, sizeof wert) || !wert[0]) break;
        scpy(p, ziel, sizeof p); scat(p, wert, sizeof p); FileDelete(p);
    }
    if (ProfileGetStr(lst, pk, "Abschnitt", "", sec, sizeof sec) && sec[0]) {
        plst(p); ProfileSetStr(p, sec, "Datei", 0); ProfileSetStr(p, sec, "Schreibtisch", 0); melden();
    }
    FileDelete(lst);
    if (l > 3) { ziel[l - 1] = 0; DirRemove(ziel); }   /* nur wenn leer */
    DbgPrint("RDSETUP: entfernt");
    frage(9, name, 0, MB_OK | MB_INFO);
    return 0;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD l, i;
    (void)show; hi = inst;
    sd.groesse = sizeof sd; SysInfo(&sd);
    while (*cmd == ' ') cmd++;
    if ((cmd[0] == '/' || cmd[0] == '-') && (cmd[1] | 0x20) == 'd') {
        for (cmd += 2; *cmd == ' '; cmd++) ;
        scpy(lst, cmd, sizeof lst); return entfernen();
    }
    scpy(inf, cmd, sizeof inf);
    if (!inf[0]) { scpy(inf, "A:\\SETUP.INF", sizeof inf); if (!InputBox(0, StrGet(hi, 2), StrGet(hi, 1), inf, sizeof inf)) return 1; }
    for (l = slen(inf); l && inf[l - 1] == ' '; ) inf[--l] = 0;
    if (l && inf[l - 1] == '\\') scat(inf, "SETUP.INF", sizeof inf);
    scpy(quelle, inf, sizeof quelle);
    for (i = l = 0; quelle[i]; i++) if (quelle[i] == '\\' || quelle[i] == ':') l = i + 1;
    quelle[l] = 0;
    return installieren();
}
