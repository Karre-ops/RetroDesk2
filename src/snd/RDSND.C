/* RDSND.C - Klang-Modul (API 300h-30Bh): Treiber laden, Pieptoene, Systemklaenge, Noten.
 * Treiber nach RD2.INI [System] Sound= (SPEAKER, ADLIB, SB mit SB=A220 I5 D1, COVOX mit Covox=1 fuer LPT1), sonst Lautsprecher (Minimum, auch fuer altes AUS).
 * [System] Systemklang=0 schaltet SndBeep und SndSystem stumm (Optionen), Noten bleiben.
 * Toene enden ueber den Kern-Takt (INT 8, 72,8 Hz, Export 14): nichts wartet, Programme laufen weiter.
 * Musik: IMF und DRO ueber OPL-Register (AdLib, spaeter SB) mit RDMUS.MOD, das nur zum Abspielen geladen wird. Das Ende meldet
 * input_poll des Kerns (Export 15 mit 2) als M_SOUNDDONE an das Fenster oder den Task. Samples (8 Bit mono) ueber pcm_start des Treibers (SB per DMA, Lautsprecher und Covox ueber den Zeitgeber), Ende als
 * M_SOUNDDONE mit p1 = 2. SM_HALBE: jede gespielte Blockhaelfte als M_SOUNDDONE p1 = 3, p2 = 0/1 (frei zum Nachfuellen,
 * mit SM_LOOP ein Ringpuffer). WAV-Dateien laedt SndWavLoad aus RD2.LIB (im Programm). */
#include <i86.h>
#include "RD2DRV.H"

typedef struct { WORD hz; BYTE t; } NOTE;         /* t in Takten zu 13,7 ms; hz 1 = Pause */
static const NOTE s_start[] = { {523,8},{659,8},{784,8},{1047,16},{0,0} };   /* wie RD1, Takte x4 */
static const NOTE s_ende[]  = { {1047,8},{784,8},{659,8},{523,16},{0,0} };
static const NOTE s_fehl[]  = { {220,12},{1,4},{165,20},{0,0} };
static const NOTE s_frage[] = { {880,8},{1175,12},{0,0} };
static const NOTE s_ding[]  = { {1319,10},{0,0} };
static const NOTE s_verb[]  = { {659,4},{880,4},{1175,4},{1568,12},{0,0} };
static const NOTE s_klick[] = { {1800,1},{0,0} };
static const struct { const char *n; const NOTE *s; } namen[] = {
    { "START", s_start }, { "ENDE", s_ende }, { "FEHLER", s_fehl }, { "FRAGE", s_frage },
    { "DING", s_ding }, { "VERBINDUNG", s_verb }, { "KLICK", s_klick } };

static SNDDRV FAR *dv; static HMOD dm; static WORD dpar; static BYTE bereit, sys = 1, ruhe;
static const NOTE * volatile q; static volatile BYTE rest; static WORD laut = 127;
/* Musik: RDMUS.MOD, nur waehrend des Abspielens geladen */
typedef struct { WORD (RDAPI *play)(SNDDRV FAR *dv, LPCSTR d, WORD fl); BYTE (RDAPI *takt)(void); void (RDAPI *stop)(void); } MUSTAB;
static MUSTAB FAR *mt; static HMOD mm; static volatile BYTE mart, mfertig; static HWND mwin; static HTASK mtask;

void RDEXPORT s_SndSampleStop(void);
static WORD aus(void);
#pragma aux aus = "pushf" "cli" "pop ax" value [ax]
static void ein(WORD f);
#pragma aux ein = "push ax" "popf" parm [ax]

