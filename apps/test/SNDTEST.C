/* SNDTEST.C - Test fuer RDSND und die Klangtreiber (test/klang.sh). Befehlszeile: S = Lautsprecher erwartet,
 * A = AdLib erwartet, B = Sound Blaster (PCM, FM), L = Lautsprecher und Systemklang aus. Prueft am Port 61h, ob der Lautsprecher an ist
 * und ob Toene ueber den Kern-Takt von selbst enden. */
#include <conio.h>
#include "RD2.H"

static WORD fehler;
static void pruef(LPCSTR was, int ok) { DbgPrint(was); if (!ok) { fehler++; DbgPrint("SNDTEST: ^^^ FALSCH"); } }
static int spk(void) { return (inp(0x61) & 3) == 3; }
static void warte(WORD ms) { DWORD t = TickCount(); while (TickCount() - t < ms) TaskYield(); }
static DWORD ende(WORD max)                       /* ms bis M_SOUNDDONE, 0xFFFFFFFF = keine */
{
    MSG m; DWORD t = TickCount();
    while (TickCount() - t < max) { if (PeekMessage(&m, 1) && m.msg == M_SOUNDDONE) return TickCount() - t; TaskYield(); }
    return 0xFFFFFFFFUL;
}
static void musik(LPCSTR d, WORD fl, LPCSTR was, WORD soll)
{
    WORD r = SndMusicPlay(0, d, fl); DWORD t;
    DbgPrint(was); pruef("SNDTEST: SndMusicPlay ok", r == 0);
    t = ende(2000); DbgHex("SNDTEST: Musik endet nach ms", t);
    pruef("SNDTEST: Endmeldung zur rechten Zeit (+-60 ms)", t != 0xFFFFFFFFUL && t + 60 >= soll && t <= soll + 60);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    char m = cmd ? cmd[0] : 'S'; WORD c; DWORD t;
    (void)inst; (void)show;
    pruef("SNDTEST: Lautsprecher beim Start aus (auch nach dem Piep des Laufs davor)", !spk());
    c = SndCaps(); DbgHex("SNDTEST: SndCaps", c);
    if (m == 'B') {
        HMEM h = MemAlloc(4000, MEM_MOVABLE); BYTE FAR *p; WORD i; DWORD t;
        pruef("SNDTEST: Sound Blaster: PCM und FM", (c & SC_PCM) && (c & SC_OPL2));
        musik("A:\\RD2\\T1.IMF", 0, "IMF ueber Sound Blaster (vor PCM)", 500);
        p = (BYTE FAR *)MemLock(h); for (i = 0; i < 4000; i++) p[i] = (BYTE)(i & 16 ? 0xA0 : 0x60); MemUnlock(h);
        pruef("SNDTEST: SndSamplePlay 4000 Bytes, 8 kHz", SndSamplePlay(0, h, 8000, 0) == 0);
        t = ende(2000); DbgHex("SNDTEST: Sample endet nach ms", t);
        pruef("SNDTEST: Sample-Ende zur rechten Zeit (500 +-80 ms)", t != 0xFFFFFFFFUL && t + 80 >= 500 && t <= 580);
        pruef("SNDTEST: Rate 2000 Hz ungueltig", SndSamplePlay(0, h, 2000, 0) != 0);
        pruef("SNDTEST: Sample-Schleife", SndSamplePlay(0, h, 8000, SM_LOOP) == 0);
        pruef("SNDTEST: Schleife: nach 1,3 s keine Endmeldung", ende(1300) == 0xFFFFFFFFUL);
        SndSampleStop(); pruef("SNDTEST: nach SndSampleStop keine Meldung", ende(300) == 0xFFFFFFFFUL);
        musik("A:\\RD2\\T1.IMF", 0, "IMF ueber Sound Blaster", 500);
        SndSamplePlay(0, h, 8000, SM_LOOP);       /* laeuft noch beim Ende: Treiber gibt IRQ und DMA frei */
        warte(100); SndSampleStop(); MemFree(h);
    } else if (m == 'A') {
        pruef("SNDTEST: AdLib erkannt (SC_OPL2)", c == SC_OPL2);
        SndBeep(880, 100); pruef("SNDTEST: Piep ueber AdLib, Lautsprecher bleibt aus", !spk());
        pruef("SNDTEST: SndNote Kanal 0", SndNote(0, 69, 100) == 0);
        pruef("SNDTEST: SndNote Kanal 9 ungueltig", SndNote(9, 69, 100) != 0);
        SndNoteOff(0); warte(200);
        musik("A:\\RD2\\T1.IMF", 0, "IMF Typ 1, 560 Hz", 500);
        musik("A:\\RD2\\T0.IMF", SM_IMF700, "IMF Typ 0, 700 Hz", 500);
        musik("A:\\RD2\\T2.DRO", 0, "DRO 2.0", 512);
        musik("A:\\RD2\\T1.DRO", 0, "DRO 1.0", 500);
        pruef("SNDTEST: fehlende Datei -> NOTFOUND", SndMusicPlay(0, "A:\\RD2\\FEHLT.IMF", 0) == RD_ERR_NOTFOUND);
        pruef("SNDTEST: Schleife startet", SndMusicPlay(0, "A:\\RD2\\T1.IMF", SM_LOOP) == 0);
        pruef("SNDTEST: Schleife: nach 1,3 s keine Endmeldung", ende(1300) == 0xFFFFFFFFUL);
        SndMusicStop(); pruef("SNDTEST: nach SndMusicStop keine Meldung", ende(300) == 0xFFFFFFFFUL);
        pruef("SNDTEST: Piep nach der Musik wieder moeglich", SndNote(0, 69, 100) == 0);
        SndNoteOff(0);
        SndMusicPlay(0, "A:\\RD2\\T1.IMF", SM_LOOP);   /* laeuft noch, wenn RD endet: Ende muss alles freigeben */
    } else {
        pruef("SNDTEST: Lautsprecher (SC_SPEAKER)", c == SC_SPEAKER);
        SndBeep(880, 100);
        if (m == 'L') pruef("SNDTEST: Systemklang aus: Piep bleibt still", !spk());
        else {
            pruef("SNDTEST: Piep an", spk());
            t = TickCount(); while (spk() && TickCount() - t < 1000) TaskYield();
            t = TickCount() - t; DbgHex("SNDTEST: Piep endet nach ms", t);
            pruef("SNDTEST: Piep endet von selbst (80..200 ms)", !spk() && t >= 70 && t <= 200);
            SndSystem("START"); pruef("SNDTEST: START klingt", spk());
            t = TickCount(); while (spk() && TickCount() - t < 2000) TaskYield();
            t = TickCount() - t; DbgHex("SNDTEST: START dauert ms", t);
            pruef("SNDTEST: START etwa 550 ms (vier Noten)", t >= 450 && t <= 700);
            SetLastError(0); SndSystem("GIBTSNICHT"); pruef("SNDTEST: unbekannter Klang -> NOTFOUND", LastError() == RD_ERR_NOTFOUND);
        }
        pruef("SNDTEST: SndNote A4", SndNote(0, 69, 100) == 0 && spk());
        SndNote(1, 72, 100); SndNoteOff(0); pruef("SNDTEST: NoteOff alter Kanal laesst neue Note", spk());
        SndNoteOff(1); pruef("SNDTEST: NoteOff aus", !spk());
        pruef("SNDTEST: Note 128 ungueltig", SndNote(0, 128, 100) != 0);
        pruef("SNDTEST: Musik ueber Lautsprecher -> NOTSUP", SndMusicPlay(0, "A:\\RD2\\T1.IMF", 0) == RD_ERR_NOTSUP);
        SndBeep(440, 2000); warte(50);
    }
    DbgHex("SNDTEST: Fehler", fehler);
    DbgPrint("SNDTEST: fertig");
    return 0;                                     /* ein laufender Piep muss beim Ende von RD verstummen */
}
