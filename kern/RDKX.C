/* RDKX.C - Kern-Zusatz: Uhrzeit, Sprache, Ressourcen, Texte, Debugger.
 *
 * Verwerfbares Systemmodul (PF_DISCARD): der Kern traegt die Exporte in die API-Tabelle ein,
 * geladen wird bei Bedarf. So bleibt der Kern unter 44 KB.
 *
 * Ressourcen (SPEC.md 2.2, 6): das Verzeichnis steht im RD2-Block der Programmdatei. Gesucht wird
 * zuerst in LangGet(), dann Englisch, dann Sprache 0. Texte (RT_STRING Nr. 1) werden je Programm
 * einmal geladen und bleiben gesperrt im Speicher des Programms (endet es, gibt der Kern sie frei).
 * HINST 0 = Systemtexte aus DE.LNG bzw. EN.LNG im RD2-Verzeichnis ("RDL1", WORD Sprache, Tabelle).
 * Ressourcen von Modulen gehen noch nicht (HINST von Modulen und Tasks sind noch nicht getrennt).
 */
#include "RD2.H"

#pragma pack(push, 1)
typedef struct { WORD typ, nr, lang, flags; DWORD off, len; } RESDIR;
#pragma pack(pop)

static WORD lang;                                 /* 0 = noch nicht gelesen */
static char pfad[80];
static char sbuf[128];                            /* StrGet ohne Tabelle */

/* ---- Uhrzeit ------------------------------------------------------------------- */
static void RDEXPORT k_TimeGet(RDTIME FAR *t)
{
    WORD j = 0, md = 0, wt = 0, hm = 0, sh = 0;
    _asm {
        mov  ah, 2Ah
        int  21h
        mov  j, cx
        mov  md, dx
        xor  ah, ah
        mov  wt, ax
        mov  ah, 2Ch
        int  21h
        mov  hm, cx
        mov  sh, dx
    }
    t->jahr = j; t->monat = (BYTE)(md >> 8); t->tag = (BYTE)md; t->wtag = (BYTE)wt;
    t->std = (BYTE)(hm >> 8); t->min = (BYTE)hm; t->sek = (BYTE)(sh >> 8); t->hsek = (BYTE)sh;
}
static BYTE bcd(WORD v) { return (BYTE)(((v / 10) << 4) | (v % 10)); }
/* bios = 1: auch die Echtzeituhr (AT, INT 1Ah 03h/05h; auf dem XT ohne Uhr tut das nichts) */
static WORD RDEXPORT k_TimeSet(const RDTIME FAR *t, WORD bios)
{
    WORD j = t->jahr, md = ((WORD)t->monat << 8) | t->tag, hm = ((WORD)t->std << 8) | t->min, sh = ((WORD)t->sek << 8) | t->hsek;
    BYTE e1 = 0, e2 = 0;
    _asm {
        mov  ah, 2Bh
        mov  cx, j
        mov  dx, md
        int  21h
        mov  e1, al
        mov  ah, 2Dh
        mov  cx, hm
        mov  dx, sh
        int  21h
        mov  e2, al
    }
    if (e1 || e2) { SetLastError(RD_ERR_FORMAT); return RD_ERR_FORMAT; }
    if (bios) {
        WORD c = ((WORD)bcd(t->std) << 8) | bcd(t->min), d = (WORD)bcd(t->sek) << 8;
        WORD c2 = ((WORD)bcd(j / 100) << 8) | bcd(j % 100), d2 = ((WORD)bcd(t->monat) << 8) | bcd(t->tag);
        _asm {
            mov  ah, 03h
            mov  cx, c
            mov  dx, d
            int  1Ah
            mov  ah, 05h
            mov  cx, c2
            mov  dx, d2
            int  1Ah
        }
    }
    return 0;
}

/* ---- Sprache ------------------------------------------------------------------- */
/* RD2.INI [System] Sprache=DE|EN, sonst Deutsch */
static WORD RDEXPORT k_LangGet(void)
{
    char s[4];
    if (!lang) {
        ProfileGetStr("RD2.INI", "System", "Sprache", "DE", s, sizeof s);
        lang = ((s[0] | 0x20) == 'e' && (s[1] | 0x20) == 'n') ? RD_LANG_EN : RD_LANG_DE;
    }
    return lang;
}

