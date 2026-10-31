/* RDTRON.C (RDTRON.EXE) - H-Tron fuer RetroDesk 2 (Open Watcom, Modell Medium, 8086)
 *
 * Nach H-TRON von RobertK (2016, Hercules, Turbo C) und der CGA/EGA/VGA/GEM-Fassung von
 * MRalser (2026, https://github.com/marralser/H-TRON). Spielregeln, Spielfeld-Logik und
 * Computergegner (AIComputerPlayer) wie im Original; neu geschrieben fuer die RD2-API:
 * Fenster statt Vollbild, M_TIMER statt delay(), M_KEY statt eigenem INT-9-Handler,
 * JoyRead (Gameport) statt Amstrad-Rohcodes, GetDC/Frame/FillRect statt BGI.
 *
 * Spielfeld passt sich der Grafikart an (CGA 640x200 bis VESA 1024x768, Seitenverhaeltnis
 * aus CAPS_ASPX/ASPY). Bis 4 Farben: Spieler 2 zeichnet gefuellte Bloecke, damit man die
 * Spuren auch auf CGA/Hercules/VGA mono unterscheidet.
 *
 * Steuerung: Spieler 1 A/D, Spieler 2 Pfeil links/rechts (bei einem Spieler gehen beide),
 * Joystick A = Spieler 2 (bzw. der Mensch), Joystick B = Spieler 1. Leertaste/Feuer/Klick:
 * Runde starten, P: Pause, Esc: Spiel abbrechen (im Titelbild: beenden), F2: neues Spiel.
 *
 * Befehlszeile "T": Testlauf (Demo bis 5 Punkte, schnell, Meldungen per DbgPrint, Exitcode
 * = Punkte Spieler 1 * 16 + Punkte Spieler 2). test/rdtron.sh */
#include "RD2.H"

#define MAXC 100
#define MAXR 100
#define SIEG 5
#define AI_WEIT 15

#define CM_NEU     101
#define CM_PAUSE   102
#define CM_ENDE    109
#define CM_DEMO    110
#define CM_EINS    111
#define CM_ZWEI    112
#define CM_LANGSAM 120
#define CM_NORMAL  121
#define CM_SCHNELL 122
#define CM_KLANG   130
#define CM_MOTOR   131
#define CM_HILFE   140
#define CM_INFO    141

enum { S_TITEL, S_WARTEN, S_LAUF, S_PAUSE, S_CRASH, S_ENDE };

static HINST hi; static HWND fe; static HMENU mn;
static BYTE feld[MAXC * MAXR];                    /* 0 frei, 1 Spieler 1, 2 Spieler 2, 3 Wand */
static short cols, rows, bw, bh, ox, oy, sh, cw, chh, th;
static short px[2], py[2], pdir[2], pts[2]; static BYTE crash[2];
static short dreh[2];                             /* gesammelte Drehungen: + links, - rechts */
static BYTE spieler = 1, zustand = S_TITEL, klang = 1, motor = 1, test, wenigf, joy[2], joyalt[2];
static WORD tempo = 50, warte;
static COLOR farbe[4];
static char ini[80];
static DWORD zufall = 1;

static short rnd(short n) { zufall = zufall * 1103515245UL + 12345; return (short)((zufall >> 16) % (WORD)n); }
#define F(x, y) feld[(WORD)(y) * cols + (x)]

static char *scat(char *d, LPCSTR s) { while (*d) d++; while ((*d = *s++) != 0) d++; return d; }
static char *zahl(char *d, WORD v)
{
    char t[6]; int i = 5; t[i] = 0;
    while (*d) d++;
    do t[--i] = (char)('0' + v % 10); while (v /= 10);
    while ((*d = t[i++]) != 0) d++;
    return d;
}

