/* RDSHELL.C - DOS-Fenster (Punkt 8, wie RD1 RDSHELL.OVL): Befehlszeile im Fenster, Verlauf oben, Eingabe unten.
 * Eingebaut (ueber die RD2-Dateifunktionen, laeuft ohne COMMAND.COM): DIR [/W], CD, X:, MD, RD, DEL, REN, COPY,
 * TYPE, VOL, MEM, VER, DATE, TIME, ECHO, CLS, EXIT, HILFE. Alles andere startet als DOS-Programm ueber RunDos
 * (RD2 lagert sich aus und kommt mit allen Fenstern zurueck): .COM/.EXE im aktuellen Verzeichnis oder in
 * RD2.INI [DOS] Pfad=; .BAT und Unbekanntes ueber COMMAND.COM /C ([DOS] Comspec=, sonst Startlaufwerk).
 * Das Fenster fuehrt ein eigenes aktuelles Verzeichnis (andere Programme merken davon nichts).
 * Befehlszeile "T": Testmodus (jede Ausgabezeile per DbgPrint); "T datei": fuehrt die Zeilen der Datei nacheinander
 * aus wie getippt (eine je Sekunde) und schliesst danach. test/rdshell.sh. */
#include "RD2.H"

#define NV   150                                  /* Zeilen im Verlauf */
#define VB   81
#define NH   8                                    /* fruehere Befehle */
#define RAND 2
#define CM_COMMAND 101
#define CM_LEER 102
#define CM_ENDE 109

static HINST hi; static HWND fe; static HMENU mn; static BYTE test;
static char cwd[80], ini[80], eb[128], hist[NH][128]; static WORD el, ex, eo, hn, hz;
static HMEM hm; static char FAR *vl; static WORD vn, zur;
static short zh, cw, cbreit, choch; static WORD rows, cols;
static SYSINFO si; static short sk = -1;          /* Testskript */

/* ---------------- Hilfen ---------------- */
static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static char gr(char c) { return (char)(c >= 'a' && c <= 'z' ? c - 32 : c); }
static BYTE gleich(LPCSTR a, LPCSTR b) { while (*a && gr(*a) == *b) { a++; b++; } return (BYTE)(!*a && !*b); }
static BYTE wild(LPCSTR s) { while (*s) { if (*s == '*' || *s == '?') return 1; s++; } return 0; }
static char *zahl(DWORD v, char *e)               /* rechtsbuendig nach e (exklusiv), mit Punkten; liefert Anfang */
{
    WORD k = 0; *--e = 0;
    do { if (k && !(k % 3)) *--e = '.'; *--e = (char)('0' + (WORD)(v % 10)); v /= 10; k++; } while (v);
    return e;
}
static void zw(char *d, WORD v) { d[0] = (char)('0' + v / 10 % 10); d[1] = (char)('0' + v % 10); }

