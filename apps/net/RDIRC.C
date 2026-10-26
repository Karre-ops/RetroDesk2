/* RDIRC.C - IRC-Client (Punkt 8, wie RD1 RDIRC.OVL): ein Fenster mit Verlauf oben und Eingabezeile unten.
 * Server, Port, Nick, Kanal, Kennwort aus RD2.INI [IRC] (RDNET, Seite Dienste). Verbindet beim Start, nur RDTCP.
 * Fuer ZNC/BitlBee auf dem NAS: Kennwort geht als PASS vor NICK/USER. Zeichensatz: UTF-8 <-> CP437 (Umlaute, ss, e/a-Akzente).
 * Eingabe: Text geht an den Kanal; /join /part /nick /me /msg /raw /quit. Bild auf/ab blaettert, Pfeil hoch holt die letzte Zeile.
 * Esc leert die Eingabe, bei leerer Eingabe schliesst es. Befehlszeile "T": Testmodus (DbgPrint), test/rdirc.sh. */
#include "RD2.H"

#define NV   120                                  /* Zeilen im Verlauf */
#define VB   81                                   /* Zeichen je Zeile + 0 */
#define RAND 2
#define CM_VERB 101
#define CM_TRENN 102
#define CM_ENDE 109
enum { I_AUS, I_NETZ, I_DNS, I_VERB, I_ANMELD, I_DA };

static HINST hi; static HWND fe; static HMENU mn; static BYTE test, phase; static WORD nw;
static HSOCK hc; static DWORD sip; static WORD sport;
static char srv[40], nick[32], kanal[40], pass[64], chan[40], titel[100];
static char zb[520], ob[520], eb[200], letzt[200]; static WORD zl, el, ex, eo;
static HMEM hm; static char FAR *vl; static WORD vn, zur;   /* vn: geschriebene Zeilen (Ring), zur: zurueckgeblaettert */
static short zh, cw, cbreit, choch; static WORD rows, cols;

/* ---------------- Hilfen ---------------- */
static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static char kl(char c) { return (char)(c >= 'A' && c <= 'Z' ? c + 32 : c); }
static BYTE gleich(LPCSTR a, LPCSTR b) { while (*a && kl(*a) == kl(*b)) { a++; b++; } return (BYTE)(!*a && !*b); }
static void pr(LPCSTR a, LPCSTR b) { if (test) { char t[120]; scpy(t, "RDIRC: ", sizeof t); scat(t, a, sizeof t); scat(t, b, sizeof t); DbgPrint(t); } }

/* ---------------- Zeichensatz ---------------- */
static const BYTE cp[] = { 0x84, 0x94, 0x81, 0x8E, 0x99, 0x9A, 0xE1, 0x82, 0x8A, 0x85, 0x87, 0xF8 };
static const BYTE u8[] = { 0xA4, 0xB6, 0xBC, 0x84, 0x96, 0x9C, 0x9F, 0xA9, 0xA8, 0xA0, 0xA7, 0xB0 };   /* nach C3, Grad nach C2 */
#define NZ 12
static void von_utf8(char *s)
{
    BYTE *r = (BYTE *)s, *w = r; WORD i;
    while (*r) {
        if (*r < 0x80) { *w++ = *r++; continue; }
        if ((*r == 0xC3 || *r == 0xC2) && r[1]) {
            for (i = 0; i < NZ && !(u8[i] == r[1] && (*r == 0xC2) == (i == NZ - 1)); i++) ;
            *w++ = (BYTE)(i < NZ ? cp[i] : '?'); r += 2; continue;
        }
        *w++ = '?'; r++;
        while ((*r & 0xC0) == 0x80) r++;
    }
    *w = 0;
}
static void nach_utf8(LPCSTR s, char *o, WORD max)
{
    WORD n = 0, i;
    while (*s && n < max - 3) {
        BYTE c = (BYTE)*s++;
        if (c < 0x80) { o[n++] = (char)c; continue; }
        for (i = 0; i < NZ && cp[i] != c; i++) ;
        if (i < NZ) { o[n++] = (char)(i == NZ - 1 ? 0xC2 : 0xC3); o[n++] = (char)u8[i]; } else o[n++] = '?';
    }
    o[n] = 0;
}

