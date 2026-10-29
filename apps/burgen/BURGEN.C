/* BURGEN.C - Burgenkampf: Artillerie-Spiel fuer RetroDesk 2 nach dem Vorbild von Ballerburg (1987).
 *
 * Zwei Burgen auf Bergen. Wer am Zug ist, kauft ein (Kanone, Kugeln, Pulver, Mauern, Bergwerk),
 * setzt die Steuern und schiesst einmal: Kanone, Winkel und Ladung, der Wind treibt die Kugel ab.
 * Gewonnen hat, wer den Koenig der anderen Burg trifft. Verloren hat auch, wessen Volk den Koenig
 * stuerzt (Volk unter 200) oder wer nicht mehr schiessen und nichts mehr kaufen kann.
 *
 * Gegner: Computer (Leicht, Mittel, Schwer), zwei Spieler an einem PC, Netz (TCP 2626 ueber RDTCP.MOD:
 * ein PC eroeffnet, der andere tritt mit dessen Adresse bei; kein Internet noetig).
 *
 * Rechnen: Welt 640 x 400 Einheiten. Flugbahn, Schaden und Zufall nur mit ganzen Zahlen (Festkomma
 * 16.16), damit beide Rechner im Netz Schritt fuer Schritt dasselbe rechnen - egal ob 8088 oder 386,
 * mit oder ohne 8087. Uebertragen werden nur die Eingaben (Kauf, Steuer, Schuss), nach jedem Zug
 * vergleichen beide eine Pruefsumme. Der Koprozessor hilft dort, wo er sich lohnt: beim Zielen des
 * Computers (KIFPU.C, geschlossene Loesung der Wurfparabel statt Suche mit Probeschuessen).
 *
 * Grafik: die Welt wird auf das Fenster skaliert (alle RD2-Grafikarten, Groesse veraenderbar).
 * 1 Bit (CGA, Hercules, VGA mono): Muster; 16/256 Farben: Farben und zweifarbige Muster.
 *
 * Befehlszeile (fuer Tests, sonst Dialog "Neues Spiel"): K gegen Computer, 2 zwei Spieler,
 *   H Netz eroeffnen, J<adresse[:port]> Netz beitreten, P<port>, S<zahl> Startwert, L<0..2> Staerke,
 *   D Computer spielt auch die eigene Seite, N 8087 nicht benutzen, T Testmeldungen, Z Bild nach SHOT.BIN. */
#include "RD2.H"

unsigned ki_fpu_ladung(short dx, short dy, short s, short c, short wind);   /* KIFPU.C (8087) */

#define WW 640                                    /* Welt */
#define WH 400
#define GS 4                                      /* Gelaenderaster */
#define GN (WW / GS + 1)
#define CS 5                                      /* Zelle einer Burg */
#define BW 20
#define BH 16
#define FXG 3328L                                 /* Schwerkraft 52/1024 Einheit je Schritt^2, in 1/65536 */
#define VPP 5120L                                 /* Geschwindigkeit je Ladungseinheit (80/1024) */
#define WIND 64L                                  /* Windstufe = 1/1024 je Schritt^2 */
#define LMAX 99
#define PORT 2626
#define NETVER 1

#define P_KANONE 250
#define P_KUGELN 60                               /* 10 Kugeln */
#define P_PULVER 60                               /* 300 Pulver */
#define P_MAUER  50                               /* 12 Steine */
#define P_MINE   300

enum { Z_LEER, Z_MAUER, Z_KANONE, Z_KOENIG, Z_PULVER, Z_GOLD, Z_MINE, Z_FAHNE };
enum { S_LEER, S_ZIEL, S_FLUG, S_KNALL, S_KI, S_FERN, S_NETZ, S_ENDE };
enum { A_MENSCH, A_KI, A_FERN };
enum { G_KI, G_ZWEI, G_HOST, G_JOIN };

#define CM_NEU 101
#define CM_AUFGEBEN 102
#define CM_ENDE 103
#define CM_KANONE 111
#define CM_KUGELN 112
#define CM_PULVER 113
#define CM_MAUER 114
#define CM_MINE 115
#define CM_STEUER 116
#define CM_FEUER 121
#define CM_WEITER 122
#define CM_PASSEN 123
#define CM_REGELN 131
#define CM_INFO 132

static const short sintab[91] = { 0,18,36,54,71,89,107,125,143,160,178,195,213,230,248,265,282,299,316,333,350,367,384,
    400,416,433,449,465,481,496,512,527,543,558,573,587,602,616,630,644,658,672,685,698,711,724,737,749,761,773,784,796,
    807,818,828,839,849,859,868,878,887,896,904,912,920,928,935,943,949,956,962,968,974,979,984,989,994,998,1002,1005,
    1008,1011,1014,1016,1018,1020,1022,1023,1023,1024,1024 };

/* Bauplan (linke Burg, der Feind steht rechts; die rechte Burg ist gespiegelt) */
static const char plan[BH][BW + 1] = {
    "....F...............",
    "...###..............",
    "...#.#..............",
    "...###........C.....",
    "..#####......###....",
    "..#####..C...###..C.",
    "..##K##.###..###.##.",
    "..#####.###########.",
    ".##################.",
    ".######M#####P#####.",
    ".##################.",
    "##G#################",
    "####################",
    "####################",
    "####################",
    "####################" };

typedef struct {
    BYTE z[BH][BW], o[BH][BW];                    /* jetzt, Bauplan (zum Ausbessern) */
    short x0, y0, x1, y1; BYTE rechts, art;
    WORD gold, pulver, kugeln, volk; BYTE steuer;
    BYTE winkel, ladung, wahl;                    /* Zielwerte, gewaehlte Kanone */
} BURG;

typedef struct { long x, y, vx, vy; } KUGEL;      /* 16.16: hohes Wort = Welteinheit */

static HINST hi; static HWND fe;
static BYTE test, demo, ohne87, fpu, mono, zshot, staerke = 1, spielart, ich, sofort;
static short cw, ch, yt, th;                      /* Fenster: Breite, Hoehe, Weltbeginn, Textzeile */
static short boden[GN];
static BURG b[2];
static BYTE dran, zustand, sieger = 0xFF, grund, zugnr;
static short wind; static WORD runde;
static DWORD zz;                                  /* gemeinsamer Zufall (Netz: gleicher Startwert) */
static DWORD kz = 12345;                          /* Zufall nur fuer den Computer */
static DWORD startwert;
static char meld[80], erg[48], ini[80], adresse[40];
static WORD port = PORT;

static KUGEL ku; static BYTE fk, fr, fc;          /* Flug: Kugel, Schuetze, Kanone */
static short bx = -1, by;                         /* Kugel auf dem Schirm (-1 = nicht gezeichnet) */
static WORD fschritt; static DWORD ftick;
static short kx, ky; static BYTE knall;           /* Einschlag (Welt), Stufe der Explosion */
static short dl = 1, dt, dr, db;                  /* beschaedigter Bereich (Welt), dl > dr = keiner */
static BYTE zielan;                               /* Ziellinie ist gezeichnet */

static BYTE getrennt; static HSOCK lsock, sock; static BYTE nbuf[96]; static WORD nn;
static DWORD pruef[2][8]; static BYTE phat[2][8];

