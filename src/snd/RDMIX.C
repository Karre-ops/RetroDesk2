/* RDMIX.C - Mischer fuer RDSND (SndMix*, API 127h-12Bh, PLAN_API 4). Wird beim ersten SndMix* geladen, danach bis zum
 * Ende von RD2 (oder SndSamplePlay) behalten. Braucht einen Treiber mit Ringpuffer (strm_start, SC_STREAM: Sound Blaster).
 * 8 Kanaele, je Kanal ein Block des Programms (8 Bit vorzeichenlos oder SM_16BIT vorzeichenbehaftet, mono, bis 4 GB lang),
 * eigene Abtastrate (Schritt 16.16), Lautstaerke 0..127 und Lage 0 (links)..64 (Mitte)..127 (rechts), SM_LOOP.
 * Ausgabe nach Karte: SB16 16 Bit stereo, Pro 8 Bit stereo, sonst 8 Bit mono; Rate [System] MixRate= (Vorgabe 22050 ab SB16,
 * sonst 11025), SndMixOut aendert sie. Der Interrupt der Karte fuellt die gespielte Ringhaelfte (gesperrt, rund 1/64 s je
 * Haelfte); Kanalende meldet input_poll (RDSND s_Pause 2) als M_SOUNDDONE p1 = 4, p2 = Kanal. Ohne Kanal endet der Ring. */
#include <i86.h>
#include <string.h>
#include "RD2DRV.H"

#define NK 8
#define MAXF 704                                  /* Abtastwerte je Haelfte hoechstens (44100 / 64) */
typedef struct {
    DWORD pos, ende, anf;                         /* lineare Adressen (Bytes) */
    WORD fr, si, sf;                              /* Nachkomma der Lage, Schritt ganz/Nachkomma */
    BYTE an, loop, b16, vm, vl, vr, fertig;
    HMEM h; HWND w; HTASK t;
} KANAL;
static KANAL k[NK];
static SNDDRV FAR *dv; static int acc[MAXF * 2];
static HMEM rh; static BYTE FAR *ring; static WORD orate, ofmt, nf, hb; static volatile BYTE laeuft;   /* nf Abtastwerte, hb Bytes je Haelfte */
static WORD aus(void);
#pragma aux aus = "pushf" "cli" "pop ax" value [ax]
static void ein(WORD f);
#pragma aux ein = "push ax" "popf" parm [ax]