static BYTE gleich(LPCSTR a, const char *b) { while (*b) if ((*a++ & 0xDF) != (*b++ & 0xDF)) return 0; return *a == 0; }
static void ton(WORD hz) { if (hz > 1) dv->beep(hz, 1); else dv->beep(0, 0); }
static WORD laden(LPCSTR n)
{
    FARPROC f; SNDDRV FAR *t;
    if (!(dm = ModLoad(n))) return 0;
    f = ModProc(dm, 1); t = f ? (SNDDRV FAR *)((void FAR * (RDAPI *)(void))f)() : 0;
    if (!t || t->h.typ != DRV_SND || t->h.groesse < sizeof(SNDDRV) || t->h.init(dpar)) { ModFree(dm); dm = 0; return 0; }
    dv = t; return 1;
}
static const char ssys[] = "System";
static void init(void)                            /* beim ersten Aufruf (laedt auf dem Stapel des Programms) */
{
    SYSINFO si; char ini[80], w[10]; WORD i = 0;
    bereit = 1;
    KernInfo(&si);
    while (si.home[i] && i < 66) { ini[i] = si.home[i]; i++; }
    if (i && ini[i - 1] != '\\') ini[i++] = '\\';
    ini[i] = 0; { const char *r = "RD2.INI"; while (*r) ini[i++] = *r++; ini[i] = 0; }
    ProfileGetStr(ini, ssys, "Sound", "SPEAKER", w, sizeof w);
    sys = ProfileGetInt(ini, ssys, "Systemklang", 1) != 0;
    if (gleich(w, "SB")) {                        /* SB=A220 I5 D1 wie BLASTER (RD2SETUP) */
        static char b[24]; char *p = b, c; WORD v, ba;   /* im Datensegment: Stapel ist SS != DS */
        ProfileGetStr(ini, ssys, "SB", "A220 I5 D1", b, sizeof b);
        while ((c = (char)(*p & 0xDF)) != 0) {
            p++; ba = c == 'A' ? 16 : 10; v = 0;
            while (*p > ' ') { v = v * ba + (WORD)(*p <= '9' ? *p - '0' : (*p & 0xDF) - 'A' + 10); p++; }
            if (c == 'A') dpar |= (v >> 4) & 0x0F; else if (c == 'I') dpar |= (v & 0x0F) << 4; else if (c == 'D') dpar |= ((v + 1) & 7) << 8;
            while (*p == ' ') p++;
        }
        if (laden("SB.DRV")) return;
        dpar = 0;
    }
    if (gleich(w, "ADLIB") && laden("ADLIB.DRV")) return;
    if (gleich(w, "COVOX")) { dpar = (ProfileGetInt(ini, ssys, "Covox", 1) - 1) & 3; if (laden("COVOX.DRV")) return; dpar = 0; }
    laden("SPEAKER.DRV");
}
static BYTE da(void) { if (!bereit) init(); if (!dv || ruhe) { SetLastError(RD_ERR_NOTSUP); return 0; } return 1; }
static void folge(const NOTE *s)
{
    WORD f = aus();
    q = s; ton(s->hz); rest = s->t; q++;
    ein(f);
}