/* ---- Hilfen ------------------------------------------------------------------------------ */
static WORD rnd(WORD n) { zz = zz * 1103515245UL + 12345; return (WORD)((zz >> 16) % n); }
static WORD krnd(WORD n) { kz = kz * 1103515245UL + 12345; return (WORD)((kz >> 16) % n); }
static short slen(LPCSTR s) { short n = 0; while (s[n]) n++; return n; }
static char *sapp(char *d, LPCSTR s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *szahl(char *d, long v)
{
    char t[12]; int i = 11; t[i] = 0;
    if (v < 0) { *d++ = '-'; v = -v; }
    do t[--i] = (char)('0' + v % 10); while ((v /= 10) != 0);
    return sapp(d, t + i);
}
static LPCSTR T(WORD id) { return StrGet(hi, id); }
static void dbg(LPCSTR s) { if (test) DbgPrint(s); }
static void dbgz(LPCSTR s, long v) { char t[80], *p; if (!test) return; p = sapp(t, "BURGEN: "); p = sapp(p, s); *p++ = ' '; szahl(p, v); DbgPrint(t); }
static short sind(short w) { return sintab[w]; }
static short cosd(short w) { return sintab[90 - w]; }
static void kopie(char *d, LPCSTR s, short max) { short i = 0; while (s[i] && i < max - 1) { d[i] = s[i]; i++; } d[i] = 0; }
static void setmeld(LPCSTR s) { kopie(meld, s, sizeof meld); }
static void seterg(LPCSTR s) { kopie(erg, s, sizeof erg); }
static void adderg(LPCSTR s) { short n = slen(erg), i = 0; while (s[i] && n < (short)sizeof erg - 1) erg[n++] = s[i++]; erg[n] = 0; }

/* ---- Welt --------------------------------------------------------------------------------- */
static short boden_y(short x) { if (x < 0) x = 0; if (x >= WW) x = WW - 1; return boden[x / GS]; }
static short glatt(long t) { return (short)(t * t * (768 - 2 * t) / 65536L); }   /* 0..256 -> 0..256, weich */

static void gelaende(void)
{
    short hl = 290 + rnd(40), hr = 290 + rnd(40), hm = 165 + rnd(90), xm = 270 + rnd(100), i, x, k, h;
    for (i = 0; i < GN; i++) {
        x = i * GS;
        if (x <= 140) h = hl;
        else if (x >= 500) h = hr;
        else if (x < xm) h = hl + (short)((long)(hm - hl) * glatt((long)(x - 140) * 256 / (xm - 140)) / 256);
        else h = hm + (short)((long)(hr - hm) * glatt((long)(x - xm) * 256 / (500 - xm)) / 256);
        boden[i] = h;
    }
    for (k = 0; k < 4; k++) {                     /* Huegel und Mulden zwischen den Burgen */
        short m = 175 + rnd(290), w = 16 + rnd(30), a = (short)rnd(50) - 22, d;
        for (i = 0; i < GN; i++) {
            x = i * GS; d = x - m; if (d < 0) d = -d;
            if (d < w && x > 150 && x < 490) boden[i] -= (short)((long)a * ((long)w * w - (long)d * d) / ((long)w * w));
        }
    }
    for (i = 0; i < GN; i++) { if (boden[i] < 120) boden[i] = 120; if (boden[i] > 390) boden[i] = 390; }
    b[0].x0 = 30;  b[0].y0 = hl - BH * CS; b[0].rechts = 0;
    b[1].x0 = 510; b[1].y0 = hr - BH * CS; b[1].rechts = 1;
    for (k = 0; k < 2; k++) { b[k].x1 = b[k].x0 + BW * CS; b[k].y1 = b[k].y0 + BH * CS; }
}

static void burg_neu(BURG *g)
{
    short r, c; BYTE t;
    for (r = 0; r < BH; r++) for (c = 0; c < BW; c++) {
        switch (plan[r][c]) {
        case '#': t = Z_MAUER; break; case 'C': t = Z_KANONE; break; case 'K': t = Z_KOENIG; break;
        case 'P': t = Z_PULVER; break; case 'G': t = Z_GOLD; break; case 'M': t = Z_MINE; break;
        case 'F': t = Z_FAHNE; break; default: t = Z_LEER;
        }
        g->z[r][c] = t; g->o[r][c] = (BYTE)(t == Z_KANONE ? Z_LEER : t);
    }
    g->gold = 400; g->pulver = 800; g->kugeln = 12; g->volk = 1000; g->steuer = 30;
    g->winkel = 45; g->ladung = 50; g->wahl = 0;
}

static short zx(BYTE i, short c) { return b[i].x0 + (b[i].rechts ? BW - 1 - c : c) * CS; }   /* Zelle -> Welt */
static short zy(BYTE i, short r) { return b[i].y0 + r * CS; }
static short kanonen(BYTE i) { short r, c, n = 0; for (r = 0; r < BH; r++) for (c = 0; c < BW; c++) if (b[i].z[r][c] == Z_KANONE) n++; return n; }
static short minen(BYTE i) { short r, c, n = 0; for (r = 0; r < BH; r++) for (c = 0; c < BW; c++) if (b[i].z[r][c] == Z_MINE) n++; return n; }
static BYTE kanone(BYTE i, short k, short *r, short *c)   /* k-te Kanone (Lesereihenfolge) */
{
    short n = 0;
    for (*r = 0; *r < BH; (*r)++) for (*c = 0; *c < BW; (*c)++) if (b[i].z[*r][*c] == Z_KANONE && n++ == k) return 1;
    return 0;
}
static void koenig(BYTE i, short *x, short *y)
{
    short r, c;
    for (r = 0; r < BH; r++) for (c = 0; c < BW; c++) if (b[i].z[r][c] == Z_KOENIG) { *x = zx(i, c) + CS / 2; *y = zy(i, r) + CS / 2; return; }
    *x = zx(i, 4) + CS / 2; *y = zy(i, 6) + CS / 2;
}

static void schaden_welt(short x0, short y0, short x1, short y1)
{
    if (dl > dr) { dl = x0; dt = y0; dr = x1; db = y1; return; }
    if (x0 < dl) dl = x0; if (y0 < dt) dt = y0; if (x1 > dr) dr = x1; if (y1 > db) db = y1;
}
static void schaden_zelle(BYTE i, short r, short c) { schaden_welt(zx(i, c) - 2 * CS, zy(i, r) - 2 * CS, zx(i, c) + 3 * CS, zy(i, r) + 2 * CS); }

/* Lose Teile (Kanonen, Koenig, Fahne, Lager) fallen, wenn unter ihnen nichts mehr ist */
static void fallen(BYTE i)
{
    short r, c, q; BYTE t;
    for (r = BH - 2; r >= 0; r--) for (c = 0; c < BW; c++) {
        t = b[i].z[r][c];
        if (t == Z_LEER || t == Z_MAUER) continue;
        for (q = r; q + 1 < BH && b[i].z[q + 1][c] == Z_LEER; q++) ;
        if (q != r) { b[i].z[q][c] = t; b[i].z[r][c] = Z_LEER; schaden_welt(zx(i, c) - 2 * CS, zy(i, r) - 3 * CS, zx(i, c) + 3 * CS, zy(i, q) + CS); }
    }
}

static void zerstoere(BYTE i, short r, short c, BYTE tief)
{
    BURG *g = &b[i]; BYTE t; short a, e;
    if (r < 0 || r >= BH || c < 0 || c >= BW || (t = g->z[r][c]) == Z_LEER) return;
    g->z[r][c] = Z_LEER;
    g->volk = g->volk > 12 ? g->volk - 12 : 0;
    schaden_zelle(i, r, c);
    switch (t) {
    case Z_KANONE: if (grund < 30) grund = 30; if (g->wahl) g->wahl--; break;
    case Z_KOENIG: sieger = (BYTE)(i ^ 1); grund = 31; break;
    case Z_PULVER:                                /* Pulverlager fliegt in die Luft */
        g->pulver = 0; if (grund < 32) grund = 32;
        if (tief) for (a = -2; a <= 2; a++) for (e = -2; e <= 2; e++)
            if ((a < 0 ? -a : a) + (e < 0 ? -e : e) <= 2) zerstoere(i, r + a, c + e, 0);
        break;
    case Z_GOLD: g->gold /= 2; if (grund < 33) grund = 33; break;
    case Z_MINE: if (grund < 34) grund = 34; break;
    default: if (grund < 29) grund = 29;
    }
}
static void treffer_burg(BYTE i, short r, short c)   /* Mitte und die vier Nachbarn */
{
    zerstoere(i, r, c, 1); zerstoere(i, r - 1, c, 1); zerstoere(i, r + 1, c, 1); zerstoere(i, r, c - 1, 1); zerstoere(i, r, c + 1, 1);
    fallen(i);
}
static void treffer_boden(short x)
{
    short i0 = x / GS, d, k;
    for (d = -2; d <= 2; d++) {
        k = i0 + d; if (k < 0 || k >= GN) continue;
        boden[k] += (3 - (d < 0 ? -d : d)) * 2; if (boden[k] > 396) boden[k] = 396;
    }
    schaden_welt(x - 4 * GS, boden_y(x) - 24, x + 4 * GS, WH);
}

/* ---- Flugbahn: ganzzahlig, fuer Spiel, Netz und Computer gleich ---------------------------- */
static void kugel_start(KUGEL *k, BYTE i, short r, short c, short w, short l)
{
    long v = (long)l * VPP;
    k->x = ((long)zx(i, c) << 16) + (CS * 32768L); k->y = ((long)zy(i, r) << 16) + 32768L;
    k->vx = v / 1024 * cosd(w); if (b[i].rechts) k->vx = -k->vx;
    k->vy = -(v / 1024 * sind(w));
}
/* Was liegt an (x, y)? 0 nichts, 1 Boden, 2/3 Burg 0/1 (Zelle in *r, *c). Die eigene Kanone zaehlt nicht. */
static BYTE pruefe(short x, short y, BYTE si, short sr, short sc, short *r, short *c)
{
    BYTE i; BURG *g;
    if (y < 0) return 0;
    if (x < 0 || x >= WW) return 0;
    for (i = 0, g = b; i < 2; i++, g++) {
        if (x < g->x0 || x >= g->x1 || y < g->y0 || y >= g->y1) continue;
        *c = (x - g->x0) / CS; if (g->rechts) *c = BW - 1 - *c;
        *r = (y - g->y0) / CS;
        if (g->z[*r][*c] && !(i == si && *r == sr && *c == sc)) return (BYTE)(2 + i);
    }
    if (y >= boden[x >> 2]) return 1;
    return 0;
}
/* ein Schritt: 0 fliegt, 1 Boden, 2/3 Burg, 9 weg; Einschlag in *hx, *hy */
static BYTE schritt(KUGEL *k, short w, BYTE si, short sr, short sc, short *r, short *c, short *hx, short *hy)
{
    short x, y; BYTE e;
    k->vy += FXG; k->vx += w * WIND;
    if (k->vx > 0x18000L || k->vx < -0x18000L || k->vy > 0x18000L || k->vy < -0x18000L) {   /* schnell: Mitte pruefen */
        x = (short)((k->x + (k->vx >> 1)) >> 16); y = (short)((k->y + (k->vy >> 1)) >> 16);
        if ((e = pruefe(x, y, si, sr, sc, r, c)) != 0) { *hx = x; *hy = y; return e; }
    }
    k->x += k->vx; k->y += k->vy;
    *hx = x = (short)(k->x >> 16); *hy = y = (short)(k->y >> 16);
    if (x < -40 || x > WW + 40 || y > WH) return 9;
    return pruefe(x, y, si, sr, sc, r, c);
}
static BYTE probe(BYTE i, short r, short c, short w, short l, short *hx, short *hy)
{
    KUGEL k; short n, rr, cc; BYTE e;
    kugel_start(&k, i, r, c, w, l);
    for (n = 0; n < 1200; n++) if ((e = schritt(&k, wind, i, r, c, &rr, &cc, hx, hy)) != 0) return e;
    return 9;
}

/* ---- Pruefsumme (Netz) -------------------------------------------------------------------- */
static DWORD summe(void)
{
    DWORD s = 0; short i, r, c;
    for (i = 0; i < 2; i++) {
        for (r = 0; r < BH; r++) for (c = 0; c < BW; c++) s = s * 31 + b[i].z[r][c];
        s = s * 7 + b[i].gold; s = s * 7 + b[i].pulver; s = s * 7 + b[i].kugeln; s = s * 7 + b[i].volk; s = s * 7 + b[i].steuer;
    }
    for (i = 0; i < GN; i++) s = s * 3 + boden[i];
    return s + (WORD)wind + zz;
}

/* ---- Zeichnen ----------------------------------------------------------------------------- */
static short SX(short x) { return (short)((long)x * cw / WW); }
static short SY(short y) { return yt + (short)((long)y * (ch - yt) / WH); }
static void wrect(RECT *q, short x0, short y0, short x1, short y1)
{
    q->l = SX(x0); q->t = SY(y0); q->r = SX(x1); q->b = SY(y1);
    if (q->r <= q->l) q->r = q->l + 1; if (q->b <= q->t) q->b = q->t + 1;
    if (q->t < yt) q->t = yt;
}

enum { F_HIMMEL, F_BODEN, F_MAUER, F_KOENIG, F_PULVER, F_GOLD, F_MINE, F_SCHWARZ, F_ROT, F_BLAU, F_LEISTE };
static void pinsel(HDC dc, BYTE f)
{
    if (mono) {                                   /* 1 Bit: Muster schwarz auf weiss */
        static const WORD mu[11] = { PAT_SOLID, PAT_GRAY25, PAT_HATCH, PAT_SOLID, PAT_GRAY75, PAT_GRAY50, PAT_GRAY50, PAT_SOLID, PAT_SOLID, PAT_SOLID, PAT_SOLID };
        SetBkColor(dc, C_WHITE);
        SetBrush(dc, (f == F_HIMMEL || f == F_LEISTE || f == F_KOENIG) ? C_WHITE : C_BLACK, mu[f]);
        return;
    }
    switch (f) {
    case F_HIMMEL: SetBrush(dc, RGB(120, 200, 255), PAT_SOLID); break;
    case F_BODEN:  SetBkColor(dc, RGB(0, 170, 0)); SetBrush(dc, RGB(0, 100, 0), PAT_GRAY25); break;
    case F_MAUER:  SetBkColor(dc, RGB(170, 170, 170)); SetBrush(dc, RGB(85, 85, 85), PAT_HATCH); break;
    case F_KOENIG: SetBrush(dc, RGB(255, 255, 85), PAT_SOLID); break;
    case F_PULVER: SetBrush(dc, RGB(85, 85, 85), PAT_SOLID); break;
    case F_GOLD:   SetBrush(dc, RGB(255, 200, 0), PAT_SOLID); break;
    case F_MINE:   SetBkColor(dc, RGB(170, 85, 0)); SetBrush(dc, C_BLACK, PAT_GRAY50); break;
    case F_ROT:    SetBrush(dc, RGB(255, 60, 60), PAT_SOLID); break;
    case F_BLAU:   SetBrush(dc, RGB(60, 60, 255), PAT_SOLID); break;
    case F_LEISTE: SetBrush(dc, RGB(192, 192, 192), PAT_SOLID); break;
    default:       SetBrush(dc, C_BLACK, PAT_SOLID);
    }
}
static BYTE schneidet(const RECT *a, const RECT *k) { return (BYTE)(a->l < k->r && a->r > k->l && a->t < k->b && a->b > k->t); }

static void rohr(HDC dc, BYTE i, short r, short c)   /* Rohr einer Kanone in Zielrichtung */
{
    short w = b[i].winkel, dir = b[i].rechts ? -1 : 1;
    long mx = (long)zx(i, c) * 16 + CS * 8, my = (long)zy(i, r) * 16 + CS * 8;
    long ex = mx + (long)dir * cosd(w) * 7 * 16 / 1024, ey = my - (long)sind(w) * 7 * 16 / 1024;
    SetPen(dc, C_BLACK);
    Line(dc, (short)(mx * cw / (WW * 16L)), yt + (short)(my * (ch - yt) / (WH * 16L)), (short)(ex * cw / (WW * 16L)), yt + (short)(ey * (ch - yt) / (WH * 16L)));
}

static void zeichne_burg(HDC dc, BYTE i, const RECT *k)
{
    short r, c, c2; BYTE t; RECT q, bb;
    wrect(&bb, b[i].x0 - CS, b[i].y0 - 3 * CS, b[i].x1 + CS, b[i].y1);
    if (!schneidet(&bb, k)) return;
    for (r = 0; r < BH; r++) {
        for (c = 0; c < BW; c = c2) {             /* Mauern einer Zeile am Stueck */
            t = b[i].z[r][c];
            for (c2 = c + 1; t == Z_MAUER && c2 < BW && b[i].z[r][c2] == Z_MAUER; c2++) ;
            if (t == Z_LEER) continue;
            if (b[i].rechts) wrect(&q, zx(i, c2 - 1), zy(i, r), zx(i, c) + CS, zy(i, r) + CS);
            else wrect(&q, zx(i, c), zy(i, r), zx(i, c2 - 1) + CS, zy(i, r) + CS);
            if (t != Z_FAHNE && t != Z_KANONE && !schneidet(&q, k)) continue;
            switch (t) {
            case Z_MAUER: pinsel(dc, F_MAUER); FillRect(dc, &q); break;
            case Z_KOENIG:
                pinsel(dc, F_KOENIG); FillRect(dc, &q); SetPen(dc, C_BLACK); Frame(dc, &q);
                { RECT z = q; short w3 = (q.r - q.l) / 3; z.l += w3; z.r -= w3; z.t += (q.b - q.t) / 2; if (z.r > z.l && z.b > z.t) { pinsel(dc, mono ? F_SCHWARZ : F_ROT); FillRect(dc, &z); } }
                break;
            case Z_PULVER: pinsel(dc, F_PULVER); FillRect(dc, &q); SetPen(dc, C_BLACK); Frame(dc, &q); break;
            case Z_GOLD: pinsel(dc, F_GOLD); FillRect(dc, &q); SetPen(dc, C_BLACK); Frame(dc, &q); break;
            case Z_MINE: pinsel(dc, F_MINE); FillRect(dc, &q); SetPen(dc, C_BLACK); Frame(dc, &q); break;
            case Z_KANONE:
                { RECT u = q; u.t += (q.b - q.t) / 2; if (u.b <= u.t) u.t = u.b - 1; pinsel(dc, F_SCHWARZ); FillRect(dc, &u); rohr(dc, i, r, c); }
                break;
            case Z_FAHNE:
                { short px = (q.l + q.r) / 2, fw = SX(2 * CS) > 3 ? SX(2 * CS) : 3; RECT f;
                  SetPen(dc, C_BLACK); Line(dc, px, q.b - 1, px, SY(zy(i, r) - 2 * CS));
                  f.t = SY(zy(i, r) - 2 * CS); f.b = SY(zy(i, r) - CS); if (f.b <= f.t + 1) f.b = f.t + 2;
                  if (b[i].rechts) { f.r = px; f.l = px - fw; } else { f.l = px + 1; f.r = px + 1 + fw; }
                  pinsel(dc, i ? F_BLAU : F_ROT); FillRect(dc, &f);
                  if (mono && i && f.r - f.l > 2 && f.b - f.t > 2) { pinsel(dc, F_HIMMEL); f.l++; f.t++; f.r--; f.b--; FillRect(dc, &f); } }
                break;
            }
        }
    }
}

static LPCSTR name(BYTE i)
{
    if (spielart == G_KI) return T(i ? 4 : 2);
    if (spielart >= G_HOST) return T(i == ich ? 5 : 6);
    return T(i ? 3 : 2);
}
static void zielmeld(void)
{
    char *p = meld; short n = kanonen(dran);
    p = sapp(p, T(20)); p = szahl(p, b[dran].wahl + 1); *p++ = '/'; p = szahl(p, n);
    p = sapp(p, T(21)); p = szahl(p, b[dran].winkel); p = sapp(p, T(22)); p = szahl(p, b[dran].ladung);
}

static void zeichne_leiste(HDC dc)
{
    char s[100], *p; RECT q; short w, n, i; BYTE z = (BYTE)(zustand == S_ENDE ? sieger : dran);
    q.l = 0; q.t = 0; q.r = cw; q.b = yt; pinsel(dc, F_LEISTE); FillRect(dc, &q);
    SetPen(dc, C_BLACK); Line(dc, 0, yt - 1, cw, yt - 1);
    SetBkMode(dc, BK_TRANSPARENT); SetTextColor(dc, C_BLACK);
    if (zustand == S_LEER || zustand == S_NETZ || z > 1) { TextOut(dc, 4, 2, meld, -1); return; }
    q.l = 3; q.t = 3; q.r = 3 + th; q.b = 1 + th; pinsel(dc, z ? F_BLAU : F_ROT); FillRect(dc, &q); SetPen(dc, C_BLACK); Frame(dc, &q);
    p = sapp(s, name(z)); p = sapp(p, ": ");
    p = sapp(p, T(11)); p = szahl(p, b[z].gold); p = sapp(p, T(12)); p = szahl(p, b[z].kugeln);
    p = sapp(p, T(13)); p = szahl(p, b[z].pulver); p = sapp(p, T(14)); p = szahl(p, b[z].volk);
    p = sapp(p, T(15)); p = szahl(p, b[z].steuer); *p++ = '%'; *p = 0;
    TextOut(dc, 6 + th, 2, s, -1);
    p = sapp(s, T(16)); n = wind < 0 ? -wind : wind;
    if (!n) *p++ = '-'; else for (i = 0; i <= (n - 1) / 2; i++) *p++ = (char)(wind < 0 ? '<' : '>');
    *p = 0;
    w = TextWidth(dc, s, -1); TextOut(dc, cw - 4 - w, 2, s, -1);
    TextOut(dc, 4, 3 + th, meld, -1);
    if (erg[0]) { w = TextWidth(dc, erg, -1); TextOut(dc, cw - 4 - w, 3 + th, erg, -1); }
}

static void kugel_xor(HDC dc)
{
    RECT q; short g = SX(3), h = (short)(3L * (ch - yt) / WH);
    if (bx < 0) return;
    if (g < 2) g = 2; if (h < 2) h = 2;
    q.l = bx - g / 2; q.t = by - h / 2; q.r = q.l + g; q.b = q.t + h;
    if (q.b <= yt) return;
    if (q.t < yt) q.t = yt;
    SetRop(dc, ROP_XOR); SetBrush(dc, C_WHITE, PAT_SOLID); FillRect(dc, &q); SetRop(dc, ROP_COPY);
}
static void ziel_xor(HDC dc)
{
    short r, c, w, dir, mx, my, len;
    if (!kanone(dran, b[dran].wahl, &r, &c)) return;
    w = b[dran].winkel; dir = b[dran].rechts ? -1 : 1; mx = zx(dran, c) + CS / 2; my = zy(dran, r) + CS / 2;
    len = 12 + b[dran].ladung;
    SetRop(dc, ROP_XOR); SetPen(dc, C_WHITE);
    Line(dc, SX(mx + (short)((long)dir * cosd(w) * 9 / 1024)), SY(my - (short)((long)sind(w) * 9 / 1024)),
         SX(mx + (short)((long)dir * cosd(w) * len / 1024)), SY(my - (short)((long)sind(w) * len / 1024)));
    SetRop(dc, ROP_COPY);
}
static void knall_xor(HDC dc)
{
    RECT q; short rx = SX(knall * 4), ry = (short)((long)knall * 4 * (ch - yt) / WH), mx = SX(kx), my = SY(ky);
    if (!knall) return;
    if (rx < 1) rx = 1; if (ry < 1) ry = 1;
    q.l = mx - rx; q.r = mx + rx + 1; q.t = my - ry; q.b = my + ry + 1;
    if (q.t < yt) return;
    SetRop(dc, ROP_XOR); SetPen(dc, C_WHITE); Ellipse(dc, &q, 0); SetRop(dc, ROP_COPY);
}
static BYTE mensch_dran(void) { return (BYTE)(zustand == S_ZIEL && b[dran].art == A_MENSCH); }
static void ziel_weg(void) { HDC dc; if (!zielan) return; dc = GetDC(fe); ziel_xor(dc); ReleaseDC(fe, dc); zielan = 0; }
static void ziel_hin(void) { HDC dc; if (zielan || !mensch_dran()) return; dc = GetDC(fe); ziel_xor(dc); ReleaseDC(fe, dc); zielan = 1; }
static void leiste_neu(void) { RECT q; q.l = 0; q.t = 0; q.r = cw; q.b = yt; InvalidateRect(fe, &q, FALSE); }
static void schaden_zeigen(void)              /* beschaedigten Bereich sofort neu (Ziellinie bleibt stimmig) */
{
    RECT q; BYTE z = zielan;
    if (dl > dr) return;
    ziel_weg(); wrect(&q, dl, dt, dr, db); dl = 1; dr = 0;
    InvalidateRect(fe, &q, FALSE); UpdateWindow(fe);
    if (z) ziel_hin();
}
static void burg_neu_zeigen(BYTE i) { schaden_welt(b[i].x0 - CS, b[i].y0 - 3 * CS, b[i].x1 + CS, b[i].y1); schaden_zeigen(); }

static void malen(HWND w)
{
    PAINT ps; HDC dc; RECT k, q; short i, j, x0, x1, y; DWORD t0 = TickCount();
    dc = BeginPaint(w, &ps); k = ps.neu;
    if (k.t < yt) zeichne_leiste(dc);
    if (k.b > yt) {
        if (k.t < yt) k.t = yt;
        q = k; pinsel(dc, F_HIMMEL); FillRect(dc, &q);  /* Himmel nur im ungueltigen Bereich */
        if (zustand != S_LEER && zustand != S_NETZ) {
            pinsel(dc, F_BODEN);
            for (i = 0; i < GN - 1; i = j) {     /* Boden als Saeulen, gleiche Hoehen zusammen */
                y = boden[i];
                for (j = i + 1; j < GN - 1 && boden[j] == y; j++) ;
                x0 = SX(i * GS); x1 = j == GN - 1 ? cw : SX(j * GS);
                if (x1 <= k.l || x0 >= k.r) continue;
                q.l = x0; q.r = x1; q.t = SY(y); q.b = ch; if (q.t < k.b) FillRect(dc, &q);
            }
            zeichne_burg(dc, 0, &k); zeichne_burg(dc, 1, &k);
            if (zielan) ziel_xor(dc);
            if (zustand == S_FLUG) kugel_xor(dc);
            if (zustand == S_KNALL) knall_xor(dc);
        }
    }
    EndPaint(w, &ps);
    if (zustand != S_LEER && ps.neu.t <= yt && ps.neu.b >= ch - 1) dbgz("Bild ms", (long)(TickCount() - t0));
    if (zshot && (zustand == S_ZIEL || zustand == S_KI) && ps.neu.t <= yt && ps.neu.b >= ch - 1) {   /* Testautomatik: Bildspeicher (1 Bit) */
        HDC d = GetDC(0); short H = GetCaps(d, CAPS_H), f;
        WORD seg = H == 348 ? 0xB000 : H == 480 ? 0xA000 : 0xB800, len = H == 348 ? 0x8000 : H == 480 ? 38400U : 0x4000;
        ReleaseDC(0, d);
        if ((f = FileCreate("SHOT.BIN", 0)) >= 0) { FileWrite(f, (const void FAR *)((DWORD)seg << 16), len); FileClose(f); }
        dbg("BURGEN: Bild gespeichert"); zshot = 0;
    }
}

static void groesse(void)
{
    RECT r; HDC dc;
    GetClientRect(fe, &r); cw = r.r - r.l; ch = r.b - r.t;
    dc = GetDC(fe); th = GetCaps(dc, CAPS_TEXTH); mono = (BYTE)(GetCaps(dc, CAPS_BPP) == 1); ReleaseDC(fe, dc);
    yt = 2 * th + 6;
    if (ch < yt + 40) ch = yt + 40;
}

/* ---- Netz --------------------------------------------------------------------------------- */
static void senden(BYTE t, BYTE a, BYTE c, BYTE d, DWORD e)
{
    BYTE p[8];
    if (!sock) return;
    p[0] = t; p[1] = a; p[2] = c; p[3] = d; p[4] = (BYTE)e; p[5] = (BYTE)(e >> 8); p[6] = (BYTE)(e >> 16); p[7] = (BYTE)(e >> 24);
    NetSend(sock, p, 8);
}
static void netz_aus(void)
{
    if (sock) { senden('B', 0, 0, 0, 0); NetClose(sock); sock = 0; }
    if (lsock) { NetClose(lsock); lsock = 0; }
}

/* ---- Spielablauf -------------------------------------------------------------------------- */
static void zug_beginn(void);
static void spiel_ende(void)
{
    char *p;
    ziel_weg(); zustand = S_ENDE;
    if (sieger <= 1) {
        p = sapp(meld, name(sieger)); p = sapp(p, T(40));
        if (grund == 41 || grund == 42 || grund == 43 || grund == 72) { p = sapp(p, " ("); p = sapp(p, T(grund)); sapp(p, ")"); }
    }
    dbgz("Sieger", sieger); dbgz("Grund", grund); dbgz("Runden", runde);
    if (test) DbgPrint(meld);
    InvalidateRect(fe, 0, FALSE);
    if (!demo) SndBeep(sieger == ich || spielart == G_ZWEI ? 880 : 220, 300);
    if (demo && test) TimerSet(fe, 2, 3000);    /* Testlauf: Fenster nach 3 s zu */
}

static void pruef_vergleich(BYTE n)
{
    BYTE k = (BYTE)(n & 7);
    if (!phat[0][k] || !phat[1][k]) return;
    phat[0][k] = phat[1][k] = 0;
    if (pruef[0][k] != pruef[1][k]) {
        dbgz("Pruefsumme FALSCH, Zug", n);
        sieger = 0xFE; grund = 75; netz_aus(); ziel_weg(); zustand = S_ENDE; setmeld(T(75)); InvalidateRect(fe, 0, FALSE);
        if (!demo) MsgBox(fe, T(75), T(1), MB_ERROR);
    } else dbgz("Pruefsumme gleich, Zug", n);
}

static void zug_ende(void)
{
    DWORD s;
    bx = -1; knall = 0;
    schaden_zeigen();
    s = summe(); dbgz("Zug", zugnr); dbgz("Summe", (long)(s & 0x7FFFFFFFUL));
    if (spielart >= G_HOST && sock) {
        BYTE k = (BYTE)(zugnr & 7); pruef[0][k] = s; phat[0][k] = 1; senden('Z', zugnr, 0, 0, s); pruef_vergleich(zugnr);
        if (zustand == S_ENDE) return;
    }
    zugnr++;
    if (sieger == 0xFF && getrennt) { sieger = ich; grund = 72; }
    if (sieger != 0xFF) { spiel_ende(); return; }
    dran ^= 1; zug_beginn();
}

static BYTE kann_nicht(BYTE i)                     /* 1 = verloren */
{
    BURG *g = &b[i];
    if (g->volk < 200) { grund = 41; return 1; }
    if (!kanonen(i) && g->gold < P_KANONE) { grund = 42; return 1; }
    if (!g->kugeln && g->gold < P_KUGELN) { grund = 42; return 1; }
    if (g->pulver < 5 && g->gold < P_PULVER) { grund = 42; return 1; }
    return 0;
}

static void ki_start(void);
static void netz_pumpe(void);
static void zug_beginn(void)
{
    BURG *g = &b[dran]; long ein; short n;
    if (dran == 0) runde++;
    wind = (short)rnd(17) - 8;
    ein = (long)g->volk * g->steuer / 200 + minen(dran) * 40L;
    g->gold = (WORD)((long)g->gold + ein > 60000L ? 60000L : g->gold + ein);
    if (g->steuer <= 20) g->volk += g->volk / 20; else if (g->steuer <= 35) g->volk += g->volk / 100;
    else if (g->steuer <= 50) g->volk -= g->volk * 3 / 100; else g->volk -= g->volk * 8 / 100;
    if (g->volk > 3000) g->volk = 3000;
    n = kanonen(dran); if (g->wahl >= n) g->wahl = 0;
    grund = 0; zielan = 0;
    if (kann_nicht(dran)) { sieger = (BYTE)(dran ^ 1); spiel_ende(); return; }
    leiste_neu();
    dbgz("Zug beginnt, Spieler", dran); dbgz("Wind", wind);
    if (g->art == A_MENSCH) { zustand = S_ZIEL; zielmeld(); ziel_hin(); }
    else if (g->art == A_KI) { zustand = S_KI; setmeld(T(23)); ki_start(); }
    else { zustand = S_FERN; setmeld(T(24)); netz_pumpe(); }
}

static void flug_start(short k, short w, short l)
{
    short r, c;
    if (!kanone(dran, k, &r, &c)) return;
    ziel_weg();
    b[dran].wahl = (BYTE)k; b[dran].winkel = (BYTE)w; b[dran].ladung = (BYTE)l;
    b[dran].kugeln--; b[dran].pulver -= l;
    kugel_start(&ku, dran, r, c, w, l); fk = dran; fr = (BYTE)r; fc = (BYTE)c;
    fschritt = 0; ftick = TickCount(); bx = -1; zustand = S_FLUG; grund = 0;
    burg_neu_zeigen(dran);                        /* Rohre zeigen die Schussrichtung */
    setmeld(""); erg[0] = 0; zielmeld(); leiste_neu();
    if (!demo) SndBeep(110, 40);
    if (test) { dbgz("Schuss Spieler", dran); dbgz("Kanone", k); dbgz("Winkel", w); dbgz("Ladung", l); }
}

/* Aktion des Spielers am Zug (lokal oder aus dem Netz). Liefert 0, wenn sie nicht geht. */
static BYTE aktion(BYTE t, BYTE a, BYTE c, BYTE d, BYTE vomnetz)
{
    BURG *g = &b[dran]; short r, q, n = 0, best = -1;
    switch (t) {
    case 'K':
        switch (a) {
        case 1:                                   /* Kanone: freie Stelle auf der Mauer, vorn zuerst */
            if (g->gold < P_KANONE) { seterg(T(50)); return 0; }
            for (q = BW - 1; q >= 0 && best < 0; q--) for (r = 0; r < BH; r++) {
                if (g->z[r][q] == Z_LEER) continue;
                if (r > 0 && g->z[r][q] == Z_MAUER && (q == 0 || g->z[r - 1][q - 1] != Z_KANONE) && (q == BW - 1 || g->z[r - 1][q + 1] != Z_KANONE)) best = (short)((r - 1) * BW + q);
                break;
            }
            if (best < 0) { seterg(T(51)); return 0; }
            g->z[best / BW][best % BW] = Z_KANONE; g->gold -= P_KANONE; schaden_zelle(dran, best / BW, best % BW);
            break;
        case 2: if (g->gold < P_KUGELN) { seterg(T(50)); return 0; } g->gold -= P_KUGELN; g->kugeln += 10; break;
        case 3: if (g->gold < P_PULVER) { seterg(T(50)); return 0; } g->gold -= P_PULVER; g->pulver += 300; break;
        case 4:                                   /* Mauer: bis 12 fehlende Steine des Bauplans, von unten */
            if (g->gold < P_MAUER) { seterg(T(50)); return 0; }
            for (r = BH - 1; r >= 0 && n < 12; r--) for (q = 0; q < BW && n < 12; q++)
                if (g->z[r][q] == Z_LEER && g->o[r][q] == Z_MAUER) { g->z[r][q] = Z_MAUER; n++; schaden_zelle(dran, r, q); }
            if (!n) { seterg(T(55)); return 0; }
            g->gold -= P_MAUER;
            break;
        case 5:                                   /* Bergwerk: Mauerstein unten, hinten */
            if (g->gold < P_MINE) { seterg(T(50)); return 0; }
            if (minen(dran) >= 4) { seterg(T(54)); return 0; }
            for (r = BH - 2; r >= 9 && best < 0; r--) for (q = 1; q < BW / 2 && best < 0; q++)
                if (g->z[r][q] == Z_MAUER && g->z[r][q - 1] != Z_MINE && g->z[r][q + 1] != Z_MINE) best = (short)(r * BW + q);
            if (best < 0) { seterg(T(54)); return 0; }
            g->z[best / BW][best % BW] = Z_MINE; g->o[best / BW][best % BW] = Z_MINE; g->gold -= P_MINE; schaden_zelle(dran, best / BW, best % BW);
            break;
        default: return 0;
        }
        if (!vomnetz) senden('K', a, 0, 0, 0);
        schaden_zeigen();
        if (zustand == S_ZIEL) zielmeld();
        leiste_neu();
        if (test) { dbgz("Kauf", a); dbgz("Gold", g->gold); }
        return 1;
    case 'T':
        if (a > 80) a = 80;
        g->steuer = a; if (!vomnetz) senden('T', a, 0, 0, 0);
        leiste_neu(); return 1;
    case 'S':
        if (a >= kanonen(dran) || c > 90 || d < 1 || d > LMAX) return 0;
        if (!g->kugeln) { seterg(T(52)); leiste_neu(); return 0; }
        if (g->pulver < d) { seterg(T(53)); leiste_neu(); return 0; }
        if (!vomnetz) senden('S', a, c, d, 0);
        flug_start(a, c, d); return 1;
    case 'N':
        if (!vomnetz) senden('N', 0, 0, 0, 0);
        ziel_weg(); seterg(""); zug_ende(); return 1;
    case 'A':
        if (!vomnetz) senden('A', 0, 0, 0, 0);
        sieger = (BYTE)(dran ^ 1); grund = 43; spiel_ende(); return 1;
    }
    return 0;
}

/* Flug: Schritte nach vergangener Zeit (gleich schnell auf XT und 386, langsamer Rechner springt) */
static void flug_tick(void)
{
    HDC dc; short n, i, r = 0, c = 0, hx = 0, hy = 0; BYTE e = 0; DWORD t = TickCount();
    n = (short)((t - ftick) / 14); if (n < 1) n = 1; if (n > 16) n = 16;
    ftick += (DWORD)n * 14; if (t - ftick > 200) ftick = t;
    dc = GetDC(fe); kugel_xor(dc); bx = -1;
    for (i = 0; i < n && !e; i++) { e = schritt(&ku, wind, fk, fr, fc, &r, &c, &hx, &hy); if (++fschritt > 1200) e = 9; }
    if (!e) { bx = SX((short)(ku.x >> 16)); by = SY((short)(ku.y >> 16)); kugel_xor(dc); ReleaseDC(fe, dc); return; }
    ReleaseDC(fe, dc);
    if (e == 1) { treffer_boden(hx); seterg(T(36)); }
    else if (e >= 2) { treffer_burg((BYTE)(e - 2), r, c); seterg(T(grund ? grund : 29)); if (e - 2 == fk) { adderg(" "); adderg(T(37)); } }
    else seterg(T(36));
    if (test) { dbgz("Einschlag Art", e); dbgz("Einschlag x", hx); dbgz("Einschlag y", hy); dbgz("Schritte", fschritt); }
    leiste_neu();
    if (e == 9) { zug_ende(); return; }
    kx = hx; ky = hy; knall = 1; zustand = S_KNALL;
    dc = GetDC(fe); knall_xor(dc); ReleaseDC(fe, dc);
    if (!demo) SndBeep(e == 1 ? 70 : 90, 120);
}
static void knall_tick(void)
{
    HDC dc = GetDC(fe);
    knall_xor(dc);                                /* alte Stufe weg (XOR) */
    if (++knall > 4) { knall = 0; ReleaseDC(fe, dc); zug_ende(); return; }
    knall_xor(dc); ReleaseDC(fe, dc);
}

/* ---- Computergegner ----------------------------------------------------------------------- */
static struct { short nk, ik, w, best, bw, bl, bk, tx, ty; DWORD t0; WORD probes; } ki;

static void ki_kaufen(void)
{
    BURG *g = &b[dran]; short fehlt = 0, r, c;
    for (r = 0; r < BH; r++) for (c = 0; c < BW; c++) if (g->o[r][c] == Z_MAUER && g->z[r][c] == Z_LEER) fehlt++;
    if (!kanonen(dran) && g->gold >= P_KANONE) aktion('K', 1, 0, 0, 0);
    if (g->kugeln < 4 && g->gold >= P_KUGELN) aktion('K', 2, 0, 0, 0);
    if (g->pulver < 250 && g->gold >= P_PULVER) aktion('K', 3, 0, 0, 0);
    if (staerke >= 1 && fehlt >= 10 && g->gold >= P_MAUER + 100) aktion('K', 4, 0, 0, 0);
    if (g->gold >= 650 && minen(dran) < 3) aktion('K', 5, 0, 0, 0);
    else if (g->gold >= 550 && kanonen(dran) < 4) aktion('K', 1, 0, 0, 0);
    if (g->volk < 600 && g->steuer > 20) aktion('T', 20, 0, 0, 0);
    else if (g->volk >= 900 && g->steuer < 30) aktion('T', 30, 0, 0, 0);
}
static void ki_start(void)
{
    ki_kaufen();
    ki.nk = kanonen(dran); ki.ik = 0; ki.w = 12; ki.best = 32767; ki.bk = -1; ki.probes = 0;
    koenig((BYTE)(dran ^ 1), &ki.tx, &ki.ty);
    ki.t0 = TickCount();
}
static short ki_abst(BYTE e, short hx, short hy)  /* Einschlag - Ziel (Welt) */
{
    short dx = hx - ki.tx, dy = hy - ki.ty;
    if (e == 9) return 30000;
    if (dx < 0) dx = -dx; if (dy < 0) dy = -dy;
    if (e == 2 + dran) return dx + dy + 400;      /* eigene Burg */
    return dx + dy;
}
static void ki_pruefe(short k, short r, short c, short w, short l)
{
    short hx, hy, a; BYTE e;
    if (l < 1 || l > LMAX || l > (short)b[dran].pulver) return;
    e = probe(dran, r, c, w, l, &hx, &hy); ki.probes++;
    a = ki_abst(e, hx, hy) + l / 16;              /* bei Gleichstand weniger Pulver */
    if (a < ki.best) { ki.best = a; ki.bw = w; ki.bl = l; ki.bk = k; }
}
static void ki_winkel(short k, short w)          /* eine Kanone, ein Winkel: Ladung suchen */
{
    short r, c, lo = 1, hi = LMAX, m, hx, hy, dir = b[dran].rechts ? -1 : 1, i; BYTE e;
    if (!kanone(dran, k, &r, &c)) return;
    if (fpu && !ohne87) {                         /* 8087: geschlossene Loesung, dann Feinsuche */
        short sx = zx(dran, c) + CS / 2, sy = zy(dran, r);
        unsigned l16 = ki_fpu_ladung((ki.tx - sx) * dir, sy - ki.ty, sind(w), cosd(w), wind * dir);
        if (!l16) return;
        m = (short)((l16 + 8) / 16);
        for (i = -1; i <= 2; i++) ki_pruefe(k, r, c, w, m + i);
        return;
    }
    while (hi - lo > 1) {                         /* ohne 8087: Halbierung mit Probeschuessen */
        m = (lo + hi) / 2;
        e = probe(dran, r, c, w, m, &hx, &hy); ki.probes++;
        if (e != 9 && (hx - ki.tx) * dir < 0) lo = m; else hi = m;
    }
    ki_pruefe(k, r, c, w, lo); ki_pruefe(k, r, c, w, hi);
}
static void ki_tick(void)
{
    DWORD t = TickCount(); short w, l;
    static const BYTE fw[3] = { 9, 5, 3 }, fl[3] = { 9, 5, 3 };    /* Streuung nach Staerke */
    while (ki.ik < ki.nk && TickCount() - t < 50) {   /* in Stuecken: RD2 bleibt bedienbar */
        ki_winkel(ki.ik, ki.w);
        ki.w += 5; if (ki.w > 77) { ki.w = 12; ki.ik++; }
    }
    if (ki.ik < ki.nk) return;
    if (!demo && TickCount() - ki.t0 < 800) return;   /* kurz "nachdenken", damit man es sieht */
    if (test) { dbgz("KI Probeschuesse", ki.probes); dbgz("KI ms", (long)(TickCount() - ki.t0)); dbgz("KI Abstand", ki.best); }
    if (ki.bk < 0) { aktion('N', 0, 0, 0, 0); return; }
    w = ki.bw + (short)krnd(fw[staerke]) - fw[staerke] / 2;
    l = ki.bl + (short)krnd(fl[staerke]) - fl[staerke] / 2;
    if (w < 0) w = 0; if (w > 90) w = 90; if (l < 1) l = 1; if (l > LMAX) l = LMAX; if (l > (short)b[dran].pulver) l = b[dran].pulver;
    if (!aktion('S', (BYTE)ki.bk, (BYTE)w, (BYTE)l, 0)) aktion('N', 0, 0, 0, 0);
}

/* ---- Netz: Pakete zu 8 Bytes (Art, a, b, c, 32 Bit) ------------------------------------------ */
static void spiel_aufbauen(DWORD seed);
static void getrennt_melden(void)                /* Gegner weg: laufenden Flug noch zu Ende zeigen */
{
    if (zustand == S_FLUG || zustand == S_KNALL) { getrennt = 1; return; }
    if (zustand != S_ENDE && zustand != S_LEER && zustand != S_NETZ) { sieger = ich; grund = 72; spiel_ende(); }
    else if (zustand == S_NETZ) { zustand = S_LEER; setmeld(T(72)); leiste_neu(); }
}
static void netz_weg(WORD n) { WORD i; nn -= n; for (i = 0; i < nn; i++) nbuf[i] = nbuf[i + n]; }
static void netz_pumpe(void)
{
    BYTE p[8]; DWORD e; WORD i;
    while (nn >= 8) {
        for (i = 0; i < 8; i++) p[i] = nbuf[i];
        e = p[4] | ((DWORD)p[5] << 8) | ((DWORD)p[6] << 16) | ((DWORD)p[7] << 24);
        switch (p[0]) {
        case 'H':                                 /* vom Eroeffner: Version, Startwert */
            netz_weg(8);
            if (zustand != S_NETZ) break;
            senden('h', NETVER, 0, 0, 0);
            if (p[1] != NETVER) { MsgBox(fe, T(76), T(1), MB_ERROR); netz_aus(); zustand = S_LEER; setmeld(T(76)); InvalidateRect(fe, 0, FALSE); break; }
            spiel_aufbauen(e); break;
        case 'h': netz_weg(8); if (p[1] != NETVER) { netz_aus(); sieger = 0xFE; zustand = S_ENDE; setmeld(T(76)); InvalidateRect(fe, 0, FALSE); } break;
        case 'Z': netz_weg(8); { BYTE k = (BYTE)(p[1] & 7); pruef[1][k] = e; phat[1][k] = 1; pruef_vergleich(p[1]); } break;
        case 'B': netz_weg(8); NetClose(sock); sock = 0; nn = 0; getrennt_melden(); return;
        case 'A': case 'N': case 'S': case 'K': case 'T':
            if (zustand != S_FERN) return;       /* erst, wenn der Gegner dran ist (eigenen Flug abwarten) */
            netz_weg(8);
            aktion(p[0], p[1], p[2], p[3], 1);
            break;
        default: netz_weg(8);
        }
    }
}

/* ---- Spiel beginnen ----------------------------------------------------------------------- */
static void spiel_aufbauen(DWORD seed)
{
    BYTE i;
    zz = seed;
    gelaende(); burg_neu(&b[0]); burg_neu(&b[1]);
    for (i = 0; i < 2; i++) b[i].art = A_MENSCH;
    if (spielart == G_KI) { b[1].art = A_KI; if (demo) b[0].art = A_KI; }
    if (spielart == G_ZWEI && demo) b[0].art = b[1].art = A_KI;
    if (spielart >= G_HOST) { b[ich ^ 1].art = A_FERN; if (demo) b[ich].art = A_KI; }
    dran = 0; runde = 0; zugnr = 0; getrennt = 0; sieger = 0xFF; grund = 0; bx = -1; knall = 0; zielan = 0; dl = 1; dr = 0; erg[0] = 0;
    for (i = 0; i < 8; i++) phat[0][i] = phat[1][i] = 0;
    dbgz("Spiel beginnt, Start", (long)(seed & 0x7FFFFFFFUL)); dbgz("Spielart", spielart);
    zustand = S_LEER;
    zug_beginn();
    InvalidateRect(fe, 0, FALSE);                 /* ganzes Bild (zeichnet auch die Ziellinie) */
}
static void netz_start(void)
{
    zustand = S_NETZ; nn = 0;
    if (NetStart(fe)) { setmeld(T(73)); zustand = S_LEER; }
    else setmeld(T(spielart == G_HOST ? 77 : 71));
    InvalidateRect(fe, 0, FALSE);
}
static void neues_spiel_starten(void)
{
    netz_aus(); ziel_weg();
    if (spielart >= G_HOST) { ich = (BYTE)(spielart == G_JOIN); netz_start(); return; }
    ich = 0;
    spiel_aufbauen(startwert ? startwert : TickCount() * 2654435761UL + 1);
    startwert = 0;
}

static BOOL RDCALLBACK nproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD i;
    (void)p2;
    if (msg == M_INITDIALOG) { DlgCheck(d, (WORD)(21 + spielart), 1); DlgCheck(d, (WORD)(41 + staerke), 1); DlgSetText(d, 30, adresse); return FALSE; }
    if (msg == M_COMMAND && p1 == ID_OK) {
        for (i = 0; i < 4; i++) if (DlgChecked(d, (WORD)(21 + i))) spielart = (BYTE)i;
        for (i = 0; i < 3; i++) if (DlgChecked(d, (WORD)(41 + i))) staerke = (BYTE)i;
        DlgGetText(d, 30, adresse, sizeof adresse);
    }
    return FALSE;
}
static void neues_spiel(void)
{
    char t[4];
    if (zustand != S_LEER && zustand != S_ENDE && zustand != S_NETZ && MsgBox(fe, T(79), T(1), MB_YESNO | MB_ASK) != ID_YES) return;
    if (DialogRun(hi, 10, fe, nproc, 0) != ID_OK) return;
    if (spielart == G_JOIN && !adresse[0]) { MsgBox(fe, T(74), T(1), MB_ERROR); return; }
    t[0] = (char)('0' + spielart); t[1] = 0; ProfileSetStr(ini, "Burgen", "Gegner", t);
    t[0] = (char)('0' + staerke); ProfileSetStr(ini, "Burgen", "Staerke", t);
    ProfileSetStr(ini, "Burgen", "Adresse", adresse);
    port = PORT;
    neues_spiel_starten();
}
static void adresse_teilen(char *host)           /* "host:port" */
{
    short i = 0; WORD p = 0;
    while (adresse[i] && adresse[i] != ':' && i < 39) { host[i] = adresse[i]; i++; } host[i] = 0;
    if (adresse[i] == ':') { i++; while (adresse[i] >= '0' && adresse[i] <= '9') p = (WORD)(p * 10 + adresse[i++] - '0'); }
    if (p) port = p;
}

