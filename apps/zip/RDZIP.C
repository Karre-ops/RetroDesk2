/* RDZIP.C - ZIP-Archive ansehen, entpacken, pruefen und packen (PKZIP 2.x: gespeichert und Deflate).
 * Befehlszeile: [T] [/X | /A | /P] [Archiv] [Ziel bzw. Quelle]
 *   Archiv        oeffnen (Liste)       /X Archiv Ziel\   alles entpacken, Ende
 *   /A Archiv Q   Datei oder Ordner Q dazupacken, Ende   /P Archiv   alles pruefen (CRC), Ende
 * "T": Meldungen fuer test/rdzip.sh. Lange Namen im Archiv werden beim Entpacken zu 8.3.
 * Speicher: Liste 22 KB, Entpacken 36 KB, Packen 68 KB, nur waehrend des Vorgangs. */
#include "RD2.H"
#include "ZK.H"

#define MAXE 256
#define ID_LISTE 10
#define ID_STAT  11
#define CM_NEU     101
#define CM_OEFFNEN 102
#define CM_ENDE    109
#define CM_ALLE    111
#define CM_AUSWAHL 112
#define CM_DATEI   113
#define CM_ORDNER  114
#define CM_PRUEFEN 115
#define CM_MARK    116

typedef struct { char n[64]; BYTE meth, mk; WORD flags, zeit, datum; DWORD crc, cs, us, off; } ZE;
static HINST hi; static HWND fe, li, sl; static HMENU mn; static BYTE test, busy, offen, fragen = 1;
static char arch[80], ziel[80], tx[120];
static HMEM hze; static ZE FAR *ze; static WORD nz; static DWORD cdoff;
static HMEM hio; static BYTE FAR *io;              /* 4 KB Puffer */

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static char gr(char c) { return (char)(c >= 'a' && c <= 'z' ? c - 32 : c); }
static int gleich(LPCSTR a, LPCSTR b) { while (*a && gr(*a) == gr(*b)) { a++; b++; } return !*a && !*b; }
static void zahl(char *d, DWORD v) { char t[12]; WORD i = 0; do t[i++] = (char)('0' + v % 10); while ((v /= 10) && i < 11); while (i) *d++ = t[--i]; *d = 0; }
static void zcat(char *d, DWORD v, WORD n) { char z[12]; zahl(z, v); scat(d, z, n); }
static void pr(LPCSTR a, LPCSTR b) { if (test) { char t[120]; scpy(t, "RDZIP: ", sizeof t); scat(t, a, sizeof t); if (b) scat(t, b, sizeof t); DbgPrint(t); } }
static WORD rw(const BYTE FAR *p) { return (WORD)(p[0] | (p[1] << 8)); }
static DWORD rl(const BYTE FAR *p) { return rw(p) | ((DWORD)rw(p + 2) << 16); }
static void ww(BYTE FAR *p, WORD v) { p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); }
static void wl(BYTE FAR *p, DWORD v) { ww(p, (WORD)v); ww(p + 2, (WORD)(v >> 16)); }
static void status(LPCSTR s) { SetWindowText(sl, s); UpdateWindow(sl); }
static void meldung(WORD id, LPCSTR a)
{
    scpy(tx, StrGet(hi, id), sizeof tx); if (a) scat(tx, a, sizeof tx);
    pr("Meldung: ", tx);
    if (!test) MsgBox(fe, tx, StrGet(hi, 1), MB_OK | MB_ERROR);
}
static void pump(void)                            /* waehrend der Arbeit: Fenster malen, Esc bricht ab */
{
    MSG m;
    while (PeekMessage(&m, 1)) {
        if (m.msg == M_KEY && m.p1 == K_ESC) zk_abbruch = 1;
        else if (m.msg == M_QUIT) { zk_abbruch = 1; PostQuit(0); return; }
        else DispatchMessage(&m);
    }
}
static void titel(void)
{
    scpy(tx, StrGet(hi, 1), sizeof tx);
    if (offen) { scat(tx, " - ", sizeof tx); scat(tx, arch, sizeof tx); }
    SetWindowText(fe, tx);
}
static void summe(void)                           /* Statuszeile: n Dateien, Bytes, gepackt */
{
    DWORD u = 0, c = 0; WORD i;
    for (i = 0; i < nz; i++) { u += ze[i].us; c += ze[i].cs; }
    tx[0] = 0; zcat(tx, nz, sizeof tx); scat(tx, StrGet(hi, 10), sizeof tx); zcat(tx, u, sizeof tx);
    scat(tx, StrGet(hi, 11), sizeof tx); zcat(tx, c, sizeof tx); scat(tx, StrGet(hi, 12), sizeof tx);
    status(offen ? tx : "");
}
static void zeile(char *t, WORD i)                /* hoechstens 32 Zeichen: [*]Name  Groesse  Quote */
{
    WORD k = 0, j, l; char z[12]; LPCSTR n = ze[i].n;
    if (ze[i].mk) t[k++] = '\xFE';
    for (j = 0; n[j] && k < 18; j++) t[k++] = n[j];
    if (n[j]) t[k - 1] = '~';
    zahl(z, ze[i].us); l = slen(z); while (k < 27 - l) t[k++] = ' '; for (j = 0; j < l; j++) t[k++] = z[j];
    t[k++] = ' ';
    { DWORD q = ze[i].us ? (ze[i].us > 0x00FFFFFFUL ? ze[i].cs / (ze[i].us / 100) : ze[i].cs * 100 / ze[i].us) : 100; if (q > 999) q = 999; zahl(z, q); }
    l = slen(z); while (l < 3) { t[k++] = ' '; l++; } for (j = 0; z[j]; j++) t[k++] = z[j];
    t[k++] = '%'; t[k] = 0;
}
static void anzeigen(WORD sel)
{
    char t[34]; WORD i;
    SendMessage(li, L_CLEAR, 0, 0);
    for (i = 0; i < nz; i++) { zeile(t, i); SendMessage(li, L_ADD, 0, (DWORD)(LPCSTR)t); }
    if (nz) SendMessage(li, L_SETSEL, sel < nz ? sel : nz - 1, 0);
    summe(); titel();
}
static BYTE speicher(void)
{
    if (!hze && (!(hze = MemAlloc((DWORD)MAXE * sizeof(ZE), MEM_MOVABLE)) || !(hio = MemAlloc(4096, MEM_MOVABLE)))) return 0;
    ze = (ZE FAR *)MemLock(hze); io = (BYTE FAR *)MemLock(hio);
    return ze && io;
}
/* ---------------- Archiv lesen ---------------- */
static WORD laden(void)                           /* 0 = ok, sonst Text-Nr. */
{
    short f; long gr_, p; WORD n, i, k, j, ne; DWORD cdgr;
    nz = 0; cdoff = 0; offen = 0;
    if ((f = FileOpen(arch, FO_READ)) < 0) return 20;
    gr_ = FileSeek(f, 0, FS_END);
    p = gr_ > 4096 ? gr_ - 4096 : 0;
    FileSeek(f, p, FS_SET); n = FileRead(f, io, 4096); if (n == 0xFFFF) n = 0;
    for (k = n >= 22 ? n - 22 + 1 : 0; k-- > 0; ) if (io[k] == 'P' && io[k + 1] == 'K' && io[k + 2] == 5 && io[k + 3] == 6) break;
    if (!gr_) { FileClose(f); offen = 1; return 0; }   /* leere Datei: neues Archiv */
    if (n < 22 || k == 0xFFFF) { FileClose(f); return 21; }
    ne = rw(io + k + 10); cdgr = rl(io + k + 12); cdoff = rl(io + k + 16);
    if (cdoff + cdgr > (DWORD)gr_) { FileClose(f); return 21; }
    FileSeek(f, (long)cdoff, FS_SET);
    {   WORD bn = 0, bp = 0;                      /* gepuffert lesen */
        for (i = 0; i < ne; i++) {
            WORD ln, lx, lk;
            if (bn - bp < 46 + 255) {             /* Rest nach vorn, nachladen */
                for (j = 0; bp + j < bn; j++) io[j] = io[bp + j];
                bn = (WORD)(bn - bp); bp = 0;
                k = FileRead(f, io + bn, (WORD)(4096 - bn)); if (k != 0xFFFF) bn += k;
            }
            if (bn - bp < 46 || rl(io + bp) != 0x02014B50UL) { FileClose(f); return 21; }
            if (nz >= MAXE) { FileClose(f); meldung(22, 0); break; }
            {   const BYTE FAR *h = io + bp; ZE FAR *e = &ze[nz];
                e->flags = rw(h + 8); e->meth = (BYTE)rw(h + 10); e->zeit = rw(h + 12); e->datum = rw(h + 14);
                e->crc = rl(h + 16); e->cs = rl(h + 20); e->us = rl(h + 24); ln = rw(h + 28); lx = rw(h + 30); lk = rw(h + 32);
                e->off = rl(h + 42); e->mk = 0;
                for (j = 0; j < ln && j < 63 && bp + 46 + j < bn; j++) e->n[j] = (char)h[46 + j];
                e->n[j] = 0;
            }
            bp += 46 + ln;
            if (bp > bn) { FileClose(f); return 21; }
            {   DWORD skip = (DWORD)lx + lk;     /* Zusatz und Kommentar ueberspringen */
                if (skip <= (DWORD)(bn - bp)) bp += (WORD)skip;
                else { skip -= bn - bp; bn = bp = 0; FileSeek(f, (long)skip, FS_CUR); }
            }
            nz++;
        }
    }
    FileClose(f);
    offen = 1;
    return 0;
}
/* ---------------- Entpacken ---------------- */
static short qf, zf; static DWORD rest, crc, anz, gesamtcs, fertigcs; static WORD zaehl;
static WORD lies(void *u, BYTE FAR *b, WORD n)
{
    WORD r;
    (void)u;
    if ((DWORD)n > rest) n = (WORD)rest;
    if (!n) return 0;
    r = FileRead(qf, b, n); if (r == 0xFFFF) r = 0;
    rest -= r; fertigcs += r;
    if (!(++zaehl & 3)) {
        tx[0] = 0; scat(tx, StrGet(hi, 30), sizeof tx);
        zcat(tx, gesamtcs ? (fertigcs > 0x00FFFFFFUL ? fertigcs / (gesamtcs / 100 + 1) : fertigcs * 100 / gesamtcs) : 0, sizeof tx); scat(tx, "%", sizeof tx);
        status(tx);
    }
    pump();
    return r;
}
static WORD schreib(void *u, const BYTE FAR *b, WORD n)
{
    (void)u;
    crc = CrcRechne(crc, b, n); anz += n;
    if (zf >= 0 && FileWrite(zf, b, n) != n) return 0;
    return n;
}
static void dosname(char *d, LPCSTR s, WORD sl_)  /* ein Namensteil -> 8.3 (gross, erlaubte Zeichen) */
{
    WORD i = 0, k = 0, p = 0xFFFF, j;
    for (j = 0; j < sl_; j++) if (s[j] == '.') p = j;
    while (k < sl_ && s[k] == '.') k++;
    for (j = k; j < (p == 0xFFFF || p < k ? sl_ : p) && i < 8; j++) {
        char c = gr(s[j]);
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '@' || c == '~' || (BYTE)c >= 128)) c = '_';
        d[i++] = c;
    }
    if (!i) d[i++] = '_';
    if (p != 0xFFFF && p >= k) {
        WORD e = 0; d[i++] = '.';
        for (j = p + 1; j < sl_ && e < 3; j++, e++) { char c = gr(s[j]); if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) c = '_'; d[i++] = c; }
        if (!e) i--;
    }
    d[i] = 0;
}
static BYTE zielpfad(char *p, WORD n, LPCSTR zn)  /* Ordner anlegen; 0 = Ordner-Eintrag (nichts zu schreiben) */
{
    WORD a = 0, b; char t[13];
    scpy(p, ziel, n);
    for (;;) {
        for (b = a; zn[b] && zn[b] != '/' && zn[b] != '\\'; b++) ;
        if (b > a && !(b - a == 1 && zn[a] == '.') && !(b - a == 2 && zn[a] == '.' && zn[a + 1] == '.')) {
            dosname(t, zn + a, (WORD)(b - a)); scat(p, t, n);
            if (zn[b]) { DirMake(p); scat(p, "\\", n); }
        }
        if (!zn[b]) return (BYTE)(b > 0 && zn[b - 1] != '/' && zn[b - 1] != '\\');
        a = (WORD)(b + 1);
        if (!zn[a]) return 0;
    }
}
static HMEM hfe, hein;
static WORD eine(WORD i, BYTE nurpruefen)          /* 0 = ok, 1 = Fehler (gemeldet), 2 = Abbruch */
{
    char p[80]; ZE FAR *e = &ze[i]; WORD r = 0, lh; FINDDATA fd;
    char zn[64];
    scpy(zn, e->n, sizeof zn);
    if (e->flags & 1) { meldung(31, zn); return 1; }
    if (e->meth != 0 && e->meth != 8) { meldung(32, zn); return 1; }
    zf = -1;
    if (!nurpruefen) {
        if (!zielpfad(p, sizeof p, zn)) return 0;
        if (!test && FileFindFirst(p, FA_RDONLY | FA_HIDDEN | FA_SYSTEM | FA_ARCH, &fd) == 0) {
            short a;
            scpy(tx, p, sizeof tx); scat(tx, StrGet(hi, 33), sizeof tx);
            a = MsgBox(fe, tx, StrGet(hi, 1), MB_YESNOCANCEL | MB_ASK);
            if (a == ID_NO) return 0;
            if (a != ID_YES) return 2;
        }
        if ((zf = FileCreate(p, 0)) < 0) { meldung(34, p); return 1; }
    }
    FileSeek(qf, (long)e->off, FS_SET);
    if (FileRead(qf, io, 30) != 30 || rl(io) != 0x04034B50UL) { if (zf >= 0) { FileClose(zf); FileDelete(p); } meldung(21, 0); return 1; }
    lh = rw(io + 26) + rw(io + 28);
    FileSeek(qf, (long)(e->off + 30 + lh), FS_SET);
    rest = e->cs; crc = 0xFFFFFFFFUL; anz = 0; zk_abbruch = 0;
    scpy(tx, StrGet(hi, nurpruefen ? 35 : 30), sizeof tx); scat(tx, zn, sizeof tx); status(tx);
    if (e->meth == 0) {
        WORD n;
        while ((n = lies(0, io, 4096)) != 0) if (schreib(0, io, n) != n) { r = ZE_SCHREIB; break; }
        if (zk_abbruch) r = ZE_ABBRUCH;
    } else {
        BYTE FAR *fw = (BYTE FAR *)MemLock(hfe), FAR *eb = (BYTE FAR *)MemLock(hein);
        r = fw && eb ? Inflate(lies, schreib, 0, fw, eb, 2048) : ZE_DATEN;
        MemUnlock(hfe); MemUnlock(hein);
    }
    if (zf >= 0) FileClose(zf);
    crc ^= 0xFFFFFFFFUL;
    if (!r && (crc != e->crc || anz != e->us)) r = ZE_DATEN;
    if (r) {
        if (zf >= 0) FileDelete(p);
        if (r == ZE_ABBRUCH) return 2;
        meldung(r == ZE_SCHREIB ? 36 : 37, zn);
        return 1;
    }
    pr(nurpruefen ? "ok " : "entpackt ", nurpruefen ? zn : p);
    return 0;
}
static void entpacken(BYTE auswahl, BYTE nurpruefen)   /* auswahl: markierte, sonst die gewaehlte; 0 = alle */
{
    WORD i, sel = (WORD)SendMessage(li, L_GETSEL, 0, 0), n = 0, fehl = 0, r, mk = 0;
    if (!offen || !nz) return;
    for (i = 0; i < nz; i++) if (ze[i].mk) mk++;
    if (!nurpruefen && !test && fragen) {
        if (!InputBox(fe, StrGet(hi, 38), StrGet(hi, 1), ziel, sizeof ziel - 1)) return;
    }
    if (!nurpruefen && ziel[0] && ziel[slen(ziel) - 1] != '\\' && ziel[slen(ziel) - 1] != ':') scat(ziel, "\\", sizeof ziel);
    if (!nurpruefen && ziel[0] && ziel[slen(ziel) - 1] == '\\' && slen(ziel) > 3) { ziel[slen(ziel) - 1] = 0; DirMake(ziel); scat(ziel, "\\", sizeof ziel); }
    if ((qf = FileOpen(arch, FO_READ)) < 0) { meldung(20, arch); return; }
    if (!(hfe = MemAlloc(32768UL, MEM_MOVABLE)) || !(hein = MemAlloc(2048, MEM_MOVABLE))) { if (hfe) MemFree(hfe); hfe = 0; FileClose(qf); meldung(39, 0); return; }
    busy = 1; CursorBusy(1);
    gesamtcs = 0; fertigcs = 0;
    for (i = 0; i < nz; i++) if (!auswahl || (mk ? ze[i].mk : i == sel)) gesamtcs += ze[i].cs;
    for (i = 0; i < nz; i++) {
        if (auswahl && !(mk ? ze[i].mk : i == sel)) continue;
        r = eine(i, nurpruefen);
        if (r == 2) { meldung(40, 0); break; }
        if (r) fehl++; else n++;
    }
    CursorBusy(0); busy = 0;
    MemFree(hfe); MemFree(hein); hfe = hein = 0; FileClose(qf);
    tx[0] = 0; zcat(tx, n, sizeof tx); scat(tx, StrGet(hi, nurpruefen ? 41 : 42), sizeof tx);
    if (fehl) { scat(tx, ", ", sizeof tx); zcat(tx, fehl, sizeof tx); scat(tx, StrGet(hi, 43), sizeof tx); }
    status(tx); pr(tx, 0);
    if (!test && !fehl) MsgBox(fe, tx, StrGet(hi, 1), MB_OK | MB_INFO);
}
/* ---------------- Packen ---------------- */
static short af; static DWORD apos; static BYTE gross;
static HMEM hb[4];
static WORD qlies(void *u, BYTE FAR *b, WORD n)
{
    WORD r;
    (void)u;
    r = FileRead(qf, b, n); if (r == 0xFFFF) r = 0;
    crc = CrcRechne(crc, b, r); rest += r;
    if (!(++zaehl & 3)) pump();
    return r;
}
static WORD aschreib(void *u, const BYTE FAR *b, WORD n)
{
    (void)u;
    anz += n;
    if (rest > 1024 && anz > rest + 1024) { gross = 1; return 0; }   /* waechst: aufhoeren, wird gespeichert */
    return FileWrite(af, b, n) == n ? n : 0;
}
static BYTE cd_schreiben(void)                    /* Inhaltsverzeichnis + Ende ab apos, Rest abschneiden */
{
    WORD i, l; DWORD cg = 0;
    FileSeek(af, (long)apos, FS_SET);
    for (i = 0; i < nz; i++) {
        ZE FAR *e = &ze[i]; WORD k;
        l = slen(e->n);
        wl(io, 0x02014B50UL); ww(io + 4, 20); ww(io + 6, e->meth ? 20 : 10); ww(io + 8, e->flags); ww(io + 10, e->meth);
        ww(io + 12, e->zeit); ww(io + 14, e->datum); wl(io + 16, e->crc); wl(io + 20, e->cs); wl(io + 24, e->us);
        ww(io + 28, l); ww(io + 30, 0); ww(io + 32, 0); ww(io + 34, 0); ww(io + 36, 0); wl(io + 38, 0x20); wl(io + 42, e->off);
        for (k = 0; k < l; k++) io[46 + k] = (BYTE)e->n[k];
        if (FileWrite(af, io, (WORD)(46 + l)) != (WORD)(46 + l)) return 0;
        cg += 46 + l;
    }
    wl(io, 0x06054B50UL); ww(io + 4, 0); ww(io + 6, 0); ww(io + 8, nz); ww(io + 10, nz); wl(io + 12, cg); wl(io + 16, apos); ww(io + 20, 0);
    if (FileWrite(af, io, 22) != 22) return 0;
    FileWrite(af, io, 0);                         /* DOS: Laenge 0 schneidet die Datei hier ab */
    cdoff = apos;
    return 1;
}
static WORD eindatei(LPCSTR pfad, LPCSTR zn, const FINDDATA FAR *fd)   /* 0 = ok, 1 = Fehler, 2 = Abbruch */
{
    WORD i, l = slen(zn), r; DWORD ds; ZE FAR *e; BYTE meth = 8;
    if (gleich(pfad, arch)) return 0;              /* das Archiv selbst nicht */
    for (i = 0; i < nz; i++) if (gleich(ze[i].n, zn)) break;   /* gleicher Name: alter Eintrag faellt weg */
    if (i < nz) { for (; i + 1 < nz; i++) ze[i] = ze[i + 1]; nz--; }
    if (nz >= MAXE) { meldung(22, 0); return 2; }
    if ((qf = FileOpen(pfad, FO_READ | FO_DENYNONE)) < 0) { meldung(20, pfad); return 1; }
    scpy(tx, StrGet(hi, 44), sizeof tx); scat(tx, zn, sizeof tx); status(tx);
    FileSeek(af, (long)apos, FS_SET);
    for (i = 0; i < 30; i++) io[i] = 0;
    wl(io, 0x04034B50UL); ww(io + 4, 20); ww(io + 8, 8); ww(io + 10, fd->zeit); ww(io + 12, fd->datum); ww(io + 26, l);
    for (i = 0; i < l; i++) io[30 + i] = (BYTE)zn[i];
    if (FileWrite(af, io, (WORD)(30 + l)) != (WORD)(30 + l)) { FileClose(qf); meldung(36, zn); return 1; }
    ds = apos + 30 + l;
    crc = 0xFFFFFFFFUL; rest = 0; anz = 0; zk_abbruch = 0; gross = 0;
    {   BYTE FAR *b0 = (BYTE FAR *)MemLock(hb[0]); WORD FAR *b1 = (WORD FAR *)MemLock(hb[1]), FAR *b2 = (WORD FAR *)MemLock(hb[2]); BYTE FAR *b3 = (BYTE FAR *)MemLock(hb[3]);
        DWORD n = 0;
        r = Deflate(qlies, aschreib, 0, b0, b1, b2, b3, &n);
        MemUnlock(hb[0]); MemUnlock(hb[1]); MemUnlock(hb[2]); MemUnlock(hb[3]);
    }
    if (r == ZE_ABBRUCH) { FileClose(qf); return 2; }
    if ((r == ZE_SCHREIB && gross) || (!r && anz >= rest)) {   /* nicht kleiner geworden: gespeichert ablegen */
        WORD n;
        meth = 0; r = 0; FileSeek(qf, 0, FS_SET); FileSeek(af, (long)ds, FS_SET); anz = 0; rest = 0; crc = 0xFFFFFFFFUL;
        while ((n = FileRead(qf, io, 4096)) != 0 && n != 0xFFFF) {
            crc = CrcRechne(crc, io, n); rest += n;
            if (FileWrite(af, io, n) != n) { r = ZE_SCHREIB; break; }
            anz += n; pump();
            if (zk_abbruch) { FileClose(qf); return 2; }
        }
    }
    FileClose(qf);
    if (r) { meldung(36, zn); return 1; }
    FileSeek(af, (long)(apos + 8), FS_SET);
    ww(io, meth); FileWrite(af, io, 2);
    FileSeek(af, (long)(apos + 14), FS_SET);
    wl(io, crc ^ 0xFFFFFFFFUL); wl(io + 4, anz); wl(io + 8, rest); FileWrite(af, io, 12);
    e = &ze[nz++];
    { WORD k; for (k = 0; zn[k] && k < 63; k++) e->n[k] = zn[k]; e->n[k] = 0; } e->meth = meth; e->mk = 0; e->flags = 0; e->zeit = fd->zeit; e->datum = fd->datum;
    e->crc = crc ^ 0xFFFFFFFFUL; e->cs = anz; e->us = rest; e->off = apos;
    apos = ds + anz;
    pr("gepackt ", zn);
    return 0;
}
static WORD baum(char *p, char *zn, WORD tiefe)    /* Ordner p (endet mit \), Namen im Archiv mit zn (endet mit /) */
{
    FINDDATA fd; WORD lp = slen(p), lz = slen(zn), r = 0, e;
    scat(p, "*.*", 80);
    e = FileFindFirst(p, FA_DIR | FA_RDONLY | FA_HIDDEN | FA_ARCH, &fd);
    p[lp] = 0;
    while (!e && r != 2) {
        if (fd.name[0] != '.') {
            scat(p, fd.name, 80); scat(zn, fd.name, 64);
            if (fd.attr & FA_DIR) { if (tiefe < 8) { scat(p, "\\", 80); scat(zn, "/", 64); r = baum(p, zn, (WORD)(tiefe + 1)); } }
            else if (eindatei(p, zn, &fd) == 2) r = 2;
            p[lp] = 0; zn[lz] = 0;
        }
        e = FileFindNext(&fd);
    }
    return r;
}
static void hinzu(LPCSTR q)                       /* Datei oder Ordner dazupacken */
{
    FINDDATA fd; char p[80], zn[64]; WORD i, r = 0, n0;
    static const WORD gr4[4] = { DEF_PUF0, DEF_PUF1, DEF_PUF2, DEF_PUF3 };
    if (!speicher()) { meldung(39, 0); return; }
    scpy(p, q, sizeof p);
    if (slen(p) > 3 && p[slen(p) - 1] == '\\') p[slen(p) - 1] = 0;
    if (FileFindFirst(p, FA_DIR | FA_RDONLY | FA_HIDDEN | FA_ARCH, &fd) != 0) { meldung(20, p); return; }
    for (i = 0; i < 4; i++) if (!(hb[i] = MemAlloc(gr4[i], MEM_MOVABLE))) { while (i--) MemFree(hb[i]); meldung(39, 0); return; }
    if (offen) af = FileOpen(arch, FO_RW); else { af = FileCreate(arch, 0); nz = 0; cdoff = 0; }
    if (af < 0) { for (i = 0; i < 4; i++) MemFree(hb[i]); meldung(34, arch); return; }
    offen = 1; apos = cdoff; n0 = nz;
    busy = 1; CursorBusy(1);
    if (fd.attr & FA_DIR) {
        scpy(zn, fd.name, sizeof zn); scat(zn, "/", sizeof zn); scat(p, "\\", sizeof p);
        r = baum(p, zn, 0);
    } else {
        for (i = slen(p); i && p[i - 1] != '\\' && p[i - 1] != ':'; i--) ;
        scpy(zn, p + i, sizeof zn);
        r = eindatei(p, zn, &fd);
    }
    if (!cd_schreiben()) meldung(36, arch);
    FileClose(af);
    CursorBusy(0); busy = 0;
    for (i = 0; i < 4; i++) MemFree(hb[i]);
    if (r == 2) meldung(40, 0);
    anzeigen(nz ? nz - 1 : 0);
    (void)n0;
}
/* ---------------- Oberflaeche ---------------- */
static void oeffnen(LPCSTR n)
{
    WORD r;
    scpy(arch, n, sizeof arch);
    for (r = 0; arch[r]; r++) arch[r] = gr(arch[r]);
    if (!speicher()) { meldung(39, 0); return; }
    r = laden();
    if (r) { meldung(r, arch); nz = 0; offen = 0; }
    else pr("geladen ", arch);
    if (!ziel[0]) {                               /* Vorschlag: Ordner des Archivs */
        WORD i; scpy(ziel, arch, sizeof ziel);
        for (i = slen(ziel); i && ziel[i - 1] != '\\' && ziel[i - 1] != ':'; i--) ;
        ziel[i] = 0;
    }
    anzeigen(0);
    if (test) { char z[12]; zahl(z, nz); pr("Eintraege ", z); }
}
static void dialog(BYTE neu)
{
    FILEDLG d; char p[80];
    scpy(p, neu ? "NEU.ZIP" : arch, sizeof p);
    d.groesse = sizeof d; d.flags = neu ? FD_SAVE | FD_ASKOVER : FD_OPEN | FD_MUSTEXIST;
    d.titel = StrGet(hi, neu ? 50 : 51); d.filter = StrGet(hi, 52); d.datei = p; d.len = sizeof p;
    if (!FileDialog(fe, &d)) return;
    if (neu) { short f = FileCreate(p, 0); if (f < 0) { meldung(34, p); return; } FileClose(f); }
    ziel[0] = 0;
    oeffnen(p);
}
static void dazu(BYTE ordner)
{
    char p[80];
    if (!offen) { dialog(1); if (!offen) return; }
    p[0] = 0;
    if (ordner) { scpy(p, ziel, sizeof p); if (!InputBox(fe, StrGet(hi, 53), StrGet(hi, 1), p, sizeof p - 1) || !p[0]) return; }
    else {
        FILEDLG d;
        d.groesse = sizeof d; d.flags = FD_OPEN | FD_MUSTEXIST; d.titel = StrGet(hi, 54); d.filter = StrGet(hi, 55); d.datei = p; d.len = sizeof p;
        if (!FileDialog(fe, &d)) return;
    }
    hinzu(p);
}
static void markieren(WORD i, BYTE alle)
{
    WORD k; char t[34];
    if (!nz) return;
    if (alle) { BYTE v = 0; for (k = 0; k < nz; k++) if (!ze[k].mk) v = 1; for (k = 0; k < nz; k++) ze[k].mk = v; anzeigen(i); return; }
    if (i >= nz) return;
    ze[i].mk ^= 1;
    (void)t;
    anzeigen(i + 1 < nz ? i + 1 : i);
}
static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    RECT r; short h;
    switch (msg) {
    case M_SIZE:
        GetClientRect(w, &r); h = 0;
        if (sl) { HDC d = GetDC(w); h = (short)(GetCaps(d, CAPS_TEXTH) + 4); ReleaseDC(w, d); MoveWindow(sl, 2, (short)(r.b - h + 2), (short)(r.r - 4), (short)(h - 4), TRUE); }
        if (li) MoveWindow(li, 0, 0, r.r, (short)(r.b - h), TRUE);
        return 0;
    case M_ACTIVATE: if (p1 && li) SetFocus(li); return 0;
    case M_SETFOCUS: if (li) SetFocus(li); return 0;
    case M_KEY:
        if (busy) { if (p1 == K_ESC) zk_abbruch = 1; return 0; }
        if (p1 == 0x0F && (HIWORD(p2) & KS_CTRL)) dialog(0);
        else if (p1 == 0x0E && (HIWORD(p2) & KS_CTRL)) dialog(1);
        else if (p1 == 0x01 && (HIWORD(p2) & KS_CTRL)) markieren(0, 1);
        else if (p1 == ' ' || p1 == K_INS) markieren((WORD)SendMessage(li, L_GETSEL, 0, 0), 0);
        else if (p1 == K_F5 || p1 == K_ENTER) entpacken(1, 0);
        else if (p1 == K_ESC) PostMessage(w, M_CLOSE, 0, 0);
        else break;
        return 0;
    case M_COMMAND:
        if (busy) return 0;
        if (p1 == ID_LISTE) { if (HIWORD(p2) == N_DBLCLK || HIWORD(p2) == N_ENTER) entpacken(1, 0); return 0; }
        if (p1 == CM_NEU) dialog(1);
        else if (p1 == CM_OEFFNEN) dialog(0);
        else if (p1 == CM_ALLE) entpacken(0, 0);
        else if (p1 == CM_AUSWAHL) entpacken(1, 0);
        else if (p1 == CM_PRUEFEN) entpacken(0, 1);
        else if (p1 == CM_DATEI) dazu(0);
        else if (p1 == CM_ORDNER) dazu(1);
        else if (p1 == CM_MARK) markieren(0, 1);
        else if (p1 == CM_ENDE) PostMessage(w, M_CLOSE, 0, 0);
        if (li) SetFocus(li);
        return 0;
    case M_CLOSE: if (busy) { zk_abbruch = 1; return 0; } DestroyWindow(w); return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
static LPCSTR wort(LPCSTR c, char *d, WORD n)     /* naechstes Wort der Befehlszeile */
{
    WORD i = 0;
    while (*c == ' ') c++;
    while (*c && *c != ' ') { if (i + 1 < n) d[i++] = *c; c++; }
    d[i] = 0;
    return c;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H, cw, fw, fh; char a1[80], a2[80], op = 0;
    (void)show; hi = inst;
    CrcInit();
    if (!cmd) cmd = "";
    if (cmd[0] == 'T' && (cmd[1] == ' ' || !cmd[1])) { test = 1; cmd++; }
    cmd = wort(cmd, a1, sizeof a1);
    if (a1[0] == '/' && a1[1]) { op = gr(a1[1]); cmd = wort(cmd, a1, sizeof a1); }
    cmd = wort(cmd, a2, sizeof a2);
    wc.name = "RdZip"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); cw = TextWidth(d, "M", 1); ReleaseDC(0, d);
    fw = (short)(34 * cw + 30); if (fw > W) fw = W;
    fh = (short)(H * 2 / 3);
    fe = CreateWindow("RdZip", StrGet(inst, 1), WS_NORMAL | WS_VISIBLE, (short)((W - fw) / 2), (short)(H / 8), fw, fh, 0, 1, inst);
    if (!fe) return 1;
    mn = MenuLoad(inst, 1); MenuSet(fe, mn);
    li = CreateWindow(CL_LIST, "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER, 0, 0, 10, 10, fe, ID_LISTE, inst);
    sl = CreateWindow(CL_STATIC, "", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, fe, ID_STAT, inst);
    if (!li || !sl) { DestroyWindow(fe); return 1; }
    SendMessage(fe, M_SIZE, 0, 0);
    if (a1[0] && op == 'A') {                     /* Packen: Archiv darf fehlen (wird angelegt) */
        FINDDATA fd; WORD i;
        if (FileFindFirst(a1, FA_RDONLY | FA_ARCH, &fd) == 0) oeffnen(a1);
        else { scpy(arch, a1, sizeof arch); for (i = 0; arch[i]; i++) arch[i] = gr(arch[i]); offen = 0; nz = 0; titel(); }
    } else if (a1[0]) oeffnen(a1); else titel();
    SetFocus(li);
    if (test) DbgPrint("RDZIP: bereit");
    if (op == 'X' && offen) { if (a2[0]) { scpy(ziel, a2, sizeof ziel); fragen = 0; } entpacken(0, 0); PostMessage(fe, M_CLOSE, 0, 0); }
    else if (op == 'N' && a1[0] && a2[0]) {        /* Shell: Ordner a1, Namen a2 ...: Archivname erfragen, alles packen */
        char q[80], z[80]; FINDDATA fd; WORD i;
        scpy(z, a1, sizeof z); scat(z, a2, sizeof z);
        for (i = slen(z); i > slen(a1) && z[i - 1] != '.'; i--) ;
        if (i > slen(a1)) z[i - 1] = 0;
        scat(z, ".ZIP", sizeof z);
        if (test || InputBox(fe, StrGet(hi, 56), StrGet(hi, 1), z, sizeof z - 1)) {
            if (FileFindFirst(z, FA_RDONLY | FA_ARCH, &fd) == 0) oeffnen(z);
            else { scpy(arch, z, sizeof arch); for (i = 0; arch[i]; i++) arch[i] = gr(arch[i]); offen = 0; nz = 0; }
            for (;;) {
                scpy(q, a1, sizeof q); scat(q, a2, sizeof q); hinzu(q);
                cmd = wort(cmd, a2, sizeof a2); if (!a2[0]) break;
            }
            if (!test) { tx[0] = 0; zcat(tx, nz, sizeof tx); scat(tx, StrGet(hi, 57), sizeof tx); scat(tx, arch, sizeof tx); MsgBox(fe, tx, StrGet(hi, 1), MB_OK | MB_INFO); }
        }
        PostMessage(fe, M_CLOSE, 0, 0);
    }
    else if (op == 'P' && offen) { entpacken(0, 1); PostMessage(fe, M_CLOSE, 0, 0); }
    else if (op == 'A' && a1[0] && a2[0]) { hinzu(a2); while (*cmd) { cmd = wort(cmd, a2, sizeof a2); if (a2[0]) hinzu(a2); } PostMessage(fe, M_CLOSE, 0, 0); }
    while (GetMessage(&m)) DispatchMessage(&m);
    MenuFree(mn);
    if (hze) { MemUnlock(hze); MemFree(hze); }
    if (hio) { MemUnlock(hio); MemFree(hio); }
    if (test) DbgPrint("RDZIP: Ende");
    return 0;
}