/* ---------------- Verlauf und Anzeige ---------------- */
static WORD vsicht(void) { return vn < NV ? vn : NV; }
static void balken(void)
{
    WORD v = vsicht(), m = v > rows ? v - rows : 0;
    if (zur > m) zur = m;
    ScrollSet(fe, SB_VERT, (short)(m - zur), (short)m, (short)rows);
}
static void put1(LPCSTR s, WORD n)
{
    char FAR *d = vl + (WORD)(vn % NV) * VB; WORD i;
    for (i = 0; i < n && i < VB - 1; i++) d[i] = s[i];
    d[i] = 0; vn++;
    if (vn == 2 * NV) vn = NV;                    /* Ring bleibt voll, Zaehler laeuft nicht ueber */
    if (zur) zur++;                               /* Zurueckgeblaettert: Bild steht */
}
static void sag(LPCSTR s)                         /* anhaengen, an Leerzeichen umbrechen, Folgezeilen eingerueckt */
{
    WORD l = slen(s), w = cols < 20 ? 20 : (cols > VB - 1 ? VB - 1 : cols), k, ein = 0, platz; char t[VB];
    if (test) pr("| ", s);
    do {
        platz = w - ein;
        if (l <= platz) k = l;
        else { for (k = platz; k > platz / 2 && s[k] != ' '; k--) ; if (k <= platz / 2) k = platz; }
        { WORD i; for (i = 0; i < ein; i++) t[i] = ' '; for (i = 0; i < k; i++) t[ein + i] = s[i]; }
        put1(t, ein + k);
        s += k; l -= k;
        while (*s == ' ') { s++; l--; }
        ein = 2;
    } while (l);
    balken(); InvalidateRect(fe, 0, TRUE);
}
static void sag2(LPCSTR a, LPCSTR b) { static char t[600]; scpy(t, a, sizeof t); scat(t, b, sizeof t); sag(t); }
static void sagz(WORD id, LPCSTR b) { sag2(StrGet(hi, id), b); }
static void kopf(void)
{
    scpy(titel, StrGet(hi, 1), sizeof titel);
    if (phase != I_AUS) { scat(titel, " - ", sizeof titel); scat(titel, nick, sizeof titel); scat(titel, " @ ", sizeof titel); scat(titel, srv, sizeof titel); }
    if (chan[0]) { scat(titel, "  ", sizeof titel); scat(titel, chan, sizeof titel); }
    SetWindowText(fe, titel);
}
static void eingabe_neu(void) { RECT r; r.l = 0; r.r = cbreit; r.t = (short)(rows * zh); r.b = choch; InvalidateRect(fe, &r, TRUE); }
static void malen(void)
{
    PAINT ps; HDC d; WORD v = vsicht(), i, erste, k, pl; short y; char t[VB + 50]; LPCSTR p;
    d = BeginPaint(fe, &ps);
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK); SetPen(d, C_BLACK);
    erste = v > rows + zur ? v - rows - zur : 0;   /* aelteste sichtbare Zeile (0 = aelteste im Ring) */
    for (i = 0, y = 0; i < rows && erste + i < v - zur; i++, y += zh) {
        char FAR *s = vl + (WORD)((vn - v + erste + i) % NV) * VB; WORD n = 0;
        if (y + zh <= ps.neu.t || y >= ps.neu.b) continue;
        while (s[n] && n < cols) { t[n] = s[n]; n++; }
        if (n) TextOut(d, RAND, y, t, (short)n);
    }
    y = (short)(rows * zh);
    if (ps.neu.b > y) {                           /* Trennstrich und Eingabezeile "[#kanal] text" */
        Line(d, 0, (short)(y + 1), (short)(cbreit - 1), (short)(y + 1));
        t[0] = '['; t[1] = 0; p = chan[0] ? chan : (phase == I_DA ? nick : StrGet(hi, 3)); scat(t, p, 30); scat(t, "] ", sizeof t);
        pl = slen(t);
        if (cols > pl + 4) {
            WORD sicht = cols - pl - 1;
            if (ex < eo) eo = ex; else if (ex > eo + sicht) eo = ex - sicht;
            for (k = 0; k < sicht && eo + k < el; k++) t[pl + k] = eb[eo + k];
            t[pl + k] = 0;
            TextOut(d, RAND, (short)(y + 3), t, -1);
            if (GetFocus() == fe) { short x = (short)(RAND + (pl + ex - eo) * cw); Line(d, x, (short)(y + 3), x, (short)(y + 2 + zh)); }
        }
    }
    EndPaint(fe, &ps);
}
static void groesse(void)
{
    RECT r; GetClientRect(fe, &r); cbreit = r.r; choch = r.b;
    rows = (WORD)((r.b - zh - 4) / zh); if (!rows || r.b < zh * 2) rows = 1;
    cols = (WORD)((r.r - 2 * RAND) / cw); if (!cols) cols = 1;
    if (cols > VB - 1) cols = VB - 1;
    balken(); InvalidateRect(fe, 0, TRUE);
}