/* ---------------- Verlauf und Anzeige (wie RDIRC) ---------------- */
static WORD vsicht(void) { return vn < NV ? vn : NV; }
static void balken(void)
{
    WORD v = vsicht(), m = v > rows ? v - rows : 0;
    if (zur > m) zur = m;
    ScrollSet(fe, SB_VERT, (short)(m - zur), (short)m, (short)rows);
}
static void neu(void) { balken(); InvalidateRect(fe, 0, TRUE); }
static void put1(LPCSTR s, WORD n)
{
    char FAR *d = vl + (WORD)(vn % NV) * VB; WORD i;
    for (i = 0; i < n && i < VB - 1; i++) d[i] = s[i];
    d[i] = 0; vn++;
    if (vn == 2 * NV) vn = NV;
    if (zur) zur++;
}
static void sag(LPCSTR s)                         /* anhaengen, hart umbrechen */
{
    WORD l = slen(s), w = cols < 20 ? 20 : (cols > VB - 1 ? VB - 1 : cols);
    if (test) { char t[100]; scpy(t, "RDSHELL: | ", sizeof t); scat(t, s, sizeof t); DbgPrint(t); }
    do { WORD k = l < w ? l : w; put1(s, k); s += k; l -= k; } while (l);
}
static void sag2(LPCSTR a, LPCSTR b) { char t[200]; scpy(t, a, sizeof t); scat(t, b, sizeof t); sag(t); }
static void sagz(WORD id) { sag(StrGet(hi, id)); }
static void prompt(char *t, WORD n) { scpy(t, cwd, n); scat(t, ">", n); }
static void eingabe_neu(void) { RECT r; r.l = 0; r.r = cbreit; r.t = (short)(rows * zh); r.b = choch; InvalidateRect(fe, &r, TRUE); }
static void malen(void)
{
    PAINT ps; HDC d; WORD v = vsicht(), i, erste, k, pl; short y; char t[VB + 90];
    d = BeginPaint(fe, &ps);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK); SetPen(d, C_BLACK);
    erste = v > rows + zur ? v - rows - zur : 0;
    for (i = 0, y = 0; i < rows && erste + i < v - zur; i++, y += zh) {
        char FAR *s = vl + (WORD)((vn - v + erste + i) % NV) * VB; WORD n = 0;
        if (y + zh <= ps.neu.t || y >= ps.neu.b) continue;
        while (s[n] && n < cols) { t[n] = s[n]; n++; }
        if (n) TextOut(d, RAND, y, t, (short)n);
    }
    y = (short)(rows * zh);
    if (ps.neu.b > y) {                           /* Trennstrich und Eingabezeile "A:\>text" */
        Line(d, 0, (short)(y + 1), (short)(cbreit - 1), (short)(y + 1));
        prompt(t, 80); pl = slen(t);
        if (cols > pl + 4) {
            WORD sicht = cols - pl - 1;
            if (ex < eo) eo = ex; else if (ex > eo + sicht) eo = ex - sicht;
            for (k = 0; k < sicht && eo + k < el; k++) t[pl + k] = eb[eo + k];
            t[pl + k] = 0;
            TextOut(d, RAND, (short)(y + 3), t, -1);
            if (GetFocus() == fe) { short x = (short)(RAND + (pl + ex - eo) * cw); Line(d, x, (short)(y + 3), x, (short)(y + 2 + zh)); }
        }
    }
    EndPaint(fe, &ps);
}
static void groesse(void)
{
    RECT r; GetClientRect(fe, &r); cbreit = r.r; choch = r.b;
    rows = (WORD)((r.b - zh - 4) / zh); if (!rows || r.b < zh * 2) rows = 1;
    cols = (WORD)((r.r - 2 * RAND) / cw); if (!cols) cols = 1;
    if (cols > VB - 1) cols = VB - 1;
    neu();
}

/* ---------------- Pfade ---------------- */
static void voll(LPCSTR a, char *o)               /* Pfad relativ zu cwd -> "X:\A\B" in Grossbuchstaben, . und .. aufgeloest */
{
    WORD n;
    while (*a == ' ') a++;
    if (a[0] && a[1] == ':') {
        if (gr(a[0]) == cwd[0]) scpy(o, cwd, 80);
        else if (DirGet((WORD)(gr(a[0]) - 'A' + 1), o)) { o[0] = gr(a[0]); o[1] = ':'; o[2] = '\\'; o[3] = 0; }
        a += 2;
    } else scpy(o, cwd, 80);
    if (*a == '\\') { o[3] = 0; a++; }
    n = slen(o);
    while (*a) {
        char t[13]; WORD k = 0;
        while (*a && *a != '\\') { if (k < 12) t[k++] = gr(*a); a++; }
        t[k] = 0; if (*a) a++;
        if (!k || (k == 1 && t[0] == '.')) continue;
        if (k == 2 && t[0] == '.' && t[1] == '.') { while (n > 3 && o[n - 1] != '\\') n--; if (n > 3) n--; o[n] = 0; continue; }
        if (n > 3 && n < 78) o[n++] = '\\';
        o[n] = 0; scat(o, t, 80); n = slen(o);
    }
}
static BYTE istdir(LPCSTR p) { WORD a; if (slen(p) <= 3) return 1; a = FileAttr(p, 0, 0); return (BYTE)(a != 0xFFFF && (a & FA_DIR)); }
static void anh(char *p, LPCSTR n) { WORD l = slen(p); if (l && p[l - 1] != '\\') scat(p, "\\", 80); scat(p, n, 80); }
static char *namez(char *p) { char *q = p; while (*p) { if (*p == '\\' || *p == ':') q = p + 1; p++; } return q; }
static void fehler(WORD e)
{
    sagz(e == RD_ERR_NOTFOUND ? 4 : e == RD_ERR_NODISK ? 5 : e == RD_ERR_WPROT ? 6 : e == RD_ERR_FULL ? 7 : e == RD_ERR_DISK ? 8 : 10);
}

