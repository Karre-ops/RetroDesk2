/* DPRUEF.EXE - Datentraeger pruefen (wie CHKDSK) fuer FAT12/FAT16 (Disketten, Festplatten-Partitionen).
 * Liest ueber INT 25h (RDDISK.MOD, grosse Partitionen mit Paket), prueft: FAT-Kopien gleich, alle Ordner und
 * Dateiketten (ungueltige Cluster, Querverweise, Schleifen, Groesse passt nicht zur Kette), verlorene Cluster.
 * Reparieren (INT 26h): verlorene Cluster freigeben, FAT 1 auf die anderen Kopien schreiben. Querverweise und
 * Groessenfehler werden nur gemeldet. Befehlszeile: "X:" Laufwerk, "T X:" Testmodus (pruefen, reparieren, pruefen). */
#include <dos.h>
#include "RD2.H"
#include "../../src/disk/RDDISK.H"

#define ID_LISTE 40
#define ID_BALKEN 41
#define ID_TEXT 42
#define ID_REP  21
#define NC 6                                      /* FAT-Sektor-Zwischenspeicher */
static HINST hi; static RDD_INT_F dInt; static BYTE lw, test, f16, gross, schmutz[NC], tiefe;
static WORD bps, spc, res, nfat, rootn, spf, ncl, nfund, fatgleich, quer, kette, groesse, ungueltig;
static DWORD tot, data0, root0, verloren; static HMEM hmk, hsek; static BYTE FAR *mk, FAR *sek; static DWORD cs[NC]; static WORD clru;
static HWND dlg; static char z[80];