/* ---------------- Senden ---------------- */
static void raus(LPCSTR a, LPCSTR b, LPCSTR c)    /* "A b :c" */
{
    scpy(ob, a, sizeof ob);
    if (b) { scat(ob, " ", sizeof ob); scat(ob, b, sizeof ob); }
    if (c) { scat(ob, " :", 400); scat(ob, c, 480); }
    if (test && !(a[0] == 'P' && a[1] == 'A' && a[2] == 'S' && a[3] == 'S')) pr("> ", ob);
    scat(ob, "\r\n", sizeof ob);
    if (hc) NetSend(hc, ob, slen(ob));
}
static void trennen(WORD txt, LPCSTR b)
{
    if (hc) { if (phase >= I_ANMELD) raus("QUIT", 0, "RetroDesk 2"); NetClose(hc); hc = 0; }
    phase = I_AUS; chan[0] = 0; kopf(); eingabe_neu();
    if (txt) sagz(txt, b);
    pr("getrennt ", b ? b : "");
}
static void verbinden(void)
{
    char ini[80], t[8]; SYSINFO si; WORD i;
    if (phase != I_AUS) return;
    KernInfo(&si); scpy(ini, si.home, sizeof ini); if (slen(ini) && ini[slen(ini) - 1] != '\\') scat(ini, "\\", sizeof ini); scat(ini, "RD2.INI", sizeof ini);
    ProfileGetStr(ini, "IRC", "Server", "", srv, sizeof srv);
    ProfileGetStr(ini, "IRC", "Port", "6667", t, sizeof t); for (sport = 0, i = 0; t[i] >= '0' && t[i] <= '9'; i++) sport = (WORD)(sport * 10 + t[i] - '0'); if (!sport) sport = 6667;
    ProfileGetStr(ini, "IRC", "Nick", "RD2", nick, sizeof nick); if (!nick[0]) scpy(nick, "RD2", sizeof nick);
    ProfileGetStr(ini, "IRC", "Kanal", "", kanal, sizeof kanal);
    ProfileGetStr(ini, "IRC", "Kennwort", "", pass, sizeof pass);
    if (!srv[0]) { sagz(4, ""); return; }
    phase = I_NETZ; nw = 0; zl = 0; kopf(); sagz(5, srv);
    if (NetStart(fe)) trennen(6, "");
}

