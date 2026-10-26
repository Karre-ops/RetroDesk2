/* RDMUS.C - Musik-Spieler fuer RDSND (IMF Typ 0/1 mit 560/700/280 Hz, DRO 1.0 und 2.0). Wird nur zum Abspielen
 * geladen (fest, nicht verwerfbar) und danach wieder freigegeben: ohne Musik kostet er keinen Speicher.
 * Export 1 liefert eine Tabelle direkter Einspruenge (Takt laeuft im INT 8, dort keine Sprungstuecke).
 * Datei ganz im Speicher (MEM_SHARE, auch ueber 64 KB), Ereignisse im Kern-Takt (72,8 Hz, je Takt alle faelligen). */
#include <i86.h>
#include "RD2DRV.H"

#define M_IMF 1
#define M_DRO1 2
#define M_DRO2 3
static SNDDRV FAR *dv; static BYTE mart, mloop, kurz, lang, cmap[128];
static HMEM mh; static WORD mseg;
static DWORD mpos, mend, manf, minc, mvor, mwarte;

static BYTE hol(void) { BYTE __far *p = (BYTE __far *)MK_FP(mseg + (WORD)(mpos >> 4), (WORD)mpos & 15); mpos++; return *p; }
static void schreib(WORD r, BYTE v, BYTE hoch) { if (!hoch) dv->reg(r, v); else if (dv->caps() & SC_OPL3) dv->reg(r | 0x100, v); }
static void ruhig(void) { WORD k; for (k = 0; k < 9; k++) dv->reg(0xB0 + k, 0); }
static DWORD schritt(void)                        /* Ereignisse bis zur naechsten Pause, Rueckgabe Pause (0xFFFFFFFF = Ende) */
{
    BYTE r, v; WORD d; static BYTE hoch;
    for (;;) {
        if (mpos >= mend) return 0xFFFFFFFFUL;
        if (mart == M_IMF) {
            if (mpos + 4 > mend) return 0xFFFFFFFFUL;
            r = hol(); v = hol(); d = hol(); d |= (WORD)hol() << 8;
            schreib(r, v, 0); if (d) return d;
        } else if (mart == M_DRO2) {
            if (mpos + 2 > mend) return 0xFFFFFFFFUL;
            r = hol(); v = hol();
            if (r == kurz) return (DWORD)v + 1;
            if (r == lang) return ((DWORD)v + 1) << 8;
            schreib(cmap[r & 0x7F], v, (BYTE)(r & 0x80));
        } else {                                  /* DRO 1.0 */
            r = hol();
            if (r == 0) return (DWORD)hol() + 1;
            if (r == 1) { d = hol(); d |= (WORD)hol() << 8; return (DWORD)d + 1; }
            if (r == 2 || r == 3) { hoch = (BYTE)(r == 3); continue; }
            if (r == 4) r = hol();
            v = hol(); schreib(r, v, hoch);
        }
    }
}
static BYTE RDEXPORT m_takt(void)                /* 1 = zu Ende */
{
    WORD n = 0; DWORD d;
    if (!mart) return 1;
    mvor += minc;
    while (mart && mvor >= mwarte) {
        mvor -= mwarte;
        d = schritt();
        if (d == 0xFFFFFFFFUL) {
            if (mloop && manf < mend) { mpos = manf; d = 0; }
            else { ruhig(); mart = 0; return 1; }
        }
        if (d > 0xFFFF) d = 0xFFFF;
        mwarte = d << 16;
        if (++n > 400) { mvor = 0; break; }       /* Interrupt kurz halten */
    }
    return (BYTE)!mart;
}
static void mfrei(void) { if (mh) { MemUnlock(mh); MemFree(mh); mh = 0; } }
static void RDEXPORT m_stop(void) { mart = 0; if (dv) ruhig(); mfrei(); }   /* RDSND ruft den Takt da nicht mehr */
static WORD lw(DWORD o) { BYTE a, b; mpos = o; a = hol(); b = hol(); return a | ((WORD)b << 8); }
static DWORD ld(DWORD o) { WORD a = lw(o); return a | ((DWORD)lw(o + 2) << 16); }
static WORD RDEXPORT m_play(SNDDRV FAR *t, LPCSTR d, WORD fl)
{
    short f; long n; DWORD i; WORD k, r; BYTE __far *p; BYTE art; static const char sig[] = "DBRAWOPL";
    dv = t;
    if ((f = FileOpen(d, FO_READ)) < 0) return LastError() ? LastError() : RD_ERR_NOTFOUND;
    n = FileSeek(f, 0, 2); FileSeek(f, 0, 0);
    r = n < 8 ? RD_ERR_FORMAT : 0;
    if (!r && !(mh = MemAlloc((DWORD)n + 16, MEM_FIXED | MEM_SHARE | MEM_NOEMS))) r = RD_ERR_NOMEM;
    if (!r) { p = (BYTE __far *)MemLock(mh); mseg = FP_SEG(p) + (FP_OFF(p) >> 4); }
    for (i = 0; !r && i < (DWORD)n; i += k) {    /* in 32-KB-Stuecken, Zeiger normiert */
        k = (DWORD)n - i > 0x8000UL ? 0x8000 : (WORD)((DWORD)n - i);
        if (FileRead(f, MK_FP(mseg + (WORD)(i >> 4), (WORD)i & 15), k) != k) r = RD_ERR_DISK;
    }
    FileClose(f);
    if (!r) {
        mend = (DWORD)n; art = M_IMF; minc = 560UL * 900;   /* Takt 72,8 Hz: 65536 / 72,8 = 900 je Hz */
        if (fl & SM_IMF700) minc = 700UL * 900; else if (fl & SM_IMF280) minc = 280UL * 900;
        for (k = 0, mpos = 0; k < 8 && hol() == (BYTE)sig[k]; k++) ;
        if (k == 8) {
            minc = 1000UL * 900;                  /* DRO: Millisekunden */
            if (lw(8) == 2 && n >= 26) {
                art = M_DRO2; mpos = 23; kurz = hol(); lang = hol();   /* 23 kurze, 24 lange Pause, 25 Codetabelle */
                k = hol(); if (k > 128) k = 128;
                for (i = 0; i < k; i++) cmap[(WORD)i] = hol();
                manf = 26 + k;
                if (manf + ld(12) * 2 < mend) mend = manf + ld(12) * 2;   /* 12: Anzahl Paare */
            } else if (lw(8) == 0 && lw(10) == 1 && n >= 24) {
                art = M_DRO1; manf = 21;          /* Hardware ein Byte, aeltere Dateien vier Bytes */
                mpos = 21; if (hol() == 0 && hol() == 0 && hol() == 0) manf = 24;
                if (manf + ld(16) < mend) mend = manf + ld(16);
            } else r = RD_ERR_FORMAT;
        } else {                                  /* IMF Typ 1: Laenge vorneweg, Typ 0: ganze Datei */
            k = lw(0); manf = 0;
            if (k && (DWORD)k + 2 <= mend && !(k & 3)) { manf = 2; mend = (DWORD)k + 2; }
        }
    }
    if (r) { mfrei(); SetLastError(r); return r; }
    mloop = (BYTE)(fl & SM_LOOP); ruhig(); mpos = manf; mvor = 0; mwarte = 0;
    mart = art;                                   /* RDSND ruft den Takt erst nach der Rueckgabe */
    return 0;
}
static struct { FARPROC p, t, s; } tab = { (FARPROC)m_play, (FARPROC)m_takt, (FARPROC)m_stop };
FARPROC RDEXPORT MusTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)MusTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