/* ---------------- Eingebaute Befehle ---------------- */
static void dir(char *p)
{
    char m[80], z[96], *e; FINDDATA fd; DRIVEINFO di; WORD n = 0, b = 0, i, wide = 0; DWORD sum = 0;
    for (e = p; *e; e++) if (*e == '/' && gr(e[1]) == 'W') { wide = 1; e[0] = e[1] = ' '; }
    while (*p == ' ') p++;
    for (e = p; *e && *e != ' '; e++) ; *e = 0;
    voll(p, m);
    if (!wild(m) && istdir(m)) anh(m, "*.*");
    { char v[80]; scpy(v, m, sizeof v); *namez(v) = 0; if (slen(v) > 3) v[slen(v) - 1] = 0; sag(""); sag2(StrGet(hi, 12), v); sag(""); }
    z[0] = 0;
    if (!FileFindFirst(m, FA_DIR, &fd)) do {
        char *pkt;
        if (fd.name[0] == '.' && !fd.name[1]) continue;
        if (wide) {
            if (fd.attr & FA_DIR) { scat(z, "[", sizeof z); scat(z, fd.name, sizeof z); scat(z, "]", sizeof z); } else scat(z, fd.name, sizeof z);
            for (i = slen(z); i % 15; i++) z[i] = ' ';
            z[i] = 0;
            if (i >= 75 || i + 15 > cols) { sag(z); z[0] = 0; }
        } else {
            for (i = 0; i < 40; i++) z[i] = ' ';
            for (i = 0, pkt = fd.name; *pkt && *pkt != '.'; pkt++) z[i++] = *pkt;
            if (*pkt == '.') for (i = 9, pkt++; *pkt; pkt++) z[i++] = *pkt;
            if (fd.attr & FA_DIR) { LPCSTR d = StrGet(hi, 39); for (i = 0; d[i]; i++) z[14 + i] = d[i]; }
            else { char t[16], *q = zahl(fd.groesse, t + 15); WORD l = slen(q); for (i = 0; i < l; i++) z[25 - l + i] = q[i]; }
            zw(z + 27, fd.datum & 31); z[29] = '.'; zw(z + 30, (fd.datum >> 5) & 15); z[32] = '.'; zw(z + 33, (1980 + (fd.datum >> 9)) % 100);
            zw(z + 37, fd.zeit >> 11); z[39] = ':'; zw(z + 40, (fd.zeit >> 5) & 63); z[42] = 0;
            sag(z);
        }
        if (fd.attr & FA_DIR) b++; else { n++; sum += fd.groesse; }
    } while (!FileFindNext(&fd));
    if (wide && z[0]) sag(z);
    if (!n && !b) { sagz(4); return; }
    { char t[16], s[80]; scpy(s, zahl(n, t + 9), sizeof s); scat(s, StrGet(hi, 13), sizeof s); scat(s, zahl(sum, t + 15), sizeof s); scat(s, StrGet(hi, 14), sizeof s); sag2("  ", s);
      if (!DriveInfo((WORD)(m[0] - 'A' + 1), &di)) { scpy(s, zahl(di.frei, t + 15), sizeof s); scat(s, StrGet(hi, 15), sizeof s); sag2("  ", s); } }
}
static void zweiarg(char *p, char **b)            /* p: "a b" -> p = a, *b = b */
{
    while (*p && *p != ' ') p++;
    if (*p) *p++ = 0;
    while (*p == ' ') p++;
    *b = p;
    while (*p && *p != ' ') p++;
    *p = 0;
}
static void kopie(char *a)
{
    char *b, q[80], z[80], s[80], d[80]; FINDDATA fd; WORD n = 0, e = 0, ziel; HMEM h; char FAR *buf;
    zweiarg(a, &b);
    if (!*a) { sagz(11); return; }
    voll(a, q); voll(*b ? b : ".", z);
    if (!wild(q) && istdir(q)) anh(q, "*.*");
    ziel = istdir(z);
    if (!(h = MemAlloc(4096, MEM_MOVABLE)) || !(buf = (char FAR *)MemLock(h))) { if (h) MemFree(h); sagz(33); return; }
    if (FileFindFirst(q, 0, &fd)) e = RD_ERR_NOTFOUND;
    else do {
        short fi, fo; WORD r;
        scpy(s, q, sizeof s); *namez(s) = 0; scat(s, fd.name, sizeof s);
        scpy(d, z, sizeof d); if (ziel) anh(d, fd.name);
        if (gleich(s, d)) { sagz(10); continue; }
        if ((fi = FileOpen(s, FO_READ)) < 0) { e = LastError(); break; }
        if ((fo = FileCreate(d, 0)) < 0) { e = LastError(); FileClose(fi); break; }
        while ((r = FileRead(fi, buf, 4096)) != 0 && r != 0xFFFF) if (FileWrite(fo, buf, r) != r) { e = RD_ERR_FULL; break; }
        FileClose(fi); FileClose(fo);
        if (e) { FileDelete(d); break; }
        sag(fd.name); n++;
        if (!ziel) break;                         /* mehrere Quellen in eine Datei: nur die erste */
    } while (!FileFindNext(&fd));
    MemUnlock(h); MemFree(h);
    if (e) fehler(e);
    { char t[10]; sag2(zahl(n, t + 9), StrGet(hi, 16)); }
}
static void loeschen(char *a)
{
    char q[80], s[80]; FINDDATA fd; WORD e = 0, n = 0;
    { char *b; zweiarg(a, &b); }
    if (!*a) { sagz(11); return; }
    voll(a, q);
    if (!wild(q) && istdir(q)) anh(q, "*.*");
    if (gleich(namez(q), "*.*") && MsgBox(fe, StrGet(hi, 17), StrGet(hi, 1), MB_YESNO) != ID_YES) { sagz(40); return; }
    if (FileFindFirst(q, 0, &fd)) e = RD_ERR_NOTFOUND;
    else do {
        scpy(s, q, sizeof s); *namez(s) = 0; scat(s, fd.name, sizeof s);
        if ((e = FileDelete(s)) != 0) break;
        n++;
    } while (!FileFindNext(&fd));
    if (e && !(e == RD_ERR_NOTFOUND && n)) fehler(e);
}
static void typ(char *a)
{
    char q[80], z[VB]; BYTE b[256]; short f; WORD r, i, n = 0, w = cols < VB - 1 ? cols : VB - 1, zeilen = 0;
    { char *x; zweiarg(a, &x); }
    if (!*a) { sagz(11); return; }
    voll(a, q);
    if ((f = FileOpen(q, FO_READ)) < 0) { fehler(LastError()); return; }
    while ((r = FileRead(f, b, sizeof b)) != 0 && r != 0xFFFF && zeilen < 2000) {
        for (i = 0; i < r; i++) {
            BYTE c = b[i];
            if (c == 26) { r = 0; break; }
            if (c == '\r') continue;
            if (c == '\n' || n >= w) { z[n] = 0; sag(z); n = 0; zeilen++; if (c == '\n') continue; }
            if (c == '\t') { do z[n++] = ' '; while (n % 8 && n < w); continue; }
            z[n++] = (char)(c < ' ' ? '.' : c);
        }
        if (!r) break;
    }
    if (n) { z[n] = 0; sag(z); }
    FileClose(f);
}
static void vol(WORD lw)
{
    DRIVEINFO di; char t[80], h[10]; WORD i; DWORD s;
    if (DriveInfo(lw, &di)) { sagz(5); return; }
    scpy(t, " ", sizeof t); scat(t, StrGet(hi, 18), sizeof t); h[0] = (char)('A' + lw - 1); h[1] = 0; scat(t, h, sizeof t);
    if (di.label[0]) { scat(t, StrGet(hi, 20), sizeof t); scat(t, di.label, sizeof t); } else scat(t, StrGet(hi, 19), sizeof t);
    sag(t);
    if (di.serial) {
        for (s = di.serial, i = 9; i-- > 0; ) { if (i == 4) { h[i] = '-'; continue; } h[i] = "0123456789ABCDEF"[(WORD)s & 15]; s >>= 4; }
        h[9] = 0; scpy(t, " ", sizeof t); scat(t, StrGet(hi, 21), sizeof t); scat(t, h, sizeof t); sag(t);
    }
}
static void kbz(WORD id, DWORD kb) { char t[60], z[14]; scpy(t, StrGet(hi, id), sizeof t); scat(t, zahl(kb, z + 13), sizeof t); scat(t, StrGet(hi, 27), sizeof t); sag(t); }
static void zeit(BYTE datum)
{
    RDTIME t; char s[20];
    TimeGet(&t);
    if (datum) { zw(s, t.tag); s[2] = '.'; zw(s + 3, t.monat); s[5] = '.'; zw(s + 6, t.jahr / 100); zw(s + 8, t.jahr); s[10] = 0; }
    else { zw(s, t.std); s[2] = ':'; zw(s + 3, t.min); s[5] = ':'; zw(s + 6, t.sek); s[8] = 0; }
    { char z[80]; scpy(z, StrGet(hi, datum ? 28 : 29), sizeof z); scat(z, s, sizeof z); scat(z, "  ", sizeof z); scat(z, StrGet(hi, 30), sizeof z); sag(z); }
}

