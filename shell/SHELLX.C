/* SHELLX.C - selten gebrauchte Teile der Shell als verwerfbares Modul (Karsten, 30.10.2026):
 * Info, Laufwerksinfo, Eigenschaften, Dateien suchen. Die Shell laedt es bei Bedarf (ModLoad),
 * ruft eine Funktion und gibt es gleich wieder frei. Dialoge und Texte kommen aus SHELL.EXE (inst).
 * Alle Funktionen: WORD f(HINST inst, LPSTR a, LPSTR b, WORD w). */
#include "RD2.H"
static char buf[200];
static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, n - l); }
static char *zahl(char *p, DWORD v) { char t[11]; short i = 0; do t[i++] = (char)('0' + v % 10); while (v /= 10); while (i) *p++ = t[--i]; *p = 0; return p; }
static void fmt(char *d, LPCSTR t, LPCSTR a, LPCSTR b)   /* %s nacheinander durch a, b */
{
    WORD n = 0;
    while (*t && n < sizeof buf - 1) {
        if (t[0] == '%' && t[1] == 's') { LPCSTR s = a ? a : ""; a = b; b = 0; while (*s && n < sizeof buf - 1) d[n++] = *s++; t += 2; }
        else d[n++] = *t++;
    }
    d[n] = 0;
}
static void dbg(LPCSTR t) { char z[40]; scpy(z, "SHELL: ", sizeof z); scat(z, t, sizeof z); DbgPrint(z); }

/* 1: Info (a: short[2] Schirmbreite, -hoehe) */
static LPCSTR nte(LPCSTR t, WORD i) { while (i-- && *t) while (*t++); return t; }   /* i-tes Wort aus "a\0b\0" */
static void zk(char *p, WORD v) { p = zahl(p, v / 10); *p++ = ','; zahl(p, v % 10); }   /* 0,1-Einheiten */
WORD RDEXPORT ShxInfo(HINST inst, LPSTR a, LPSTR b, WORD w)
{
    static const char cpu[] = "8088\0" "8086\0" "NEC V20\0" "80186\0" "80286\0" "80386\0" "80486\0" "Pentium\0";
    static const char gfx[] = "?\0" "CGA\0" "Hercules\0" "VGA mono\0" "EGA\0" "VGA\0" "VESA\0" "EGA 320\0" "EGA mono\0" "EGA 4\0" "VGA 4\0" "EGA 200\0" "VESA 16\0" "MCGA\0" "VESA 800\0" "VESA 1024\0" "CGA 4\0";
    SYSINFO si; char t[12], u[24], *p; WORD v = KernVersion(); short FAR *wh = (short FAR *)a;
    (void)b; (void)w;
    KernInfo(&si);
    p = zahl(t, v >> 8); *p++ = '.'; zahl(p, v & 0xFF);
    fmt(buf, StrGet(inst, 1), t, 0);                                        /* RetroDesk 2, API x.y */
    scat(buf, StrGet(inst, 21), sizeof buf); scat(buf, nte(cpu, si.cpu), sizeof buf);
    if (si.mhz10) { scat(buf, ", ca. ", sizeof buf); zk(t, si.mhz10); scat(buf, t, sizeof buf); scat(buf, " MHz", sizeof buf); }
    scat(buf, StrGet(inst, 22), sizeof buf);                                 /* Koprozessor */
    if (si.fpu && si.cpu >= CPU_486) scat(buf, StrGet(inst, 30), sizeof buf);   /* 486DX und spaeter: eingebaut */
    else if (si.fpu) { zahl(t, si.fpu == 87 ? 8087 : 80000u + si.fpu); scat(buf, t, sizeof buf); } else scat(buf, StrGet(inst, 26), sizeof buf);
    scat(buf, StrGet(inst, 23), sizeof buf); zahl(t, si.kb_konv); scat(buf, t, sizeof buf);   /* Speicher */
    scat(buf, StrGet(inst, 24), sizeof buf); zahl(t, (WORD)(MemAvail(MEMA_KONV) >> 10)); scat(buf, t, sizeof buf); scat(buf, " KB", sizeof buf);
    if (si.kb_ext) { scat(buf, " + XMS ", sizeof buf); zahl(t, si.kb_ext); scat(buf, t, sizeof buf); scat(buf, " KB", sizeof buf); }
    scat(buf, "\nDOS ", sizeof buf); p = zahl(t, si.dos >> 8); *p++ = '.'; zahl(p, si.dos & 0xFF); scat(buf, t, sizeof buf);
    {   GFXINFO gl[17]; WORD n = GfxList(gl, 17), k, g = si.grafik;   /* laufende Grafikart (RDGDI), nicht nur der Startschalter */
        for (k = 0; k < n; k++) if (gl[k].flags & GI_AKTIV) g = gl[k].typ;
        scat(buf, StrGet(inst, 25), sizeof buf); scat(buf, nte(gfx, g <= GFX_CGA4 ? g : 0), sizeof buf); }   /* Grafik */
    scat(buf, " ", sizeof buf); zahl(t, wh[0]); scat(buf, t, sizeof buf); scat(buf, "x", sizeof buf); zahl(t, wh[1]); scat(buf, t, sizeof buf);
    scat(buf, StrGet(inst, 27), sizeof buf);                                 /* Sound */
    {   WORD sc = SndCaps();                      /* geladener Klangtreiber (RDSND), SYSINFO kennt ihn nicht */
        scat(buf, sc & SC_OPL3 ? "Sound Blaster/OPL3" : sc & SC_OPL2 ? "AdLib" : sc & SC_SPEAKER ? "PC-Speaker" : sc ? "Sound Blaster" : StrGet(inst, 26), sizeof buf); }
    scpy(u, StrGet(inst, 5), sizeof u); MsgBox(GetActive(), buf, u, MB_OK | MB_INFO);
    return 0;
}

