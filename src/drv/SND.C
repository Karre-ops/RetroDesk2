/* SND.C - Klangtreiber PC-Lautsprecher (-DSPK), AdLib/OPL2 an 388h (-DADL), Sound Blaster (-DSB) und Covox (-DCOVOX).
 * Noten als MIDI-Nummer (69 = A4 = 440 Hz). Lautsprecher: einstimmig, letzte Note gewinnt.
 * AdLib: 9 Kanaele, Pieptoene auf Kanal 8 mit eigenem Glockenklang (wie RD1). Der Aufrufer sperrt Interrupts.
 * Sound Blaster 1.x bis AWE und Kompatible: FM wie AdLib an 388h (alle SB spiegeln dorthin, OPL3: zweite Bank
 * an 38Ah, OPL3-Modus mit Kanaelen 9..17), PCM 8 Bit mono per DMA (Einzelblock, Befehl 14h; RDSND teilt an 64-KB-Grenzen),
 * Ring fuer den Mischer RDMIX (strm: auto-init ab DSP 2.00, Stereo ab Pro, 16 Bit ab SB16 ueber den hohen DMA-Kanal). Ohne FM-Chip
 * gehen Pieptoene und Noten ueber den Lautsprecher. Lautsprecher und Covox spielen Samples ueber den Zeitgeber: PIT-Kanal 0 laeuft
 * mit der Abtastrate, der Interrupt gibt ein Byte aus (Lautsprecher: Pulsbreite ueber Kanal 2 im Modus 0, Covox: Byte an den
 * Druckerport) und ruft den Kern-Takt (145,6 Hz) im alten Rhythmus weiter. Covox: init(param) Bits 0-1 LPT-Nummer - 1;
 * Pieptoene und Noten ueber den Lautsprecher. SB init(param): Bits 0-3 Port (2..8 = 220h..280h),
 * 4-7 IRQ, 8-10 DMA+1, 11-13 hoher DMA (H5..H7) - 3; 0 = Vorgabe A220 I5 D1 (H5 auf SB16). Bit 15 (alle Treiber): nur FM-Chip neu setzen (nach Musik). */
#include <conio.h>
#include <i86.h>
#include "RD2DRV.H"

static const WORD okt[12] = { 33488, 35479, 37589, 39824, 42192, 44701, 47359, 50175, 53159, 56320, 59669, 63217 };
/* okt: Hz x 4 fuer MIDI 120..131; Oktave o (MIDI 12*o..) = okt >> (12 - o) */
static WORD hz(WORD n) { return okt[n % 12] >> (12 - n / 12); }   /* n <= 127: Oktave <= 10 */