/* ---- Zeichnen ------------------------------------------------------------- */
static void block(HDC d, short x, short y)
{
    RECT r; BYTE a = F(x, y);
    r.l = (short)(ox + x * bw); r.t = (short)(oy + y * bh); r.r = (short)(r.l + bw); r.b = (short)(r.t + bh);
    if (a == 2 && wenigf) { SetBrush(d, farbe[2], PAT_SOLID); FillRect(d, &r); }
    else { SetPen(d, farbe[a]); Frame(d, &r); }   /* wie das Original: Rechteck in Spielerfarbe */
}
static void wand(HDC d)
{
    RECT r;
    SetPen(d, farbe[3]);
    r.l = (short)(ox + bw / 2); r.t = (short)(oy + bh / 2);
    r.r = (short)(ox + (cols - 1) * bw + bw / 2 + 1); r.b = (short)(oy + (rows - 1) * bh + bh / 2 + 1);
    Frame(d, &r);
}
static void punkte(HDC d)
{
    char s[40]; RECT r; short w; WORD t1, t2;
    r.l = 0; r.t = 0; r.r = cw; r.b = sh;
    SetBrush(d, C_BLACK, PAT_SOLID); FillRect(d, &r);
    r.l = 2; r.t = 1; r.r = (short)(cw - 2); r.b = (short)(sh - 1);
    SetPen(d, farbe[3]); Frame(d, &r);
    t1 = spieler == 0 ? 3 : spieler == 1 ? 5 : 7; t2 = (WORD)(t1 + 1);
    SetBkMode(d, BK_TRANSPARENT);
    s[0] = 0; zahl(scat(s, StrGet(hi, t1)), (WORD)pts[0]);
    SetTextColor(d, farbe[1]); TextOut(d, 12, 3, s, -1);
    s[0] = 0; zahl(scat(s, StrGet(hi, t2)), (WORD)pts[1]);
    w = TextWidth(d, s, -1); SetTextColor(d, farbe[2]); TextOut(d, (short)(cw - 12 - w), 3, s, -1);
    { LPCSTR k = StrGet(hi, 9); w = TextWidth(d, k, -1); SetTextColor(d, farbe[3]); TextOut(d, (short)((cw - w) / 2), 3, k, -1); }
}
static void hinweis(HDC d, LPCSTR a, LPCSTR b)    /* Meldung mitten im Spielfeld */
{
    RECT r; short wa = TextWidth(d, a, -1), wb = b ? TextWidth(d, b, -1) : 0, w = wa > wb ? wa : wb, n = (short)(b ? 2 : 1), y;
    r.l = (short)((cw - w) / 2 - 8); r.r = (short)((cw + w) / 2 + 8);
    y = (short)(oy + (rows * bh - n * th) / 2); r.t = (short)(y - 5); r.b = (short)(y + n * th + 5);
    SetBrush(d, C_BLACK, PAT_SOLID); FillRect(d, &r); SetPen(d, farbe[3]); Frame(d, &r);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, farbe[3]);
    TextOut(d, (short)((cw - wa) / 2), y, a, -1);
    if (b) TextOut(d, (short)((cw - wb) / 2), (short)(y + th), b, -1);
}
static void meldung(HDC d)
{
    char s[48];
    switch (zustand) {
    case S_TITEL:  hinweis(d, StrGet(hi, 30), StrGet(hi, 31)); break;
    case S_WARTEN: if (spieler) hinweis(d, StrGet(hi, 10), 0); break;
    case S_PAUSE:  hinweis(d, StrGet(hi, 11), 0); break;
    case S_ENDE:
        s[0] = 0; scat(s, StrGet(hi, 12)); zahl(s, (WORD)pts[0]); scat(s, " : "); zahl(s, (WORD)pts[1]); scat(s, " - ");
        scat(s, StrGet(hi, pts[0] == pts[1] ? 19 : (WORD)(13 + spieler * 2 + (pts[1] > pts[0]))));
        hinweis(d, s, StrGet(hi, 20));
        break;
    }
}
static void alles(HDC d)
{
    short x, y; RECT r;
    r.l = 0; r.t = sh; r.r = cw; r.b = chh;
    SetBrush(d, C_BLACK, PAT_SOLID); FillRect(d, &r);
    punkte(d);
    if (zustand != S_TITEL) {
        wand(d);
        for (y = 1; y < rows - 1; y++) for (x = 1; x < cols - 1; x++) if (F(x, y)) block(d, x, y);
    }
    meldung(d);
}
static void neu_zeigen(void) { InvalidateRect(fe, 0, FALSE); }