/* ---------------- Empfangen ---------------- */
static char *wort(char **p)                       /* naechstes Wort, ":" leitet den Rest ein */
{
    char *s = *p, *a;
    while (*s == ' ') s++;
    if (*s == ':') { *p = s + slen(s); return s + 1; }
    a = s; while (*s && *s != ' ') s++;
    if (*s) *s++ = 0;
    *p = s; return a;
}
static void zeile(char *z)
{
    char *p = z, *wer = "", *cmd, *a1, *a2, *a3, *q; char t[560]; WORD code = 0;
    if (test) pr("< ", z);
    von_utf8(z);
    if (*p == ':') { wer = ++p; while (*p && *p != ' ') p++; if (*p) *p++ = 0; for (q = wer; *q; q++) if (*q == '!') { *q = 0; break; } }
    cmd = wort(&p); a1 = wort(&p); a2 = wort(&p); a3 = wort(&p);
    if (cmd[0] >= '0' && cmd[0] <= '9') code = (WORD)((cmd[0] - '0') * 100 + (cmd[1] - '0') * 10 + cmd[2] - '0');
    if (gleich(cmd, "PING")) { raus("PONG", 0, a1); return; }
    if (gleich(cmd, "PRIVMSG") || gleich(cmd, "NOTICE")) {
        BYTE nt = (BYTE)(cmd[0] == 'N' || cmd[0] == 'n');
        if (a2[0] == 1) {                         /* CTCP */
            if (!nt && a2[1] == 'A' && a2[2] == 'C' && a2[3] == 'T' && a2[4] == 'I' && a2[5] == 'O' && a2[6] == 'N') {
                for (q = a2 + 7; *q; q++) if (*q == 1) *q = 0;
                scpy(t, "* ", sizeof t); scat(t, wer, sizeof t); scat(t, " ", sizeof t); scat(t, a2 + 8, sizeof t);
                sag(t); return;
            }
            if (!nt && a2[1] == 'V') { char v[40]; v[0] = 1; v[1] = 0; scat(v, "VERSION RetroDesk 2", sizeof v); scat(v, "\1", sizeof v); raus("NOTICE", wer, v); }
            return;
        }
        if (nt) { scpy(t, "-", sizeof t); scat(t, wer[0] ? wer : srv, sizeof t); scat(t, "- ", sizeof t); }
        else if (gleich(a1, nick)) { scpy(t, "*", sizeof t); scat(t, wer, sizeof t); scat(t, "* ", sizeof t); }
        else {
            t[0] = 0;
            if (!gleich(a1, chan)) { scat(t, "[", sizeof t); scat(t, a1, sizeof t); scat(t, "] ", sizeof t); }
            scat(t, "<", sizeof t); scat(t, wer, sizeof t); scat(t, "> ", sizeof t);
        }
        sag2(t, a2); return;
    }
    if (gleich(cmd, "JOIN")) {
        if (gleich(wer, nick)) { scpy(chan, a1, sizeof chan); kopf(); eingabe_neu(); }
        scpy(t, "-!- ", sizeof t); scat(t, wer, sizeof t); scat(t, StrGet(hi, 20), sizeof t); sag2(t, a1); return;
    }
    if (gleich(cmd, "PART")) {
        if (gleich(wer, nick) && gleich(a1, chan)) { chan[0] = 0; kopf(); eingabe_neu(); }
        scpy(t, "-!- ", sizeof t); scat(t, wer, sizeof t); scat(t, StrGet(hi, 21), sizeof t); sag2(t, a1); return;
    }
    if (gleich(cmd, "QUIT")) { scpy(t, "-!- ", sizeof t); scat(t, wer, sizeof t); scat(t, StrGet(hi, 22), sizeof t); scat(t, a1, sizeof t); sag2(t, ")"); return; }
    if (gleich(cmd, "NICK")) {
        scpy(t, "-!- ", sizeof t); scat(t, wer, sizeof t); scat(t, StrGet(hi, 23), sizeof t);
        if (gleich(wer, nick)) { scpy(nick, a1, sizeof nick); kopf(); }
        sag2(t, a1); return;
    }
    if (gleich(cmd, "KICK")) {
        scpy(t, "-!- ", sizeof t); scat(t, wer, sizeof t); scat(t, StrGet(hi, 24), sizeof t); scat(t, a2, sizeof t);
        scat(t, StrGet(hi, 25), sizeof t); scat(t, a3, sizeof t); sag2(t, ")");
        if (gleich(a2, nick) && gleich(a1, chan)) { chan[0] = 0; kopf(); eingabe_neu(); }
        return;
    }
    if (gleich(cmd, "TOPIC")) { scpy(t, "-!- ", sizeof t); scat(t, StrGet(hi, 26), sizeof t); sag2(t, a2); return; }
    if (gleich(cmd, "ERROR")) { sag2("-!- ", a1); return; }
    if (code == 1) { phase = I_DA; nw = 0; kopf(); eingabe_neu(); sag2("-!- ", a2); if (kanal[0]) raus("JOIN", kanal, 0); pr("angemeldet ", nick); return; }
    if (code == 433 && phase == I_ANMELD) { scat(nick, "_", sizeof nick); raus("NICK", nick, 0); return; }
    if (code == 332) { scpy(t, "-!- ", sizeof t); scat(t, StrGet(hi, 26), sizeof t); sag2(t, a3); return; }
    if (code == 353) { char *n = a3; char *r = wort(&p); scpy(t, "-!- ", sizeof t); scat(t, StrGet(hi, 27), sizeof t); sag2(t, *r ? r : n); return; }
    if (code == 4 || code == 5 || code == 333 || code == 366) return;
    if (code) { if (*p) { char *r = wort(&p); sag2("-!- ", r); } else sag2("-!- ", *a3 ? a3 : a2); }
}
static void lesen(void)
{
    char b[128]; WORD n, i;
    while (hc && (n = NetRecv(hc, b, sizeof b)) != 0 && n != 0xFFFF) {
        nw = 0;
        for (i = 0; i < n; i++) {
            if (b[i] == '\n') { zb[zl] = 0; if (zl) zeile(zb); zl = 0; if (!hc) return; }
            else if (b[i] != '\r' && zl < sizeof zb - 1) zb[zl++] = b[i];
        }
    }
}