#pragma intrinsic(inp, outp)
#if defined(SPK) || defined(SB) || defined(COVOX)
static WORD teil(WORD f);                         /* 1193182 / f (f >= 19): DIV statt Bibliotheksroutine */
#pragma aux teil = "mov dx, 12h" "mov ax, 34DEh" "div bx" parm [bx] value [ax] modify [dx]
static void spk(WORD f)
{
    WORD d;
    if (f < 20) { outp(0x61, inp(0x61) & 0xFC); return; }
    d = teil(f);
    outp(0x43, 0xB6); outp(0x42, d & 0xFF); outp(0x42, d >> 8);
    outp(0x61, inp(0x61) | 3);
}
#endif
#if defined(SPK) || defined(COVOX)
/* Samples ueber den Zeitgeber (Kanal 0 mit der Abtastrate, Kern-Takt weiter im alten Rhythmus) */
static void (__interrupt __far *alt8)(void);
static void (RDAPI *weiter)(void);
static const BYTE FAR * volatile tp; static volatile WORD tn; static WORD tdiv, tacc; static volatile BYTE takt;
#ifdef SPK
static BYTE skal[256];                            /* Sample -> Zaehlwert fuer Kanal 2 (Pulsbreite) */
#define AUSGABE(v) outp(0x42, skal[v])
#else
static WORD lpt;
#define AUSGABE(v) outp(lpt, v)
#endif
static void pit(WORD d) { outp(0x43, 0x36); outp(0x40, d & 0xFF); outp(0x40, d >> 8); }
static void __interrupt __far tirq(void);
static void tstop(void)                           /* gesperrt: Kern-Takt wieder allein */
{
    if (!takt) return;
    takt = 0; tn = 0; weiter = 0;
    if (*(void (__interrupt __far * __far *)(void))MK_FP(0, 8 * 4) == tirq) {   /* nur wenn noch eingehaengt: nach rd_timer_off (Ende, RunDos) nicht zurueck auf 145,6 Hz (Audit 24.11.) */
        pit(0x2000);                              /* Teiler 8192 = 145,6 Hz wie der Kern (KASM rd_timer_on) */
        *(void (__interrupt __far * __far *)(void))MK_FP(0, 8 * 4) = alt8;
    }
#ifdef SPK
    outp(0x61, inp(0x61) & 0xFC);
#else
    outp(lpt, 0x80);
#endif
}
static void __interrupt __far tirq(void)
{
    if (tn) {
        AUSGABE(*tp); tp++;
        if (!--tn) { if (weiter) weiter(); if (!tn) tstop(); }   /* RDSND: naechster Block oder Ende */
    }
    tacc += tdiv;
    if (tacc < tdiv) alt8();                      /* 65536 = ein Kern-Takt vergangen: Kern (sendet EOI) */
    else outp(0x20, 0x20);
}
static WORD RDEXPORT d_pcm(const BYTE FAR *b, WORD n, WORD rate, void (RDAPI *f)(void))
{
    WORD d;
    if (!n || rate < 4000 || rate > 22050) return RD_ERR_HANDLE;
    weiter = f;
    if (takt && *(void (__interrupt __far * __far *)(void))MK_FP(0, 8 * 4) == tirq) { tp = b; tn = n; return 0; }   /* Folgeblock (aus dem Interrupt): Rate bleibt */
    takt = 0;                                     /* nach RunDos ausgehaengt: neu einhaengen */
    d = teil(rate); tdiv = d << 3; tacc = 0;      /* 65536 / 8 = 8192 Zaehler je Kern-Interrupt */
#ifdef SPK
    { WORD i, m = d - 2 < 254 ? d - 2 : 254;
      for (i = 0; i < 256; i++) skal[i] = (BYTE)(1 + ((i * m) >> 8)); }   /* 255 * 254 passt in WORD */
    outp(0x43, 0x90); outp(0x42, 1); outp(0x61, inp(0x61) | 3);   /* Kanal 2: Modus 0, nur Low-Byte */
#endif
    tp = b; tn = n;
    alt8 = *(void (__interrupt __far * __far *)(void))MK_FP(0, 8 * 4);
    *(void (__interrupt __far * __far *)(void))MK_FP(0, 8 * 4) = tirq;
    takt = 1; pit(d);
    return 0;
}
static void RDEXPORT d_pcmstop(void) { tstop(); }
#define D_PCM d_pcm, d_pcmstop
#endif

