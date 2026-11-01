/* AUSLTEST.C - Auslagern (Checkliste Punkt 1): mehr verschiebbare Bloecke anlegen als in den Speicher
 * passen, jeden mit eigenem Muster. Danach jeden sperren und pruefen (holt ausgelagerte zurueck),
 * einige ausgelagerte freigeben, MemRealloc auf einen ausgelagerten, alles freigeben.
 * Ohne Ziel (Diskette) endet das Anlegen sauber mit RD_ERR_NOMEM. */
#include "RD2.H"
#define N 60
#define GR 16384
static HMEM h[N], druck[32];
static WORD fehler;
static void fuell(HMEM x, WORD pat) { WORD FAR *p = (WORD FAR *)MemLock(x); WORD j; if (!p) { fehler++; return; } for (j = 0; j < GR / 2; j++) p[j] = pat + j; MemUnlock(x); }
static int pruef(HMEM x, WORD pat) { WORD FAR *p = (WORD FAR *)MemLock(x); WORD j; int ok = 1; if (!p) return 0; for (j = 0; j < GR / 2; j++) if (p[j] != (WORD)(pat + j)) { ok = 0; break; } MemUnlock(x); return ok; }
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD k, i, aus = 0; DWORD frei0 = MemAvail(MEMA_KONV);
    (void)inst; (void)cmd; (void)show;
    DbgHex("AUSLTEST: frei am Anfang (KB)", frei0 >> 10);
    for (k = 0; k < N; k++) { if (!(h[k] = MemAlloc(GR, MEM_MOVABLE))) break; fuell(h[k], 0x100 * k); }
    DbgHex("AUSLTEST: angelegt (16-KB-Bloecke)", k);
    DbgHex("AUSLTEST: mehr als frei war", (DWORD)k * GR > frei0);
    if (k < N) DbgHex("AUSLTEST: Abbruch mit Fehler", LastError());
    if (k == N) {                                /* volle Swap-Platzliste erzwingen, auch mit mehr freiem DOS-RAM */
        for (i = 0; i < 32; i++) if (!(druck[i] = MemAlloc(GR, MEM_MOVABLE))) break;
        DbgHex("AUSLTEST: zusaetzliche Druckbloecke", i);
        while (i) MemFree(druck[--i]);            /* danach muessen auch Dateiluecken wieder nutzbar sein */
    }
    for (i = 0; i < k; i++) if (MemFlags(h[i]) & 0x4000) aus++;   /* 0x4000: verworfen (darf nicht sein) */
    DbgHex("AUSLTEST: als verworfen gemeldet", aus);
    for (i = 1; i < k; i += 7) { MemFree(h[i]); h[i] = 0; }     /* einige (auch ausgelagerte) freigeben */
    if (k > 2 && h[0] && !MemRealloc(h[0], GR + 4096, 0)) fehler++;
    for (aus = 0, i = 0; i < k; i++) if (h[i] && !pruef(h[i], 0x100 * i)) aus++;
    DbgHex("AUSLTEST: Abweichungen", aus + fehler);
    for (aus = 0, i = k; i--; ) if (h[i] && !pruef(h[i], 0x100 * i)) aus++;   /* rueckwaerts: noch mal hin und her */
    DbgHex("AUSLTEST: Abweichungen rueckwaerts", aus);
    for (i = 0; i < k; i++) if (h[i]) MemFree(h[i]);
    DbgHex("AUSLTEST: frei am Ende gleich", MemAvail(MEMA_KONV) == frei0);
    DbgPrint("AUSLTEST: fertig");
    return 0;
}