/* ---- Spiel ---------------------------------------------------------------- */
static void runde(void)                          /* InitPlayfield */
{
    short x, y;
    for (y = 0; y < rows; y++) for (x = 0; x < cols; x++) F(x, y) = (BYTE)((x == 0 || y == 0 || x == cols - 1 || y == rows - 1) ? 3 : 0);
    px[0] = (short)(cols * 13 / 80); px[1] = (short)(cols - 1 - px[0]); py[0] = py[1] = (short)(rows / 2);
    pdir[0] = 0; pdir[1] = 180; crash[0] = crash[1] = 0; dreh[0] = dreh[1] = 0;
    F(px[0], py[0]) = 1; F(px[1], py[1]) = 2;
    zustand = S_WARTEN; warte = (WORD)(1000 / tempo);   /* Demo: Runde startet nach 1 s von selbst */
    if (test) warte = 2;
    neu_zeigen();
}
static void spiel(void)
{
    SYSINFO si; JOYSTATE js; WORD i;
    pts[0] = pts[1] = 0;
    KernInfo(&si);
    for (i = 0; i < 2; i++) { joy[i] = (BYTE)(si.modus && JoyRead(i, &js) == 0); joyalt[i] = 0; }   /* nur abfragen, wenn ein Gameport antwortet */
    runde();
    if (test) DbgPrint("HTRON: Spiel");
}
static void drehe(short *dir, short n)           /* n > 0 links, n < 0 rechts, je 90 Grad */
{
    while (n > 0) { *dir += 90; if (*dir >= 360) *dir = 0; n--; }
    while (n < 0) { *dir -= 90; if (*dir < 0) *dir = 270; n++; }
}
static short weit(short x, short y, short dx, short dy, short max)   /* AICheckDistance... */
{
    short d = 1;
    for (;;) {
        short nx = (short)(x + dx * d), ny = (short)(y + dy * d);
        if (nx < 0 || ny < 0 || nx >= cols || ny >= rows || F(nx, ny) || d > max) return d;
        d++;
    }
}
static const short DX[4] = { 1, 0, -1, 0 }, DY[4] = { 0, -1, 0, 1 };   /* 0 rechts, 90 hoch, 180 links, 270 runter */
static void computer(short i)                    /* AIComputerPlayer */
{
    short k = (short)(pdir[i] / 90), l = (short)((k + 1) & 3), r = (short)((k + 3) & 3), max = (short)(AI_WEIT - rnd(10));
    short vorn = weit(px[i], py[i], DX[k], DY[k], max), li = weit(px[i], py[i], DX[l], DY[l], max), re = weit(px[i], py[i], DX[r], DY[r], max);
    if (vorn >= li && vorn >= re) return;          /* freie Fahrt */
    if (li > re) drehe(&pdir[i], 1);
    else if (li < re) drehe(&pdir[i], -1);
    else drehe(&pdir[i], rnd(100) < 50 ? 1 : -1);
}
static void joystick(void)
{
    JOYSTATE js; WORD n; BYTE s;
    for (n = 0; n < 2; n++) {
        if (!joy[n] || JoyRead(n, &js)) continue;
        s = (BYTE)((js.x < -50 ? 1 : js.x > 50 ? 2 : 0) | (js.tasten & 3 ? 4 : 0));
        if (zustand == S_LAUF) {                    /* A = Spieler 2 (Mensch), B = Spieler 1 */
            short p = (short)(n == 0 ? 1 : 0);
            if ((s & 1) && !(joyalt[n] & 1)) dreh[p]++;
            if ((s & 2) && !(joyalt[n] & 2)) dreh[p]--;
        } else if ((s & 4) && !(joyalt[n] & 4)) PostMessage(fe, M_KEY, ' ', 0);
        joyalt[n] = s;
    }
}
/* ---- Klang: kleine Tonfolgen (MIDI-Noten, Dauer in Spieltakten) ueber SndNote, sonst SndBeep ---- */
typedef struct { BYTE n, t; } TON;               /* n 0 = Pause, t 0 = Ende */
static const TON t_start[] = { {72,1},{0,1},{72,1},{0,1},{84,2},{0,0} };
static const TON t_dreh[]  = { {79,1},{0,0} };
static const TON t_crash[] = { {64,1},{58,1},{52,1},{46,1},{40,1},{34,2},{0,1},{0,0} };
static const TON t_beide[] = { {52,1},{40,1},{52,1},{40,1},{28,3},{0,1},{0,0} };
static const TON t_sieg[]  = { {72,2},{76,2},{79,2},{84,5},{0,1},{0,0} };
static const TON t_verl[]  = { {67,3},{63,3},{60,3},{55,6},{0,1},{0,0} };
static const TON t_remis[] = { {72,2},{67,4},{0,1},{0,0} };
static const TON *seq; static BYTE seqt, seqp, notean, mtakt;
static WORD hz(BYTE n)
{
    static const WORD b[12] = { 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494 };
    WORD f = b[n % 12]; short o = (short)(n / 12 - 5);
    while (o > 0) { f <<= 1; o--; }
    while (o < 0) { f >>= 1; o++; }
    return f;
}
static void ton_aus(void) { if (notean) { SndNoteOff(0); notean = 0; } }
static void ton(BYTE n)
{
    ton_aus();
    if (!n) return;
    if (SndNote(0, n, 100) == 0) notean = 1;
    else SndBeep(hz(n), tempo);                   /* ohne Notentreiber: Piepser so lang wie ein Takt */
}
static void spiele(const TON *t, BYTE prio)      /* hoehere Prioritaet unterbricht, niedrigere wartet nicht */
{
    if (!klang || (seq && prio < seqp)) return;
    seq = t; seqt = 0; seqp = prio;
}
static void still(void) { seq = 0; ton_aus(); }
static void klang_takt(void)
{
    if (!klang) return;
    if (seq) {
        if (seqt) { seqt--; return; }
        if (seq->t) { ton(seq->n); seqt = (BYTE)(seq->t - 1); seq++; return; }
        seq = 0;
    }
    if (zustand == S_LAUF && motor) {             /* Motor: tiefes Brummen im Wechsel, jeder zweite Takt still */
        mtakt++;
        if (mtakt & 1) ton((BYTE)(mtakt & 2 ? 31 : 33)); else ton_aus();
    } else ton_aus();
}
static void schritt(void)                        /* ein Takt: Eingaben, Computer, MovePlayers */
{
    HDC d; short i, altd[2];
    altd[0] = pdir[0]; altd[1] = pdir[1];
    if (spieler >= 1) drehe(&pdir[1], dreh[1]);
    if (spieler == 2) drehe(&pdir[0], dreh[0]);
    if (spieler == 1) drehe(&pdir[1], dreh[0]);    /* ein Spieler: A/D lenken auch den Menschen */
    dreh[0] = dreh[1] = 0;
    if (spieler < 2) computer(0);
    if (spieler == 0) computer(1);
    if ((spieler == 2 && pdir[0] != altd[0]) || (spieler && pdir[1] != altd[1])) spiele(t_dreh, 1);   /* nur Menschen klicken */
    for (i = 0; i < 2; i++) {
        short k = (short)(pdir[i] / 90);
        px[i] = (short)(px[i] + DX[k]); py[i] = (short)(py[i] + DY[k]);
        if (F(px[i], py[i])) crash[i] = 1;
    }
    if (px[0] == px[1] && py[0] == py[1]) crash[0] = crash[1] = 1;   /* Zusammenstoss */
    if (crash[0] && !crash[1]) pts[1]++;
    if (crash[1] && !crash[0]) pts[0]++;
    d = GetDC(fe);
    for (i = 0; i < 2; i++) if (!crash[i]) { F(px[i], py[i]) = (BYTE)(i + 1); block(d, px[i], py[i]); }
    if (crash[0] || crash[1]) punkte(d);
    ReleaseDC(fe, d);
    if (crash[0] || crash[1]) {
        spiele(crash[0] && crash[1] ? t_beide : t_crash, 2);
        zustand = S_CRASH; warte = (WORD)(250 / tempo + 1);
        if (test) { char s[32]; s[0] = 0; scat(s, "HTRON: Runde "); zahl(s, (WORD)pts[0]); scat(s, ":"); zahl(s, (WORD)pts[1]); DbgPrint(s); }
    }
}
static void takt(void)
{
    if (zustand == S_TITEL || zustand == S_ENDE || zustand == S_WARTEN) { if (joy[0] || joy[1]) joystick(); }
    klang_takt();
    switch (zustand) {
    case S_WARTEN: if (!spieler && !--warte) { zustand = S_LAUF; spiele(t_start, 1); neu_zeigen(); } break;
    case S_LAUF:   if (joy[0] || joy[1]) joystick(); schritt(); break;
    case S_CRASH:
        if (--warte) break;
        if (pts[0] < SIEG && pts[1] < SIEG) { runde(); break; }
        zustand = S_ENDE; neu_zeigen();
        spiele(pts[0] == pts[1] ? t_remis : spieler == 1 && pts[0] > pts[1] ? t_verl : t_sieg, 3);
        if (test) { DbgPrint("HTRON: Ende"); PostMessage(fe, M_CLOSE, 0, 0); }
        break;
    }
}
static void starten(void)                        /* Leertaste, Feuer, Klick */
{
    if (zustand == S_TITEL || zustand == S_ENDE) spiel();
    else if (zustand == S_WARTEN) { zustand = S_LAUF; spiele(t_start, 1); neu_zeigen(); }
    else if (zustand == S_PAUSE) { zustand = S_LAUF; neu_zeigen(); }
}
static void pause(BYTE an)
{
    if (an && zustand == S_LAUF) { zustand = S_PAUSE; still(); neu_zeigen(); }
    else if (!an && zustand == S_PAUSE) { zustand = S_LAUF; neu_zeigen(); }
}

