/* SND.C - Klangtreiber PC-Lautsprecher (-DSPK), AdLib/OPL2 an 388h (-DADL) und Sound Blaster (-DSB).
 * Noten als MIDI-Nummer (69 = A4 = 440 Hz). Lautsprecher: einstimmig, letzte Note gewinnt.
 * AdLib: 9 Kanaele, Pieptoene auf Kanal 8 mit eigenem Glockenklang (wie RD1). Der Aufrufer sperrt Interrupts.
 * Sound Blaster 1.x bis AWE und Kompatible: FM wie AdLib an 388h (alle SB spiegeln dorthin, OPL3: zweite Bank
 * an 38Ah), PCM 8 Bit mono per DMA (Einzelblock, Befehl 14h; RDSND teilt an 64-KB-Grenzen). Ohne FM-Chip
 * gehen Pieptoene und Noten ueber den Lautsprecher. init(param): Bits 0-3 Port (2..8 = 220h..280h),
 * 4-7 IRQ, 8-10 DMA+1; 0 = Vorgabe A220 I5 D1. Bit 15 (alle Treiber): nur FM-Chip neu setzen (nach Musik). */
#include <conio.h>
#include <i86.h>
#include "RD2DRV.H"

static const WORD okt[12] = { 33488, 35479, 37589, 39824, 42192, 44701, 47359, 50175, 53159, 56320, 59669, 63217 };
/* okt: Hz x 4 fuer MIDI 120..131; Oktave o (MIDI 12*o..) = okt >> (12 - o) */
static WORD hz(WORD n) { return okt[n % 12] >> (12 - n / 12); }   /* n <= 127: Oktave <= 10 */

#if defined(SPK) || defined(SB)
static void spk(WORD f)
{
    WORD d;
    if (f < 20) { outp(0x61, inp(0x61) & 0xFC); return; }
    d = (WORD)(1193182UL / f);
    outp(0x43, 0xB6); outp(0x42, d & 0xFF); outp(0x42, d >> 8);
    outp(0x61, inp(0x61) | 3);
}
#endif
#ifdef SPK
#define NAME "PC-Lautsprecher"
static WORD kn = 0xFFFF;
static WORD RDEXPORT d_init(WORD p) { (void)p; spk(0); return 0; }
static void RDEXPORT d_done(void) { spk(0); }
static WORD RDEXPORT d_caps(void) { return SC_SPEAKER; }
static void RDEXPORT d_beep(WORD f, WORD ms) { kn = 0xFFFF; spk(ms ? f : 0); }
static WORD RDEXPORT d_note(WORD k, WORD n, WORD v) { if (!v) { if (k == kn) spk(0); return 0; } kn = k; spk(hz(n)); return 0; }
static void RDEXPORT d_noteoff(WORD k) { if (k == kn) { spk(0); kn = 0xFFFF; } }
static WORD RDEXPORT d_instr(WORD k, const INSTR FAR *i) { (void)k; (void)i; return 0; }
#define D_REG 0
#define D_PCM 0, 0
#endif

