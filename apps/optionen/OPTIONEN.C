/* OPTIONEN.C - RD > Optionen: alle Einstellungen auf Karteikarten (29.10.2026, Karsten). Je Karte ein Dialog
 * (11..16) mit Reitern "1 Grafik" .. "6 System" (Alt+1..6) wie RDNET; die Werte bleiben beim Wechsel erhalten,
 * geschrieben wird erst bei OK.
 *  1 Grafik:   Grafikart (GfxList: nur, was die Karte kann) oder automatisch, Profil. RD2.INI [System] Grafik=, Profil=.
 *  2 Klang:    Geraet, Sound-Blaster-Einstellung, Systemklaenge. [System] Sound=, SB=, Systemklang=.
 *  3 Maus:     Tempo, Doppelklick, Tasten tauschen ([Maus], bisher MAUS.EXE), Anschluss (Schalter der RDMAUS-Zeile in
 *              CONFIG.SYS, [System] MausHW=). Ohne RDMAUS-Zeile (anderer Treiber) ist der Anschluss gesperrt.
 *  4 Tastatur: Deutsch/US/Commodore PC10 (31.10.: PC10.KBD/DE.KBD werden ins laufende RDKEYB kopiert). Ist RDKEYB geladen (INT 2Fh AD80h, "RDKB"), schaltet es sofort um; dazu [System] Tastatur=
 *              und der Schalter der RDKEYB-Zeile in CONFIG.SYS. Fremder KEYB: gesperrt.
 *  5 Schoner:  Art und Wartezeit ([Schoner], bisher Dialog in SCHONER.EXE), Vorschau startet SCHONER.EXE S.
 *  6 System:   Start nach dem Booten, Netzzeit, XMS-Ablage ([System] XMS=), Netzwerk... (RDNET), Hardware erkennen...
 *              (RD2SETUP im Textmodus ueber RunDos; danach liest Optionen die Werte neu).
 * Start nach dem Booten: RD2 startet als SHELL= in CONFIG.SYS (RD2SETUP). Aus: die RD-Zeile und INSTALL=-Zeilen, deren
 * Programm auch in AUTOEXEC.BAT steht, werden "REM RD2 ", die alte "REM SHELL="-Zeile gilt wieder. An: umgekehrt.
 * Aus geht nur, wenn danach eine Shell da ist (alte SHELL=-Datei oder COMMAND.COM), sonst startet der Rechner nicht.
 * Grafik gilt sofort (Karsten, 29.10.2026): nach OK startet RD2 neu (RunDos RD_DOS_NEU, andere Programme enden), die
 * alten Werte stehen in [System] GrafikAlt=/ProfilAlt=. Die Shell startet dann "OPTIONEN.EXE B": "Behalten?" mit 15 s;
 * Ja loescht GrafikAlt, Nein oder Zeit um stellt die alten Werte zurueck und startet RD2 noch einmal neu.
 * Befehlszeile "T[n] [Ordner]": Testmodus ab Karte n (1..6), CONFIG.SYS/AUTOEXEC.BAT/COMMAND.COM aus dem Ordner. */
#include "RD2.H"

#define MKFP(s, o) ((BYTE FAR *)(((DWORD)(s) << 16) | (WORD)(o)))
#define ID_S1   91                                /* Reiter 91..96 */
#define NS      6
#define CMAX    8192
static HINST hi; static BYTE test, seite; static char ord[64], cs[80], home[66], ini[80];
static HMEM hm; static char FAR *ein, FAR *aus, FAR *ae; static WORD nein, nae; static BYTE csneu;
static SYSINFO si;

/* Werte aller Karten (0 = erste Auswahl), *0 = Stand beim Oeffnen */
static GFXINFO gl[16]; static WORD gn; static BYTE gsel, gsel0, prof, prof0;
static BYTE snd, snd0, sb, sb0, sysk; static char sbeig[16];
static BYTE tempo, dk, tausch, mw0, mhw, mhw0, rdm; static DWORD letzt;
static BYTE tast, tast0, kbart; static BYTE FAR *kbm;    /* kbart 0 kein Treiber, 1 RDKEYB, 2 fremd */
static BYTE sart, szeit, szeit0, start, start0, netz, xms, xms0;