/* ---- Bedienung ---------------------------------------------------------------------------- */
static void ziel_aendern(short dw, short dlad)
{
    BURG *g = &b[dran]; short w = g->winkel + dw, l = g->ladung + dlad;
    if (w < 0) w = 0; if (w > 90) w = 90; if (l < 1) l = 1; if (l > LMAX) l = LMAX;
    ziel_weg(); g->winkel = (BYTE)w; g->ladung = (BYTE)l;
    if (dw) burg_neu_zeigen(dran);                /* Rohre drehen sich mit */
    zielmeld(); leiste_neu(); ziel_hin();
}
static void maus_ziel(short mx, short my, BYTE klick)
{
    BURG *g = &b[dran]; short r, c, x, y, dx, dy, w, d, l, bw = 45; long dd, best = 0x7FFFFFFFL;
    if (!mensch_dran() || my < yt || !kanone(dran, g->wahl, &r, &c)) return;
    x = (short)((long)mx * WW / cw); y = (short)((long)(my - yt) * WH / (ch - yt));
    if (klick) {                                  /* Klick auf eine eigene Kanone waehlt sie */
        short k, n = kanonen(dran), rr, cc;
        for (k = 0; k < n; k++) if (kanone(dran, k, &rr, &cc) && x >= zx(dran, cc) - 3 && x < zx(dran, cc) + CS + 3 && y >= zy(dran, rr) - 3 && y < zy(dran, rr) + CS + 3) {
            ziel_weg(); g->wahl = (BYTE)k; zielmeld(); leiste_neu(); ziel_hin(); return; }
    }
    dx = (x - zx(dran, c) - CS / 2) * (g->rechts ? -1 : 1); dy = zy(dran, r) + CS / 2 - y;
    if (dx < 0) dx = 0; if (dy < 0) dy = 0;
    if (!dx && !dy) return;
    for (w = 0; w <= 90; w++) {                   /* Richtung -> Winkel (Tabelle statt atan) */
        dd = (long)dx * sind(w) - (long)dy * cosd(w); if (dd < 0) dd = -dd;
        if (dd < best) { best = dd; bw = w; }
    }
    d = dx > dy ? dx + dy / 2 : dy + dx / 2;      /* Laenge, ungefaehr */
    l = d - 12; if (l < 1) l = 1; if (l > LMAX) l = LMAX;
    if (bw == g->winkel && l == g->ladung) return;
    ziel_weg(); g->ladung = (BYTE)l;
    if (bw != g->winkel) { g->winkel = (BYTE)bw; burg_neu_zeigen(dran); }
    zielmeld(); leiste_neu(); ziel_hin();
}
static void feuer(void) { if (mensch_dran()) aktion('S', b[dran].wahl, b[dran].winkel, b[dran].ladung, 0); }
static void kaufen(BYTE a) { if (mensch_dran()) { if (!aktion('K', a, 0, 0, 0)) leiste_neu(); } }