/* ---------------- Eingabe ---------------- */
static char *naechstes(char *p) { while (*p && *p != ' ') p++; if (*p) *p++ = 0; while (*p == ' ') p++; return p; }
static void ausfuehren(void)
{
    char z[200], u[480], t[240], *c, *p;
    if (!el) return;
    eb[el] = 0; scpy(letzt, eb, sizeof letzt); scpy(z, eb, sizeof z); el = ex = eo = 0; eingabe_neu();
    if (z[0] != '/') {
        if (phase != I_DA || !chan[0]) { sagz(7, ""); return; }
        nach_utf8(z, u, sizeof u); raus("PRIVMSG", chan, u);
        scpy(t, "<", sizeof t); scat(t, nick, sizeof t); scat(t, "> ", sizeof t); sag2(t, z); return;
    }
    c = z + 1; p = naechstes(c);
    if (gleich(c, "quit")) { if (phase != I_AUS) trennen(8, ""); return; }
    if (phase != I_DA) { sagz(7, ""); return; }
    if (gleich(c, "join") || gleich(c, "j")) { if (*p) raus("JOIN", p, 0); return; }
    if (gleich(c, "part")) { raus("PART", *p ? p : chan, 0); return; }
    if (gleich(c, "nick")) { if (*p) raus("NICK", p, 0); return; }
    if (gleich(c, "raw") || gleich(c, "quote")) { nach_utf8(p, u, sizeof u); raus(u, 0, 0); return; }
    if (gleich(c, "me")) {
        if (!chan[0]) { sagz(7, ""); return; }
        u[0] = 1; nach_utf8(p, u + 1, sizeof u - 2);
        { static char v[490]; scpy(v, "\1ACTION ", sizeof v); scat(v, u + 1, sizeof v); scat(v, "\1", sizeof v); raus("PRIVMSG", chan, v); }
        scpy(t, "* ", sizeof t); scat(t, nick, sizeof t); scat(t, " ", sizeof t); sag2(t, p); return;
    }
    if (gleich(c, "msg")) {
        char *ziel = p; p = naechstes(p);
        if (!*ziel || !*p) { sagz(9, ""); return; }
        nach_utf8(p, u, sizeof u); raus("PRIVMSG", ziel, u);
        scpy(t, "-> *", sizeof t); scat(t, ziel, sizeof t); scat(t, "* ", sizeof t); sag2(t, p); return;
    }
    sagz(9, "");
}
static void taste(WORD k)
{
    WORD i;
    switch (k) {
    case K_ENTER: ausfuehren(); return;
    case K_BACK: if (ex) { for (i = ex - 1; i < el - 1; i++) eb[i] = eb[i + 1]; el--; ex--; } break;
    case K_DEL: if (ex < el) { for (i = ex; i < el - 1; i++) eb[i] = eb[i + 1]; el--; } break;
    case K_LEFT: if (ex) ex--; break;
    case K_RIGHT: if (ex < el) ex++; break;
    case K_HOME: ex = 0; break;
    case K_END: ex = el; break;
    case K_UP: scpy(eb, letzt, sizeof eb); el = ex = slen(eb); eo = 0; break;
    case K_PGUP: zur += rows > 1 ? rows - 1 : 1; balken(); InvalidateRect(fe, 0, TRUE); return;
    case K_PGDN: zur = zur > rows - 1 ? zur - (rows > 1 ? rows - 1 : 1) : 0; balken(); InvalidateRect(fe, 0, TRUE); return;
    case K_ESC: if (el) { el = ex = eo = 0; break; } PostMessage(fe, M_CLOSE, 0, 0); return;
    default: return;
    }
    eingabe_neu();
}
static void zeichen(char c)
{
    WORD i;
    if (el >= sizeof eb - 1) return;
    for (i = el; i > ex; i--) eb[i] = eb[i - 1];
    eb[ex++] = c; el++; eingabe_neu();
}