/* ---- Einstellungen und Menue --------------------------------------------- */
static void haken(void)
{
    MenuCheck(mn, CM_DEMO, spieler == 0); MenuCheck(mn, CM_EINS, spieler == 1); MenuCheck(mn, CM_ZWEI, spieler == 2);
    MenuCheck(mn, CM_LANGSAM, tempo == 70); MenuCheck(mn, CM_NORMAL, tempo == 50); MenuCheck(mn, CM_SCHNELL, tempo == 35);
    MenuCheck(mn, CM_KLANG, klang); MenuCheck(mn, CM_MOTOR, motor);
}
static void merken(void)
{
    char s[6];
    if (test) return;
    s[0] = 0; zahl(s, spieler); ProfileSetStr(ini, "HTron", "Spieler", s);
    s[0] = 0; zahl(s, tempo);   ProfileSetStr(ini, "HTron", "Tempo", s);
    s[0] = 0; zahl(s, klang);   ProfileSetStr(ini, "HTron", "Klang", s);
    s[0] = 0; zahl(s, motor);   ProfileSetStr(ini, "HTron", "Motor", s);
}
static void setze_tempo(WORD t) { tempo = t; TimerSet(fe, 1, test ? 20 : tempo); }
static void befehl(WORD id)
{
    switch (id) {
    case CM_NEU: spiel(); break;
    case CM_PAUSE: if (zustand == S_PAUSE) pause(0); else pause(1); break;
    case CM_ENDE: PostMessage(fe, M_CLOSE, 0, 0); break;
    case CM_DEMO: case CM_EINS: case CM_ZWEI:
        spieler = (BYTE)(id - CM_DEMO); zustand = S_TITEL; pts[0] = pts[1] = 0; neu_zeigen(); break;
    case CM_LANGSAM: setze_tempo(70); break;
    case CM_NORMAL:  setze_tempo(50); break;
    case CM_SCHNELL: setze_tempo(35); break;
    case CM_KLANG: klang ^= 1; if (!klang) still(); break;
    case CM_MOTOR: motor ^= 1; break;
    case CM_HILFE: pause(1); MsgBox(fe, StrGet(hi, 40), StrGet(hi, 1), MB_OK | MB_INFO); return;
    case CM_INFO:  pause(1); MsgBox(fe, StrGet(hi, 41), StrGet(hi, 1), MB_OK | MB_INFO); return;
    default: return;
    }
    if (id >= CM_DEMO) { haken(); merken(); }
}

