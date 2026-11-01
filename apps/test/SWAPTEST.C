/* SWAPTEST - SDK-Beispiel: Speicherfehler behandeln, bestehende Daten pruefen und weiterlaufen.
 * T: ohne Dialog, Meldungen auf COM1. NORMAL: interaktiver Test fuer das Browser-Liveimage. */
#include "RD2.H"
#include <string.h>
#define N 70
#define GR 16384
static HMEM block[N];
static WORD angelegt, grund, fehler, sdkfehler;
static char bericht[400];
static void nummer(WORD n) { char t[6]; WORD i = 0; do t[i++] = (char)('0' + n % 10); while (n /= 10); while (i) { WORD l = (WORD)strlen(bericht); bericht[l] = t[--i]; bericht[l + 1] = 0; } }

static const char *ursache(WORD e)
{
    switch (e) {
    case MEMERR_NONE: return "kein Speicherfehler";
    case MEMERR_RAM: return "RAM nicht ausreichend (oder Swap aus)";
    case MEMERR_SWAPLIMIT: return "eingestelltes Swap-Dateilimit erreicht";
    case MEMERR_RESERVE: return "Freiplatzreserve auf der Festplatte";
    case MEMERR_DISK: return "Swapdatei / Laufwerk nicht verfuegbar";
    case MEMERR_SLOTS: return "alle 32 Swapplaetze belegt";
    }
    return "unbekannter Zusatzgrund";
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD i, j, n = 0, anzahl = cmd && cmd[0] == 'S' ? N : 40; WORD FAR *p; BYTE test = (BYTE)(cmd && (cmd[0] == 'T' || cmd[0] == 'S'));
    (void)inst; (void)show;
    if (cmd && cmd[0] == 'V') { TaskExec("TASKMAN.EXE", "", SW_SHOW); { DWORD t = TickCount(); while (TickCount() - t < 1500) TaskYield(); } }
    for (i = 0; i < anzahl; i++) {
        HMEM neu = MemAlloc(GR, MEM_MOVABLE);
        if (!neu) { sdkfehler = LastError(); grund = MemError(); break; }
        block[i] = neu;
        p = (WORD FAR *)MemLock(neu);
        if (!p) { sdkfehler = LastError(); grund = MemError(); MemFree(neu); block[i] = 0; break; }
        for (j = 0; j < GR / 2; j++) p[j] = (WORD)(i * 97 + j);
        MemUnlock(neu); n++;
    }
    angelegt = n;
    if (cmd && cmd[0] == 'V') { DbgPrint("SWAPTEST: Anzeige unter Last"); { DWORD t = TickCount(); while (TickCount() - t < 8000) TaskYield(); } }
    { MEMSTATS m; WORD le = LastError(), me = MemError();
      if (MemStats(&m) || LastError() != le || MemError() != me || m.ram_free > m.ram_total) fehler++;
      DbgHex("SWAPTEST: Swap belegt", m.swap_used); DbgHex("SWAPTEST: Swapdatei", m.swap_file);
      DbgHex("SWAPTEST: Puffer", m.swap_cached); DbgHex("SWAPTEST: Swap frei", m.swap_free);
      DbgHex("SWAPTEST: Limit", m.swap_maxkb); DbgHex("SWAPTEST: Reserve", m.swap_reservekb);
      DbgHex("SWAPTEST: Swap aktiv", m.flags); }

    /* Zwei Arbeitsbloecke freigeben, bevor wir ausgelagerte Daten einlesen.
     * Die App behaelt bei Fehlern keine FAR-Zeiger und prueft jeden MemLock erneut. */
    for (i = 0; i < 2 && n; i++) { n--; MemFree(block[n]); block[n] = 0; }
    for (i = 0; i < n; i++) {
        p = (WORD FAR *)MemLock(block[i]);
        if (!p) {                              /* Fehler sichern, Handle behalten: ein begrenzter Wiederholversuch */
            sdkfehler = LastError(); grund = MemError(); p = (WORD FAR *)MemLock(block[i]);
        }
        if (!p) fehler++;
        else {
            for (j = 0; j < GR / 2; j++) if (p[j] != (WORD)(i * 97 + j)) { fehler++; break; }
            MemUnlock(block[i]);
        }
        MemFree(block[i]); block[i] = 0;
    }
    { MEMSTATS m; if (MemStats(&m) || (m.swap_used && test)) fehler++;
      DbgHex("SWAPTEST: Swap nach Freigabe", m.swap_used); DbgHex("SWAPTEST: Dateilaenge danach", m.swap_file);
      DbgHex("SWAPTEST: Puffer danach", m.swap_cached); DbgHex("SWAPTEST: Frei danach", m.swap_free); }
    DbgHex("SWAPTEST: angelegt", angelegt); DbgHex("SWAPTEST: LastError", sdkfehler);
    DbgHex("SWAPTEST: MemError", grund); DbgHex("SWAPTEST: Datenfehler", fehler);
    DbgPrint("SWAPTEST: App laeuft weiter, alle Handles freigegeben");
    nummer(angelegt); strcat(bericht, " von 40 Bloecken zu 16 KB angelegt.\n\nUrsache: "); strcat(bericht, ursache(grund));
    strcat(bericht, ".\nLastError="); nummer(sdkfehler); strcat(bericht, ", MemError="); nummer(grund);
    strcat(bericht, "\nDatenpruefung: "); nummer(fehler);
    strcat(bericht, " Fehler.\n\nDie App laeuft weiter. Alle Testbloecke sind freigegeben.\nEinstellungen: Optionen > System > Auslagerung.\nAenderungen gelten nach dem Neustart von RD2.");
    if (!test && !(cmd && cmd[0] == 'V')) MsgBox(0, bericht, "Speicher-Test (SDK 2.1)", MB_OK | (fehler ? MB_ERROR : MB_INFO));
    if (cmd && cmd[0] == 'V') { DbgPrint("SWAPTEST: Anzeige nach Freigabe"); { DWORD t = TickCount(); while (TickCount() - t < 8000) TaskYield(); } }
    return (short)fehler;
}