/* ---------------- DOS-Programme ---------------- */
static BYTE suche(LPCSTR name, char *o)           /* name ohne Pfad/Endung im cwd und [DOS] Pfad=; 1 = .COM/.EXE, 2 = .BAT */
{
    static const char ext[] = ".COM\0.EXE\0.BAT\0"; char pf[160], d[80], *p; WORD k; BYTE hatext = 0, pfad = 0;
    for (p = (char *)name; *p; p++) { if (*p == '.') hatext = 1; if (*p == '\\' || *p == ':') { pfad = 1; hatext = 0; } }
    ProfileGetStr(ini, "DOS", "Pfad", "", pf, sizeof pf);
    for (p = 0; ; ) {
        if (!p) scpy(d, cwd, sizeof d);
        else { WORD i = 0; while (*p == ';' || *p == ' ') p++; if (!*p) return 0; while (*p && *p != ';' && i < 78) d[i++] = *p++; d[i] = 0; voll(d, o); scpy(d, o, sizeof d); }
        for (k = 0; k < 3; k++) {
            WORD a;
            if (pfad) voll(name, o); else { scpy(o, d, 80); anh(o, name); }
            if (!hatext) scat(o, ext + k * 5, 80);
            a = FileAttr(o, 0, 0);
            if (a != 0xFFFF && !(a & FA_DIR)) { WORD l = slen(o); return (BYTE)(gr(o[l - 3]) == 'B' ? 2 : 1); }
            if (hatext) break;
        }
        if (pfad) return 0;
        if (!p) p = pf;
    }
}
static BYTE comspec(char *o)
{
    WORD a;
    ProfileGetStr(ini, "DOS", "Comspec", "", o, 80);
    if (!o[0]) { o[0] = (char)('A' + si.bootlw - 1); o[1] = 0; scat(o, ":\\COMMAND.COM", 80); }
    a = FileAttr(o, 0, 0);
    return (BYTE)(a != 0xFFFF && !(a & FA_DIR));
}
static void dos(LPCSTR prog, LPCSTR arg)
{
    WORD r; char t[16];
    if (test) { char s[200]; scpy(s, "RDSHELL: RunDos ", sizeof s); scat(s, prog, sizeof s); scat(s, " ", sizeof s); scat(s, arg, sizeof s); DbgPrint(s); }
    {   LPCSTR b = prog, q;                       /* RD.EXE im laufenden RD2: zweiter Kern, nicht starten */
        for (q = prog; *q; q++) if (*q == '\\' || *q == ':') b = q + 1;
        if ((b[0] | 0x20) == 'r' && (b[1] | 0x20) == 'd' && (!b[2] || b[2] == '.')) { sagz(41); return; }
    }
    DirChange(cwd);
    r = RunDos(prog, arg, RD_DOS_KEEP | RD_DOS_PAUSE);
    if (r == 0xFFFF) { r = LastError(); sagz(r == RD_ERR_NOTSUP ? 32 : r == RD_ERR_NOMEM ? 33 : 3); }
    else if (r) sag2(StrGet(hi, 31), zahl(r, t + 15));
    neu();
}
static void extern_(char *c, char *p)
{
    char o[80], a[140], *x; BYTE k;
    for (x = c; *x; x++) *x = gr(*x);
    k = suche(c, o);
    if (k == 1) { dos(o, p); return; }
    if (k == 2 || slen(c)) {
        char cs[80];
        if (!comspec(cs)) { sagz(k == 2 ? 38 : 3); return; }
        scpy(a, "/C ", sizeof a); scat(a, k == 2 ? o : c, sizeof a); if (*p) { scat(a, " ", sizeof a); scat(a, p, sizeof a); }
        dos(cs, a);
    }
}

