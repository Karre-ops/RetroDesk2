/* WURM.C - ein Arbeiter fuer STRESS: Speicher, Nachrichten, 8087 (mit -fpi87 gebaut) */
#include "RD2.H"
#define NB 12
static HMEM h[NB]; static WORD hs[NB];
static DWORD zufall = 1;
static WORD rnd(WORD n) { zufall = zufall * 1103515245UL + 12345; return (WORD)((zufall >> 16) % n); }
static int fill(HMEM x, WORD n, WORD pat, int pruefen)
{
    BYTE FAR *p = (BYTE FAR *)MemLock(x); WORD i; int ok = 1;
    if (!p) return 0;
    for (i = 0; i < n; i++) { if (pruefen) { if (p[i] != (BYTE)(pat + i)) ok = 0; } else p[i] = (BYTE)(pat + i); }
    MemUnlock(x);
    return ok;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HTASK chef = (HTASK)(cmd[0] - 'A'); WORD k = cmd[1] - '0', it, i, j, fehler = 0, n;
    volatile double s = 0;
    (void)inst; (void)show;
    zufall = k * 7919UL;
    for (it = 1; it <= 400; it++) {
        i = rnd(NB);
        if (h[i]) { if (!fill(h[i], hs[i], i * 31 + k, 1)) fehler++; MemFree(h[i]); h[i] = 0; }
        n = 64 + rnd(3000);
        if ((h[i] = MemAlloc(n, (it & 3) ? MEM_MOVABLE : MEM_FIXED)) != 0) { hs[i] = n; fill(h[i], n, i * 31 + k, 0); }
        for (j = 1; j <= 50; j++) s += (double)j * k;      /* 8087, wird mitten drin unterbrochen */
        if (!(it % 20)) { PostTask(chef, M_USER + 10, k, 0); TaskYield(); }
    }
    for (i = 0; i < NB; i++) if (h[i]) { if (!fill(h[i], hs[i], i * 31 + k, 1)) fehler++; MemFree(h[i]); }
    /* Summe: 400 * k * (50*51/2) = 510000 * k, exakt als double */
    if (s != 510000.0 * k) { fehler++; DbgHex("WURM: 8087-Summe FALSCH, Wurm", k); }
    if (fehler) DbgHex("WURM: Speicherfehler, Wurm", k);
    PostTask(chef, M_USER + 11, fehler ? 0 : 1, 0);
    return 0;
}
