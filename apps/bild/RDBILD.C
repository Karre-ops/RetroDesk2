/* RDBILD.C - Bildbetrachter: BMP, PCX, GIF, JPEG (Baseline). Befehlszeile: [T] [/O] [Datei]  (/O = Originalgroesse)
 * Das Bild wird beim Lesen auf die Fenstergroesse gebracht (JPEG schon beim Entpacken 1/2..1/8), auf die Farben
 * des Schirms gerastert (256: Wuerfel 6x6x6, 16: Standardfarben, sonst S/W; Fehlerverteilung "Sierra Lite")
 * und in Streifen zu 16 Zeilen im Geraeteformat gehalten (Neuzeichnen ohne neues Lesen). Exif-Lage wird beachtet.
 * Tasten: Esc Ende, Bild ab/Leertaste naechstes, Bild auf voriges, F einpassen, 1 Originalgroesse, Pfeile rollen. */
#include "RD2.H"
#include "BILD.H"

#define MAXS 64
#define CM_OEFFNEN 101
#define CM_NAECHST 102
#define CM_VORIG   103
#define CM_INFO    104
#define CM_ENDE    109
#define CM_FIT     201
#define CM_ORIG    202
static HINST hi; static HWND fe; static HMENU mn; static BYTE test, orig, abbruch, lauf;
static char pfad[80], tx[120];
static short fh = -1; static BILD bi; static WORD fehler;
static HBMP st[MAXS]; static BYTE FAR *sp[MAXS]; static WORD nst, dw, dh, bpr; static BYTE bpp, dreh;
static short ox, oy;                              /* Rollstand */
static HMEM mh[24]; static WORD nmh;
static short FAR *e1, FAR *e2;                    /* Fehler: laufende und naechste Linie */
static WORD FAR *pm;                              /* Punkt i der Linie -> Spalte der Quellzeile */
static WORD ll, lc, ls, fertig;                   /* Linienlaenge, Linien, Quellzeilen, erledigte Linien */
static BYTE lut[4096]; static BYTE lev[256];
static const BYTE p16[48] = { 0,0,0, 0,0,170, 0,170,0, 0,170,170, 170,0,0, 170,0,170, 170,85,0, 170,170,170,
    85,85,85, 85,85,255, 85,255,85, 85,255,255, 255,85,85, 255,85,255, 255,255,85, 255,255,255 };

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static void zahl(char *d, DWORD v) { char t[12]; WORD i = 0; do t[i++] = (char)('0' + v % 10); while ((v /= 10) && i < 11); while (i) *d++ = t[--i]; *d = 0; }
static void zcat(char *d, DWORD v) { char z[12]; zahl(z, v); scat(d, z, sizeof tx); }
static void pr(LPCSTR a) { if (test) { char t[120]; scpy(t, "RDBILD: ", sizeof t); scat(t, a, sizeof t); DbgPrint(t); } }