static const char *const sndw[] = { "SPEAKER", "ADLIB", "SB", "COVOX" };   /* Minimum PC-Lautsprecher, altes AUS -> SPEAKER */
static const char *const sbw[] = { "A220 I5 D1", "A220 I7 D1", "A240 I5 D1", "A240 I7 D1", "A220 I10 D1" };
static const char *const sbt[] = { "220h, IRQ 5, DMA 1", "220h, IRQ 7, DMA 1", "240h, IRQ 5, DMA 1", "240h, IRQ 7, DMA 1", "220h, IRQ 10, DMA 1" };
static const char *const mschalter[] = { "", "/S1", "/S2", "/S3", "/S4", "/M1", "/M2", "/P", "/B", "/I1", "/I2" };
static const char *const mhww[] = { "", "SERIELL:COM1", "SERIELL:COM2", "SERIELL:COM3", "SERIELL:COM4", "MSYS:COM1", "MSYS:COM2",
                                    "PS2", "BUS:23C", "INPORT:23C", "INPORT:238" };
#define NM 11
static const short dz[3] = { 300, 500, 700 };
static const short zt[6] = { 0, 60, 120, 300, 600, 900 };
static const short xw[5] = { 0, 64, 128, 256, 512 };
static const char *const xt[] = { 0, "64 KB", "128 KB", "256 KB", "512 KB" };

static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static BYTE gr(BYTE c) { return (BYTE)(c >= 'a' && c <= 'z' ? c - 32 : c); }
static BYTE gleich(LPCSTR a, LPCSTR b) { while (*a && gr(*a) == gr(*b)) a++, b++; return *a == *b; }
static void zahl(char *p, short v) { char r[6]; short n = 0; do r[n++] = (char)('0' + v % 10); while (v /= 10); while (n) *p++ = r[--n]; *p = 0; }
static void setze(LPCSTR sec, LPCSTR k, short v) { char t[8]; zahl(t, v); ProfileSetStr(ini, sec, k, t); }
static void en(HWND d, WORD id, BYTE an) { EnableWindow(DlgItem(d, id), an); }
static HWND it(HWND d, WORD id) { return DlgItem(d, id); }
static void ladd(HWND d, WORD id, LPCSTR s) { SendMessage(it(d, id), L_ADD, 0, (DWORD)s); }
static void lsel(HWND d, WORD id, WORD i) { SendMessage(it(d, id), L_SETSEL, i, 0); }
static BYTE lget(HWND d, WORD id) { WORD i = (WORD)SendMessage(it(d, id), L_GETSEL, 0, 0); return (BYTE)(i == 0xFFFF ? 0 : i); }
static BYTE radio(HWND d, WORD id, BYTE n) { BYTE i; for (i = 0; i < n; i++) if (DlgChecked(d, (WORD)(id + i))) return i; return 0; }

