/* MIXTEST.C - Test fuer den Mischer SndMix* (RDMIX.MOD, SB.DRV-Ring), test/mischer.sh. Befehlszeile: Kartenart
 * 1 (SB 1.x), 2 (SB 2.0), P (Pro), 6 (SB16). Zwei Toene zugleich (440 Hz 8 Bit 11025 Hz 1 s, 1000 Hz 16 Bit 22050 Hz 0,5 s),
 * Endmeldungen zur rechten Zeit, Schleife, Stopp, acht Kanaele, Ausgabeformat, Wechsel mit SndSamplePlay. Die Toene
 * prueft test/mischer.sh in der Tonaufnahme (QEMU) per Fourier. */
#include "RD2.H"

static WORD fehler; static short df = -1;         /* MIX.TXT: DOSBox 0.74 hat keine COM1-Datei */
static void aus(LPCSTR z) { WORD n = 0; DbgPrint(z); if (df < 0) return; while (z[n]) n++; FileWrite(df, z, n); FileWrite(df, "\r\n", 2); }
static void hex(LPCSTR z, DWORD v)
{
    static char b[64]; WORD n = 0, i; DbgHex(z, v); if (df < 0) return;
    while (z[n] && n < 50) { b[n] = z[n]; n++; }
    b[n++] = ' '; for (i = 0; i < 8; i++) b[n++] = "0123456789ABCDEF"[(WORD)(v >> (28 - 4 * i)) & 15];
    b[n] = 0; FileWrite(df, b, n); FileWrite(df, "\r\n", 2);
}
static void pruef(LPCSTR was, int ok) { aus(was); if (!ok) { fehler++; aus("MIXTEST: ^^^ FALSCH"); } }
static DWORD t0;
static WORD ende(WORD max, WORD FAR *k)            /* naechste Mischer-Endmeldung: Kanal (0 = keine), *k unberuehrt bei keiner */
{
    MSG m; DWORD t = TickCount();
    while (TickCount() - t < max) {
        if (PeekMessage(&m, 1) && m.msg == M_SOUNDDONE) { if (m.p1 == 4) { *k = (WORD)m.p2; return (WORD)m.p2; } }
        TaskYield();
    }
    return 0;
}
static int sinus(WORD ph)                          /* ph 0..65535 -> -127..127 (Parabeln) */
{
    long x = (long)(ph & 0x7FFF) - 16384;          /* -16384..16383 je Halbwelle */
    int v = (int)(127 - (x * x * 127 / (16384L * 16384L)));
    return (ph & 0x8000) ? -v : v;
}
static HMEM ton(WORD hz, WORD rate, WORD n, BYTE b16)
{
    HMEM h = MemAlloc((DWORD)n * (b16 ? 2 : 1), MEM_MOVABLE); WORD i, ph = 0, st = (WORD)(((DWORD)hz << 16) / rate);
    if (!h) return 0;
    if (b16) { int FAR *p = (int FAR *)MemLock(h); for (i = 0; i < n; i++, ph += st) p[i] = sinus(ph) * 200; }
    else { BYTE FAR *p = (BYTE FAR *)MemLock(h); for (i = 0; i < n; i++, ph += st) p[i] = (BYTE)(sinus(ph) + 128); }
    MemUnlock(h); return h;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    char art = cmd && cmd[0] ? cmd[0] : '6'; WORD c = SndCaps(), k1, k2, k = 0, i, n; DWORD t, ta, tb;
    HMEM a, b, kl[9];
    (void)inst; (void)show;
    if (cmd && cmd[0] && cmd[1] == 'F') df = FileCreate("MIX.TXT", 0);
    hex("MIXTEST: SndCaps", c);
    pruef("MIXTEST: Ring vorhanden (SC_STREAM)", (c & SC_STREAM) != 0);
    pruef("MIXTEST: Stereo wie erwartet", ((c & SC_STEREO) != 0) == (art == 'P' || art == '6'));
    pruef("MIXTEST: 16 Bit wie erwartet", ((c & SC_PCM16) != 0) == (art == '6'));
    a = ton(440, 11025, 11025, 0); b = ton(1000, 22050, 11025, 1);
    pruef("MIXTEST: Toene angelegt", a && b);
    k1 = SndMixPlay(0, a, 11025, 0); t0 = TickCount();
    k2 = SndMixPlay(0, b, 22050, SM_16BIT);
    hex("MIXTEST: Kanaele", ((DWORD)k1 << 16) | k2);
    pruef("MIXTEST: zwei Kanaele (1 und 2)", k1 == 1 && k2 == 2);
    pruef("MIXTEST: SndMixState(0) = 3", SndMixState(0) == 3);
    ta = tb = 0;
    for (i = 0; i < 2; i++) { k = 0; if (ende(2500, &k)) { t = TickCount() - t0; if (k == 1) ta = t; else if (k == 2) tb = t; } }
    hex("MIXTEST: 440 Hz endet nach ms", ta); hex("MIXTEST: 1000 Hz endet nach ms", tb);
    pruef("MIXTEST: 1000 Hz (16 Bit) endet nach 500 +-100 ms", tb + 100 >= 500 && tb <= 600);
    pruef("MIXTEST: 440 Hz (8 Bit) endet nach 1000 +-100 ms", ta + 100 >= 1000 && ta <= 1100);
    pruef("MIXTEST: danach nichts mehr", SndMixState(0) == 0);
    k1 = SndMixPlay(0, a, 11025, SM_LOOP); SndMixSet(k1, 64, 0);
    pruef("MIXTEST: Schleife: nach 1,5 s keine Endmeldung", ende(1500, &k) == 0 && SndMixState(k1) == 1);
    SndMixStop(k1);
    pruef("MIXTEST: nach SndMixStop still und keine Meldung", SndMixState(0) == 0 && ende(300, &k) == 0);
    for (i = 0, n = 0; i < 8; i++) if (SndMixPlay(0, a, 11025, SM_LOOP)) n++;
    SetLastError(0); k = SndMixPlay(0, b, 22050, SM_16BIT);
    hex("MIXTEST: Kanaele belegt", n);
    pruef("MIXTEST: acht Kanaele, der neunte -> BUSY", n == 8 && k == 0 && LastError() == RD_ERR_BUSY);
    pruef("MIXTEST: SndMixState(0) = FFh", SndMixState(0) == 0xFF);
    SndMixStop(0); pruef("MIXTEST: SndMixStop(0): alle still", SndMixState(0) == 0 && ende(200, &k) == 0);
    pruef("MIXTEST: SndMixOut 11025 Hz, 8 Bit mono", SndMixOut(11025, 0) == 0);
    k1 = SndMixPlay(0, a, 11025, 0); t0 = TickCount(); k = 0; ende(2500, &k); t = TickCount() - t0;
    hex("MIXTEST: 8 Bit mono endet nach ms", t);
    pruef("MIXTEST: 8 Bit mono: Ende nach 1000 +-100 ms", k == k1 && t + 100 >= 1000 && t <= 1100);
    pruef("MIXTEST: SndMixOut 22050 Hz, alles was geht", SndMixOut(22050, MX_16BIT | MX_STEREO) == 0);
    pruef("MIXTEST: SndSamplePlay nach dem Mischer", SndSamplePlay(0, a, 11025, 0) == 0);
    { MSG m; t0 = TickCount(); t = 0; while (TickCount() - t0 < 2500) { if (PeekMessage(&m, 1) && m.msg == M_SOUNDDONE && m.p1 == 2) { t = TickCount() - t0; break; } TaskYield(); } }
    hex("MIXTEST: Einzelsample endet nach ms", t);
    pruef("MIXTEST: Einzelsample endet nach 1000 +-100 ms", t + 100 >= 1000 && t <= 1100);
    k1 = SndMixPlay(0, b, 22050, SM_16BIT); t0 = TickCount(); k = 0; ende(2000, &k); t = TickCount() - t0;
    pruef("MIXTEST: Mischer wieder nach dem Einzelsample", k1 == 1 && k == 1 && t + 100 >= 500 && t <= 600);
    for (i = 0; i < 9; i++) kl[i] = 0; (void)kl;
    SndMixStop(0); MemFree(a); MemFree(b);
    hex("MIXTEST: Fehler", fehler);
    aus("MIXTEST: fertig"); if (df >= 0) FileClose(df);
    return 0;
}