/* ---------------- Befehlszeile ---------------- */
static void ausfuehren(void)
{
    char z[130], t[200], *c, *p; WORD e;
    eb[el] = 0; prompt(t, sizeof t); scat(t, eb, sizeof t); sag(t);
    scpy(z, eb, sizeof z); el = ex = eo = 0;
    if (z[0]) { WORD i; for (i = NH - 1; i; i--) scpy(hist[i], hist[i - 1], sizeof hist[0]); scpy(hist[0], z, sizeof hist[0]); if (hn < NH) hn++; }
    hz = 0;
    for (c = z; *c == ' '; c++) ;
    for (p = c; *p && *p != ' '; p++) {                /* "dir/w", "cd\", "cd.." */
        if (p > c && *p == '/') break;
        if (p == c + 2 && gr(c[0]) == 'C' && gr(c[1]) == 'D' && (*p == '\\' || *p == '.')) break;
    }
    if (*p) { char *q = p; if (*q == ' ') *q++ = 0; else { WORD l = slen(q); for (; l + 1; l--) q[l + 1] = q[l]; *q++ = 0; } p = q; }
    while (*p == ' ') p++;
    if (!*c) ;
    else if (c[1] == ':' && !c[2]) {
        DRIVEINFO di; WORD lw = (WORD)(gr(c[0]) - 'A' + 1);
        if (lw > 26 || DriveInfo(lw | DI_NURTYP, &di) || di.typ == DT_NONE || DirGet(lw, t)) sagz(5);
        else scpy(cwd, t, sizeof cwd);
    }
    else if (gleich(c, "CD") || gleich(c, "CHDIR")) {
        char *b; zweiarg(p, &b);
        if (!*p) sag(cwd);
        else if (p[1] == ':' && !p[2]) { if (DirGet((WORD)(gr(p[0]) - 'A' + 1), t)) sagz(5); else sag(t); }
        else { voll(p, t); if (istdir(t)) scpy(cwd, t, sizeof cwd); else sagz(9); }
    }
    else if (gleich(c, "DIR")) dir(p);
    else if (gleich(c, "MD") || gleich(c, "MKDIR")) { char *b; zweiarg(p, &b); if (!*p) sagz(11); else { voll(p, t); if ((e = DirMake(t)) != 0) fehler(e == RD_ERR_NOTFOUND ? RD_ERR_HANDLE : e); } }
    else if (gleich(c, "RD") || gleich(c, "RMDIR")) { char *b; zweiarg(p, &b); if (!*p) sagz(11); else { voll(p, t); if (DirRemove(t)) sagz(34); } }
    else if (gleich(c, "DEL") || gleich(c, "ERASE")) loeschen(p);
    else if (gleich(c, "REN") || gleich(c, "RENAME")) {
        char *b, n[80]; zweiarg(p, &b);
        if (!*p || !*b) sagz(11);
        else { voll(p, t); scpy(n, t, sizeof n); *namez(n) = 0; { char *x = b; while (*x) { *x = gr(*x); x++; } } scat(n, b, sizeof n); if ((e = FileRename(t, n)) != 0) fehler(e); }
    }
    else if (gleich(c, "COPY")) kopie(p);
    else if (gleich(c, "TYPE")) typ(p);
    else if (gleich(c, "VOL")) vol((WORD)((p[0] && p[1] == ':' ? gr(p[0]) : cwd[0]) - 'A' + 1));
    else if (gleich(c, "MEM")) {
        kbz(22, si.kb_konv); kbz(23, MemAvail(MEMA_KONV) / 1024); kbz(24, MemAvail(MEMA_GROESSTER) / 1024);
        if (si.kb_ems) kbz(25, si.kb_ems);
        if (si.kb_ext) kbz(26, si.kb_ext);
    }
    else if (gleich(c, "VER")) {
        WORD v = KernVersion(); char s[60], n[8];
        scpy(s, "RetroDesk 2 (Kern ", sizeof s); n[0] = (char)('0' + (v >> 8)); n[1] = '.'; zw(n + 2, v & 0xFF); n[4] = 0; scat(s, n, sizeof s);
        scat(s, "), DOS ", sizeof s); n[0] = (char)('0' + (si.dos >> 8)); zw(n + 2, si.dos & 0xFF); scat(s, n, sizeof s); sag(""); sag(s);
    }
    else if (gleich(c, "DATE")) zeit(1);
    else if (gleich(c, "TIME")) zeit(0);
    else if (gleich(c, "ECHO")) sag(p);
    else if (gleich(c, "CLS")) vn = zur = 0;
    else if (gleich(c, "EXIT")) PostMessage(fe, M_CLOSE, 0, 0);
    else if (gleich(c, "HILFE") || gleich(c, "HELP") || gleich(c, "?")) { sagz(35); sagz(36); sagz(37); }
    else extern_(c, p);
    neu();
}
static void geschichte(BYTE hoch)
{
    if (hoch) { if (hz < hn) hz++; } else if (hz) hz--;
    if (hz) scpy(eb, hist[hz - 1], sizeof eb); else eb[0] = 0;
    el = ex = slen(eb); eo = 0;
}
static void taste(WORD k)
{
    WORD i;
    switch (k) {
    case K_ENTER: ausfuehren(); return;
    case K_BACK: if (ex) { for (i = ex - 1; i < el - 1; i++) eb[i] = eb[i + 1]; el--; ex--; } break;
    case K_DEL: if (ex < el) { for (i = ex; i < el - 1; i++) eb[i] = eb[i + 1]; el--; } break;
    case K_LEFT: if (ex) ex--; break;
    case K_RIGHT: if (ex < el) ex++; break;
    case K_HOME: ex = 0; break;
    case K_END: ex = el; break;
    case K_UP: geschichte(1); break;
    case K_DOWN: geschichte(0); break;
    case K_PGUP: zur += rows > 1 ? rows - 1 : 1; neu(); return;
    case K_PGDN: zur = zur > rows - 1 ? zur - (rows > 1 ? rows - 1 : 1) : 0; neu(); return;
    case K_ESC: el = ex = eo = 0; hz = 0; break;
    default: return;
    }
    eingabe_neu();
}
static void zeichen(char c)
{
    WORD i;
    if (el >= sizeof eb - 1) return;
    for (i = el; i > ex; i--) eb[i] = eb[i - 1];
    eb[ex++] = c; el++; eingabe_neu();
}