static void kanal(KANAL *c, WORD n)               /* einen Kanal in acc mischen (n Abtastwerte) */
{
    int *a = acc; WORD i = 0, o, f, sf = c->sf, si = c->si, lim, st = (ofmt & SF_STEREO) ? 2 : 1;
    int vm = c->vm, vl = c->vl, vr = c->vr;
    while (i < n && c->an) {
        DWORD rest = c->ende - c->pos;
        BYTE __far *p = (BYTE __far *)MK_FP((WORD)(c->pos >> 4), (WORD)c->pos & 15);
        o = 0; f = c->fr;
        lim = rest > 0xFF00UL ? 0xFF00U : (WORD)rest;
        if (c->b16) {
            int __far *q = (int __far *)p; lim >>= 1;
            for (; i < n && o < lim; i++) {
                int v = (int)(((long)q[o] * vm) >> 10), w;
                if (st == 2) { w = (int)(((long)q[o] * vl) >> 10); a[2 * i] += w; a[2 * i + 1] += (int)(((long)q[o] * vr) >> 10); } else a[i] += v;
                w = f; f += sf; o += si + (f < (WORD)w);
            }
            c->pos += (DWORD)o << 1;
        } else {
            for (; i < n && o < lim; i++) {
                int s = (int)p[o] - 128; WORD w;
                if (st == 2) { a[2 * i] += (s * vl) >> 2; a[2 * i + 1] += (s * vr) >> 2; } else a[i] += (s * vm) >> 2;
                w = f; f += sf; o += si + (f < w);
            }
            c->pos += o;
        }
        c->fr = f;
        if (c->pos >= c->ende) {
            if (c->loop) c->pos = c->anf + (c->pos - c->ende) % (c->ende - c->anf);
            else { c->an = 0; c->fertig = 1; }
        }
    }
}
static void fuell(WORD h)                         /* Ringhaelfte h neu mischen */
{
    WORD i, n = nf * ((ofmt & SF_STEREO) ? 2 : 1);
    memset(acc, 0, n * sizeof(int));
    for (i = 0; i < NK; i++) if (k[i].an) kanal(&k[i], nf);
    if (ofmt & SF_16BIT) {
        int FAR *z = (int FAR *)(ring + h * hb);
        for (i = 0; i < n; i++) { int v = acc[i]; z[i] = v > 4095 ? 32767 : v < -4096 ? -32768 : v << 3; }
    } else {
        BYTE FAR *z = ring + h * hb;
        for (i = 0; i < n; i++) { int v = acc[i] >> 5; z[i] = (BYTE)((v > 127 ? 127 : v < -128 ? -128 : v) + 128); }
    }
}
static void RDEXPORT halbe(WORD h) { fuell(h); }
static void ringaus(void)
{
    WORD f = aus(); if (laeuft) dv->strm_stop(); laeuft = 0; ein(f);
    if (rh) { MemUnlock(rh); MemFree(rh); rh = 0; ring = 0; }
}
static WORD ringan(void)                          /* Ring anlegen (DMA-tauglich) und starten */
{
    WORD r, f, b = ((ofmt & SF_16BIT) ? 2 : 1) * ((ofmt & SF_STEREO) ? 2 : 1); DWORD a, g = (ofmt & SF_16BIT) ? 0x20000UL : 0x10000UL;
    nf = (orate / 64 + 7) & ~7; if (nf < 32) nf = 32; if (nf > MAXF) nf = MAXF;
    hb = nf * b;
    if (!(rh = MemAlloc(4UL * hb + 16, MEM_FIXED | MEM_NOEMS))) return RD_ERR_NOMEM;
    ring = (BYTE FAR *)MemLock(rh);
    a = ((DWORD)FP_SEG(ring) << 4) + FP_OFF(ring); a = (a + 1) & ~1UL;
    if ((a & (g - 1)) + 2UL * hb > g) a = (a | (g - 1)) + 1;   /* naechste DMA-Seite: Block ist doppelt so gross */
    ring = (BYTE FAR *)MK_FP((WORD)(a >> 4), (WORD)a & 15);
    fuell(0); fuell(1);
    f = aus(); r = dv->strm_start(ring, 2 * hb, orate, ofmt, halbe); if (!r) laeuft = 1; ein(f);
    if (r) { MemUnlock(rh); MemFree(rh); rh = 0; ring = 0; }
    return r;
}
static void frei(KANAL *c) { if (c->h) { MemUnlock(c->h); c->h = 0; } c->an = 0; c->fertig = 0; }
static void RDEXPORT m_stop(WORD n)               /* 0 = alle (und Ring aus) */
{
    WORD i, f;
    for (i = 0; i < NK; i++) if (!n || n == i + 1) { f = aus(); k[i].an = 0; ein(f); frei(&k[i]); }
    if (!n) ringaus();
}
static WORD RDEXPORT m_out(WORD rate, WORD fmt)   /* Ausgabe waehlen; laufender Ring startet neu */
{
    WORD c = dv->caps(), r = 0;
    if (rate < 4000 || rate > 44100U) return RD_ERR_HANDLE;
    if (!(c & SC_PCM16)) fmt &= ~SF_16BIT;
    if (!(c & SC_STEREO)) fmt &= ~SF_STEREO;
    if (!(c & SC_PCM16) && rate > ((fmt & SF_STEREO) ? 22050U : 44100U)) rate = (fmt & SF_STEREO) ? 22050U : 44100U;
    if (laeuft || rh) { ringaus(); orate = rate; ofmt = fmt; r = ringan(); } else { orate = rate; ofmt = fmt; }
    return r;
}
static WORD RDEXPORT m_init(SNDDRV FAR *d, WORD rate)
{
    WORD c;
    if (d->h.groesse < sizeof(SNDDRV) || !d->strm_start || !((c = d->caps()) & SC_STREAM)) return RD_ERR_NOTSUP;
    dv = d;
    if (!rate) rate = (c & SC_PCM16) ? 22050 : 11025;
    return m_out(rate, SF_16BIT | SF_STEREO);
}
static void lage(KANAL *c, WORD l, WORD p)
{
    WORD f;
    if (l > 127) l = 127;
    if (p > 127) p = 127;
    f = aus();
    c->vm = (BYTE)l;
    c->vl = (BYTE)(p <= 64 ? l : (DWORD)l * (127 - p) / 63);
    c->vr = (BYTE)(p >= 64 ? l : (DWORD)l * p / 64);
    ein(f);
}
static WORD RDEXPORT m_set(WORD n, WORD l, WORD p) { if (!n || n > NK) return RD_ERR_HANDLE; lage(&k[n - 1], l, p); return 0; }
static WORD RDEXPORT m_play(HWND w, HMEM d, WORD rate, WORD fl)   /* Kanal 1..8, 0 = Fehler (SetLastError) */
{
    WORD i, f, r; KANAL *c; BYTE FAR *p; DWORD n, st;
    for (i = 0; i < NK && (k[i].an || k[i].h); i++) ;
    if (i == NK) { SetLastError(RD_ERR_BUSY); return 0; }
    if (rate < 1000 || rate > 48000U) { SetLastError(RD_ERR_HANDLE); return 0; }
    c = &k[i];
    if (!(p = (BYTE FAR *)MemLock(d)) || (n = MemSize(d)) < 2) { if (p) MemUnlock(d); SetLastError(RD_ERR_HANDLE); return 0; }
    if (fl & SM_16BIT) n &= ~1UL;
    st = ((DWORD)rate << 16) / orate;
    c->h = d; c->w = w; c->t = TaskSelf(); c->b16 = (BYTE)((fl & SM_16BIT) != 0); c->loop = (BYTE)(fl & SM_LOOP);
    c->anf = ((DWORD)FP_SEG(p) << 4) + FP_OFF(p); c->pos = c->anf; c->ende = c->anf + n; c->fr = 0;
    c->si = (WORD)(st >> 16); c->sf = (WORD)st; c->fertig = 0;
    lage(c, 127, 64);
    f = aus(); c->an = 1; ein(f);
    if (!laeuft && (r = ringan()) != 0) { frei(c); SetLastError(r); return 0; }
    return i + 1;
}
static WORD RDEXPORT m_state(WORD n)              /* 0: Bits der laufenden Kanaele, sonst 1 = laeuft */
{
    WORD i, b = 0;
    for (i = 0; i < NK; i++) if (k[i].an) b |= 1 << i;
    return n ? (n <= NK && (b >> (n - 1)) & 1) : b;
}
static void RDEXPORT m_poll(void)                 /* input_poll (Task): Kanalenden melden, ohne Kanal Ring aus */
{
    WORD i, b = 0;
    for (i = 0; i < NK; i++) {
        KANAL *c = &k[i];
        if (c->fertig) { HWND w = c->w; HTASK t = c->t; frei(c); if (w) PostMessage(w, M_SOUNDDONE, 4, i + 1); else PostTask(t, M_SOUNDDONE, 4, i + 1); }
        b |= c->an;
    }
    if (!b && laeuft) ringaus();
}
static struct { FARPROC i, p, s, x, o, z, l; } tab = { (FARPROC)m_init, (FARPROC)m_play, (FARPROC)m_set, (FARPROC)m_stop, (FARPROC)m_out, (FARPROC)m_state, (FARPROC)m_poll };
FARPROC RDEXPORT MixTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)MixTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