static void mstop(void)                           /* im Task: anhalten, RDMUS frei, Klaenge fuer Pieptoene wieder */
{
    WORD f = aus(); mart = 0; mfertig = 0; ein(f);
    if (!mm) return;
    mt->stop();                                   /* Takt ruft RDMUS nicht mehr (mart = 0) */
    ModFree(mm); mm = 0; mt = 0;
    if (dv) { f = aus(); dv->h.init(0x8000); ein(f); }   /* nur FM-Chip neu: Glockenklaenge fuer Pieptoene zurueck */
}
void RDEXPORT s_Ende(void) { WORD f; mstop(); s_SndSampleStop(); f = aus(); q = 0; rest = 0; if (dv) { dv->beep(0, 0); dv->h.done(); } dv = 0; ein(f); if (dm) ModFree(dm); dm = 0; }
WORD RDEXPORT s_SndCaps(void) { return da() ? dv->caps() : 0; }
void RDEXPORT s_SndBeep(WORD hz, WORD ms)
{
    static NOTE b[2];
    WORD t;
    if (!da() || !sys || mart) return;          /* waehrend Musik: still (AdLib-Kanal 8) */
    t = ms / 14 + 1; if (t > 255) t = 255;       /* Takte zu 13,7 ms, ohne 32-Bit-Division */
    b[0].hz = hz ? hz : 1; b[0].t = (BYTE)t; b[1].t = 0;
    folge(b);
}
void RDEXPORT s_SndSystem(LPCSTR name)
{
    WORD i;
    if (!da() || !sys || !name || mart) return;
    for (i = 0; i < sizeof namen / sizeof namen[0]; i++) if (gleich(name, namen[i].n)) { folge(namen[i].s); return; }
    SetLastError(RD_ERR_NOTFOUND);
}
WORD RDEXPORT s_SndNote(WORD k, WORD n, WORD v)
{
    WORD f, r;
    if (!da()) return RD_ERR_NOTSUP;
    if (n > 127 || v > 127) return RD_ERR_HANDLE;
    f = aus(); r = dv->note(k, n, (WORD)((DWORD)v * laut / 127)); ein(f);
    return r;
}
void RDEXPORT s_SndNoteOff(WORD k) { WORD f; if (!da()) return; f = aus(); dv->noteoff(k); ein(f); }
WORD RDEXPORT s_SndInstr(WORD k, const INSTR FAR *ins) { WORD f, r; if (!da()) return RD_ERR_NOTSUP; f = aus(); r = dv->instr(k, ins); ein(f); return r; }
WORD RDEXPORT s_SndMusicPlay(HWND w, LPCSTR d, WORD fl)
{
    FARPROC f; WORD r;
    if (!da()) return RD_ERR_NOTSUP;
    if (!dv->reg) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }   /* Lautsprecher: keine FM-Musik */
    mstop();
    if (!(mm = ModLoad("RDMUS.MOD"))) return LastError() ? LastError() : RD_ERR_NOTFOUND;
    f = ModProc(mm, 1); mt = f ? (MUSTAB FAR *)((void FAR * (RDAPI *)(void))f)() : 0;
    if (!mt || (r = mt->play(dv, d, fl)) != 0) { if (!mt) r = RD_ERR_FORMAT; ModFree(mm); mm = 0; mt = 0; SetLastError(r); return r; }
    mwin = w; mtask = TaskSelf(); mfertig = 0; mart = 1;
    return 0;
}
void RDEXPORT s_SndMusicStop(void) { if (bereit) mstop(); }
/* Samples: 8 Bit mono vorzeichenlos im Block des Programms (bleibt gesperrt, solange er klingt), in Stuecken bis zur
   naechsten 64-KB-Seite (DMA); den naechsten startet der Interrupt der Karte ueber s_Weiter. */