/* ---------------- Rueckrufe der Decoder ---------------- */
WORD BRead(BYTE FAR *b, WORD n) { WORD r = FileRead(fh, b, n); return r == 0xFFFF ? 0 : r; }
void BSeek(DWORD p) { FileSeek(fh, (long)p, FS_SET); }
DWORD BSize(void) { long a = FileSeek(fh, 0, FS_CUR), g = FileSeek(fh, 0, FS_END); FileSeek(fh, a, FS_SET); return (DWORD)g; }
void FAR *BMem(DWORD n)
{
    HMEM h; void FAR *p;
    if (nmh >= 24 || n > 65000UL || !(h = MemAlloc(n, MEM_MOVABLE | MEM_ZEROINIT))) return 0;
    if (!(p = MemLock(h))) { MemFree(h); return 0; }
    mh[nmh++] = h; return p;
}
static void memweg(void) { while (nmh) { nmh--; MemUnlock(mh[nmh]); MemFree(mh[nmh]); } bl_puf = 0; }
static void pump(void)
{
    MSG m;
    while (PeekMessage(&m, 1)) {
        if (m.msg == M_KEY && m.p1 == K_ESC) abbruch = 1;
        else if (m.msg == M_QUIT) { abbruch = 1; PostQuit(0); return; }
        else if (m.msg == M_KEY || m.msg == M_COMMAND) ;   /* waehrend des Lesens: andere Tasten verfallen */
        else DispatchMessage(&m);
    }
}
static void titel(WORD proz)
{
    WORD i;
    scpy(tx, StrGet(hi, 1), sizeof tx);
    if (pfad[0]) {
        for (i = slen(pfad); i && pfad[i - 1] != '\\' && pfad[i - 1] != ':'; i--) ;
        scat(tx, " - ", sizeof tx); scat(tx, pfad + i, sizeof tx);
        if (proz < 100) { scat(tx, " (", sizeof tx); zcat(tx, proz); scat(tx, "%)", sizeof tx); }
        else if (!fehler && nst) { scat(tx, " ", sizeof tx); zcat(tx, bi.qw); scat(tx, "x", sizeof tx); zcat(tx, bi.qh); }
    }
    SetWindowText(fe, tx);
}
/* ---------------- Rastern und ablegen ---------------- */
static void setz(WORD x, WORD y, BYTE v)          /* Geraetepunkt in den Streifen */
{
    BYTE FAR *p = sp[y >> 4] + (WORD)(y & 15) * bpr;
    if (bpp == 8) p[x] = v;
    else if (bpp == 4) { p += x >> 1; *p = (BYTE)((x & 1) ? (*p & 0xF0) | v : (*p & 0x0F) | (v << 4)); }
    else { p += x >> 3; if (v) *p |= (BYTE)(0x80 >> (x & 7)); else *p &= (BYTE)~(0x80 >> (x & 7)); }
}
static void linie(WORD k, const BYTE FAR *rgb)   /* Linie k aus der Quellzeile: rastern, nach Lage ablegen */
{
    WORD i; short r = 0, g = 0, b = 0, FAR *t; BYTE FAR *row = 0; WORD x, y;
    for (i = 0; i <= ll + 1; i++) e2[3 * i] = e2[3 * i + 1] = e2[3 * i + 2] = 0;
    if (dreh == 1) row = sp[k >> 4] + (WORD)(k & 15) * bpr;
    for (i = 0; i < ll; i++) {
        const BYTE FAR *q = rgb + 3 * pm[i]; short FAR *e = e1 + 3 * (i + 1), FAR *n = e2 + 3 * (i + 1); BYTE v; short f;
        if (bpp == 1) {
            short gr = (short)(((WORD)q[0] * 77 + (WORD)q[1] * 150 + (WORD)q[2] * 29) >> 8) + e[0] + r;
            if (gr < 0) gr = 0; else if (gr > 255) gr = 255;
            v = (BYTE)(gr >= 128); f = gr - (v ? 255 : 0);
            r = f >> 1; n[-3] += f >> 2; n[0] += f - (f >> 1) - (f >> 2);
        } else {
            short c0 = q[0] + e[0] + r, c1 = q[1] + e[1] + g, c2 = q[2] + e[2] + b;
            if (c0 < 0) c0 = 0; else if (c0 > 255) c0 = 255;
            if (c1 < 0) c1 = 0; else if (c1 > 255) c1 = 255;
            if (c2 < 0) c2 = 0; else if (c2 > 255) c2 = 255;
            if (bpp == 8) { BYTE a = lev[c0], bb = lev[c1], c = lev[c2]; v = (BYTE)(16 + 36 * a + 6 * bb + c); c0 -= a * 51; c1 -= bb * 51; c2 -= c * 51; }
            else { v = lut[((c0 >> 4) << 8) | ((c1 >> 4) << 4) | (c2 >> 4)]; c0 -= p16[3 * v]; c1 -= p16[3 * v + 1]; c2 -= p16[3 * v + 2]; }
            r = c0 >> 1; g = c1 >> 1; b = c2 >> 1;
            n[-3] += c0 >> 2; n[-2] += c1 >> 2; n[-1] += c2 >> 2;
            n[0] += c0 - (c0 >> 1) - (c0 >> 2); n[1] += c1 - (c1 >> 1) - (c1 >> 2); n[2] += c2 - (c2 >> 1) - (c2 >> 2);
        }
        if (row) {
            if (bpp == 8) row[i] = v;
            else if (bpp == 4) { BYTE FAR *p = row + (i >> 1); *p = (BYTE)((i & 1) ? (*p & 0xF0) | v : (*p & 0x0F) | (v << 4)); }
            else if (v) row[i >> 3] |= (BYTE)(0x80 >> (i & 7));
            continue;
        }
        switch (dreh) {                           /* Lage: 3 = 180 Grad, 6 = 90 rechts, 8 = 90 links */
        case 3: x = dw - 1 - i; y = dh - 1 - k; break;
        case 6: x = dw - 1 - k; y = i; break;
        default: x = k; y = dh - 1 - i; break;
        }
        setz(x, y, v);
    }
    t = e1; e1 = e2; e2 = t;
}
BYTE BZeile(WORD y, const BYTE FAR *rgb)
{
    DWORD k0 = ((DWORD)y * lc + ls - 1) / ls, k1 = ((DWORD)(y + 1) * lc + ls - 1) / ls, k;
    for (k = k0; k < k1 && k < lc; k++) {
        linie((WORD)k, rgb); fertig++;
        if (dreh == 1 && ((k & 15) == 15 || k + 1 == lc)) {   /* Streifen fertig: gleich zeigen */
            RECT r; GetClientRect(fe, &r);
            r.t = (short)(((short)k & ~15) - oy + (r.b > (short)dh ? (r.b - (short)dh) / 2 : 0)); r.b = r.t + 16;
            InvalidateRect(fe, &r, FALSE);
        }
    }
    if (!(y & 7)) {
        titel((WORD)((DWORD)fertig * 100 / (lc ? lc : 1)));
        if (dreh != 1 && !(y & 63)) InvalidateRect(fe, 0, FALSE);
        pump();
    }
    return (BYTE)!abbruch;
}
/* ---------------- Laden ---------------- */
static void streifen_weg(void) { WORD i; for (i = 0; i < nst; i++) { if (sp[i]) MemUnlock(st[i]); BmpFree(st[i]); } nst = 0; }
static void meldung(WORD id)
{
    scpy(tx, StrGet(hi, id), sizeof tx); scat(tx, pfad, sizeof tx);
    pr(tx);
    if (!test) MsgBox(fe, tx, StrGet(hi, 1), MB_OK | MB_ERROR);
}
static void laden(void)
{
    RECT r; HDC d; WORD i, tw, th, sw, sh, ax, ay; BYTE rot, sk = 1; DWORD frei;
    streifen_weg(); memweg(); fehler = 0; abbruch = 0; ox = oy = 0;
    if (fh >= 0) FileClose(fh);
    if ((fh = FileOpen(pfad, FO_READ | FO_DENYNONE)) < 0) { fehler = 1; meldung(20); titel(100); InvalidateRect(fe, 0, TRUE); return; }
    if ((fehler = BildStart(&bi)) != 0) { meldung((WORD)(20 + fehler)); FileClose(fh); fh = -1; memweg(); titel(100); InvalidateRect(fe, 0, TRUE); return; }
    dreh = bi.dreh == 3 || bi.dreh == 6 || bi.dreh == 8 ? bi.dreh : bi.dreh == 5 ? 8 : bi.dreh == 7 ? 6 : bi.dreh == 4 ? 3 : 1;
    rot = (BYTE)(dreh == 6 || dreh == 8);
    d = GetDC(fe); bpp = (BYTE)GetCaps(d, CAPS_BPP); i = (WORD)GetCaps(d, CAPS_FARBEN); ax = (WORD)GetCaps(d, CAPS_ASPX); ay = (WORD)GetCaps(d, CAPS_ASPY); ReleaseDC(fe, d);
    if (!ax || !ay) ax = ay = 100;
    if (bpp == 8) ; else if (bpp == 4 && i >= 16) bpp = 4; else bpp = 1;
    GetClientRect(fe, &r);
    tw = (WORD)(rot ? r.b : r.r); th = (WORD)(rot ? r.r : r.b);   /* Ziel in Lage der Datei */
    if (orig) { tw = 1024; th = 1024; }
    if (tw < 16) tw = 16; if (th < 16) th = 16;
    if (bi.art == BA_JPG) {                       /* groesste Verkleinerung, die noch 3/4 des Ziels fuellt */
        for (sk = 8; sk > 1; sk >>= 1) {
            WORD w8 = (WORD)((bi.qw + sk - 1) / sk), h8 = (WORD)((bi.qh + sk - 1) / sk);
            if (orig ? (w8 >= bi.qw && h8 >= bi.qh) : ((DWORD)w8 * 4 >= (DWORD)tw * 3 || (DWORD)h8 * 4 >= (DWORD)th * 3)) break;
        }
        while (sk < 8 && (bi.qw + sk - 1) / sk > 1024) sk <<= 1;
    }
    bi.skal = sk;
    sw = (WORD)((bi.qw + sk - 1) / sk); sh = (WORD)((bi.qh + sk - 1) / sk);
    for (;;) {                                    /* Anzeigegroesse (Lage der Datei): einpassen, nie vergroessern */
        DWORD a = rot ? (DWORD)sw * ax / ay : sw, b = rot ? sh : (DWORD)sh * ax / ay;   /* Punkte nicht quadratisch (CGA, EGA): Geraeteeinheiten */
        if (a > tw) { b = b * tw / a; a = tw; }
        if (b > th) { a = a * th / b; b = th; }
        if (a > 1024) { b = b * 1024 / a; a = 1024; }
        if (!a) a = 1; if (!b) b = 1;
        dw = (WORD)(rot ? b : a); dh = (WORD)(rot ? a : b);
        bpr = (WORD)(((DWORD)dw * bpp + 7) / 8);
        frei = MemAvail(MEMA_KONV);
        if ((DWORD)bpr * 16 * ((dh + 15) / 16) + 90000UL < frei || (tw < 64 && th < 64)) break;
        tw = (WORD)(tw * 3 / 4); th = (WORD)(th * 3 / 4);   /* zu wenig Speicher: kleiner */
    }
    nst = (WORD)((dh + 15) / 16); if (nst > MAXS) { nst = MAXS; dh = MAXS * 16; }
    for (i = 0; i < nst; i++) {
        if (!(st[i] = BmpCreate(dw, 16, bpp)) || !(sp[i] = (BYTE FAR *)MemLock(st[i]))) { if (st[i]) BmpFree(st[i]); nst = i; streifen_weg(); fehler = BE_SPEICHER; meldung(23); FileClose(fh); fh = -1; memweg(); return; }
        sp[i] += 8;                               /* hinter den Bitmapkopf (w, h, bpp, bpr) */
    }
    ll = rot ? dh : dw; lc = rot ? dw : dh; ls = sh;
    e1 = (short FAR *)BMem(6UL * (ll + 2)); e2 = (short FAR *)BMem(6UL * (ll + 2)); pm = (WORD FAR *)BMem(2UL * ll);
    if (!e1 || !e2 || !pm) { streifen_weg(); fehler = BE_SPEICHER; meldung(23); FileClose(fh); fh = -1; memweg(); return; }
    for (i = 0; i < ll; i++) pm[i] = (WORD)((DWORD)i * sw / ll);
    if (bpp == 1) for (i = 0; i < nst; i++) { WORD k; for (k = 0; k < 16 * bpr; k++) sp[i][k] = 0; }
    fertig = 0; lauf = 1; CursorBusy(1);
    { char z[60]; z[0] = 0; scpy(z, "Lesen ", sizeof z); zahl(z + 6, bi.qw); scat(z, "x", sizeof z); { char q[8]; zahl(q, bi.qh); scat(z, q, sizeof z); } scat(z, " skal ", sizeof z); { char q[4]; zahl(q, sk); scat(z, q, sizeof z); } pr(z); }
    InvalidateRect(fe, 0, TRUE);
    fehler = BildLesen(&bi);
    CursorBusy(0); lauf = 0;
    FileClose(fh); fh = -1;
    for (i = 0; i < nst; i++) { sp[i] -= 8; MemUnlock(st[i]); sp[i] = 0; }
    memweg();
    if (fehler == BE_ABBRUCH) pr("abgebrochen");
    else if (fehler) { meldung((WORD)(20 + fehler)); fehler = 0; }   /* Rest (kaputt) trotzdem zeigen */
    titel(100);
    InvalidateRect(fe, 0, TRUE);
    if (test) { char z[60]; scpy(z, "fertig ", sizeof z); zahl(z + 7, dw); scat(z, "x", sizeof z); { char q[8]; zahl(q, dh); scat(z, q, sizeof z); } scat(z, " Lage ", sizeof z); { char q[4]; zahl(q, dreh); scat(z, q, sizeof z); } pr(z); }
}
/* ---------------- Fenster ---------------- */
static void rollen(short x, short y)
{
    RECT r; short mx, my;
    GetClientRect(fe, &r);
    mx = (short)dw - r.r; my = (short)dh - r.b; if (mx < 0) mx = 0; if (my < 0) my = 0;
    if (x > mx) x = mx; if (y > my) y = my; if (x < 0) x = 0; if (y < 0) y = 0;
    if (x != ox || y != oy) { ox = x; oy = y; InvalidateRect(fe, 0, FALSE); }
    ScrollSet(fe, SB_HORZ, ox, mx, r.r); ScrollSet(fe, SB_VERT, oy, my, r.b);
}
static void malen(HDC d, const RECT *nr)
{
    RECT r, f; short x0, y0; WORD i;
    GetClientRect(fe, &r);
    x0 = r.r > (short)dw ? (r.r - (short)dw) / 2 : -ox; y0 = r.b > (short)dh ? (r.b - (short)dh) / 2 : -oy;
    SetBrush(d, C_GRAY, PAT_SOLID);
    if (!nst) { FillRect(d, &r); return; }
    f = r; f.b = y0; if (f.b > f.t) FillRect(d, &f);
    f = r; f.t = (short)(y0 + dh); if (f.b > f.t) FillRect(d, &f);
    f = r; f.t = y0; f.b = (short)(y0 + dh); f.r = x0; if (f.r > f.l) FillRect(d, &f);
    f.l = (short)(x0 + dw); f.r = r.r; if (f.r > f.l) FillRect(d, &f);
    for (i = 0; i < nst; i++) {
        short y = (short)(y0 + 16 * i), h = (short)(dh - 16 * i > 16 ? 16 : dh - 16 * i);
        HDC b;
        if (y + h <= nr->t || y >= nr->b || y + h <= 0 || y >= r.b) continue;
        if ((b = BmpDC(st[i])) != 0) { BitBlt(d, x0, y, (short)dw, h, b, 0, 0, ROP_COPY); DCFree(b); }
    }
}
static BYTE bildname(LPCSTR n)
{
    static const char e[] = "BMPPCXGIFJPGJPE"; WORD i, l = slen(n);
    if (l < 4 || n[l - 4] != '.') return 0;
    for (i = 0; i < sizeof e - 1; i += 3) if ((n[l - 3] & 0xDF) == e[i] && (n[l - 2] & 0xDF) == e[i + 1] && (n[l - 1] & 0xDF) == e[i + 2]) return 1;
    return 0;
}
static void blaettern(BYTE vor)                   /* naechstes/voriges Bild im Ordner (Reihenfolge des Verzeichnisses) */
{
    FINDDATA fd; char d[80], erst[13], vorig[13], treffer[13]; WORD i, k; BYTE gefunden = 0;
    if (!pfad[0] || lauf) return;
    scpy(d, pfad, sizeof d);
    for (i = slen(d); i && d[i - 1] != '\\' && d[i - 1] != ':'; i--) ;
    k = i; d[k] = 0; scat(d, "*.*", sizeof d);
    erst[0] = vorig[0] = treffer[0] = 0;
    if (FileFindFirst(d, FA_RDONLY | FA_ARCH, &fd)) return;
    do {
        if (!bildname(fd.name)) continue;
        if (!erst[0]) scpy(erst, fd.name, 13);
        if (gefunden) { if (vor && !treffer[0]) scpy(treffer, fd.name, 13); continue; }
        { WORD j; for (j = 0; fd.name[j] && (fd.name[j] & 0xDF) == (pfad[k + j] & 0xDF); j++) ; if (!fd.name[j] && !pfad[k + j]) { gefunden = 1; if (!vor) scpy(treffer, vorig, 13); continue; } }
        scpy(vorig, fd.name, 13);
    } while (!FileFindNext(&fd));
    if (vor && !treffer[0]) scpy(treffer, erst, 13);
    if (!vor && !treffer[0]) scpy(treffer, vorig, 13);   /* vom ersten zum letzten */
    if (!treffer[0]) return;
    pfad[k] = 0; scat(pfad, treffer, sizeof pfad);
    laden();
}
static void oeffnen(void)
{
    FILEDLG d; char p[80];
    scpy(p, pfad, sizeof p);
    d.groesse = sizeof d; d.flags = FD_OPEN | FD_MUSTEXIST; d.titel = StrGet(hi, 5); d.filter = StrGet(hi, 6); d.datei = p; d.len = sizeof p;
    if (!FileDialog(fe, &d)) return;
    scpy(pfad, p, sizeof pfad); laden();
}
static void info(void)
{
    static const char *an[] = { "?", "BMP", "PCX", "GIF", "JPEG" };
    if (!nst) return;
    scpy(tx, an[bi.art <= 4 ? bi.art : 0], sizeof tx); scat(tx, ", ", sizeof tx);
    zcat(tx, bi.qw); scat(tx, "x", sizeof tx); zcat(tx, bi.qh); scat(tx, ", ", sizeof tx); zcat(tx, bi.farben); scat(tx, StrGet(hi, 8), sizeof tx);
    zcat(tx, dw); scat(tx, "x", sizeof tx); zcat(tx, dh);
    if (bi.skal > 1) { scat(tx, " (1/", sizeof tx); zcat(tx, bi.skal); scat(tx, ")", sizeof tx); }
    if (dreh != 1) scat(tx, StrGet(hi, 9), sizeof tx);
    MsgBox(fe, tx, StrGet(hi, 1), MB_OK | MB_INFO);
}
static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps; RECT r;
    switch (msg) {
    case M_PAINT: BeginPaint(w, &ps); malen(ps.dc, &ps.neu); EndPaint(w, &ps); return 0;
    case M_ERASEBKGND: return 1;
    case M_SIZE: if (!lauf) rollen(ox, oy); return 0;
    case M_SCROLL:
        GetClientRect(w, &r);
        {   short *v = HIWORD(p2) == SB_VERT ? &oy : &ox, pg = HIWORD(p2) == SB_VERT ? r.b : r.r, n = *v;
            switch (p1) {
            case SB_LINEUP: n -= 16; break; case SB_LINEDOWN: n += 16; break;
            case SB_PAGEUP: n -= pg; break; case SB_PAGEDOWN: n += pg; break;
            case SB_THUMB: n = (short)LOWORD(p2); break; case SB_TOP: n = 0; break; case SB_BOTTOM: n = 32000; break;
            }
            if (v == &oy) rollen(ox, n); else rollen(n, oy);
        }
        return 0;
    case M_KEY:
        if (lauf) { if (p1 == K_ESC) abbruch = 1; return 0; }
        GetClientRect(w, &r);
        if (p1 == K_ESC) PostMessage(w, M_CLOSE, 0, 0);
        else if (p1 == K_PGDN || p1 == ' ') blaettern(1);
        else if (p1 == K_PGUP || p1 == K_BACK) blaettern(0);
        else if (p1 == K_LEFT) rollen((short)(ox - 32), oy);
        else if (p1 == K_RIGHT) rollen((short)(ox + 32), oy);
        else if (p1 == K_UP) rollen(ox, (short)(oy - 32));
        else if (p1 == K_DOWN) rollen(ox, (short)(oy + 32));
        else if (p1 == K_HOME) rollen(0, 0);
        else if (p1 == 'f' || p1 == 'F') PostMessage(w, M_COMMAND, CM_FIT, 0);
        else if (p1 == '1') PostMessage(w, M_COMMAND, CM_ORIG, 0);
        else if (p1 == 'i' || p1 == 'I') info();
        else if (p1 == 0x0F && (HIWORD(p2) & KS_CTRL)) oeffnen();
        else break;
        return 0;
    case M_COMMAND:
        if (lauf) return 0;
        if (p1 == CM_OEFFNEN) oeffnen();
        else if (p1 == CM_NAECHST) blaettern(1);
        else if (p1 == CM_VORIG) blaettern(0);
        else if (p1 == CM_INFO) info();
        else if (p1 == CM_FIT || p1 == CM_ORIG) { orig = (BYTE)(p1 == CM_ORIG); MenuCheck(mn, CM_FIT, !orig); MenuCheck(mn, CM_ORIG, orig); if (pfad[0]) laden(); }
        else if (p1 == CM_ENDE) PostMessage(w, M_CLOSE, 0, 0);
        return 0;
    case M_CLOSE: if (lauf) { abbruch = 1; return 0; } DestroyWindow(w); return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H; WORD i;
    (void)show; hi = inst;
    if (!cmd) cmd = "";
    for (;;) {
        while (*cmd == ' ') cmd++;
        if (cmd[0] == 'T' && (cmd[1] == ' ' || !cmd[1])) { test = 1; cmd++; }
        else if (cmd[0] == '/' && (cmd[1] == 'O' || cmd[1] == 'o')) { orig = 1; cmd += 2; }
        else break;
    }
    scpy(pfad, cmd, sizeof pfad);
    for (i = 0; i < 256; i++) lev[i] = (BYTE)((i * 5 + 127) / 255);
    for (i = 0; i < 4096; i++) {                  /* RGB (je 4 Bit) -> naechste der 16 Standardfarben */
        WORD k, best = 0; DWORD bd = 0xFFFFFFFFUL;
        short r = (short)(((i >> 8) << 4) + 8), g = (short)((((i >> 4) & 15) << 4) + 8), b = (short)(((i & 15) << 4) + 8);
        for (k = 0; k < 16; k++) {
            long dr = r - p16[3 * k], dg = g - p16[3 * k + 1], db = b - p16[3 * k + 2]; DWORD dd = (DWORD)(dr * dr * 3 + dg * dg * 4 + db * db * 2);
            if (dd < bd) { bd = dd; best = k; }
        }
        lut[i] = (BYTE)best;
    }
    wc.name = "RdBild"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); ReleaseDC(0, d);
    fe = CreateWindow("RdBild", StrGet(inst, 1), WS_NORMAL | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, (short)(W / 20), (short)(H / 20), (short)(W * 9 / 10), (short)(H * 9 / 10), 0, 1, inst);
    if (!fe) return 1;
    mn = MenuLoad(inst, 1); MenuSet(fe, mn); MenuCheck(mn, CM_FIT, !orig); MenuCheck(mn, CM_ORIG, orig);
    titel(100);
    if (test) DbgPrint("RDBILD: bereit");
    UpdateWindow(fe);
    if (pfad[0]) { laden(); rollen(0, 0); }
    while (GetMessage(&m)) DispatchMessage(&m);
    streifen_weg(); memweg();
    MenuFree(mn);
    if (test) DbgPrint("RDBILD: Ende");
    return 0;
}