/* 2: Laufwerksinfo (w: Laufwerksbuchstabe) */
WORD RDEXPORT ShxLwInfo(HINST inst, LPSTR a0, LPSTR b0, WORD w)
{
    DRIVEINFO di; char a[48], g[12], f[12], *p; WORD e;
    (void)a0; (void)b0;
    a[0] = (char)w; a[1] = ':'; a[2] = 0;
    CursorBusy(TRUE); e = DriveInfo((WORD)(w - 'A' + 1), &di); CursorBusy(FALSE);
    if (e) { fmt(buf, StrGet(inst, 4), a, 0); MsgBox(GetActive(), buf, "RD", MB_OK | MB_WARN); return 0; }
    p = zahl(g, di.gesamt >> 10); scpy(p, " KB", 4);
    p = zahl(f, di.frei >> 10); scpy(p, " KB", 4);
    if (di.label[0]) { scat(a, " ", sizeof a); scat(a, di.label, sizeof a); }
    fmt(buf, StrGet(inst, 13), a, g); scat(buf, f, sizeof buf);
    scpy(g, StrGet(inst, 14), sizeof g); MsgBox(GetActive(), buf, g, MB_OK | MB_INFO);
    dbg("Laufwerksinfo");
    return 0;
}

/* 3: Eigenschaften (a: voller Pfad). 0xFFFF: nichts aendern; 0xF000 | Fehler; sonst alt << 8 | neu */
static char ez[3][48]; static WORD eat;
static BOOL FAR __pascal __loadds eproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD i;
    (void)p2;
    if (msg == M_INITDIALOG) {
        for (i = 0; i < 3; i++) DlgSetText(d, (WORD)(41 + i), ez[i]);
        for (i = 0; i < 4; i++) DlgCheck(d, (WORD)(31 + i), (eat & (i == 3 ? FA_ARCH : 1 << i)) != 0);
    }
    if (msg == M_COMMAND && p1 == ID_OK)
        for (eat &= FA_DIR, i = 0; i < 4; i++) if (DlgChecked(d, (WORD)(31 + i))) eat |= i == 3 ? FA_ARCH : 1 << i;
    return FALSE;
}
static void z2(char *p, WORD v) { p[0] = (char)('0' + v / 10 % 10); p[1] = (char)('0' + v % 10); }
WORD RDEXPORT ShxEigen(HINST inst, LPSTR qv, LPSTR b0, WORD w)
{
    FINDDATA fd; char *p; WORD at;
    (void)b0; (void)w;
    if (FileFindFirst(qv, FA_RDONLY | FA_HIDDEN | FA_SYSTEM | FA_DIR | FA_ARCH, &fd)) return 0xF000 | (LastError() & 0xFFF);
    scpy(ez[0], StrGet(inst, 32), sizeof ez[0]); scat(ez[0], qv, sizeof ez[0]);
    scpy(ez[1], StrGet(inst, 33), sizeof ez[1]);
    if (fd.attr & FA_DIR) scat(ez[1], StrGet(inst, 31), sizeof ez[1]);
    else { p = ez[1]; while (*p) p++; p = zahl(p, fd.groesse); scpy(p, " Bytes", 8); }
    scpy(ez[2], StrGet(inst, 34), sizeof ez[2]); p = ez[2]; while (*p) p++;
    z2(p, fd.datum & 31); p[2] = '.'; z2(p + 3, (fd.datum >> 5) & 15); p[5] = '.'; p = zahl(p + 6, 1980 + (fd.datum >> 9));
    *p++ = ' '; z2(p, fd.zeit >> 11); p[2] = ':'; z2(p + 3, (fd.zeit >> 5) & 63); p[5] = 0;
    eat = at = fd.attr;
    dbg("Eigenschaften"); DbgPrint(ez[1]); DbgPrint(ez[2]);
    if (DialogRun(inst, 20, GetActive(), eproc, 0) != ID_OK || eat == at) return 0xFFFF;
    return (WORD)((at & 0xFF) << 8 | (eat & 0xFF));
}