static HMEM ph; static HWND pwin; static HTASK ptask; static WORD prate; static volatile BYTE part, pfertig; static BYTE ploop;
static DWORD panf, plen, pa, prest, pmitte; static volatile BYTE phfert;   /* pmitte 0 ohne SM_HALBE; phfert = Haelfte + 1 */
static void RDEXPORT s_Weiter(void);
static void pblock(void)
{
    DWORD k = 0x10000UL - (pa & 0xFFFF);
    if (k > prest) k = prest;
    if (k > 0xFFFF) k = 0xFFFF;
    if (pa < pmitte) { DWORD m = pmitte - pa; if (k > m) k = m; }   /* SM_HALBE: Stueck endet an der Blockmitte */
    if (dv->pcm_start((const BYTE FAR *)MK_FP((WORD)(pa >> 4), (WORD)pa & 15), (WORD)k, prate, s_Weiter)) { part = 0; pfertig = 1; return; }
    pa += k; prest -= k;
}
static void RDEXPORT s_Weiter(void)
{
    if (!part) return;
    if (pa == pmitte) phfert = 1; if (!prest && pmitte) phfert = 2;   /* im Interrupt nur merken */
    if (!prest && ploop) { pa = panf; prest = plen; }
    if (prest) pblock(); else { part = 0; pfertig = 1; }
}
void RDEXPORT s_SndSampleStop(void)
{
    WORD f;
    if (!dv) return;
    f = aus(); if (part) dv->pcm_stop(); part = 0; pfertig = 0; phfert = 0; ein(f);
    if (ph) { MemUnlock(ph); ph = 0; }
}
WORD RDEXPORT s_SndSamplePlay(HWND w, HMEM d, WORD r, WORD fl)
{
    BYTE FAR *p; WORD f;
    if (!da()) return RD_ERR_NOTSUP;
    if (!dv->pcm_start) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }
    s_SndSampleStop();
    if (!(p = (BYTE FAR *)MemLock(d)) || !(plen = MemSize(d))) { if (p) MemUnlock(d); SetLastError(RD_ERR_HANDLE); return RD_ERR_HANDLE; }
    ph = d; pwin = w; ptask = TaskSelf(); prate = r; ploop = (BYTE)(fl & SM_LOOP);
    panf = ((DWORD)FP_SEG(p) << 4) + FP_OFF(p); pa = panf; prest = plen; pmitte = (fl & SM_HALBE) ? panf + (plen >> 1) : 0;
    f = aus(); part = 1; pblock(); ein(f);
    if (!part && pfertig) { pfertig = 0; MemUnlock(ph); ph = 0; SetLastError(RD_ERR_HANDLE); return RD_ERR_HANDLE; }   /* Rate ungueltig */
    return 0;
}
WORD RDEXPORT s_SndVolume(WORD art, WORD wert)    /* art 0: Gesamtlautstaerke 0..127 fuer Noten; Rueckgabe alter Wert */
{
    WORD alt = laut;
    if (art == 0 && wert <= 127) laut = wert;
    if (da() && dv->volume) dv->volume(art, wert);
    return alt;
}
BOOL RDEXPORT s_SndFocus(HWND w) { (void)w; return TRUE; }
void RDEXPORT s_Takt(void)                        /* INT 8, gesperrt: Ton beenden oder naechste Note */
{
    if (!dv) return;
    if (rest && !--rest) {
        if (q && q->t) { ton(q->hz); rest = q->t; q++; }
        else { q = 0; dv->beep(0, 0); }
    }
    if (mart && !ruhe && mt->takt()) { mart = 0; mfertig = 1; }
    if (dv->tick) dv->tick();
}
static void melde(HWND w, HTASK t, WORD p1, WORD p2) { if (w) PostMessage(w, M_SOUNDDONE, p1, p2); else PostTask(t, M_SOUNDDONE, p1, p2); }
void RDEXPORT s_Pause(WORD an)                    /* RunDos: 0 still und Treiber aus, 1 wieder an */
{
    WORD f;
    if (!bereit) { if (an == 2) init(); return; }   /* Treiber gleich beim ersten Warten laden, nicht mitten in der Bedienung */
    if (!dv) return;
    if (an == 2) {                                /* input_poll: Musik zu Ende melden, Speicher frei */
        if (mfertig) { melde(mwin, mtask, 1, 0); mstop(); }   /* erst melden: mstop setzt den FM-Chip neu */
        if (phfert) { melde(pwin, ptask, 3, phfert - 1); phfert = 0; }
        if (pfertig) { pfertig = 0; if (ph) { MemUnlock(ph); ph = 0; } melde(pwin, ptask, 2, 0); }
        return;
    }
    f = aus();
    if (!an) { q = 0; rest = 0; mart = 0; part = 0; dv->beep(0, 0); dv->h.done(); ruhe = 1; }
    else if (ruhe) { ruhe = 0; dv->h.init(dpar); }
    ein(f);
}
static struct { WORD n; FARPROC f[15]; } ex = { 15, {
    (FARPROC)s_Ende, (FARPROC)s_SndCaps, (FARPROC)s_SndBeep, (FARPROC)s_SndSystem, (FARPROC)s_SndNote, (FARPROC)s_SndNoteOff,
    (FARPROC)s_SndInstr, (FARPROC)s_SndMusicPlay, (FARPROC)s_SndMusicStop, (FARPROC)s_SndSamplePlay, (FARPROC)s_SndSampleStop,
    (FARPROC)s_SndVolume, (FARPROC)s_SndFocus, (FARPROC)s_Takt, (FARPROC)s_Pause } };
void FAR * RDAPI RdModInit(void) { return &ex; }