static WORD spt, kpf;                             /* Disketten: Geometrie aus dem Bootsektor */
static WORD abs(BYTE wr, DWORD s, WORD n, void FAR *p)    /* 0 = ok; Disketten ueber INT 13h, sonst INT 25h/26h */
{
    union REGS r; struct SREGS sr; struct { DWORD s; WORD n; void FAR *p; } pk; WORD v, c, h, t;
    if (lw < 2) {
        if (!spt) { c = 0; h = 0; t = 1; } else { t = (WORD)(s % spt + 1); h = (WORD)(s / spt % kpf); c = (WORD)(s / spt / kpf); }
        for (v = 0; v < 3; v++) {
            r.x.ax = (WORD)((wr ? 0x0300 : 0x0200) | 1); r.x.cx = (WORD)((c << 8) | ((c >> 2) & 0xC0) | t); r.x.dx = (WORD)((h << 8) | lw);
            r.x.bx = FP_OFF(p); sr.es = FP_SEG(p); sr.ds = 0; dInt(0x13, &r, &r, &sr);
            if (!r.x.cflag) return 0;
            r.x.ax = 0; r.x.dx = lw; dInt(0x13, &r, &r, &sr);
        }
        return (WORD)(r.x.ax >> 8 | 0x100);
    }
    r.h.ah = 0; r.h.al = lw; sr.ds = FP_SEG(p); r.x.bx = FP_OFF(p);
    if (gross) { pk.s = s; pk.n = n; pk.p = p; r.x.cx = 0xFFFF; sr.ds = FP_SEG((void FAR *)&pk); r.x.bx = FP_OFF((void FAR *)&pk); }
    else { r.x.cx = n; r.x.dx = (WORD)s; }
    sr.es = 0; dInt(wr ? 0x26 : 0x25, &r, &r, &sr);
    return r.x.cflag ? (r.x.ax ? r.x.ax : 0xFF) : 0;
}
static BYTE FAR *fsek(DWORD s)                    /* FAT-Sektor aus dem Zwischenspeicher */
{
    WORD i;
    for (i = 0; i < NC; i++) if (cs[i] == s) return sek + i * 512;
    i = clru; clru = (WORD)((clru + 1) % NC);
    if (schmutz[i]) { WORD k; for (k = 0; k < nfat; k++) abs(1, cs[i] + (DWORD)k * spf, 1, sek + i * 512); schmutz[i] = 0; }
    cs[i] = s; if (abs(0, s, 1, sek + i * 512)) { cs[i] = 0xFFFFFFFFL; return 0; }
    return sek + i * 512;
}
static void schreibe_alle(void) { WORD i, k; for (i = 0; i < NC; i++) if (schmutz[i]) { for (k = 0; k < nfat; k++) abs(1, cs[i] + (DWORD)k * spf, 1, sek + i * 512); schmutz[i] = 0; } }
static BYTE FAR *fpos(WORD c, WORD *o, DWORD *s)  /* Byte-Lage des Eintrags c */
{
    DWORD b = f16 ? (DWORD)c * 2 : (DWORD)c * 3 / 2;
    *s = res + b / 512; *o = (WORD)(b % 512); return fsek(*s);
}
static BYTE byte(DWORD s, WORD o) { BYTE FAR *p = fsek(s + o / 512); return p ? p[o % 512] : 0; }
static WORD fat(WORD c)
{
    WORD o, v; DWORD s; BYTE FAR *p = fpos(c, &o, &s);
    if (!p) return 0xFFFF;
    if (f16) return (WORD)(p[o] | (byte(s, o + 1) << 8));
    v = (WORD)(p[o] | (byte(s, o + 1) << 8));
    return (WORD)(c & 1 ? v >> 4 : v & 0xFFF);
}
static void setfat(WORD c, WORD v)
{
    WORD o, i; DWORD s; BYTE FAR *p = fpos(c, &o, &s), FAR *q;
    if (!p) return;
    for (i = 0; i < NC; i++) if (cs[i] == s) schmutz[i] = 1;
    if (f16) { p[o] = (BYTE)v; if (o == 511) { q = fsek(s + 1); if (q) { q[0] = (BYTE)(v >> 8); for (i = 0; i < NC; i++) if (cs[i] == s + 1) schmutz[i] = 1; } } else p[o + 1] = (BYTE)(v >> 8); return; }
    if (c & 1) v = (WORD)(v << 4 | (p[o] & 0x0F)); else v |= (WORD)((o == 511 ? (q = fsek(s + 1)) ? q[0] : 0 : p[o + 1]) & 0xF0) << 8;
    if (!(p = fsek(s))) return;
    p[o] = (BYTE)v; if (o != 511) p[o + 1] = (BYTE)(v >> 8);
    for (i = 0; i < NC; i++) if (cs[i] == s) schmutz[i] = 1;   /* nach dem Nachladen neu: fsek(s + 1) kann s verdraengt haben */
    if (o == 511 && (q = fsek(s + 1)) != 0) { q[0] = (BYTE)(v >> 8); for (i = 0; i < NC; i++) if (cs[i] == s + 1) schmutz[i] = 1; }
}
static BYTE ende(WORD v) { return (BYTE)(f16 ? v >= 0xFFF8 : v >= 0xFF8); }
static BYTE schlecht(WORD v) { return (BYTE)(f16 ? v == 0xFFF7 : v == 0xFF7); }
static void melde(WORD t, LPCSTR a)
{
    char *p = z; LPCSTR s;
    for (s = StrGet(hi, t); *s && p < z + 40; ) *p++ = *s++;
    if (a) for (s = a; *s && p < z + 78; ) *p++ = *s++;
    *p = 0; SendMessage(DlgItem(dlg, ID_LISTE), L_ADD, 0, (DWORD)(LPCSTR)z); nfund++;
    DbgPrint("DPRUEF:"); DbgPrint(z);
}
static WORD folge(WORD c, LPCSTR wo)              /* Kette ab c markieren, liefert Zahl der Cluster */
{
    WORD n = 0, v;
    while (c >= 2 && !ende(c)) {
        if (c >= ncl + 2) { melde(4, wo); ungueltig++; break; }
        if (mk[c >> 3] & (1 << (c & 7))) { melde(5, wo); quer++; break; }
        mk[c >> 3] |= (BYTE)(1 << (c & 7)); n++;
        if ((v = fat(c)) == 0 || schlecht(v)) { melde(6, wo); kette++; break; }
        c = v;
    }
    return n;
}
static void ordner(WORD cl, char *pfad)           /* cl = 0: Hauptverzeichnis */
{
    static BYTE b[512]; WORD i, j, n, l; DWORD s, s0; WORD c = cl, k = 0; char *e;   /* static: Stapel klein, nach Unterordner neu lesen */
    if (++tiefe > 16) { tiefe--; return; }
    l = (WORD)(e = pfad, 0); while (*e) e++, l++;
    for (;;) {
        if (!cl) { if (k >= (WORD)(rootn * 32 / 512)) break; s0 = root0 + k; n = 1; }
        else { if (c < 2 || ende(c) || c >= ncl + 2) break; s0 = data0 + (DWORD)(c - 2) * spc; n = spc; }
        for (j = 0; j < n; j++) {
            s = s0 + j; if (abs(0, s, 1, (void FAR *)b)) { melde(7, pfad); tiefe--; return; }
            for (i = 0; i < 512; i += 32) {
                BYTE FAR *d = b + i; WORD st = *(WORD *)(b + i + 26); DWORD gr = *(DWORD *)(b + i + 28); WORD m, q;
                if (!d[0]) { tiefe--; return; }
                if (d[0] == 0xE5 || d[0] == '.' || (d[11] & 0x08) || d[11] == 0x0F) continue;
                e = pfad + l; for (m = 0; m < 8 && d[m] != ' '; m++) *e++ = d[m];
                if (d[8] != ' ') { *e++ = '.'; for (m = 8; m < 11 && d[m] != ' '; m++) *e++ = d[m]; }
                *e = 0;
                q = folge(st, pfad);
                if (d[11] & 0x10) { if (e < pfad + 60) { *e++ = '\\'; *e = 0; ordner(st, pfad); abs(0, s, 1, (void FAR *)b); } }
                else if (q != (WORD)((gr + (DWORD)spc * 512 - 1) / ((DWORD)spc * 512))) { melde(8, pfad); groesse++; }
                pfad[l] = 0;
            }
        }
        if (!cl) k++; else { c = fat(c); if (c == 0xFFFF) break; }
    }
    tiefe--;
}
static BYTE pruefen(void)                         /* 0 = Lesefehler/unbekannt */
{
    BYTE b[512]; WORD i, k; char pf[80]; DWORD n;
    SendMessage(DlgItem(dlg, ID_LISTE), L_CLEAR, 0, 0); nfund = quer = kette = groesse = ungueltig = 0; verloren = 0; fatgleich = 1;
    for (i = 0; i < NC; i++) { cs[i] = 0xFFFFFFFFL; schmutz[i] = 0; }
    gross = 0; spt = 0; if (abs(0, 0, 1, (void FAR *)b)) { gross = 1; if (abs(0, 0, 1, (void FAR *)b)) { melde(9, 0); return 0; } }
    spt = *(WORD *)(b + 24); kpf = *(WORD *)(b + 26); if (lw < 2 && (!spt || spt > 63 || !kpf || kpf > 2)) { melde(9, 0); return 0; }
    bps = *(WORD *)(b + 11); spc = b[13]; res = *(WORD *)(b + 14); nfat = b[16]; rootn = *(WORD *)(b + 17);
    tot = *(WORD *)(b + 19); if (!tot) tot = *(DWORD *)(b + 32); spf = *(WORD *)(b + 22);
    if (bps != 512 || !spc || !nfat || !spf || !tot) { melde(9, 0); return 0; }
    root0 = res + (DWORD)nfat * spf; data0 = root0 + (DWORD)rootn * 32 / 512;
    n = (tot - data0) / spc; if (n > 65524L) { melde(9, 0); return 0; }
    ncl = (WORD)n; f16 = (BYTE)(ncl >= 4085);
    if (hmk) MemFree(hmk); if (!(hmk = MemAlloc(ncl / 8 + 2, MEM_MOVABLE)) || !(mk = (BYTE FAR *)MemLock(hmk))) { melde(10, 0); return 0; }
    for (i = 0; i < ncl / 8 + 2; i++) mk[i] = 0;
    DbgHex("DPRUEF: Cluster", ncl);
    CursorBusy(TRUE); DlgSetText(dlg, ID_TEXT, StrGet(hi, 11));
    for (k = 1; k < nfat && fatgleich; k++) for (i = 0; i < spf && fatgleich; i++) {    /* FAT-Kopien vergleichen */
        BYTE FAR *p = fsek(res + i); WORD m;
        if (!p || abs(0, res + (DWORD)k * spf + i, 1, (void FAR *)b)) break;
        for (m = 0; m < 512; m++) if (p[m] != b[m]) { fatgleich = 0; break; }
    }
    if (!fatgleich) melde(3, 0);
    SendMessage(DlgItem(dlg, ID_BALKEN), C_SETPOS, 0, 20);
    pf[0] = (char)('A' + lw); pf[1] = ':'; pf[2] = '\\'; pf[3] = 0; tiefe = 0; ordner(0, pf);
    SendMessage(DlgItem(dlg, ID_BALKEN), C_SETPOS, 0, 70);
    for (i = 2; i < ncl + 2; i++) { WORD v = fat(i); if (v && v != 0xFFFF && !schlecht(v) && !(mk[i >> 3] & (1 << (i & 7)))) verloren++; }
    if (verloren) { char t[12], *p = t; DWORD v = verloren; char r[11]; short m = 0; do r[m++] = (char)('0' + v % 10); while (v /= 10); while (m) *p++ = r[--m]; *p = 0; melde(12, t); }
    DbgHex("DPRUEF: verloren", verloren);
    SendMessage(DlgItem(dlg, ID_BALKEN), C_SETPOS, 0, 100); CursorBusy(FALSE);
    DlgSetText(dlg, ID_TEXT, StrGet(hi, nfund ? 13 : 14));
    DbgHex("DPRUEF: Funde", nfund);
    return 1;
}
static void reparieren(void)
{
    WORD i, k; BYTE b[512];
    if (!test && MsgBox(dlg, StrGet(hi, 15), StrGet(hi, 1), MB_YESNO | MB_WARN | MB_DEFNO) != ID_YES) return;
    CursorBusy(TRUE);
    if (verloren) for (i = 2; i < ncl + 2; i++) { WORD v = fat(i); if (v && v != 0xFFFF && !schlecht(v) && !(mk[i >> 3] & (1 << (i & 7)))) setfat(i, 0); }
    schreibe_alle();
    if (!fatgleich || verloren) for (i = 0; i < spf; i++) {   /* FAT 1 auf alle Kopien */
        if (abs(0, res + i, 1, (void FAR *)b)) break;
        for (k = 1; k < nfat; k++) abs(1, res + (DWORD)k * spf + i, 1, (void FAR *)b);
    }
    CursorBusy(FALSE); DbgPrint("DPRUEF: repariert");
    pruefen();
}
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    (void)p2; dlg = d;
    if (msg == M_INITDIALOG) {
        z[0] = (char)('A' + lw); z[1] = ':'; z[2] = 0; DlgSetText(d, ID_TEXT, z);
        if (test) PostMessage(d, M_COMMAND, ID_OK, 0);
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        pruefen();
        if (test) { reparieren(); DbgPrint("DPRUEF: Test fertig"); DialogEnd(d, ID_CANCEL); }
        return TRUE;
    }
    if (msg == M_COMMAND && p1 == ID_REP) { if (nfund) reparieren(); return TRUE; }
    return FALSE;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HMOD hm;
    (void)show; hi = inst; lw = 2;
    while (cmd && *cmd == ' ') cmd++;
    if (cmd && *cmd == 'T') { test = 1; cmd++; while (*cmd == ' ') cmd++; }
    if (cmd && ((*cmd | 0x20) >= 'a' && (*cmd | 0x20) <= 'z')) lw = (BYTE)((*cmd | 0x20) - 'a');
    if (!(hm = ModLoad("RDDISK.MOD"))) { MsgBox(0, StrGet(inst, 2), StrGet(inst, 1), MB_OK | MB_ERROR); return 1; }
    dInt = (RDD_INT_F)ModProc(hm, RDD_INT);
    if (!(hsek = MemAlloc(NC * 512, MEM_MOVABLE)) || !(sek = (BYTE FAR *)MemLock(hsek))) return 1;
    DialogRun(inst, 10, 0, dproc, 0);
    if (hmk) MemFree(hmk); MemUnlock(hsek); MemFree(hsek); ModFree(hm);
    return 0;
}