/* ---- CONFIG.SYS ---------------------------------------------------------------------------------- */
static BYTE beginnt(const char FAR *a, LPCSTR k) { while (*k) if (gr(*a++) != (BYTE)*k++) return 0; return 1; }
static const char FAR *suche(const char FAR *a, WORD n, LPCSTR k)   /* k gross, in a[0..n) ohne Gross/Klein */
{
    WORD i, l = slen(k);
    for (i = 0; i + l <= n; i++) { WORD j = 0; while (j < l && gr(a[i + j]) == (BYTE)k[j]) j++; if (j == l) return a + i; }
    return 0;
}
static BYTE gibts(LPCSTR p) { FINDDATA f; return FileFindFirst(p, 0, &f) == 0; }
static WORD lesen(LPCSTR n, char FAR *b)
{
    short f; WORD r = 0;
    if ((f = FileOpen(n, FO_READ)) >= 0) { r = FileRead(f, b, CMAX - 1); if (r == 0xFFFF) r = 0; FileClose(f); }
    b[r] = 0; return r;
}
static const char FAR *sp(const char FAR *a) { while (*a == ' ' || *a == '\t') a++; return a; }
static BYTE shellz(const char FAR *a) { a = sp(a); return beginnt(a, "SHELL") && (*sp(a + 5) == '='); }
static WORD zlen(const char FAR *a) { WORD n = 0; while (a[n] && a[n] != '\n') n++; return n; }
static BYTE rdz(const char FAR *a) { return suche(a, zlen(a), "RD.EXE") != 0; }
#define ZEILEN(a) for (a = ein; *a; a += zlen(a) + (a[zlen(a)] != 0))
static BYTE an(void)                              /* aktive SHELL=-Zeile mit RD.EXE? */
{
    const char FAR *a;
    ZEILEN(a) if (shellz(a) && rdz(a)) return 1;
    return 0;
}
static const char FAR *instz(LPCSTR prog)         /* aktive INSTALL=-Zeile mit prog (gross) */
{
    const char FAR *a;
    ZEILEN(a) if (beginnt(sp(a), "INSTALL") && suche(a, zlen(a), prog)) return a;
    return 0;
}
static BYTE inae(const char FAR *a)               /* INSTALL=-Programm steht auch in AUTOEXEC.BAT */
{
    char n[13]; WORD i = 0; const char FAR *e, FAR *s;
    a = sp(sp(a) + 7); if (*a == '=') a = sp(a + 1);
    for (e = a; *e && *e != ' ' && *e != '\r' && *e != '\n'; e++) ;
    for (s = e; s > a && s[-1] != '\\' && s[-1] != ':'; s--) ;
    while (s < e && *s != '.' && i < 8) n[i++] = (char)gr(*s++);
    n[i] = 0;
    return i && suche(ae, nae, n) != 0;
}
static WORD put(WORD o, const char FAR *s, WORD n) { while (n-- && o < CMAX - 1) aus[o++] = *s++; return o; }
static void zurueck(WORD o)                       /* aus -> ein (naechste Aenderung baut darauf auf) */
{
    WORD i; for (i = 0; i < o; i++) ein[i] = aus[i];
    ein[o] = 0; nein = o; csneu = 1;
}
/* Schalter der INSTALL=-Zeile von prog ersetzen: alles hinter "prog" bis Zeilenende wird " neu" (neu "" = nichts) */
static void schalter(LPCSTR prog, LPCSTR neu)
{
    const char FAR *z = instz(prog), FAR *p; WORD o, l;
    if (!z) return;
    p = suche(z, zlen(z), prog) + slen(prog);
    o = put(0, ein, (WORD)(p - ein));
    if (*neu) { o = put(o, " ", 1); o = put(o, neu, slen(neu)); }
    l = zlen(z); if (l && z[l - 1] == '\r') l--;
    o = put(o, z + l, slen(z + l));
    zurueck(o);
}
/* Start nach dem Booten. Rueckgabe 0 ok, 2 keine Shell fuer "aus" */
static WORD schalten(BYTE soll)
{
    const char FAR *a; WORD o = 0, l, alt = 0; BYTE neu = 0; char p[80];
    for (a = ein; *a; a += l + (a[l] != 0)) {
        l = zlen(a);
        if (soll) {
            if (beginnt(sp(a), "REM RD2 ")) { a = sp(a) + 8; l = zlen(a); if (shellz(a)) neu = 1; }
            else if (shellz(a) && !rdz(a)) o = put(o, "REM ", 4);
            else if (shellz(a)) neu = 1;
        } else {
            if ((shellz(a) && rdz(a)) || (beginnt(sp(a), "INSTALL") && inae(a))) o = put(o, "REM RD2 ", 8);
            else if (!alt && beginnt(sp(a), "REM ") && shellz(sp(a) + 4) && !rdz(a)) {   /* alte Shell wieder */
                WORD i = 0; const char FAR *q = sp(sp(a) + 4); q = sp(sp(q + 5) + 1);
                while (q[i] && q[i] != ' ' && q[i] != '\r' && q[i] != '\n' && i < sizeof p - 1) { p[i] = q[i]; i++; }
                p[i] = 0;
                if (gibts(p)) { a = sp(a) + 4; l = zlen(a); alt = 1; }
            }
        }
        o = put(o, a, l); if (a[l]) o = put(o, "\n", 1);
    }
    if (soll && !neu) {                           /* keine RD-Zeile mehr da: neu anlegen wie RD2SETUP */
        if (o && aus[o - 1] != '\n') o = put(o, "\r\n", 2);
        o = put(o, "SHELL=", 6); o = put(o, home, slen(home)); o = put(o, "RD.EXE /P ", 10);
        o = put(o, home, slen(home)); o = put(o, "SHELL.EXE\r\n", 11);
    }
    if (!soll && !alt) { scpy(p, ord, sizeof p); scat(p, "COMMAND.COM", sizeof p); if (!gibts(p)) return 2; }
    zurueck(o);
    return 0;
}