/* ---- Ressourcen ---------------------------------------------------------------- */
/* Eintrag suchen: Datei offen lassen (f), Position und Laenge zurueck. 0 = nicht gefunden. */
static short rfind(HINST i, WORD typ, WORD nr, DWORD FAR *pos, WORD FAR *len)
{
    DWORD blk = InstRes(i, pfad, sizeof pfad);
    static struct { char sig[4]; WORD api, flags, stack, kb; char name[16]; WORD ver, nexp, nres, oexp, ores; } rb;
    RESDIR d; WORD k, best = 0, w, l = k_LangGet(); short f;
    if (!blk) return -1;
    if ((f = FileOpen(pfad, FO_READ | FO_DENYNONE)) < 0) return -1;
    if (FileSeek(f, (long)blk, 0) < 0 || FileRead(f, &rb, sizeof rb) != sizeof rb) goto weg;
    FileSeek(f, (long)(blk + rb.ores), 0);
    for (k = 0; k < rb.nres; k++) {
        if (FileRead(f, &d, sizeof d) != sizeof d) break;
        if (d.typ != typ || d.nr != nr || d.len > 0xFFF0UL) continue;
        w = d.lang == l ? 3 : d.lang == RD_LANG_EN ? 2 : d.lang == 0 ? 1 : 0;
        if (w > best) { best = w; *pos = blk + d.off; *len = (WORD)d.len; }
    }
    if (best) return f;
weg:
    FileClose(f);
    return -1;
}
static HMEM rload(short f, DWORD pos, WORD len, WORD vor, WORD fl)  /* vor: Bytes frei am Anfang */
{
    HMEM h = MemAlloc((DWORD)len + vor, MEM_MOVABLE | fl); BYTE FAR *p;
    if (h && (p = (BYTE FAR *)MemLock(h)) != 0) {
        if (FileSeek(f, (long)pos, 0) >= 0 && FileRead(f, p + vor, len) == len) { MemUnlock(h); FileClose(f); return h; }
        MemUnlock(h);
    }
    if (h) MemFree(h);
    FileClose(f);
    SetLastError(h ? RD_ERR_DISK : RD_ERR_NOMEM);
    return 0;
}
static HMEM RDEXPORT k_ResLoad(HINST i, WORD typ, WORD nr)
{
    DWORD pos; WORD len; short f = rfind(i, typ, nr, &pos, &len);
    if (f < 0) { SetLastError(RD_ERR_NOTFOUND); return 0; }
    return rload(f, pos, len, 0, 0);
}
static void RDEXPORT k_ResFree(HMEM r) { if (r) MemFree(r); }

/* ---- Texte --------------------------------------------------------------------- */
/* Zwischenspeicher je Programm: Block = WORD inst, WORD blk-low, dann die Tabelle
   (WORD anzahl, je Text WORD id, WORD offset ab Tabellenanfang, Texte mit 0). */