#if defined(SPK) || defined(COVOX)
#ifdef SPK
#define NAME "PC-Lautsprecher"
#define BELEGT takt                               /* Sample laeuft: Kanal 2 gehoert ihm */
#else
#define NAME "Covox"
#define BELEGT 0
#endif
static WORD kn = 0xFFFF;
#ifdef SPK
static WORD RDEXPORT d_init(WORD p) { (void)p; spk(0); return 0; }
static WORD RDEXPORT d_caps(void) { return SC_SPEAKER | SC_PCM; }
#else
static WORD RDEXPORT d_init(WORD p)
{
    if (p & 0x8000) return 0;
    lpt = *(WORD FAR *)MK_FP(0x40, 8 + 2 * (p & 3));   /* BIOS-Datenbereich: Druckerports */
    if (!lpt) return RD_ERR_NOTFOUND;
    outp(lpt, 0x80); spk(0); return 0;
}
static WORD RDEXPORT d_caps(void) { return SC_SPEAKER | SC_PCM; }
#endif
static void RDEXPORT d_done(void) { tstop(); spk(0); }
static void RDEXPORT d_beep(WORD f, WORD ms) { if (BELEGT) return; kn = 0xFFFF; spk(ms ? f : 0); }
static WORD RDEXPORT d_note(WORD k, WORD n, WORD v) { if (BELEGT) return 0; if (!v) { if (k == kn) spk(0); return 0; } kn = k; spk(hz(n)); return 0; }
static void RDEXPORT d_noteoff(WORD k) { if (BELEGT) return; if (k == kn) { spk(0); kn = 0xFFFF; } }
static WORD RDEXPORT d_instr(WORD k, const INSTR FAR *i) { (void)k; (void)i; return 0; }
#define D_REG 0
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
#ifdef SB
static BYTE opl3;                                 /* OPL3: Modus an, Kanaele 9..17 in Bank 2 (38Ah), C0h mit Ausgang links+rechts */
#define NK (opl3 ? 18 : 9)
#define LR (opl3 ? 0x30 : 0)
#else
#define NK 9
#define LR 0
#endif
static void oplk(WORD k, BYTE r, BYTE v) { oplp(k >= 9 ? 0x38A : 0x388, r, v); }   /* Register r des Kanals k (Bank nach k) */
static const BYTE opo[9] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };  /* Modulator je Kanal, Traeger +3 */
static BYTE b0[18], tl[18];                         /* B0h je Kanal (Key-On), Traeger-Pegel 40h des Klangs */
static const INSTR glocke = { { 0x01, 0x18, 0xF4, 0x55, 0 }, { 0x01, 0x00, 0xF4, 0x55, 0 }, 0x06, 0 };
static WORD RDEXPORT d_instr(WORD k, const INSTR FAR *i)
{
    BYTE o; WORD j; static const BYTE rg[5] = { 0x20, 0x40, 0x60, 0x80, 0xE0 };
    if (k >= NK) return RD_ERR_HANDLE;
    o = opo[k % 9];
    for (j = 0; j < 5; j++) { oplk(k, (BYTE)(rg[j] + o), i->mod[j]); oplk(k, (BYTE)(rg[j] + o + 3), i->car[j]); }
    oplk(k, (BYTE)(0xC0 + k % 9), (BYTE)(i->feedback | LR)); tl[k] = i->car[1];
    return 0;
}
static void spiel(WORD k, WORD f, WORD v)         /* f in Hz, v 0..127 */
{
    DWORD fn; WORD b = 0, c = k % 9; BYTE p, t = (BYTE)(tl[k] & 0x3F);
    oplk(k, (BYTE)(0xB0 + c), (BYTE)(b0[k] & 0x1F));
    if (f < 20 || !v) return;
    do { fn = (DWORD)f * (1UL << (20 - b)) / 49716UL; if (fn < 1024) break; } while (++b < 8);
    p = (BYTE)(t + ((127 - v) >> 1)); if (p > 63) p = 63;         /* leiser: Traeger daempfen */
    oplk(k, (BYTE)(0x43 + opo[c]), (BYTE)((tl[k] & 0xC0) | p));
    oplk(k, (BYTE)(0xA0 + c), (BYTE)fn);
    b0[k] = (BYTE)(0x20 | (b << 2) | ((fn >> 8) & 3));
    oplk(k, (BYTE)(0xB0 + c), b0[k]);
}
static WORD da(void)                              /* OPL2-Zeitgeber pruefen (wie RD1 und RD2SETUP) */
{
    BYTE s1, s2; WORD i;
    opl(4, 0x60); opl(4, 0x80); s1 = (BYTE)inp(0x388);
    opl(2, 0xFF); opl(4, 0x21); for (i = 0; i < 200; i++) inp(0x388);
    s2 = (BYTE)inp(0x388); opl(4, 0x60); opl(4, 0x80);
    return (s1 & 0xE0) == 0 && (s2 & 0xE0) == 0xC0;
}
static void still(void) { WORD k; for (k = 0; k < NK; k++) { oplk(k, (BYTE)(0xB0 + k % 9), 0); b0[k] = 0; } }
static void fmreset(void)
{
    WORD r;
    for (r = 0x01; r <= 0xF5; r++) { opl((BYTE)r, 0); if (NK > 9 && r != 5) oplp(0x38A, (BYTE)r, 0); }
    opl(0x01, 0x20);                              /* Wellenformen an */
    for (r = 0; r < NK; r++) d_instr(r, &glocke);
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
/* DSP-Fassung (E1h): 1.x Einzelbloecke, ab 2.00 Ringpuffer (auto-init), ab 3.00 (Pro) Stereo ueber Mischer 0Eh und
   High-Speed, ab 4.00 (SB16/AWE) Befehle Bxh/Cxh mit eigener Rate (41h), 16 Bit ueber den hohen DMA-Kanal (H). */
static WORD sp = 0x220, irq = 5, dma = 1, hdma, caps, dver; static BYTE fm, altmask, vec, kn = 0xFF;
static void (__interrupt __far *altvec)(void);
static void (RDAPI *weiter)(void);
static void (RDAPI *halbe)(WORD h); static BYTE sfmt, shf; static WORD shn;   /* Ring: Format, naechste Haelfte, Zaehler je Haelfte - 1 */
static BYTE dspw(BYTE v) { WORD i; for (i = 0; i < 0xFFFF; i++) if (!(inp(sp + 0x0C) & 0x80)) { outp(sp + 0x0C, v); return 1; } return 0; }
static WORD dspr(void) { WORD i; for (i = 0; i < 0xFFFF; i++) if (inp(sp + 0x0E) & 0x80) return inp(sp + 0x0A); return 0xFFFF; }
static void dspw3(BYTE c, WORD n) { dspw(c); dspw((BYTE)n); dspw((BYTE)(n >> 8)); }
static void mix(BYTE r, BYTE v) { outp(sp + 4, r); outp(sp + 5, v); }
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
    if (sfmt & SF_16BIT) inp(sp + 0x0F); else inp(sp + 0x0E);   /* 16- oder 8-Bit-DMA-Interrupt quittieren */
    if (irq >= 8) outp(0xA0, 0x20);
    outp(0x20, 0x20);
    if (halbe) {                                  /* Ring: Haelfte shf ist gespielt */
        WORD h = shf; shf ^= 1;
        if (dver < 0x200) dspw3(0x14, shn);       /* SB 1.x: kein auto-init, naechsten Block sofort */
        halbe(h);
    } else if (weiter) weiter();                  /* RDSND: naechster Block oder Ende */
}
static void dmaprog(DWORD a, WORD n, BYTE mode)  /* n Bytes ab a; mode 48h einzeln, 58h Ring */
{
    static const BYTE pg[8] = { 0x87, 0x83, 0x81, 0x82, 0, 0x8B, 0x89, 0x8A };
    WORD c = (sfmt & SF_16BIT) ? hdma : dma;
    if (c >= 4) {                                 /* 16 Bit: Adresse und Zaehler in Worten, Seite je 128 KB */
        WORD b = 0xC0 + ((c - 4) << 2), w = (WORD)(a >> 1); n = (n >> 1) - 1;
        outp(0xD4, 4 | (c - 4)); outp(0xD8, 0); outp(0xD6, mode | (c - 4));
        outp(b, (BYTE)w); outp(b, (BYTE)(w >> 8)); outp(pg[c], (BYTE)(a >> 16) & 0xFE);
        outp(b + 2, (BYTE)n); outp(b + 2, (BYTE)(n >> 8));
        outp(0xD4, c - 4);
    } else {
        n--;
        outp(0x0A, 4 | c); outp(0x0C, 0); outp(0x0B, mode | c);
        outp(c << 1, (BYTE)a); outp(c << 1, (BYTE)(a >> 8)); outp(pg[c], (BYTE)(a >> 16));
        outp((c << 1) + 1, (BYTE)n); outp((c << 1) + 1, (BYTE)(n >> 8));
        outp(0x0A, c);
    }
}
static void dmaaus(void) { if (hdma >= 4) outp(0xD4, 4 | (hdma - 4)); outp(0x0A, 4 | dma); }
static void RDEXPORT d_strmstop(void)
{
    if (!halbe && !weiter) return;
    halbe = 0; weiter = 0;
    if (dver >= 0x200) dspreset();                /* beendet auto-init und High-Speed sicher */
    else dspw(0xD0);
    dspw(0xD3); dmaaus();
    if (dver >= 0x300 && dver < 0x400) { outp(sp + 4, 0x0E); outp(sp + 5, inp(sp + 5) & ~2); }   /* Pro: Stereo aus */
    sfmt = 0;
}
#define d_pcmstop d_strmstop
static BYTE tk(WORD r) { return (BYTE)(256 - (WORD)(1000000UL / r)); }   /* Zeitkonstante (SB bis Pro) */
static WORD RDEXPORT d_pcm(const BYTE FAR *b, WORD n, WORD rate, void (RDAPI *f)(void))
{
    DWORD a = ((DWORD)FP_SEG(b) << 4) + FP_OFF(b);
    if (!n || rate < 4000 || rate > 22050 || halbe) return RD_ERR_HANDLE;   /* Ring laeuft: erst SndMixStop */
    if ((a & 0xFFFF) + n - 1 > 0xFFFF) return RD_ERR_HANDLE;   /* RDSND teilt an 64-KB-Seiten */
    if (!weiter) dspw(0xD1);                      /* Ausgang an (nur beim ersten Block) */
    weiter = f; sfmt = 0;
    dmaprog(a, n, 0x48);                          /* Einzelbetrieb, Speicher -> Karte */
    dspw(0x40); dspw(tk(rate));
    dspw3(0x14, n - 1);
    return 0;
}
/* Ring fuer den Mischer: n Bytes (zwei Haelften), DMA-tauglich (8 Bit: in einer 64-KB-Seite, 16 Bit: in 128 KB, gerade).
   fmt SF_16BIT (vorzeichenbehaftet) / SF_STEREO; f(h) im Interrupt (EOI gesendet, gesperrt), wenn Haelfte h gespielt ist. */