static void befehl(WORD id)
{
    char t[8]; short v, i;
    switch (id) {
    case CM_NEU: neues_spiel(); break;
    case CM_AUFGEBEN: if (mensch_dran() && MsgBox(fe, T(78), T(1), MB_YESNO | MB_ASK | MB_DEFNO) == ID_YES) aktion('A', 0, 0, 0, 0); break;
    case CM_ENDE: DestroyWindow(fe); break;
    case CM_KANONE: kaufen(1); break;
    case CM_KUGELN: kaufen(2); break;
    case CM_PULVER: kaufen(3); break;
    case CM_MAUER:  kaufen(4); break;
    case CM_MINE:   kaufen(5); break;
    case CM_STEUER:
        if (!mensch_dran()) break;
        szahl(t, b[dran].steuer);
        if (InputBox(fe, T(60), T(61), t, sizeof t)) {
            for (v = 0, i = 0; t[i] >= '0' && t[i] <= '9' && v < 1000; i++) v = v * 10 + t[i] - '0';
            if (v > 80) v = 80;
            aktion('T', (BYTE)v, 0, 0, 0);
        }
        break;
    case CM_FEUER: feuer(); break;
    case CM_WEITER: if (mensch_dran()) { short n = kanonen(dran); ziel_weg(); b[dran].wahl = (BYTE)(n ? (b[dran].wahl + 1) % n : 0); zielmeld(); leiste_neu(); ziel_hin(); } break;
    case CM_PASSEN: if (mensch_dran()) aktion('N', 0, 0, 0, 0); break;
    case CM_REGELN: MsgBox(fe, T(80), T(81), MB_INFO); break;
    case CM_INFO: MsgBox(fe, T(fpu && !ohne87 ? 83 : 82), T(1), MB_INFO); break;
    }
}