/* ---- RDKEYB (INT 2Fh AX=AD80h: AL=FFh, BX=0100h, ES:DI -> "RDKB", danach Belegung 0 US / 1 DE) ---- */
typedef struct { WORD ax, bx, di, es; } KB;
extern void kb2f(KB *k);
#pragma aux kb2f = "push si" "mov ax,0AD80h" "xor bx,bx" "xor di,di" "mov es,di" "int 2Fh" "pop si" \
                   "mov [si],ax" "mov [si+2],bx" "mov [si+4],di" "mov [si+6],es" parm [si] modify [ax bx cx dx di es];
static BYTE kbsetz(BYTE t)                        /* laufendes RDKEYB umschalten; DE/PC10 aus DE.KBD/PC10.KBD (31.10.) */
{
    char p[80], b[185]; short f; WORD n, i;
    if (t == 1) { *kbm = 0; return 1; }
    scpy(p, home, sizeof p); scat(p, t ? "PC10.KBD" : "DE.KBD", sizeof p);
    if ((f = FileOpen(p, FO_READ)) == -1) return 0;
    n = FileRead(f, b, sizeof b); FileClose(f);
    if (n != 184 || b[0] != 'R' || b[4] != 'D' || b[5] != 1) return 0;
    _asm cli;
    for (i = 0; i < 178; i++) kbm[18 + i] = b[6 + i];   /* yz liegt 22 Bytes hinter "RDKB" */
    *kbm = 1;
    _asm sti;
    return 1;
}
static void tastatur(void)
{
    KB k; char w[6];
    kb2f(&k); kbart = 0; kbm = 0;
    if ((k.ax & 0xFF) == 0xFF) {
        BYTE FAR *p = MKFP(k.es, k.di);
        kbart = 2;
        if (k.bx == 0x0100 && k.es && p[0] == 'R' && p[1] == 'D' && p[2] == 'K' && p[3] == 'B') { kbart = 1; kbm = p + 4; }
    }
    ProfileGetStr(ini, "System", "Tastatur", "DE", w, sizeof w);
    tast = (BYTE)(gleich(w, "US") ? 1 : gleich(w, "PC10") ? 2 : 0);
    if (kbm && !*kbm) tast = 1;
    else if (tast == 1 && kbm) tast = 0;
}

/* ---- Werte laden ---------------------------------------------------------------------------------- */
static BYTE cgaf, deskf, farb0;                   /* [Farben] CGA= (Vordergrund Modus 6), Schreibtisch= (Farbmodi), 29./30.10.2026 */
static void laden(void)
{
    char w[20]; WORD i; short v; const char FAR *z;
    gn = GfxList(gl, 16); gsel = 0;
    for (i = 1; i < gn; i++) {                    /* 30.10.: nach Aufloesung (Pixel), dann Farben, jeweils absteigend */
        GFXINFO t = gl[i]; WORD j = i;
        while (j && ((DWORD)gl[j - 1].w * gl[j - 1].h < (DWORD)t.w * t.h || ((DWORD)gl[j - 1].w * gl[j - 1].h == (DWORD)t.w * t.h && gl[j - 1].farben < t.farben))) { gl[j] = gl[j - 1]; j--; }
        gl[j] = t;
    }
    for (i = 0; i < gn; i++) if (gl[i].flags & GI_GEWAEHLT) gsel = (BYTE)(i + 1);
    ProfileGetStr(ini, "System", "Profil", "AUTO", w, sizeof w);
    prof = (BYTE)((w[0] | 0x20) == 's' ? 1 : (w[0] | 0x20) == 'k' ? 2 : 0);
    ProfileGetStr(ini, "System", "Sound", "SPEAKER", w, sizeof w);
    for (snd = 0; snd < 4 && !gleich(w, sndw[snd]); snd++) ;
    if (snd == 4) snd = 0;
    ProfileGetStr(ini, "System", "SB", "A220 I5 D1", sbeig, sizeof sbeig);
    for (sb = 0; sb < 5 && !gleich(sbeig, sbw[sb]); sb++) ;
    sysk = (BYTE)(ProfileGetInt(ini, "System", "Systemklang", 1) != 0);
    v = ProfileGetInt(ini, "Maus", "Tempo", 3); tempo = (BYTE)(v < 1 || v > 5 ? 2 : v - 1);
    v = ProfileGetInt(ini, "Maus", "Doppelklick", 500); dk = (BYTE)(v < 400 ? 0 : v < 600 ? 1 : 2);
    tausch = (BYTE)(ProfileGetInt(ini, "Maus", "Tasten", 0) != 0);
    rdm = 0; mhw = 0;
    if ((z = instz("RDMAUS.COM")) != 0) {         /* Schalter der RDMAUS-Zeile */
        const char FAR *p = suche(z, zlen(z), "RDMAUS.COM") + 10; WORD k;
        rdm = 1; p = sp(p);
        for (k = 1; k < NM; k++) { WORD j = 0; const char *s = mschalter[k]; while (s[j] && gr(p[j]) == (BYTE)s[j]) j++; if (!s[j] && (p[j] <= ' ')) { mhw = (BYTE)k; break; } }
    }
    tastatur();
    sart = (BYTE)ProfileGetInt(ini, "Schoner", "Art", 0); if (sart > 2) sart = 0;
    v = ProfileGetInt(ini, "Schoner", "Zeit", 300);
    for (szeit = 5; szeit > 0 && zt[szeit] > v; szeit--) ;
    start = an();
    netz = (BYTE)(ProfileGetInt(ini, "Netz", "Zeit", 0) != 0);
    v = ProfileGetInt(ini, "System", "XMS", 256);
    for (xms = 4; xms > 0 && xw[xms] > v; xms--) ;
    cgaf = (BYTE)(ProfileGetInt(ini, "Farben", "CGA", 15) & 15); deskf = (BYTE)(ProfileGetInt(ini, "Farben", "Schreibtisch", 3) & 15); farb0 = (BYTE)(cgaf | deskf << 4);
    mw0 = (BYTE)(tempo | dk << 3 | tausch << 5); gsel0 = gsel; prof0 = prof; snd0 = snd; sb0 = sb; mhw0 = mhw; tast0 = tast; szeit0 = szeit; start0 = start; xms0 = xms;
}