#define NSC 6
static struct { HINST i; HMEM h; BYTE FAR *p; } sc[NSC];
static BYTE FAR *stab(HINST i)
{
    WORD k, frei = NSC, len, tag; DWORD pos, blk; short f; HMEM h; BYTE FAR *p;
    blk = i ? InstRes(i, 0, 0) : 0;
    tag = i ? (WORD)blk ^ i : 0x4C4E;            /* nie 0: freigegebener Speicher sieht sonst gueltig aus */
    for (k = 0; k < NSC; k++) {
        if (sc[k].h && sc[k].i == i) {
            if (((WORD FAR *)sc[k].p)[0] == i && ((WORD FAR *)sc[k].p)[1] == tag) return sc[k].p + 4;
            sc[k].h = 0;                           /* Programm beendet, Block gehoert nicht mehr uns */
        }
        if (!sc[k].h && frei == NSC) frei = k;
    }
    if (frei == NSC) { frei = 0; MemFree(sc[0].h); sc[0].h = 0; }
    if (i) {
        if ((f = rfind(i, RT_STRING, 1, &pos, &len)) < 0) return 0;
    } else {                                      /* Systemtexte: <home>\DE.LNG */
        static SYSINFO si; static BYTE kopf[6]; WORD n;
        KernInfo(&si);
        for (n = 0; n < 66 && si.home[n]; n++) pfad[n] = si.home[n];
        if (n && pfad[n - 1] != '\\') pfad[n++] = '\\';
        pfad[n++] = k_LangGet() == RD_LANG_EN ? 'E' : 'D'; pfad[n++] = k_LangGet() == RD_LANG_EN ? 'N' : 'E';
        pfad[n++] = '.'; pfad[n++] = 'L'; pfad[n++] = 'N'; pfad[n++] = 'G'; pfad[n] = 0;
        if ((f = FileOpen(pfad, FO_READ | FO_DENYNONE)) < 0) return 0;
        pos = FileSeek(f, 0, 2);
        if (pos < 6 || pos > 0xFFF0UL || (FileSeek(f, 0, 0), FileRead(f, kopf, 6)) != 6 || kopf[0] != 'R' || kopf[3] != '1') { FileClose(f); return 0; }
        len = (WORD)pos - 6; pos = 6;
    }
    if (!(h = rload(f, pos, len, 4, 0))) return 0;   /* gehoert dem Programm: endet es, ist der Block weg */
    p = (BYTE FAR *)MemLock(h);                   /* bleibt gesperrt: StrGet-Zeiger gelten weiter */
    ((WORD FAR *)p)[0] = i; ((WORD FAR *)p)[1] = tag;
    sc[frei].i = i; sc[frei].h = h; sc[frei].p = p;
    return p + 4;
}
static LPCSTR sfind(HINST i, WORD id)
{
    BYTE FAR *t = stab(i); WORD n, k;
    if (!t) return 0;
    n = *(WORD FAR *)t;
    for (k = 0; k < n; k++)
        if (((WORD FAR *)t)[1 + 2 * k] == id) return (LPCSTR)(t + ((WORD FAR *)t)[2 + 2 * k]);
    return 0;
}
static WORD RDEXPORT k_StrLoad(HINST i, WORD id, LPSTR buf, WORD len)
{
    LPCSTR s = sfind(i, id); WORD n = 0;
    if (!len) return 0;
    if (s) while (n + 1 < len && s[n]) { buf[n] = s[n]; n++; }
    else SetLastError(RD_ERR_NOTFOUND);
    buf[n] = 0;
    return n;
}
/* Zeiger direkt in die gesperrte Tabelle; fehlt der Text: "" */
static LPCSTR RDEXPORT k_StrGet(HINST i, WORD id)
{
    LPCSTR s = sfind(i, id);
    if (!s) { SetLastError(RD_ERR_NOTFOUND); sbuf[0] = 0; return sbuf; }
    return s;
}

/* ---- Debugger ------------------------------------------------------------------ */
/* INT 3 nur, wenn der Vektor nicht auf ein IRET zeigt (sonst ist kein Debugger da) */
static void RDEXPORT k_DbgBreak(void)
{
    BYTE FAR * FAR *v = (BYTE FAR * FAR *)0x0000000CUL;
    if (*v && **v != 0xCF) { _asm { int 3 } }
}

/* ---- Bilder (.RDB, SPEC.md 7) -------------------------------------------------------
   "RDB1", WORD w, h, bpp (1/4/8), flags (1 = mit Maske, 4 = CGA-Fassung), Palette bei bpp > 1
   (n x RGB), Zeilen von oben auf ganze Bytes, danach die Maske (1 Bit, 1 = deckend).
   Ein Symbol (RT_ICON) darf zwei Bilder hintereinander enthalten: normal und CGA (32x16).
   Bit 1 = hell (wie der Bildspeicher). Mehr Bit pro Punkt: hell ab Helligkeit 128.
   */
typedef struct { char sig[4]; WORD w, h, bpp, flags; } RDBH;
static WORD rdblen(const RDBH FAR *k)             /* Laenge eines Bildes in Bytes */
{
    WORD z = (WORD)(((DWORD)k->w * k->bpp + 7) >> 3), m = (k->w + 7) >> 3;
    return sizeof(RDBH) + (k->bpp > 1 ? 3 << k->bpp : 0) + z * k->h + (k->flags & 1 ? m * k->h : 0);
}
/* Bild Nr. "cga" (0/1) suchen, sonst das erste. weiss/schwarz: Punkte, die hell bzw. dunkel
   gezeichnet werden (ohne Maske ist alles deckend). schwarz = 0: nur das Bild (BmpLoad). */
