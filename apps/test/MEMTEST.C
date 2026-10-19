/* MEMTEST.C - Speicher fuellen, Luecken erzeugen, grossen Block erzwingen (Verdichten),
 * Inhalte pruefen, verwerfbaren Block verwerfen lassen, alles freigeben. */
#include "RD2.H"
#define N 120
static HMEM h[N];
static int check(HMEM x, WORD pat)
{
    WORD FAR *p = (WORD FAR *)MemLock(x); WORD i, ok = 1;
    if (!p) return 0;
    for (i = 0; i < 2048; i++) if (p[i] != pat) { ok = 0; break; }
    MemUnlock(x);
    return ok;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HTASK ping = (HTASK)(cmd[0] - 'A'); WORD i, k, j, bad = 0; HMEM big, dis; DWORD frei0;
    WORD FAR *p;
    (void)inst; (void)show;
    frei0 = MemAvail(MEMA_KONV);
    DbgHex("MEMTEST: frei am Anfang", frei0);
    dis = MemAlloc(4096, MEM_DISCARDABLE);                     /* soll spaeter verworfen werden */
    for (k = 0; k < N; k++) {                                   /* in 4-KB-Bloecken alles belegen */
        if (!(h[k] = MemAlloc(4096, MEM_MOVABLE))) break;
        p = (WORD FAR *)MemLock(h[k]);
        for (j = 0; j < 2048; j++) p[j] = 0x1000 + k;
        MemUnlock(h[k]);
    }
    DbgHex("MEMTEST: 4-KB-Bloecke", k);
    DbgHex("MEMTEST: groesster freier Block", MemAvail(MEMA_GROESSTER));
    for (i = 0; i < k; i += 2) { MemFree(h[i]); h[i] = 0; }     /* jede zweite: Luecken */
    DbgHex("MEMTEST: nach Luecken groesster Block", MemAvail(MEMA_GROESSTER));
    big = MemAlloc(65000UL, MEM_MOVABLE);                       /* geht nur mit Verdichten */
    DbgHex("MEMTEST: 65000-Byte-Block", big);
    for (i = 1; i < k; i += 2) if (!check(h[i], 0x1000 + i)) bad++;
    DbgHex("MEMTEST: falsche Bloecke nach Verdichten", bad);
    DbgPrint(MemLock(dis) ? "MEMTEST: verwerfbarer Block noch da" : "MEMTEST: verwerfbarer Block verworfen");
    MemFree(dis); MemFree(big);
    for (i = 0; i < k; i++) if (h[i]) MemFree(h[i]);
    DbgPrint(MemAvail(MEMA_KONV) == frei0 ? "MEMTEST: alles frei" : "MEMTEST: LECK");
    PostTask(ping, M_USER + 3, 2, 0);
    return 0;
}