static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_PAINT: malen(w); return 0;
    case M_ERASEBKGND: return 1;
    case M_SIZE: if (fe) { groesse(); zielan = 0; InvalidateRect(w, 0, FALSE); UpdateWindow(w); ziel_hin(); } return 0;
    case M_TIMER:
        if (p1 == 2) { DestroyWindow(w); return 0; }
        if (zustand == S_FLUG) flug_tick();
        else if (zustand == S_KNALL) knall_tick();
        else if (zustand == S_KI) ki_tick();
        return 0;
    case M_KEY:
        if (!mensch_dran()) break;
        switch (p1) {
        case K_LEFT:  ziel_aendern(b[dran].rechts ? -1 : 1, 0); return 0;
        case K_RIGHT: ziel_aendern(b[dran].rechts ? 1 : -1, 0); return 0;
        case K_PGUP:  ziel_aendern(0, 10); return 0;
        case K_PGDN:  ziel_aendern(0, -10); return 0;
        case K_UP:    ziel_aendern(0, 1); return 0;
        case K_DOWN:  ziel_aendern(0, -1); return 0;
        case K_HOME:  ziel_aendern(b[dran].rechts ? -10 : 10, 0); return 0;
        case K_END:   ziel_aendern(b[dran].rechts ? 10 : -10, 0); return 0;
        case K_TAB:   befehl(CM_WEITER); return 0;
        case K_ENTER: case ' ': feuer(); return 0;
        }
        break;
    case M_CHAR: if (p1 == ' ' && mensch_dran()) { feuer(); return 0; } break;
    case M_BUTTONDOWN:
        if ((p1 & 0xFF) == 2) { feuer(); return 0; }
        if ((p1 & 0xFF) == 1) { maus_ziel((short)LOWORD(p2), (short)HIWORD(p2), 1); CaptureMouse(w); }
        return 0;
    case M_MOUSEMOVE: if (p1 & 1) maus_ziel((short)LOWORD(p2), (short)HIWORD(p2), 0); return 0;
    case M_BUTTONUP: ReleaseMouse(); return 0;
    case M_COMMAND: befehl(p1); return 0;
    case M_NETSTATE:
        if (zustand != S_NETZ) return 0;
        if (p1 != NS_OPEN) { setmeld(T(73)); zustand = S_LEER; leiste_neu(); return 0; }
        if (spielart == G_HOST) {
            char *p; DWORD ip = NetLocalIP();
            lsock = NetListen(w, port, 0);
            p = sapp(meld, T(70)); p = szahl(p, ip & 255); *p++ = '.'; p = szahl(p, (ip >> 8) & 255); *p++ = '.'; p = szahl(p, (ip >> 16) & 255); *p++ = '.'; p = szahl(p, ip >> 24);
            if (port != PORT) { *p++ = ':'; szahl(p, port); }
            dbg("BURGEN: lauscht"); leiste_neu();
        } else {
            char host[40]; adresse_teilen(host);
            if (NetResolve(w, host)) { setmeld(T(74)); zustand = S_LEER; leiste_neu(); }
        }
        return 0;
    case M_NETRESOLVED:
        if (zustand != S_NETZ) return 0;
        if (p1) { setmeld(T(74)); zustand = S_LEER; leiste_neu(); return 0; }
        sock = NetOpen(w, p2, port, NO_TCP);
        if (!sock) { setmeld(T(72)); zustand = S_LEER; leiste_neu(); }
        return 0;
    case M_NETCONNECT:
        if ((HSOCK)p1 != sock) return 0;
        if (p2) { sock = 0; setmeld(T(72)); zustand = S_LEER; leiste_neu(); }
        else dbg("BURGEN: verbunden");
        return 0;
    case M_NETACCEPT:
        if (zustand != S_NETZ || sock) { NetClose((HSOCK)p1); return 0; }
        sock = (HSOCK)p1; if (lsock) { NetClose(lsock); lsock = 0; }
        dbg("BURGEN: Mitspieler da");
        { DWORD s = startwert ? startwert : TickCount() * 2654435761UL + 1; startwert = 0; senden('H', NETVER, 0, 0, s); spiel_aufbauen(s); }
        return 0;
    case M_NETDATA:
        if ((HSOCK)p1 != sock) return 0;
        { WORD n; while (nn < sizeof nbuf && (n = NetRecv(sock, nbuf + nn, (WORD)(sizeof nbuf - nn))) != 0) nn += n; }
        netz_pumpe();
        return 0;
    case M_NETCLOSED:
        if ((HSOCK)p1 != sock) return 0;
        { WORD n; while (nn < sizeof nbuf && (n = NetRecv(sock, nbuf + nn, (WORD)(sizeof nbuf - nn))) != 0) nn += n; }
        netz_pumpe();                             /* letzte Pakete (Aufgabe, Pruefsumme) noch lesen */
        if (sock) { NetClose(sock); sock = 0; }
        getrennt_melden();
        return 0;
    case M_CLOSE: DestroyWindow(w); return 0;
    case M_DESTROY: netz_aus(); TimerKill(w, 1); TimerKill(w, 2); dbg("BURGEN: Ende"); PostQuit(sieger); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; SYSINFO si; short W, H, i, n; char t[8];
    (void)show; hi = inst;
    KernInfo(&si); fpu = (BYTE)(si.fpu != 0);
    for (n = 0; si.home[n] && n < 60; n++) ini[n] = si.home[n];
    if (n && ini[n - 1] != '\\') ini[n++] = '\\';
    sapp(ini + n, "RD2.INI");
    ProfileGetStr(ini, "Burgen", "Gegner", "0", t, sizeof t); spielart = (BYTE)(t[0] >= '0' && t[0] <= '3' ? t[0] - '0' : 0);
    ProfileGetStr(ini, "Burgen", "Staerke", "1", t, sizeof t); staerke = (BYTE)(t[0] >= '0' && t[0] <= '2' ? t[0] - '0' : 1);
    ProfileGetStr(ini, "Burgen", "Adresse", "", adresse, sizeof adresse);
    for (i = 0; cmd && cmd[i]; i++) {
        switch (cmd[i]) {
        case 'T': test = 1; break;
        case 'D': demo = 1; break;
        case 'N': ohne87 = 1; break;
        case 'Z': zshot = 1; break;
        case 'K': spielart = G_KI; sofort = 1; break;
        case '2': spielart = G_ZWEI; sofort = 1; break;
        case 'H': spielart = G_HOST; sofort = 1; break;
        case 'S': startwert = 0; while (cmd[i + 1] >= '0' && cmd[i + 1] <= '9') startwert = startwert * 10 + cmd[++i] - '0'; break;
        case 'L': if (cmd[i + 1] >= '0' && cmd[i + 1] <= '2') staerke = (BYTE)(cmd[++i] - '0'); break;
        case 'P': port = 0; while (cmd[i + 1] >= '0' && cmd[i + 1] <= '9') port = (WORD)(port * 10 + cmd[++i] - '0'); if (!port) port = PORT; break;
        case 'J': n = 0; while (cmd[i + 1] && cmd[i + 1] != ' ' && n < 39) adresse[n++] = cmd[++i]; adresse[n] = 0; spielart = G_JOIN; sofort = 1; break;
        }
    }
    wc.name = "Burgen"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    if (!RegisterClass(&wc)) return 1;
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); ReleaseDC(0, d);
    fe = CreateWindow("Burgen", T(1), WS_NORMAL | WS_VISIBLE, 4, H / 10, W - 8, H - H / 10 - 4, 0, 0, inst);
    if (!fe) return 2;
    MenuSet(fe, MenuLoad(inst, 1));
    groesse();
    TimerSet(fe, 1, 55);
    setmeld(T(90));
    dbgz("bereit, 8087", fpu && !ohne87);
    if (sofort) neues_spiel_starten();            /* Befehlszeile: ohne Dialog */
    while (GetMessage(&m)) DispatchMessage(&m);
    return 0;
}