static WORD rdbmake(const BYTE FAR *p, WORD len, WORD cga, HBMP FAR *weiss, HBMP FAR *schwarz)
{
    const RDBH FAR *k = 0, FAR *x; const BYTE FAR *q = p, FAR *pal, FAR *z, FAR *mk; WORD n, y, c, sb, db, v, b; BYTE FAR *w, FAR *s;
    while (len >= sizeof(RDBH)) {
        x = (const RDBH FAR *)q;
        if (x->sig[0] != 'R' || x->sig[3] != '1' || !x->w || !x->h || (x->bpp != 1 && x->bpp != 4 && x->bpp != 8)) break;
        if ((n = rdblen(x)) > len) break;
        if (!k || ((x->flags >> 2) & 1) == cga) k = x;
        if (((x->flags >> 2) & 1) == cga) break;
        q += n; len -= n;
    }
    if (!k) { SetLastError(RD_ERR_FORMAT); return 0; }
    pal = (const BYTE FAR *)(k + 1); z = pal + (k->bpp > 1 ? 3 << k->bpp : 0);
    sb = (WORD)(((DWORD)k->w * k->bpp + 7) >> 3); mk = z + sb * k->h; db = (k->w + 7) >> 3;
    *weiss = BmpCreate(k->w, k->h, 1);
    if (schwarz) *schwarz = BmpCreate(k->w, k->h, 1);
    if (!*weiss || (schwarz && !*schwarz)) { if (*weiss) BmpFree(*weiss); if (schwarz && *schwarz) BmpFree(*schwarz); return 0; }
    w = (BYTE FAR *)MemLock(*weiss) + 8; s = schwarz ? (BYTE FAR *)MemLock(*schwarz) + 8 : 0;
    for (y = 0; y < k->h; y++, z += sb, mk += db, w += db, s += schwarz ? db : 0)
        for (c = 0; c < k->w; c++) {
            if (k->bpp == 1) v = (z[c >> 3] >> (7 - (c & 7))) & 1;
            else {
                b = k->bpp == 8 ? z[c] : (z[c >> 1] >> (c & 1 ? 0 : 4)) & 15;
                v = (pal[3 * b] * 3 + pal[3 * b + 1] * 6 + pal[3 * b + 2]) >= 1280;
            }
            b = !(k->flags & 1) || ((mk[c >> 3] >> (7 - (c & 7))) & 1);
            if (!schwarz) { if (v || !b) w[c >> 3] |= (BYTE)(0x80 >> (c & 7)); }   /* durchsichtig = hell */
            else if (b) { if (v) w[c >> 3] |= (BYTE)(0x80 >> (c & 7)); else s[c >> 3] |= (BYTE)(0x80 >> (c & 7)); }
        }
    MemUnlock(*weiss); if (schwarz) MemUnlock(*schwarz);
    return 1;
}
static WORD fromres(HINST i, WORD typ, WORD id, WORD cga, HBMP FAR *a, HBMP FAR *b)
{
    HMEM r = ResLoad(i, typ, id); BYTE FAR *p; WORD ok = 0;
    if (!r) return 0;
    if ((p = (BYTE FAR *)MemLock(r)) != 0) { ok = rdbmake(p, (WORD)MemSize(r), cga, a, b); MemUnlock(r); }
    ResFree(r);
    return ok;
}
static HBMP RDEXPORT x_BmpLoad(HINST i, WORD id) { HBMP b = 0; fromres(i, RT_BITMAP, id, 0, &b, 0); return b; }
static HBMP RDEXPORT x_BmpLoadFile(LPCSTR name)
{
    short f = FileOpen(name, FO_READ | FO_DENYNONE); long n; HMEM m = 0; BYTE FAR *p; HBMP b = 0;
    if (f < 0) return 0;
    n = FileSeek(f, 0, 2);
    if (n <= 0 || n > 0xFFF0L) SetLastError(RD_ERR_FORMAT);
    else if ((m = MemAlloc(n, MEM_MOVABLE)) != 0 && (p = (BYTE FAR *)MemLock(m)) != 0) {
        FileSeek(f, 0, 0);
        if (FileRead(f, p, (WORD)n) == (WORD)n) rdbmake(p, (WORD)n, 0, &b, 0);
        MemUnlock(m);
    }
    if (m) MemFree(m);
    FileClose(f);
    return b;
}
/* Symbole bleiben nach dem ersten Zeichnen geladen (8 Stueck, danach das aelteste weg). CGA-Fassung,
   wenn der Schirm hoechstens 200 Zeilen hat. */