/* ---- Karten fuellen und lesen --------------------------------------------------------------------- */
static void klang_en(HWND d) { BYTE i = radio(d, 121, 4); en(d, 127, (BYTE)(i == 2)); }
static void fuellen(HWND d)
{
    WORD i; char t[24];
    switch (seite) {
    case 0:
        scpy(t, StrGet(hi, 7), sizeof t); ladd(d, 101, t);
        for (i = 0; i < gn; i++) {                /* "VGA 640x480, 16 Farben" */
            char z[40], n[8]; WORD v, k;
            scpy(z, gl[i].name, sizeof z);
            for (k = 0; k < 3; k++) {
                char *q = n + 7; v = k == 0 ? gl[i].w : k == 1 ? gl[i].h : gl[i].farben; *q = 0;
                do *--q = (char)('0' + v % 10); while (v /= 10);
                if (k == 2 && gl[i].farben == 2) break;
                scat(z, k == 0 ? " " : k == 1 ? "x" : ", ", sizeof z); scat(z, q, sizeof z);
            }
            scat(z, StrGet(hi, (WORD)(gl[i].farben == 2 ? 67 : 66)), sizeof z);
            ladd(d, 101, z);
        }
        lsel(d, 101, gsel); DlgCheck(d, (WORD)(102 + prof), 1);
        if (si.grafik) DlgSetText(d, 106, StrGet(hi, 18));
        for (i = 0; i < 16; i++) { scpy(t, StrGet(hi, (WORD)(50 + i)), sizeof t); ladd(d, 107, t); ladd(d, 108, t); }
        lsel(d, 107, cgaf); lsel(d, 108, deskf);
        break;
    case 1:
        DlgCheck(d, (WORD)(121 + snd), 1); DlgCheck(d, 126, sysk);
        for (i = 0; i < 5; i++) ladd(d, 127, sbt[i]);
        if (sb == 5) ladd(d, 127, sbeig);
        lsel(d, 127, sb); klang_en(d);
        break;
    case 2:
        DlgCheck(d, (WORD)(141 + tempo), 1); DlgCheck(d, (WORD)(146 + dk), 1); DlgCheck(d, 149, tausch);
        if (rdm) for (i = 0; i < NM; i++) { scpy(t, StrGet(hi, (WORD)(20 + i)), sizeof t); ladd(d, 153, t); }
        else { scpy(t, StrGet(hi, 16), sizeof t); ladd(d, 153, t); }
        lsel(d, 153, rdm ? mhw : 0); en(d, 153, rdm);
        break;
    case 3:
        DlgCheck(d, (WORD)(161 + tast), 1);
        if (kbart == 2) for (i = 161; i < 164; i++) en(d, i, 0);
        DlgSetText(d, 165, StrGet(hi, (WORD)(kbart == 1 ? 11 : kbart == 2 ? 12 : 14)));
        if (kbart != 1) DlgSetText(d, 166, StrGet(hi, (WORD)(kbart == 2 ? 13 : 15)));
        break;
    case 4:
        DlgCheck(d, (WORD)(181 + sart), 1); DlgCheck(d, (WORD)(184 + szeit), 1);
        break;
    case 5:
        DlgCheck(d, 201, start); DlgCheck(d, 202, netz);
        scpy(t, StrGet(hi, 31), sizeof t); ladd(d, 207, t);
        for (i = 1; i < 5; i++) ladd(d, 207, xt[i]);
        lsel(d, 207, xms);
        break;
    }
}
static void holen(HWND d)
{
    switch (seite) {
    case 0: gsel = lget(d, 101); prof = radio(d, 102, 3); cgaf = (BYTE)lget(d, 107); deskf = (BYTE)lget(d, 108); break;
    case 1: snd = radio(d, 121, 4); sysk = (BYTE)DlgChecked(d, 126); sb = lget(d, 127); break;
    case 2: tempo = radio(d, 141, 5); dk = radio(d, 146, 3); tausch = (BYTE)DlgChecked(d, 149); if (rdm) mhw = lget(d, 153); break;
    case 3: if (kbart != 2) tast = radio(d, 161, 3); break;
    case 4: sart = radio(d, 181, 3); szeit = radio(d, 184, 6); break;
    case 5: start = (BYTE)DlgChecked(d, 201); netz = (BYTE)DlgChecked(d, 202); xms = lget(d, 207); break;
    }
}