static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    if (!fe) fe = w;
    switch (msg) {
    case M_PAINT: malen(); return 0;
    case M_SIZE: if (zh) groesse(); return 0;
    case M_SETFOCUS: case M_KILLFOCUS: eingabe_neu(); return 0;
    case M_KEY: taste(p1); return 0;
    case M_CHAR: if ((BYTE)p1 >= ' ' && !(HIWORD(p2) & (KS_CTRL | KS_ALT))) zeichen((char)p1); return 0;
    case M_BUTTONDOWN: SetFocus(w); return 0;
    case M_SCROLL:
        if (HIWORD(p2) == SB_VERT) {
            WORD v = vsicht(), m = v > rows ? v - rows : 0, pos = m - zur;
            switch (p1) {
            case SB_LINEUP: if (pos) pos--; break;
            case SB_LINEDOWN: pos++; break;
            case SB_PAGEUP: pos = pos > rows ? pos - rows : 0; break;
            case SB_PAGEDOWN: pos += rows; break;
            case SB_THUMB: pos = LOWORD(p2); break;
            case SB_TOP: pos = 0; break;
            case SB_BOTTOM: pos = m; break;
            }
            if (pos > m) pos = m;
            zur = m - pos; neu();
        }
        return 0;
    case M_COMMAND:
        if (p1 == CM_COMMAND) { char cs[80]; if (comspec(cs)) dos(cs, ""); else { sagz(38); neu(); } }
        else if (p1 == CM_LEER) { vn = zur = 0; neu(); }
        else if (p1 == CM_ENDE) PostMessage(w, M_CLOSE, 0, 0);
        return 0;
    case M_TIMER:                                 /* Testskript: naechste Zeile */
        {   char c; WORD n = 0;
            while (FileRead(sk, &c, 1) == 1 && c != '\n') if (c != '\r' && n < sizeof eb - 1) eb[n++] = c;
            if (!n) { TimerKill(w, 1); FileClose(sk); sk = -1; PostMessage(w, M_CLOSE, 0, 0); return 0; }
            el = n; ausfuehren();
        }
        return 0;
    case M_CLOSE: if (test) DbgPrint("RDSHELL: Ende"); DestroyWindow(w); return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H;
    (void)show; hi = inst;
    if (cmd && cmd[0] == 'T') test = 1;
    if (!(hm = MemAlloc((DWORD)NV * VB, MEM_MOVABLE)) || !(vl = (char FAR *)MemLock(hm))) return 1;
    KernInfo(&si); scpy(ini, si.home, sizeof ini); anh(ini, "RD2.INI");
    if (DirGet(0, cwd)) scpy(cwd, si.home, sizeof cwd);
    wc.name = "RdShell"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); zh = GetCaps(d, CAPS_TEXTH); cw = TextWidth(d, "M", 1); ReleaseDC(0, d);
    fe = CreateWindow("RdShell", StrGet(inst, 1), WS_NORMAL | WS_VISIBLE | WS_VSCROLL, (short)(W / 16), (short)(H / 10), (short)(W - W / 8), (short)(H - H / 10 - H / 12), 0, 1, inst);
    if (!fe) { MemUnlock(hm); MemFree(hm); return 1; }
    mn = MenuLoad(inst, 1); MenuSet(fe, mn);
    groesse(); SetFocus(fe);
    sagz(2); sag(""); neu();
    if (test) DbgPrint("RDSHELL: bereit");
    if (test && cmd[1] == ' ' && (sk = FileOpen(cmd + 2, FO_READ)) >= 0) TimerSet(fe, 1, 1000);
    while (GetMessage(&m)) DispatchMessage(&m);
    MenuFree(mn); MemUnlock(hm); MemFree(hm);
    return 0;
}