/* 4: Dateien suchen: Muster mit * und ? (8.3), ab einem Ordner durch alle Unterordner (8 Ebenen), 100 Treffer.
   a: Ordner mit \ am Ende (80 Zeichen), danach der Ordner des gewaehlten Treffers; b: Muster (13, bleibt in der Shell).
   1 = a oeffnen, 0 = nichts, 0xF000 | Fehler */
#define SMAX 100
#define SLEN 66
static HMEM sh; static WORD sn; static short ssel; static char sm[13], sp[SLEN + 14];
static BYTE teil(const char *n, const char *m, WORD k)   /* Namens- oder Endungsteil, hoechstens k Zeichen */
{
    WORD i;
    for (i = 0; i < k; i++) {
        if (*m == '*') return 1;
        if (!*m) return (BYTE)!*n;
        if (*m == '?') { if (*n) n++; m++; continue; }
        if (!*n || (*n & 0xDF) != (*m & 0xDF)) return 0;
        n++; m++;
    }
    return 1;
}
static BYTE passt(const char *n)                  /* n gegen sm (beide NAME.EXT) */
{
    char a[13], b[13], *q; const char *e1, *e2;
    scpy(a, n, 13); scpy(b, sm, 13);
    q = a; while (*q && *q != '.') q++; e1 = *q ? q + 1 : q; *q = 0;
    q = b; while (*q && *q != '.') q++; e2 = *q ? q + 1 : "*"; if (!*q && b[0] == '*') e2 = "*"; else if (!*q) e2 = ""; *q = 0;
    return teil(a, b, 8) && teil(e1, e2, 3);
}
static void durch(WORD l, WORD tief)              /* sp[0..l) = Ordner mit \ am Ende */
{
    FINDDATA f; char FAR *t; WORD k;
    sp[l] = 0; scat(sp, "*.*", sizeof sp);
    if (FileFindFirst(sp, FA_DIR | FA_HIDDEN | FA_SYSTEM | FA_RDONLY | FA_ARCH, &f)) return;
    do {
        if (f.name[0] == '.' || sn >= SMAX) continue;
        sp[l] = 0;
        if (passt(f.name) && (t = (char FAR *)MemLock(sh)) != 0) {
            t += sn * SLEN; for (k = 0; k < l && k < SLEN - 13; k++) t[k] = sp[k];
            for (t += k, k = 0; f.name[k]; k++) t[k] = f.name[k];
            t[k] = (char)((f.attr & FA_DIR) ? '\\' : 0); t[k + (t[k] != 0)] = 0;
            MemUnlock(sh); sn++;
        }
        if ((f.attr & FA_DIR) && tief < 8 && l + 13 < SLEN) {
            sp[l] = 0; scat(sp, f.name, sizeof sp); k = (WORD)slen(sp); sp[k++] = '\\'; sp[k] = 0;
            durch(k, (WORD)(tief + 1));
            sp[l] = 0; scat(sp, "*.*", sizeof sp);   /* DTA gehoert f, Suche geht weiter */
        }
    } while (!FileFindNext(&f));
}
static BOOL FAR __pascal __loadds sproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    HWND l = DlgItem(d, 40); char FAR *t; WORD i;
    if (msg == M_INITDIALOG) {
        DlgSetText(d, 41, buf);
        if ((t = (char FAR *)MemLock(sh)) != 0) { for (i = 0; i < sn; i++) SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)(t + i * SLEN)); MemUnlock(sh); }
        if (sn) SendMessage(l, L_SETSEL, 0, 0);
        SetFocus(l);
    }
    if (msg == M_COMMAND && p1 == 40 && HIWORD(p2) == N_DBLCLK) { ssel = (short)SendMessage(l, L_GETSEL, 0, 0); DialogEnd(d, ID_OK); return TRUE; }
    if (msg == M_COMMAND && p1 == ID_OK) ssel = (short)SendMessage(l, L_GETSEL, 0, 0);
    return FALSE;
}
static WORD suche(HINST inst, LPSTR ab)
{
    char z[8], qv[80], *q; char FAR *t; WORD i;
    scpy(sp, ab, sizeof sp); fmt(buf, StrGet(inst, 36), sp, 0);
    if (!sm[0]) scpy(sm, "*.*", sizeof sm);
    if (!InputBox(GetActive(), buf, "RD", sm, sizeof sm) || !sm[0]) return 0;
    for (q = sm; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    if (!(sh = MemAlloc((DWORD)SMAX * SLEN, MEM_MOVABLE))) return 0xF000 | RD_ERR_NOMEM;
    sn = 0; ssel = -1; CursorBusy(TRUE); durch((WORD)slen(sp), 0); CursorBusy(FALSE);
    zahl(z, sn); scpy(qv, ab, sizeof qv); fmt(buf, StrGet(inst, 37), z, qv);
    dbg("Suche"); DbgPrint(buf);
    if (!sn) { MsgBox(GetActive(), StrGet(inst, 38), "RD", MB_OK | MB_INFO); return 0; }
    if ((t = (char FAR *)MemLock(sh)) != 0) { DbgPrint((LPCSTR)t); MemUnlock(sh); }
    if (DialogRun(inst, 21, GetActive(), sproc, 0) != ID_OK || ssel < 0 || ssel >= (short)sn) return 0;
    if (!(t = (char FAR *)MemLock(sh))) return 0;
    t += ssel * SLEN; for (i = 0; t[i] && i < 79; i++) ab[i] = t[i]; ab[i] = 0; MemUnlock(sh);
    if (i && ab[i - 1] == '\\') ab[--i] = 0;     /* Ordner-Treffer: den Ordner selbst nicht, sondern den darueber */
    while (i && ab[i - 1] != '\\') i--; ab[i] = 0;
    return 1;
}
WORD RDEXPORT ShxSuchen(HINST inst, LPSTR ab, LPSTR muster, WORD w)
{
    WORD r, i;
    (void)w;
    for (i = 0; i < 13; i++) sm[i] = muster[i];
    sh = 0; r = suche(inst, ab);
    if (sh) MemFree(sh);
    for (i = 0; i < 13; i++) muster[i] = sm[i];
    return r;
}

static struct { WORD n; FARPROC f[4]; } ex = { 4, { (FARPROC)ShxInfo, (FARPROC)ShxLwInfo, (FARPROC)ShxEigen, (FARPROC)ShxSuchen } };
void FAR * RDAPI RdModInit(void) { return &ex; }