static WORD RDEXPORT d_strm(const BYTE FAR *b, WORD n, WORD rate, WORD fmt, void (RDAPI *f)(WORD h))
{
    DWORD a = ((DWORD)FP_SEG(b) << 4) + FP_OFF(b); WORD r = rate, h = n >> 1;
    if ((fmt & SF_16BIT) && !(caps & SC_PCM16)) return RD_ERR_NOTSUP;
    if ((fmt & SF_STEREO) && !(caps & SC_STEREO)) return RD_ERR_NOTSUP;
    if (fmt & SF_STEREO) r <<= 1;
    if (n < 16 || rate < 4000 || r > (dver >= 0x201 ? 44100U : 22050U) || halbe) return RD_ERR_HANDLE;
    if (weiter) d_strmstop();                     /* Einzelsample zu Ende (oder noch da): Karte frei machen */
    sfmt = (BYTE)fmt; shf = 0;
    if ((fmt & SF_16BIT) ? (a & 1) || (a & 0x1FFFF) + n - 1 > 0x1FFFF : (a & 0xFFFF) + n - 1 > 0xFFFF) { sfmt = 0; return RD_ERR_HANDLE; }
    halbe = f;
    dmaprog(a, n, 0x58);                          /* Ring (auto-init), Speicher -> Karte */
    dspw(0xD1);
    if (dver >= 0x400) {                          /* SB16: Rate direkt, Zaehler in Abtastwerten je Haelfte */
        dspw(0x41); dspw((BYTE)(rate >> 8)); dspw((BYTE)rate);
        shn = ((fmt & SF_16BIT) ? h >> 1 : h) - 1;
        dspw((fmt & SF_16BIT) ? 0xB6 : 0xC6); dspw((BYTE)(((fmt & SF_STEREO) ? 0x20 : 0) | ((fmt & SF_16BIT) ? 0x10 : 0)));
        dspw((BYTE)shn); dspw((BYTE)(shn >> 8));
        return 0;
    }
    shn = h - 1;
    if (fmt & SF_STEREO) { outp(sp + 4, 0x0E); outp(sp + 5, inp(sp + 5) | 2); }   /* Pro: Stereo */
    dspw(0x40); dspw(tk(r));
    if (dver < 0x200) dspw3(0x14, shn);           /* 1.x: Einzelblock, der Interrupt startet den naechsten */
    else { dspw3(0x48, shn); dspw(r > 22050 || (fmt & SF_STEREO) ? 0x90 : 0x1C); }   /* Blockgroesse, auto-init (High-Speed) */
    return 0;
}
static void RDEXPORT d_vol(WORD art, WORD w)      /* art 1 PCM, 2 Gesamt, w 0..127 (Mischer ab Pro) */
{
    BYTE v;
    if (w > 127 || (art != 1 && art != 2)) return;
    if (dver >= 0x400) { v = (BYTE)((w >> 2) << 3); mix((BYTE)(art == 1 ? 0x32 : 0x30), v); mix((BYTE)(art == 1 ? 0x33 : 0x31), v); }
    else if (dver >= 0x300) { v = (BYTE)(w >> 3); mix((BYTE)(art == 1 ? 0x04 : 0x22), (BYTE)((v << 4) | v)); }
}
static WORD RDEXPORT d_init(WORD p)
{
    void (__interrupt __far * __far *iv)(void); WORD pic, v;
    if (p & 0x8000) { if (fm) fmreset(); return 0; }   /* nach Musik: nur FM-Chip neu */
    if (p & 0x0F) sp = 0x200 + ((p & 0x0F) << 4);
    if (p & 0xF0) irq = (p >> 4) & 0x0F;
    if (p & 0x700) dma = ((p >> 8) & 7) - 1;
    if (p & 0x3800) hdma = ((p >> 11) & 7) + 3;   /* H5..H7 (+1 kodiert wie D) */
    if (dma > 3 || irq < 2 || irq > 15 || !dspreset()) return RD_ERR_NOTFOUND;
    dspw(0xE1); v = dspr(); dver = (v << 8) | (dspr() & 0xFF);
    if (v == 0xFFFF) dver = 0x100;
    caps = SC_PCM;
    if (dver >= 0x200) caps |= SC_STREAM;
    if (dver >= 0x300) caps |= SC_STEREO;
    if (dver >= 0x400) { if (!hdma) hdma = 5; caps |= SC_PCM16; }
    fm = (BYTE)da();
    if (fm) { opl3 = (BYTE)!(inp(0x388) & 0x06); caps |= opl3 ? SC_OPL2 | SC_OPL3 : SC_OPL2; if (opl3) oplp(0x38A, 5, 1); fmreset(); }
    else caps |= SC_SPEAKER;
    if (dver < 0x200) caps |= SC_STREAM;          /* 1.x: Ring ueber Einzelbloecke (kleine Luecke je Haelfte) */
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
    if (fm) { still(); if (opl3) oplp(0x38A, 5, 0); } else spk(0);
    outp(pic, inp(pic) | altmask); *(void (__interrupt __far * __far *)(void))MK_FP(0, vec * 4) = altvec;
}
static WORD RDEXPORT d_caps(void) { return caps; }
static void RDEXPORT d_beep(WORD f, WORD ms) { if (fm) spiel(8, ms ? f : 0, 127); else spk(ms ? f : 0); }
static WORD RDEXPORT d_note(WORD k, WORD n, WORD v)
{
    if (k >= NK) return RD_ERR_HANDLE;
    if (fm) spiel(k, hz(n), v); else if (v) { kn = (BYTE)k; spk(hz(n)); } else if (k == kn) spk(0);
    return 0;
}
static void RDEXPORT d_noteoff(WORD k) { if (k >= NK) return; if (fm) spiel(k, 0, 0); else if (k == kn) { spk(0); kn = 0xFF; } }
static void RDEXPORT d_reg(WORD r, BYTE v)
{
    if (!fm) return;
    if (opl3 && (r & 0xF0) == 0xC0) v |= 0x30;    /* OPL3-Modus: ohne Ausgangsbits still (IMF/DRO kennen sie nicht) */
    if (r & 0x100) { if (opl3 && (r & 0xFF) != 5) oplp(0x38A, (BYTE)r, v); } else opl((BYTE)r, v);
}
#define D_REG d_reg
#define D_PCM d_pcm, d_pcmstop
#define D_VOL d_vol
#define D_STRM d_strm, d_strmstop
#endif

#ifndef SB
#define D_VOL 0
#define D_STRM 0, 0
#endif
static SNDDRV tab = {
    { { 'R', 'D', 'R', 'V' }, DRV_SND, 0x0100, sizeof(SNDDRV), NAME, d_init, d_done },
    d_caps, d_beep, D_REG, d_note, d_noteoff, d_instr, D_PCM, D_VOL, 0, D_STRM
};
FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