#if defined(ADL) || defined(SB)
#ifdef ADL
#define NAME "AdLib OPL2"
#else
#define NAME "Sound Blaster"
#endif
static void oplp(WORD p, BYTE r, BYTE v)
{
    WORD i;
    outp(p, r); for (i = 0; i < 6; i++) inp(0x388);            /* 3,3 us */
    outp(p + 1, v); for (i = 0; i < 35; i++) inp(0x388);       /* 23 us (OPL3 braucht weniger) */
}
#define opl(r, v) oplp(0x388, r, v)
static const BYTE opo[9] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };  /* Modulator je Kanal, Traeger +3 */
static BYTE b0[9], tl[9];                         /* B0h je Kanal (Key-On), Traeger-Pegel 40h des Klangs */
static const INSTR glocke = { { 0x01, 0x18, 0xF4, 0x55, 0 }, { 0x01, 0x00, 0xF4, 0x55, 0 }, 0x06, 0 };
static WORD RDEXPORT d_instr(WORD k, const INSTR FAR *i)
{
    BYTE o; WORD j; static const BYTE rg[5] = { 0x20, 0x40, 0x60, 0x80, 0xE0 };
    if (k > 8) return RD_ERR_HANDLE;
    o = opo[k];
    for (j = 0; j < 5; j++) { opl((BYTE)(rg[j] + o), i->mod[j]); opl((BYTE)(rg[j] + o + 3), i->car[j]); }
    opl((BYTE)(0xC0 + k), i->feedback); tl[k] = i->car[1];
    return 0;
}
static void spiel(WORD k, WORD f, WORD v)         /* f in Hz, v 0..127 */
{
    DWORD fn; WORD b = 0; BYTE p, t = (BYTE)(tl[k] & 0x3F);
    opl((BYTE)(0xB0 + k), (BYTE)(b0[k] & 0x1F));
    if (f < 20 || !v) return;
    do { fn = (DWORD)f * (1UL << (20 - b)) / 49716UL; if (fn < 1024) break; } while (++b < 8);
    p = (BYTE)(t + ((127 - v) >> 1)); if (p > 63) p = 63;         /* leiser: Traeger daempfen */
    opl((BYTE)(0x43 + opo[k]), (BYTE)((tl[k] & 0xC0) | p));
    opl((BYTE)(0xA0 + k), (BYTE)fn);
    b0[k] = (BYTE)(0x20 | (b << 2) | ((fn >> 8) & 3));
    opl((BYTE)(0xB0 + k), b0[k]);
}
static WORD da(void)                              /* OPL2-Zeitgeber pruefen (wie RD1 und RD2SETUP) */
{
    BYTE s1, s2; WORD i;
    opl(4, 0x60); opl(4, 0x80); s1 = (BYTE)inp(0x388);
    opl(2, 0xFF); opl(4, 0x21); for (i = 0; i < 200; i++) inp(0x388);
    s2 = (BYTE)inp(0x388); opl(4, 0x60); opl(4, 0x80);
    return (s1 & 0xE0) == 0 && (s2 & 0xE0) == 0xC0;
}
static void still(void) { WORD k; for (k = 0; k < 9; k++) { opl((BYTE)(0xB0 + k), 0); b0[k] = 0; } }
static void fmreset(void)
{
    WORD r;
    for (r = 0x01; r <= 0xF5; r++) opl((BYTE)r, 0);
    opl(0x01, 0x20);                              /* Wellenformen an */
    for (r = 0; r < 9; r++) d_instr(r, &glocke);
}
#ifdef ADL
static WORD RDEXPORT d_init(WORD p) { if (!(p & 0x8000) && !da()) return RD_ERR_NOTFOUND; fmreset(); return 0; }
static void RDEXPORT d_done(void) { still(); }
static WORD RDEXPORT d_caps(void) { return SC_OPL2; }
static void RDEXPORT d_beep(WORD f, WORD ms) { spiel(8, ms ? f : 0, 127); }
static WORD RDEXPORT d_note(WORD k, WORD n, WORD v) { if (k > 8) return RD_ERR_HANDLE; spiel(k, hz(n), v); return 0; }
static void RDEXPORT d_noteoff(WORD k) { if (k <= 8) spiel(k, 0, 0); }
static void RDEXPORT d_reg(WORD r, BYTE v) { opl((BYTE)r, v); }
#define D_REG d_reg
#define D_PCM 0, 0
#endif
#endif