static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    if (!fe) fe = w;
    switch (msg) {
    case M_PAINT: malen(); return 0;
    case M_SIZE: if (zh) groesse(); return 0;
    case M_SETFOCUS: case M_KILLFOCUS: eingabe_neu(); return 0;
    case M_KEY: taste(p1); return 0;
    case M_CHAR: if ((BYTE)p1 >= ' ' && !(HIWORD(p2) & (KS_CTRL | KS_ALT))) zeichen((char)p1); return 0;
    case M_BUTTONDOWN: SetFocus(w); return 0;
    case M_SCROLL:
        if (HIWORD(p2) == SB_VERT) {
            WORD v = vsicht(), m = v > rows ? v - rows : 0, pos = m - zur;
            switch (p1) {
            case SB_LINEUP: if (pos) pos--; break;
            case SB_LINEDOWN: pos++; break;
            case SB_PAGEUP: pos = pos > rows ? pos - rows : 0; break;
            case SB_PAGEDOWN: pos += rows; break;
            case SB_THUMB: pos = LOWORD(p2); break;
            case SB_TOP: pos = 0; break;
            case SB_BOTTOM: pos = m; break;
            }
            if (pos > m) pos = m;
            zur = m - pos; balken(); InvalidateRect(fe, 0, TRUE);
        }
        return 0;
    case M_TIMER:
        if (phase != I_AUS && phase != I_DA && ++nw > (phase == I_NETZ ? 30 : 25)) trennen(6, "");
        return 0;
    case M_NETSTATE:
        if (phase != I_NETZ) return 0;
        if (p1 != NS_OPEN) { trennen(6, ""); return 0; }
        phase = I_DNS; nw = 0; if (NetResolve(w, srv)) trennen(10, srv);
        return 0;
    case M_NETRESOLVED:
        if (phase != I_DNS) return 0;
        if (p1) { trennen(10, srv); return 0; }
        sip = p2; phase = I_VERB; nw = 0;
        if (!(hc = NetOpen(w, sip, sport, NO_TCP))) trennen(11, "");
        return 0;
    case M_NETCONNECT:
        if (p1 != hc || phase != I_VERB) return 0;
        if (p2) { hc = 0; trennen(11, ""); return 0; }
        phase = I_ANMELD; nw = 0;
        if (pass[0]) raus("PASS", pass, 0);
        raus("NICK", nick, 0);
        { char u[60]; scpy(u, nick, sizeof u); scat(u, " 0 *", sizeof u); raus("USER", u, "RetroDesk 2"); }
        return 0;
    case M_NETDATA: if (p1 == hc && hc) lesen(); return 0;
    case M_NETCLOSED: if (p1 == hc && hc) { lesen(); if (hc) { NetClose(hc); hc = 0; } trennen(12, ""); } return 0;
    case M_COMMAND:
        if (p1 == CM_VERB) verbinden(); else if (p1 == CM_TRENN) { if (phase != I_AUS) trennen(8, ""); }
        else if (p1 == CM_ENDE) PostMessage(w, M_CLOSE, 0, 0);
        return 0;
    case M_MENUOPEN: MenuEnable(mn, CM_VERB, phase == I_AUS); MenuEnable(mn, CM_TRENN, phase != I_AUS); return 0;
    case M_CLOSE: TimerKill(w, 1); trennen(0, 0); pr("Ende", ""); DestroyWindow(w); return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H;
    (void)show; hi = inst;
    if (cmd && cmd[0] == 'T') test = 1;
    if (!(hm = MemAlloc((DWORD)NV * VB, MEM_MOVABLE)) || !(vl = (char FAR *)MemLock(hm))) return 1;
    wc.name = "RdIrc"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); zh = GetCaps(d, CAPS_TEXTH); cw = TextWidth(d, "M", 1); ReleaseDC(0, d);
    fe = CreateWindow("RdIrc", StrGet(inst, 1), WS_NORMAL | WS_VISIBLE | WS_VSCROLL, (short)(W / 32), (short)(H / 12), (short)(W - W / 16), (short)(H - H / 12 - H / 20), 0, 1, inst);
    if (!fe) { MemUnlock(hm); MemFree(hm); return 1; }
    mn = MenuLoad(inst, 1); MenuSet(fe, mn);
    groesse(); kopf(); SetFocus(fe); TimerSet(fe, 1, 1000);
    pr("bereit", "");
    verbinden();
    while (GetMessage(&m)) DispatchMessage(&m);
    MenuFree(mn); MemUnlock(hm); MemFree(hm);
    return 0;
}