static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    PAINT ps;
    switch (msg) {
    case M_PAINT: BeginPaint(w, &ps); alles(ps.dc); EndPaint(w, &ps); return 0;
    case M_TIMER: if (p1 == 1) takt(); return 0;
    case M_KEY:
        if (zustand == S_PAUSE && p1 != K_ESC && p1 != K_F1 && p1 != K_F2) { pause(0); return 0; }   /* wie das Original: beliebige Taste beendet die Pause */
        switch (p1) {
        case 'a': case 'A': if (zustand == S_LAUF) dreh[0]++; return 0;
        case 'd': case 'D': if (zustand == S_LAUF) dreh[0]--; return 0;
        case K_LEFT:  if (zustand == S_LAUF) dreh[1]++; return 0;
        case K_RIGHT: if (zustand == S_LAUF) dreh[1]--; return 0;
        case ' ': case K_ENTER: starten(); return 0;
        case 'p': case 'P': befehl(CM_PAUSE); return 0;
        case K_F1: befehl(CM_HILFE); return 0;
        case K_F2: spiel(); return 0;
        case K_ESC:
            if (zustand == S_TITEL) PostMessage(w, M_CLOSE, 0, 0);
            else { zustand = S_TITEL; pts[0] = pts[1] = 0; neu_zeigen(); }
            return 0;
        }
        break;
    case M_CHAR: return 0;
    case M_BUTTONDOWN: if ((p1 & 0xFF) == 1) starten(); return 0;
    case M_ACTIVATE: if (!p1) pause(1); break;    /* andere Fenster vorn: Pause */
    case M_SIZE: if (p1 == SIZE_MIN) pause(1); break;
    case M_COMMAND: befehl(p1); return 0;
    case M_DESTROY: TimerKill(w, 1); still(); PostQuit((WORD)(pts[0] * 16 + pts[1])); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; SYSINFO si; RECT r, c; short W, H, ax, ay, n, fw, fh, mh;
    (void)show; hi = inst; test = (BYTE)(cmd && cmd[0] == 'T');
    KernInfo(&si); for (n = 0; si.home[n] && n < 66; n++) ini[n] = si.home[n];
    if (n && ini[n - 1] != '\\') ini[n++] = '\\';
    ini[n] = 0; scat(ini, "RD2.INI");
    spieler = (BYTE)ProfileGetInt(ini, "HTron", "Spieler", 1); if (spieler > 2) spieler = 1;
    tempo = (WORD)ProfileGetInt(ini, "HTron", "Tempo", 50); if (tempo != 70 && tempo != 35) tempo = 50;
    klang = (BYTE)(ProfileGetInt(ini, "HTron", "Klang", 1) != 0);
    motor = (BYTE)(ProfileGetInt(ini, "HTron", "Motor", 1) != 0);
    if (test) { spieler = 0; klang = 0; }
    zufall = TickCount() ^ 0x5A5AUL;               /* randomize() */

    d = GetDC(0);
    W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); th = GetCaps(d, CAPS_TEXTH);
    ax = GetCaps(d, CAPS_ASPX); ay = GetCaps(d, CAPS_ASPY); n = GetCaps(d, CAPS_FARBEN);
    wenigf = (BYTE)(n <= 4);
    farbe[0] = C_BLACK;
    farbe[1] = RGB(64, 64, 255);                   /* direkt, nicht ColorNearest: dessen Ergebnis (85,85,255) wird beim Zeichnen wieder Weiss */   /* hellblau (LIGHTBLUE), auch mit aelteren 16-Farben-Treibern */
    farbe[2] = RGB(255, 255, 64);  /* gelb (YELLOW) */
    farbe[3] = C_WHITE;
    if (n <= 2) farbe[1] = farbe[2] = C_WHITE;
    ReleaseDC(0, d);
    if (ax <= 0) ax = 100;
    if (ay <= 0) ay = 100;
    bw = (short)(W >= 800 ? W / 80 : 8);
    bh = (short)(((long)bw * 3 * ax + 2L * ay) / (4L * ay));   /* 8x6 bei quadratischen Punkten */
    if (bh < 2) bh = 2;
    sh = (short)(th + 6); mh = (short)(th + 4);

    wc.name = "HTron"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = C_BLACK;
    if (!RegisterClass(&wc)) return 1;
    fe = CreateWindow("HTron", StrGet(inst, 1), WS_TITLE | WS_CLOSE | WS_MIN | WS_BORDER, 0, mh, 200, 100, 0, 1, inst);
    if (!fe) return 2;
    GetWindowRect(fe, &r); GetClientRect(fe, &c);  /* Rahmen ausrechnen, dann Spielfeld so gross wie moeglich */
    fw = (short)(r.r - r.l - c.r); fh = (short)(r.b - r.t - c.b);
    ox = 2; oy = (short)(sh + 1);
    cols = (short)((W - fw - 2 * ox) / bw); if (cols > MAXC) cols = MAXC;
    rows = (short)((H - mh - fh - oy - 4 - (H >= 300 ? th : 0)) / bh); if (rows > MAXR) rows = MAXR;
    if (cols < 20 || rows < 15) { MsgBox(0, StrGet(inst, 2), StrGet(inst, 1), MB_OK | MB_ERROR); DestroyWindow(fe); return 3; }
    cw = (short)(cols * bw + 2 * ox); chh = (short)(oy + rows * bh + 2);
    MoveWindow(fe, (short)((W - cw - fw) / 2), mh, (short)(cw + fw), (short)(chh + fh), FALSE);
    mn = MenuLoad(inst, 1); MenuSet(fe, mn); haken();
    ShowWindow(fe, SW_SHOW); SetFocus(fe);
    TimerSet(fe, 1, test ? 20 : tempo);
    if (test) { DbgHex("HTRON: Feld", MAKELONG(cols, rows)); DbgHex("HTRON: Block", MAKELONG(bw, bh)); spiel(); }
    while (GetMessage(&m)) DispatchMessage(&m);
    MenuFree(mn);
    return (short)(pts[0] * 16 + pts[1]);
}