/* ---- Speichern: 0 ok, sonst Text-Nr. der Warnung ------------------------------------------------- */
static BYTE neurd, neupc, gneu, rest = 15;
static WORD speichern(void)
{
    char t[8]; WORD e;
    neurd = neupc = gneu = 0; csneu = 0;
    if ((gsel != gsel0 || prof != prof0) && !test &&   /* Grafik geaendert: Hinweis auf den Neustart, Abbrechen verwirft sie (30.10.2026) */
        MsgBox(GetActive(), StrGet(hi, 40), StrGet(hi, 1), MB_OKCANCEL | MB_INFO) != ID_OK) { gsel = gsel0; prof = prof0; }
    if (gsel != gsel0 || prof != prof0) {         /* alte Werte fuer "Behalten?" merken */
        char a[16];
        ProfileGetStr(ini, "System", "Grafik", "AUTO", a, sizeof a); ProfileSetStr(ini, "System", "GrafikAlt", a);
        ProfileGetStr(ini, "System", "Profil", "AUTO", a, sizeof a); ProfileSetStr(ini, "System", "ProfilAlt", a);
        GfxSelect(gsel ? gl[gsel - 1].typ : 0); gneu = 1;
        ProfileSetStr(ini, "System", "Profil", prof == 1 ? "SCHNELL" : prof == 2 ? "KOMFORT" : "AUTO");
    }
    if ((BYTE)(cgaf | deskf << 4) != farb0) { setze("Farben", "CGA", cgaf); setze("Farben", "Schreibtisch", deskf); neurd = 1; }   /* gilt nach dem Neustart von RD2 */
    ProfileSetStr(ini, "System", "Sound", sndw[snd]);
    if (sb < 5) ProfileSetStr(ini, "System", "SB", sbw[sb]);
    if (snd != snd0 || sb != sb0) neurd = 1;
    ProfileSetStr(ini, "System", "Systemklang", sysk ? "1" : "0");
    setze("Maus", "Tempo", (short)(tempo + 1)); setze("Maus", "Doppelklick", dz[dk]); setze("Maus", "Tasten", tausch);
    if ((BYTE)(tempo | dk << 3 | tausch << 5) != mw0) neurd = 1;   /* Tempo, Doppelklick, Tasten liest RD2 beim Start */
    if (rdm && mhw != mhw0) {
        schalter("RDMAUS.COM", mschalter[mhw]);
        if (mhw) ProfileSetStr(ini, "System", "MausHW", mhww[mhw]);
        neupc = 1;
    }
    if (kbart != 2 && tast != tast0) {
        static const char *const kw[] = { "DE", "US", "PC10" };
        char a[80];
        ProfileSetStr(ini, "System", "Tastatur", kw[tast]);
        if (tast == 2) { scpy(a, home, sizeof a); scat(a, "PC10.KBD", sizeof a); } else scpy(a, kw[tast], sizeof a);
        schalter("RDKEYB.COM", a);
        if (!kbm || !kbsetz(tast)) neupc = 1;
    }
    t[0] = (char)('0' + sart); t[1] = 0; ProfileSetStr(ini, "Schoner", "Art", t);
    setze("Schoner", "Zeit", zt[szeit]); if (szeit != szeit0) neurd = 1;
    ProfileSetStr(ini, "Netz", "Zeit", netz ? "1" : "0");
    if (xms != xms0) { setze("System", "XMS", xw[xms]); neurd = 1; }
    if (start != start0 && (e = schalten(start)) != 0) { start = start0; return 3; }
    if (csneu) {
        if (test) { WORD i = 0, j; char p[80]; while (i < nein) { for (j = 0; i < nein && ein[i] != '\n' && j < 78; i++) if (ein[i] != '\r') p[j++] = ein[i];
                    p[j] = 0; i++; DbgPrint(p); } }   /* neue CONFIG.SYS Zeile fuer Zeile */
        if (FileSaveSafe(cs, ein, nein)) return 2;
        start0 = start;
    }
    return 0;
}
static void starte(HWND d, LPCSTR n, LPCSTR a, BYTE dos)    /* Programm aus dem RD2-Verzeichnis */
{
    char p[80]; scpy(p, home, sizeof p); scat(p, n, sizeof p);
    if (!gibts(p)) { MsgBox(d, StrGet(hi, 17), StrGet(hi, 1), MB_OK | MB_WARN); return; }
    if (dos) RunDos(p, a, RD_DOS_KEEP); else TaskExec(p, a, SW_SHOW);
}
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD e;
    if (msg == M_INITDIALOG) {
        en(d, (WORD)(ID_S1 + seite), 0); fuellen(d);
        if (test) { char t[24]; scpy(t, "OPTIONEN: Karte ", sizeof t); t[16] = (char)('1' + seite); t[17] = 0; DbgPrint(t); }
        return FALSE;
    }
    if (msg != M_COMMAND) return FALSE;
    if (p1 >= ID_S1 && p1 < ID_S1 + NS) { holen(d); DialogEnd(d, (short)p1); return TRUE; }
    if (p1 == ID_OK) {
        holen(d);
        e = speichern();
        if (test) DbgHex("OPTIONEN: gespeichert, Fehler", e);
        if (e) { MsgBox(d, StrGet(hi, e), StrGet(hi, 1), MB_OK | MB_WARN); if (seite == 5) DlgCheck(d, 201, start); return TRUE; }
        if (!test && !gneu) {
            char m[200]; scpy(m, StrGet(hi, 4), sizeof m);
            if (neurd) scat(m, StrGet(hi, 5), sizeof m);
            if (neupc) scat(m, StrGet(hi, 6), sizeof m);
            MsgBox(d, m, StrGet(hi, 1), MB_OK | MB_INFO);
        }
        DialogEnd(d, ID_OK); return TRUE;
    }
    if (seite == 1 && p1 >= 121 && p1 <= 124) { klang_en(d); return FALSE; }
    if (seite == 2 && p1 == 150) {                /* Doppelklick-Test mit der gewaehlten Zeit */
        DWORD n = TickCount(), g = (DWORD)dz[radio(d, 146, 3)];
        DlgSetText(d, 151, StrGet(hi, (WORD)(letzt && n - letzt < g ? 8 : 9)));
        letzt = letzt && n - letzt < g ? 0 : n;
        return TRUE;
    }
    if (seite == 4 && p1 == 190) {                /* Vorschau: Art gilt sofort */
        char t[2]; t[0] = (char)('0' + radio(d, 181, 3)); t[1] = 0; ProfileSetStr(ini, "Schoner", "Art", t);
        starte(d, "SCHONER.EXE", "S", 0); return TRUE;
    }
    if (seite == 5 && p1 == 204) { starte(d, "RDNET.EXE", "", 0); return TRUE; }
    if (seite == 5 && p1 == 205) { holen(d); starte(d, "RD2SETUP.EXE", "", 1); laden(); DialogEnd(d, (short)(ID_S1 + 5)); return TRUE; }
    (void)p2;
    return FALSE;
}
/* "B": nach dem Neustart mit neuer Grafik (die Shell startet es, solange [System] GrafikAlt= gesetzt ist) */
static void zaehler(HWND d)
{
    char m[80]; scpy(m, StrGet(hi, 42), sizeof m); zahl(m + slen(m), rest); scat(m, StrGet(hi, 43), sizeof m); DlgSetText(d, 172, m);
}
static BOOL RDCALLBACK bproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    (void)p2;
    if (msg == M_INITDIALOG) { zaehler(d); TimerSet(d, 1, 1000); if (test) DbgPrint("OPTIONEN: Behalten?"); return FALSE; }
    if (msg == M_TIMER) { if (!--rest) DialogEnd(d, ID_NO); else zaehler(d); return TRUE; }
    if (msg == M_COMMAND && (p1 == ID_YES || p1 == ID_NO)) { DialogEnd(d, (short)p1); return TRUE; }
    return FALSE;
}
static void neustart(void) { if (test) DbgPrint("OPTIONEN: RD2 startet neu"); RunDos("", "", RD_DOS_NEU); }
static void behalten(void)
{
    char a[16];
    if (DialogRun(hi, 17, 0, bproc, 0) == ID_YES) { ProfileSetStr(ini, "System", "GrafikAlt", ""); if (test) DbgPrint("OPTIONEN: Grafik behalten"); return; }
    ProfileGetStr(ini, "System", "GrafikAlt", "AUTO", a, sizeof a); ProfileSetStr(ini, "System", "Grafik", a);
    ProfileGetStr(ini, "System", "ProfilAlt", "AUTO", a, sizeof a); ProfileSetStr(ini, "System", "Profil", a);
    ProfileSetStr(ini, "System", "GrafikAlt", "");
    if (test) DbgPrint("OPTIONEN: alte Grafik");
    neustart();
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    short r;
    (void)show; hi = inst;
    KernInfo(&si); scpy(home, si.home, sizeof home); if (slen(home) && home[slen(home) - 1] != '\\') scat(home, "\\", sizeof home);
    scpy(ini, home, sizeof ini); scat(ini, "RD2.INI", sizeof ini);   /* im RD2-Verzeichnis wie RD2SETUP */
    ord[0] = (char)('@' + (si.bootlw ? si.bootlw : 3)); ord[1] = ':'; ord[2] = '\\'; ord[3] = 0;
    if (cmd && cmd[0] == 'B') { test = (BYTE)(cmd[1] == 'T'); behalten(); return 0; }
    if (cmd && cmd[0] == 'T') {
        test = 1; cmd++;
        if (*cmd >= '1' && *cmd <= '0' + NS) seite = (BYTE)(*cmd++ - '1');
        while (*cmd == ' ') cmd++;
        if (*cmd) scpy(ord, cmd, sizeof ord);
    }
    if (!(hm = MemAlloc(3L * CMAX, MEM_MOVABLE)) || !(ein = (char FAR *)MemLock(hm))) return 1;
    aus = ein + CMAX; ae = aus + CMAX;
    scpy(cs, ord, sizeof cs); scat(cs, "AUTOEXEC.BAT", sizeof cs); nae = lesen(cs, ae);
    scpy(cs, ord, sizeof cs); scat(cs, "CONFIG.SYS", sizeof cs); nein = lesen(cs, ein);
    laden();
    if (test) DbgPrint(start ? "OPTIONEN: bereit, Start an" : "OPTIONEN: bereit, Start aus");
    for (;;) {
        r = DialogRun(inst, (WORD)(11 + seite), 0, dproc, 0);
        if (r >= ID_S1 && r < ID_S1 + NS) { seite = (BYTE)(r - ID_S1); continue; }
        break;
    }
    MemUnlock(hm); MemFree(hm);
    if (gneu) neustart();                          /* neue Grafik gilt sofort (beim OK bestaetigt): RD2 neu starten, danach "Behalten?" */
    return 0;
}