#ifdef SB
static WORD sp = 0x220, irq = 5, dma = 1, caps; static BYTE fm, opl3, altmask, vec, kn = 0xFF;
static void (__interrupt __far *altvec)(void);
static void (RDAPI *weiter)(void);
static BYTE dspw(BYTE v) { WORD i; for (i = 0; i < 0xFFFF; i++) if (!(inp(sp + 0x0C) & 0x80)) { outp(sp + 0x0C, v); return 1; } return 0; }
static WORD dspr(void) { WORD i; for (i = 0; i < 0xFFFF; i++) if (inp(sp + 0x0E) & 0x80) return inp(sp + 0x0A); return 0xFFFF; }
static WORD dspreset(void)
{
    WORD i;
    outp(sp + 6, 1); for (i = 0; i < 16; i++) inp(sp + 6);   /* >= 3 us */
    outp(sp + 6, 0);
    for (i = 0; i < 100; i++) if (dspr() == 0xAA) return 1;
    return 0;
}
static void __interrupt __far sbirq(void)
{
    inp(sp + 0x0E);                               /* 8-Bit-DMA-Interrupt quittieren */
    if (irq >= 8) outp(0xA0, 0x20);
    outp(0x20, 0x20);
    if (weiter) weiter();                         /* RDSND: naechster Block oder Ende */
}
static void RDEXPORT d_pcmstop(void)
{
    if (!weiter) return;
    weiter = 0;
    dspw(0xD0); dspw(0xD3);                       /* DMA anhalten, Ausgang aus */
    outp(0x0A, 4 | dma);                          /* Kanal sperren */
}
static WORD RDEXPORT d_pcm(const BYTE FAR *b, WORD n, WORD rate, void (RDAPI *f)(void))
{
    static const BYTE pg[4] = { 0x87, 0x83, 0x81, 0x82 };
    DWORD a = ((DWORD)FP_SEG(b) << 4) + FP_OFF(b);
    if (!n || rate < 4000 || rate > 22050) return RD_ERR_HANDLE;
    if ((a & 0xFFFF) + n - 1 > 0xFFFF) return RD_ERR_HANDLE;   /* RDSND teilt an 64-KB-Seiten */
    if (!weiter) dspw(0xD1);                      /* Ausgang an (nur beim ersten Block) */
    weiter = f; n--;
    outp(0x0A, 4 | dma); outp(0x0C, 0); outp(0x0B, 0x48 | dma);   /* Einzelbetrieb, Speicher -> Karte */
    outp(dma << 1, (BYTE)a); outp(dma << 1, (BYTE)(a >> 8)); outp(pg[dma], (BYTE)(a >> 16));
    outp((dma << 1) + 1, (BYTE)n); outp((dma << 1) + 1, (BYTE)(n >> 8));
    outp(0x0A, dma);
    dspw(0x40); dspw((BYTE)(256 - (WORD)(1000000UL / rate)));
    dspw(0x14); dspw((BYTE)n); dspw((BYTE)(n >> 8));
    return 0;
}
static WORD RDEXPORT d_init(WORD p)
{
    void (__interrupt __far * __far *iv)(void); WORD pic;
    if (p & 0x8000) { if (fm) fmreset(); return 0; }   /* nach Musik: nur FM-Chip neu */
    if (p & 0x0F) sp = 0x200 + ((p & 0x0F) << 4);
    if (p & 0xF0) irq = (p >> 4) & 0x0F;
    if (p & 0x700) dma = ((p >> 8) & 7) - 1;
    if (dma > 3 || irq < 2 || irq > 15 || !dspreset()) return RD_ERR_NOTFOUND;
    fm = (BYTE)da(); caps = SC_PCM;
    if (fm) { opl3 = (BYTE)!(inp(0x388) & 0x06); caps |= opl3 ? SC_OPL2 | SC_OPL3 : SC_OPL2; fmreset(); if (opl3) oplp(0x38A, 5, 0); }
    else caps |= SC_SPEAKER;
    vec = (BYTE)(irq < 8 ? 8 + irq : 0x68 + irq); pic = irq < 8 ? 0x21 : 0xA1;
    iv = (void (__interrupt __far * __far *)(void))MK_FP(0, vec * 4);
    altvec = *iv; *iv = sbirq;                    /* IRQ noch gesperrt, Aufrufer sperrt ohnehin */
    altmask = (BYTE)(inp(pic) & (1 << (irq & 7))); outp(pic, inp(pic) & ~(1 << (irq & 7)));
    if (irq >= 8) outp(0x21, inp(0x21) & ~4);
    return 0;
}
static void RDEXPORT d_done(void)
{
    WORD pic = irq < 8 ? 0x21 : 0xA1;
    d_pcmstop();
    if (fm) still(); else spk(0);
    outp(pic, inp(pic) | altmask); *(void (__interrupt __far * __far *)(void))MK_FP(0, vec * 4) = altvec;
}
static WORD RDEXPORT d_caps(void) { return caps; }
static void RDEXPORT d_beep(WORD f, WORD ms) { if (fm) spiel(8, ms ? f : 0, 127); else spk(ms ? f : 0); }
static WORD RDEXPORT d_note(WORD k, WORD n, WORD v)
{
    if (k > 8) return RD_ERR_HANDLE;
    if (fm) spiel(k, hz(n), v); else if (v) { kn = (BYTE)k; spk(hz(n)); } else if (k == kn) spk(0);
    return 0;
}
static void RDEXPORT d_noteoff(WORD k) { if (k > 8) return; if (fm) spiel(k, 0, 0); else if (k == kn) { spk(0); kn = 0xFF; } }
static void RDEXPORT d_reg(WORD r, BYTE v) { if (!fm) return; if (r & 0x100) { if (opl3) oplp(0x38A, (BYTE)r, v); } else opl((BYTE)r, v); }
#define D_REG d_reg
#define D_PCM d_pcm, d_pcmstop
#endif

static SNDDRV tab = {
    { { 'R', 'D', 'R', 'V' }, DRV_SND, 0x0100, sizeof(SNDDRV), NAME, d_init, d_done },
    d_caps, d_beep, D_REG, d_note, d_noteoff, d_instr, D_PCM, 0, 0
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