static struct { HINST i; WORD id, cga; HBMP w, s; } ic[8]; static WORD icn;
static void RDEXPORT x_IconDraw(HDC dc, short x, short y, HINST i, WORD id)
{
    WORD k, cga = GetCaps(0, CAPS_H) <= 200; HDC s; COLOR alt; WORD FAR *bh, bw, bhh;
    for (k = 0; k < 8; k++)
        if (ic[k].w && ic[k].i == i && ic[k].id == id && ic[k].cga == cga && MemSize(ic[k].w)) break;
    if (k == 8) {
        k = icn++ & 7;
        if (ic[k].w) { BmpFree(ic[k].w); BmpFree(ic[k].s); ic[k].w = 0; }
        if (!fromres(i, RT_ICON, id, cga, &ic[k].w, &ic[k].s)) { ic[k].w = 0; return; }
        ic[k].i = i; ic[k].id = id; ic[k].cga = cga;
    }
    bh = (WORD FAR *)MemLock(ic[k].w); bw = bh[0]; bhh = bh[1]; MemUnlock(ic[k].w);   /* Kopf: w, h, bpp, bpr */
    alt = SetTextColor(dc, C_WHITE);
    if ((s = BmpDC(ic[k].w)) != 0) { BitBlt(dc, x, y, bw, bhh, s, 0, 0, ROP_MASK); DCFree(s); }
    SetTextColor(dc, C_BLACK);
    if ((s = BmpDC(ic[k].s)) != 0) { BitBlt(dc, x, y, bw, bhh, s, 0, 0, ROP_MASK); DCFree(s); }
    SetTextColor(dc, alt);
}

static void RDEXPORT k_ende(void) { }
/* ---- Netz: erst beim ersten NetStart RDTCP.MOD laden; es traegt sich selbst in die API-Tabelle ein ---- */
extern FARPROC FAR *_rd2api;
static WORD RDEXPORT k_NetStart(HWND w)
{
    FARPROC f = _rd2api[0x381];
    if (!ModLoad("RDTCP.MOD") || _rd2api[0x381] == f) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }
    return ((WORD (RDAPI *)(HWND))_rd2api[0x381])(w);
}
/* ---- Drucken: RDPRN.MOD je Aufruf laden (haelt sich selbst von PrnOpen bis PrnClose/PrnAbort) ---- */
static WORD prn(WORD nr, HWND w, PRNJOB FAR *j)    /* RDPRN-Exporte nehmen alle (HWND bzw. HDC, PRNJOB FAR *) */
{
    HMOD m = ModLoad("RDPRN.MOD"); FARPROC f; WORD r = 0;
    if (!m) return 0;                             /* LastError von ModLoad */
    if ((f = ModProc(m, nr)) != 0) r = ((WORD (RDAPI *)(HWND, PRNJOB FAR *))f)(w, j);
    ModFree(m);
    return r;
}
static HDC  RDEXPORT k_PrnOpen(HWND w, PRNJOB FAR *j) { return prn(2, w, j); }
static WORD RDEXPORT k_PrnPage(HDC dc) { return prn(3, dc, 0); }
static WORD RDEXPORT k_PrnClose(HDC dc) { return prn(4, dc, 0); }
static void RDEXPORT k_PrnAbort(HDC dc) { prn(5, dc, 0); }
static struct { WORD n; FARPROC f[17]; } ext = { 17, {
    (FARPROC)k_ende, (FARPROC)k_TimeGet, (FARPROC)k_TimeSet, (FARPROC)k_LangGet, (FARPROC)k_ResLoad,
    (FARPROC)k_ResFree, (FARPROC)k_StrLoad, (FARPROC)k_StrGet, (FARPROC)k_DbgBreak,
    (FARPROC)x_BmpLoad, (FARPROC)x_BmpLoadFile, (FARPROC)x_IconDraw, (FARPROC)k_NetStart,
    (FARPROC)k_PrnOpen, (FARPROC)k_PrnPage, (FARPROC)k_PrnClose, (FARPROC)k_PrnAbort } };
void FAR * RDAPI RdModInit(void) { return &ext; }
