/* MAUS.EXE - Mauseinstellungen: Geschwindigkeit (1..5), Doppelklick-Zeit (300/500/700 ms), Tasten tauschen.
 * Schreibt <RD2>\RD2.INI [Maus] Tempo=, Doppelklick=, Tasten=. Kern (Doppelklick, Tasten) und RDGDI (Tempo,
 * nach jedem Treiber-Reset) lesen die Werte beim Start von RD2. Testfeld: zwei Klicks auf "Test" innerhalb der
 * gewaehlten Zeit gelten als Doppelklick. Befehlszeile "T": Testmodus ohne Dialog (Tempo 5, 700 ms, getauscht). */
#include "RD2.H"

#define ID_T1 31                                  /* 31..35 Tempo */
#define ID_D1 41                                  /* 41..43 Doppelklick kurz/mittel/lang */
#define ID_TAUSCH 51
#define ID_TEST 52
#define ID_TTEXT 53
static HINST hi; static char ini[80]; static BYTE test; static DWORD letzt; static const short dz[3] = { 300, 500, 700 };

static void zahl(char *p, short v) { char r[6]; short n = 0; do r[n++] = (char)('0' + v % 10); while (v /= 10); while (n) *p++ = r[--n]; *p = 0; }
static void setze(LPCSTR k, short v) { char t[8]; zahl(t, v); ProfileSetStr(ini, "Maus", k, t); }
static short dwahl(HWND d) { short i; for (i = 0; i < 3; i++) if (DlgChecked(d, ID_D1 + i)) return dz[i]; return 500; }
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    short i, v;
    (void)p2;
    if (msg == M_INITDIALOG) {
        v = ProfileGetInt(ini, "Maus", "Tempo", 3); if (v < 1 || v > 5) v = 3; DlgCheck(d, ID_T1 + v - 1, 1);
        v = ProfileGetInt(ini, "Maus", "Doppelklick", 500); DlgCheck(d, ID_D1 + (v < 400 ? 0 : v < 600 ? 1 : 2), 1);
        DlgCheck(d, ID_TAUSCH, ProfileGetInt(ini, "Maus", "Tasten", 0) ? 1 : 0);
        return FALSE;
    }
    if (msg != M_COMMAND) return FALSE;
    if (p1 == ID_TEST) {
        DWORD n = TickCount();
        DlgSetText(d, ID_TTEXT, StrGet(hi, letzt && n - letzt < (DWORD)dwahl(d) ? 3 : 4));
        letzt = letzt && n - letzt < (DWORD)dwahl(d) ? 0 : n;
        return TRUE;
    }
    if (p1 == ID_OK) {
        for (i = 0; i < 5; i++) if (DlgChecked(d, ID_T1 + i)) setze("Tempo", i + 1);
        setze("Doppelklick", dwahl(d)); setze("Tasten", DlgChecked(d, ID_TAUSCH) ? 1 : 0);
        DbgPrint("MAUS: gespeichert");
        if (!test) MsgBox(d, StrGet(hi, 2), StrGet(hi, 1), MB_OK | MB_INFO);
        DialogEnd(d, ID_OK); return TRUE;
    }
    return FALSE;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    SYSINFO si; short n;
    (void)show; hi = inst; test = (BYTE)(cmd && cmd[0] == 'T');
    KernInfo(&si); for (n = 0; si.home[n] && n < 66; n++) ini[n] = si.home[n];
    if (n && ini[n - 1] != '\\') ini[n++] = '\\';
    { static const char f[] = "RD2.INI"; short k; for (k = 0; f[k]; k++) ini[n++] = f[k]; ini[n] = 0; }
    if (test) { setze("Tempo", 5); setze("Doppelklick", 700); setze("Tasten", 1); DbgPrint("MAUS: gespeichert"); return 0; }   /* Testmodus ohne Dialog */
    DialogRun(inst, 10, 0, dproc, 0);
    return 0;
}
