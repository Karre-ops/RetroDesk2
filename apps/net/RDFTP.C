/* RDFTP.C - zweiseitiger Dateimanager (RD > Dateimanager), rechte Seite wahlweise FTP-Server (Punkt 8, wie RD1 RDFTP.OVL).
 * Beide Seiten zeigen zuerst die eigene Platte; "FTP verbinden" (Alt+V) schaltet die rechte Seite auf den Server,
 * "Trennen" wieder zurueck. Server, Port, Benutzer, Kennwort aus RD2.INI [FTP] (RDNET, Seite Dienste). Nur RDTCP:
 * Steuerverbindung zeilenweise, Daten passiv (PASV, immer an die Adresse der Steuerverbindung, NAT/Tunnel).
 * Bedienung: Tab wechselt die Seite, Eingabe/Doppelklick oeffnet Ordner (".." hoch, "[-A-]" Laufwerk), Laufwerksleiste
 * ueber jeder Seite, Mehrfachauswahl (Leertaste/Einfg, Strg+Klick, Strg+A), F5 kopiert auf die andere Seite, F7 Ordner,
 * F8/Entf loescht, Strg+R liest neu, Rechtsklick/Umschalt+F10 Menue (auch Neue Textdatei). NOOP nach 60 s Ruhe.
 * Fenster mit Minimieren/Maximieren, Listen wachsen mit (30.10.2026).
 * Ordner (31.10.2026): F5 kopiert Ordner samt Inhalt in alle Richtungen (Platte, Server hoch/runter ueber eine
 * Auftragsliste: MKD/STOR bzw. LIST/RETR mit relativen Pfaden), F8 loescht lokale Ordner samt Inhalt.
 * Lange Servernamen: angezeigt gekuerzt, beim Herunterladen als DOS-Name (8.3, ungueltige Zeichen "_").
 * Befehlszeile "[V] [T] [Ordner]": V verbindet sofort, T Testmodus (DbgPrint, Startordner), test/rdftp.sh. */
#include "RD2.H"

#define ID_HIER  30
#define ID_DORT  31
#define ID_STAT  32
#define ID_LEISTE 33
#define ID_LEISTE2 34
#define ID_LL    40
#define ID_LR    41
#define ID_VERB  50
#define ID_KOPIE 51
#define ID_LOESCH 52
#define ID_ORDNER 53
#define ID_TEXT  54
#define ID_NEU   55
#define ID_ALLE  56
#define ID_LW    60                                /* 60 + Laufwerk (1 = A:), linke Leiste */
#define ID_RW    90                                /* rechte Leiste */
#define NE 160                                    /* Eintraege je Seite */
#define RN 48                                     /* Servername hoechstens */
#define NLID 12
#define JQ 3072                                   /* Auftragsliste fuer Ordner (31.10.): Typ + Pfad + 0 je Eintrag */
typedef struct { char n[RN]; BYTE dir, mk; DWORD gr; } ENT;   /* dir: 0 Datei, 1 Ordner, 2 Laufwerk */

enum { F_AUS, F_NETZ, F_DNS, F_VERB, F_GRUSS, F_USER, F_PASS, F_TYPE, F_PWD, F_BEREIT,
       F_PASV, F_DATEN, F_XFER, F_CWD, F_EINZEL, F_MKD };
enum { O_LIST = 1, O_RETR, O_STOR };

static HINST hi; static HWND dl; static BYTE test, phase, op, z226, dzu, pumpen, ende_f, stapel, lauf, autov; static WORD nw;
static HSOCK hc, hd; static DWORD sip; static WORD sport;
static char srv[40], user[24], pass[24], pf[2][80], rp[128], zb[260], cmdb[160], dname[100], status[80];
static WORD zl, ne[2], rzl, qi, leer, cw0, ch0; static short fh = -1, bw, bx0[27]; static DWORD xbytes, xgesamt;
static HMEM hm; static ENT FAR *e[2]; static char FAR *xb; static WORD xn, xo; static HMENU pm;   /* stapel: 1 laden, 2 senden, 3 Server loeschen */
static const BYTE lid[NLID] = { ID_HIER, ID_DORT, ID_LEISTE, ID_LEISTE2, ID_LL, ID_LR, ID_STAT, ID_VERB, ID_KOPIE, ID_LOESCH, ID_ORDNER, 2 };
static RECT r0[NLID];
static char FAR *jq; static WORD jn, jp, jz; static BYTE jauf, jlist, jvoll; static char jlp[100], kq[80], kz[80];   /* jauf 1 senden, 2 laden; kq/kz Quelle/Ziel lokal */
static char rzeile[160]; static HWND seite;   /* zuletzt bediente Liste (Alt+Buchstabe setzt den Fokus auf den Knopf) */
#define LID(r) ((WORD)((r) ? ID_LR : ID_LL))
#define SRV(r) ((r) && phase != F_AUS)            /* Seite r zeigt den Server */

/* ---------------- Hilfen ---------------- */
static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static void fcpy(char FAR *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static char gr(char c) { return (char)(c >= 'a' && c <= 'z' ? c - 32 : c); }
static void zahl(char *d, DWORD v) { char t[12]; WORD i = 0; do t[i++] = (char)('0' + v % 10); while ((v /= 10) && i < 11); while (i) *d++ = t[--i]; *d = 0; }
static void pr(LPCSTR a, LPCSTR b) { if (test) { char t[100]; scpy(t, "RDFTP: ", sizeof t); scat(t, a, sizeof t); scat(t, b, sizeof t); DbgPrint(t); } }
static void stat(LPCSTR a, LPCSTR b) { scpy(status, a, sizeof status); scat(status, b, sizeof status); DlgSetText(dl, ID_STAT, status); }
static void statz(WORD id, LPCSTR b) { stat(StrGet(hi, id), b); }
static int vgl2(LPCSTR a, LPCSTR b, WORD l) { while (l && gr(*a) == gr(*b)) { a++; b++; l--; } return l; }
static int vgl(LPCSTR a, LPCSTR b) { while (*a && gr(*a) == gr(*b)) { a++; b++; } return gr(*a) - gr(*b); }
static void zeile_fmt(char *t, LPCSTR n, BYTE dir, DWORD g);
static void jadd(char typ, LPCSTR p)              /* Auftrag anhaengen (voll: merken, Rest entfaellt) */
{
    WORD l = slen(p);
    if (jn + l + 2 > JQ) { jvoll = 1; return; }
    jq[jn++] = typ; while (*p) jq[jn++] = *p++; jq[jn++] = 0;
}
static void zeile_mk(char *t, LPCSTR n, BYTE dir, DWORD g, BYTE mk)   /* markiert: Kaestchen vorn */
{
    WORD k;
    zeile_fmt(t + (mk ? 1 : 0), n, dir, g);
    if (mk) { t[0] = '\xFE'; for (k = 1; t[k] && k < 32; k++) ; t[k] = 0; }
}
static void zeile_fmt(char *t, LPCSTR n, BYTE dir, DWORD g)   /* Listenzeile, hoechstens 32 Zeichen */
{
    WORD i = 0, k;
    if (dir) { t[i++] = '['; for (k = 0; n[k] && i < 30; k++) t[i++] = n[k]; if (n[k]) t[i - 1] = '~'; t[i++] = ']'; t[i] = 0; return; }
    for (k = 0; n[k] && i < 21; k++) t[i++] = n[k];
    if (n[k]) t[i - 1] = '~';
    { char z[12]; WORD l; zahl(z, g); l = slen(z); while (i < 31 - l) t[i++] = ' '; for (k = 0; k < l; k++) t[i++] = z[k]; }
    t[i] = 0;
}
static void dosname(char *d, LPCSTR s)            /* Servername -> 8.3 */
{
    WORD i = 0, k = 0; LPCSTR p = 0, q;
    for (q = s; *q; q++) if (*q == '.') p = q;
    if (p == s) p = 0;                            /* ".profile": ganz als Name */
    for (q = s; *q && q != p && k < 8; q++) { char c = gr(*q); if (c == '.' || c == ' ') continue; d[i++] = (char)((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '!' || c == '~' ? c : '_'); k++; }
    if (!i) d[i++] = '_';
    if (p) { d[i++] = '.'; for (q = p + 1, k = 0; *q && k < 3; q++, k++) { char c = gr(*q); d[i++] = (char)((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : '_'); } }
    d[i] = 0;
}

static char *weiter(char *p) { while (*p && *p != ' ') p++; while (*p == ' ') p++; return p; }
static DWORD zahlvon(const char *p) { DWORD g = 0; while (*p >= '0' && *p <= '9') g = g * 10 + (BYTE)(*p++ - '0'); return g; }
static void listzeile(char *s)                    /* Unix "drwxr-xr-x 2 u g 4096 Okt 26 10:00 Name" oder DOS "10-26-26 10:00AM <DIR> Name" */
{
    char *p = s, *g = 0, *q; BYTE dir; WORD i;
    if (ne[1] >= NE && !jlist) return;
    if (s[0] == 'd' || s[0] == '-' || s[0] == 'l') {
        for (i = 0; i < 8 && *p; i++) { if (i == 4) g = p; p = weiter(p); }
        if (i < 8 || !*p) return;
        dir = (BYTE)(s[0] == 'd');
        if (s[0] == 'l') for (q = p; *q; q++) if (q[0] == ' ' && q[1] == '-' && q[2] == '>') { *q = 0; break; }
    } else if (s[0] >= '0' && s[0] <= '9') {
        p = weiter(weiter(p)); g = p; dir = (BYTE)(*p == '<'); p = weiter(p);
        if (!*p) return;
    } else return;
    if ((p[0] == '.' && !p[1]) || (p[0] == '.' && p[1] == '.' && !p[2])) return;
    if (jlist) { char t[100]; scpy(t, jlp, sizeof t); scat(t, "/", sizeof t); scat(t, p, sizeof t); jadd((char)(dir ? 'L' : 'F'), t); return; }   /* Ordner laden: Inhalt in die Auftragsliste */
    for (i = 0; p[i] && i < RN - 1; i++) e[1][ne[1]].n[i] = p[i];
    if (p[i]) return;                             /* zu lang: weglassen */
    e[1][ne[1]].n[i] = 0; e[1][ne[1]].dir = dir; e[1][ne[1]].gr = dir ? 0 : zahlvon(g); ne[1]++;
}

/* ---------------- Steuerverbindung ---------------- */
static void senden(LPCSTR a, LPCSTR b)
{
    scpy(cmdb, a, sizeof cmdb); if (b) { scat(cmdb, " ", sizeof cmdb); scat(cmdb, b, sizeof cmdb); }
    if (test && !(a[0] == 'P' && a[1] == 'A' && a[2] == 'S' && a[3] == 'S')) pr("> ", cmdb);
    scat(cmdb, "\r\n", sizeof cmdb);
    NetSend(hc, cmdb, slen(cmdb)); nw = 0; leer = 0;
}
/* ---------------- Listen ---------------- */
static void anzeigen(BYTE r, WORD sel)
{
    char t[34], n[RN]; WORD i; HWND l = DlgItem(dl, LID(r)); ENT FAR *v = e[r];
    SendMessage(l, L_CLEAR, 0, 0);
    for (i = 0; i < ne[r]; i++) {
        if (v[i].dir == 2) { t[0] = '['; t[1] = '-'; t[2] = v[i].n[0]; t[3] = '-'; t[4] = ']'; t[5] = 0; }
        else { scpy(n, v[i].n, sizeof n); zeile_mk(t, n, v[i].dir, v[i].gr, v[i].mk); }
        SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)t);
    }
    SendMessage(l, L_SETSEL, sel, 0);
}
static void sortieren(BYTE r)                     /* ".." bleibt oben, Ordner zuerst, dann nach Namen */
{
    WORD i, j, a = (WORD)(ne[r] && e[r][0].n[0] == '.' && e[r][0].n[1] == '.'); ENT x; ENT FAR *v = e[r];
    for (i = a + 1; i < ne[r]; i++)
        for (j = i; j > a && (v[j].dir > v[j - 1].dir || (v[j].dir == v[j - 1].dir && vgl(v[j].n, v[j - 1].n) < 0)); j--) { x = v[j]; v[j] = v[j - 1]; v[j - 1] = x; }
    for (i = 0; i < ne[r]; i++) v[i].mk = 0;
}
static void lokal(BYTE r)                         /* Seite r: Ordner pf[r] der eigenen Platte lesen */
{
    FINDDATA f; char m[90], t[8]; WORD n = 0; ENT FAR *v = e[r]; char *p = pf[r];
    if (slen(p) > 3) { fcpy(v[0].n, "..", RN); v[0].dir = 1; v[0].gr = 0; n = 1; }
    scpy(m, p, sizeof m); scat(m, "*.*", sizeof m);
    if (!FileFindFirst(m, FA_DIR, &f)) do {
        if (f.name[0] == '.' || n >= NE - 26) continue;
        fcpy(v[n].n, f.name, 13); v[n].dir = (BYTE)((f.attr & FA_DIR) != 0); v[n].gr = f.groesse; n++;
    } while (!FileFindNext(&f));
    ne[r] = n; sortieren(r);
    /* Laufwerke stehen in der Leiste oben, nicht mehr am Listenende (31.10.) */
    anzeigen(r, 0);
    DlgSetText(dl, r ? ID_DORT : ID_HIER, p);
    if (test) { zahl(t, n); pr(r ? "rlokal " : "links ", t); }
}
static void beide(BYTE r) { lokal(r); if (phase == F_AUS && !vgl(pf[0], pf[1])) lokal((BYTE)!r); }   /* gleicher Ordner: andere Seite auch */
static void lw_zeigen(BYTE an) { WORD i; for (i = 1; i <= 26; i++) if (bx0[i]) ShowWindow(DlgItem(dl, (WORD)(ID_RW + i)), (WORD)(an ? SW_SHOW : SW_HIDE)); }

static void knoepfe(void)
{
    BYTE da = (BYTE)(phase == F_BEREIT || phase == F_AUS);
    SetWindowText(DlgItem(dl, ID_VERB), StrGet(hi, phase == F_AUS ? 4 : 5));
    EnableWindow(DlgItem(dl, ID_KOPIE), da);
    EnableWindow(DlgItem(dl, ID_LOESCH), da);
    EnableWindow(DlgItem(dl, ID_ORDNER), da);
}
static void trennen(WORD txt, LPCSTR b)
{
    if (hd) { NetClose(hd); hd = 0; }
    if (hc) { if (phase >= F_BEREIT) NetSend(hc, "QUIT\r\n", 6); NetClose(hc); hc = 0; }
    if (fh >= 0) { FileClose(fh); fh = -1; if (op == O_RETR) FileDelete(dname); }   /* halbe Datei nicht liegen lassen */
    phase = F_AUS; op = 0; pumpen = 0; stapel = 0; jauf = 0; jlist = 0; TimerKill(dl, 1);
    if (lauf) { lokal(1); lw_zeigen(1); }
    knoepfe();
    if (txt) statz(txt, b);
    pr("getrennt ", b ? b : "");
}
static void liste(void) { op = O_LIST; ne[1] = 0; rzl = 0; if (slen(rp) > 1) { fcpy(e[1][0].n, "..", RN); e[1][0].dir = 1; e[1][0].gr = 0; ne[1] = 1; } phase = F_PASV; senden("PASV", 0); }
static void jlos(void);
static BYTE weiter_loe(void);
static WORD wahl(WORD id) { return (WORD)SendMessage(DlgItem(dl, id), L_GETSEL, 0, 0); }
static void fertig_op(void)                       /* Datenuebertragung vorbei (226 und Datenkanal zu) */
{
    char t[34], n[RN]; WORD i;
    if (fh >= 0) { FileClose(fh); fh = -1; }
    phase = F_BEREIT; pumpen = 0; TimerKill(dl, 1);
    if (op == O_LIST && jlist) { op = 0; jlist = 0; jlos(); return; }
    if (op == O_LIST) {
        op = 0; sortieren(1); anzeigen(1, 0); { char t2[100]; scpy(t2, srv, sizeof t2); scat(t2, ":", sizeof t2); scat(t2, rp, sizeof t2); DlgSetText(dl, ID_DORT, t2); }   /* Server (IP) vor dem Pfad */
        if (test) { for (i = 0; i < ne[1]; i++) { scpy(n, e[1][i].n, sizeof n); zeile_fmt(t, n, e[1][i].dir, e[1][i].gr); pr("  ", t); } zahl(t, ne[1]); pr("rechts ", t); }
        zahl(t, ne[1] - (slen(rp) > 1)); statz(6, t); knoepfe(); return;
    }
    zahl(t, xbytes);
    if (jauf && op) { pr(op == O_RETR ? "geladen " : "gesendet ", t); op = 0; jz++; jlos(); return; }
    if (op == O_STOR) { pr("gesendet ", t); statz(8, t); liste(); return; }   /* leere Textdatei (Neue Textdatei) */
}
static void datenende(void) { if (z226 && dzu) fertig_op(); }
static void antwort(WORD code, char *z)
{
    char *p; WORD i, v[6];
    if (test) pr("< ", z);
    switch (phase) {
    case F_GRUSS: if (code != 220) break; phase = F_USER; senden("USER", user[0] ? user : "anonymous"); return;
    case F_USER:
        if (code == 230) { phase = F_TYPE; senden("TYPE", "I"); return; }
        if (code != 331) break; phase = F_PASS; senden("PASS", pass[0] ? pass : "rd2@"); return;
    case F_PASS: if (code != 230 && code != 202) break; phase = F_TYPE; senden("TYPE", "I"); return;
    case F_TYPE: if (code != 200) break; phase = F_PWD; senden("PWD", 0); return;
    case F_PWD:
        if (code != 257) break;
        for (p = z + 4; *p && *p != '"'; p++) ;
        if (*p) { for (p++, i = 0; *p && i < sizeof rp - 1; p++) { if (*p == '"') { if (p[1] != '"') break; p++; } rp[i++] = *p; } rp[i] = 0; }
        if (!rp[0]) { rp[0] = '/'; rp[1] = 0; }
        pr("verbunden ", rp); knoepfe(); liste(); return;
    case F_CWD: if (code != 250 && code != 200) { phase = F_BEREIT; statz(9, z); return; } phase = F_PWD; senden("PWD", 0); return;
    case F_EINZEL:
        phase = F_BEREIT; if (code / 100 != 2) statz(9, z);
        if (stapel == 3) { if (weiter_loe()) return; stapel = 0; }
        else if (code / 100 != 2) return;
        liste(); return;
    case F_MKD: phase = F_BEREIT; if (code / 100 != 2) pr("MKD ", z); jlos(); return;   /* Ordner gibt es schon: weiter */
    case F_PASV:
        if (code != 227) break;
        for (p = z + 4; *p && (*p < '0' || *p > '9'); p++) ;
        for (i = 0; i < 6; i++) { v[i] = 0; while (*p >= '0' && *p <= '9') v[i] = (WORD)(v[i] * 10 + *p++ - '0'); if (*p == ',') p++; }
        z226 = 0; dzu = 0; xbytes = 0; rzl = 0;
        if (!(hd = NetOpen(dl, sip, (WORD)((v[4] << 8) | v[5]), NO_TCP))) { trennen(10, ""); return; }   /* Datei zu, Auftraege weg */
        phase = F_DATEN; return;
    case F_DATEN: case F_XFER:
        if (code == 150 || code == 125) { if (op == O_STOR) { pumpen = 1; TimerSet(dl, 1, 55); } return; }
        if (code == 226 || code == 250) { z226 = 1; datenende(); return; }
        if (code / 100 == 4 || code / 100 == 5) {
            if (hd) { NetClose(hd); hd = 0; }
            if (fh >= 0) { FileClose(fh); fh = -1; if (op == O_RETR) FileDelete(dname); }
            phase = F_BEREIT; op = 0; pumpen = 0; stapel = 0; jauf = 0; jlist = 0; TimerKill(dl, 1); statz(9, z); pr("Fehler ", z); knoepfe(); return;
        }
        return;
    default: return;
    }
    trennen(9, z);                                /* unerwartete Antwort beim Anmelden */
}
static void steuer_daten(void)
{
    char b[128]; WORD n, i;
    while ((n = NetRecv(hc, b, sizeof b)) != 0 && n != 0xFFFF) {
        nw = 0;
        for (i = 0; i < n; i++) {
            if (b[i] == '\n') {
                zb[zl] = 0;
                if (zl >= 4 && zb[0] >= '1' && zb[0] <= '5' && zb[3] == ' ') antwort((WORD)((zb[0] - '0') * 100 + (zb[1] - '0') * 10 + zb[2] - '0'), zb);
                zl = 0; if (!hc) return;
            } else if (b[i] != '\r' && zl < sizeof zb - 1) zb[zl++] = b[i];
        }
    }
}

/* ---------------- Datenkanal ---------------- */
static void daten_lesen(void)
{
    char b[512]; WORD n, i;
    while ((n = NetRecv(hd, b, sizeof b)) != 0 && n != 0xFFFF) {
        nw = 0; xbytes += n;
        if (op == O_LIST) {
            for (i = 0; i < n; i++) {
                if (b[i] == '\n') { rzeile[rzl] = 0; listzeile(rzeile); rzl = 0; }
                else if (b[i] != '\r' && rzl < sizeof rzeile - 1) rzeile[rzl++] = b[i];
            }
        } else if (op == O_RETR && fh >= 0) {
            if (FileWrite(fh, b, n) != n) { trennen(11, dname); return; }
            { char t[24]; zahl(t, xbytes); if (xgesamt) { scat(t, " / ", sizeof t); zahl(t + slen(t), xgesamt); } statz(12, t); }
        }
    }
}
static void pumpe(void)                           /* STOR: Datei in den Sendepuffer, am Ende FIN */
{
    WORD k;
    if (!pumpen || !hd) return;
    for (;;) {
        if (xo >= xn) {
            if (ende_f) { NetClose(hd); hd = 0; dzu = 1; pumpen = 0; TimerKill(dl, 1); datenende(); return; }
            xn = FileRead(fh, xb, 1024); xo = 0;
            if (xn == 0xFFFF) { trennen(11, dname); return; }
            if (xn < 1024) ende_f = 1;
            if (!xn) continue;
        }
        k = NetSend(hd, xb + xo, (WORD)(xn - xo));
        if (k == 0xFFFF) { trennen(10, ""); return; }
        if (!k) return;                           /* Puffer voll: beim naechsten Takt weiter */
        xo += k; xbytes += k; nw = 0;
        { char t[24]; zahl(t, xbytes); scat(t, " / ", sizeof t); zahl(t + slen(t), xgesamt); statz(13, t); }
    }
}

/* ---------------- Befehle ---------------- */
static void verbinden(void)
{
    char ini[80], t[8]; SYSINFO si; WORD i;
    KernInfo(&si); scpy(ini, si.home, sizeof ini); if (slen(ini) && ini[slen(ini) - 1] != '\\') scat(ini, "\\", sizeof ini); scat(ini, "RD2.INI", sizeof ini);
    ProfileGetStr(ini, "FTP", "Server", "", srv, sizeof srv);
    ProfileGetStr(ini, "FTP", "Port", "21", t, sizeof t); for (sport = 0, i = 0; t[i] >= '0' && t[i] <= '9'; i++) sport = (WORD)(sport * 10 + t[i] - '0'); if (!sport) sport = 21;
    ProfileGetStr(ini, "FTP", "Benutzer", "anonymous", user, sizeof user);
    ProfileGetStr(ini, "FTP", "Kennwort", "", pass, sizeof pass);
    if (!srv[0]) { MsgBox(dl, StrGet(hi, 14), StrGet(hi, 1), MB_OK | MB_INFO); return; }
    phase = F_NETZ; nw = 0; zl = 0; rp[0] = 0; knoepfe(); statz(15, srv);
    ne[1] = 0; SendMessage(DlgItem(dl, ID_LR), L_CLEAR, 0, 0); DlgSetText(dl, ID_DORT, srv); lw_zeigen(0);
    if (NetStart(dl)) trennen(16, "");
}
static void oeffnen(WORD id)
{
    BYTE r = (BYTE)(id == ID_LR); WORD s = wahl(id), k; ENT FAR *v = e[r]; char *p = pf[r];
    if (!SRV(r)) {
        if (s >= ne[r]) return;
        if (v[s].dir == 2) { p[0] = v[s].n[0]; p[1] = ':'; p[2] = '\\'; p[3] = 0; }
        else if (!v[s].dir) return;
        else if (v[s].n[0] == '.') { k = slen(p) - 1; while (k > 2 && p[k - 1] != '\\') k--; p[k] = 0; }
        else { char n[13]; scpy(n, v[s].n, sizeof n); scat(p, n, 80); scat(p, "\\", 80); }
        lokal(r); return;
    }
    if (phase != F_BEREIT || s >= ne[1] || !v[s].dir) return;
    phase = F_CWD;
    if (v[s].n[0] == '.' && v[s].n[1] == '.') senden("CDUP", 0);
    else { char n[RN]; scpy(n, v[s].n, sizeof n); senden("CWD", n); }
}
static BYTE rechts_f(void) { return (BYTE)(seite == DlgItem(dl, ID_LR)); }
static BYTE mkbar(BYTE r, WORD i) { return (BYTE)(i < ne[r] && e[r][i].dir != 2 && e[r][i].n[0] != '.'); }
static WORD naechst(BYTE r) { WORD i; for (i = 0; i < ne[r]; i++) if (e[r][i].mk) return i; return 0xFFFF; }
static WORD anzahl(BYTE r) { WORD i, k = 0; for (i = 0; i < ne[r]; i++) if (e[r][i].mk) k++; return k; }
static void markiere(BYTE r, WORD i, BYTE wie) { if (mkbar(r, i)) e[r][i].mk = (BYTE)(wie == 2 ? !e[r][i].mk : wie); }   /* 0 aus, 1 an, 2 um */
static BYTE ueber(LPCSTR z)                       /* Ziel vorhanden: fragen; 1 = schreiben */
{
    FINDDATA fd; char t[110];
    if (test || FileFindFirst(z, 0, &fd)) return 1;
    scpy(t, StrGet(hi, 19), sizeof t); scat(t, z, sizeof t);
    return (BYTE)(MsgBox(dl, t, StrGet(hi, 1), MB_YESNO | MB_ASK | MB_DEFNO) == ID_YES);
}
static BYTE kopie_lokal(LPCSTR q, LPCSTR z)       /* 0 = ok */
{
    short a, b; WORD n; BYTE f = 0;
    if ((a = FileOpen(q, 0)) < 0) return 1;
    if ((b = FileCreate(z, 0)) < 0) { FileClose(a); return 1; }
    CursorBusy(TRUE);
    for (;;) {
        n = FileRead(a, xb, 1024);
        if (n == 0xFFFF) { f = 1; break; }
        if (n && FileWrite(b, xb, n) != n) { f = 1; break; }
        if (n < 1024) break;
    }
    CursorBusy(FALSE);
    FileClose(a); FileClose(b);
    if (f) FileDelete(z);
    return f;
}
static BYTE in_sich(void) { WORD l = slen(kq); return (BYTE)(!vgl2(kq, kz, l) && (kz[l] == '\\' || !kz[l])); }   /* Ziel liegt im Quellordner */
static BYTE baum(BYTE dir)                        /* lokal kq -> kz, Ordner samt Inhalt; 0 = ok */
{
    FINDDATA f; WORD lq = slen(kq), lz = slen(kz); BYTE e = 0;
    if (!dir) { if (!ueber(kz)) return 0; if (kopie_lokal(kq, kz)) { statz(11, kz); return 1; } pr("kopiert ", kz); return 0; }
    if (in_sich()) { statz(26, ""); return 1; }
    DirMake(kz); pr("Ordner ", kz);               /* gibt es ihn schon: hinein kopieren */
    scat(kq, "\\*.*", sizeof kq);
    if (!FileFindFirst(kq, FA_DIR | FA_HIDDEN | FA_SYSTEM, &f)) do {
        if (f.name[0] == '.' || (f.attr & FA_LABEL)) continue;
        kq[lq] = '\\'; scpy(kq + lq + 1, f.name, sizeof kq - lq - 1); kz[lz] = '\\'; scpy(kz + lz + 1, f.name, sizeof kz - lz - 1);
        e = baum((BYTE)((f.attr & FA_DIR) != 0));
        kq[lq] = 0; kz[lz] = 0;
    } while (!e && !FileFindNext(&f));
    kq[lq] = 0;
    return e;
}
static BYTE weg(BYTE dir)                         /* lokal kq loeschen, Ordner samt Inhalt; 0 = ok */
{
    FINDDATA f; WORD l = slen(kq); BYTE e = 0;
    if (dir) {
        scat(kq, "\\*.*", sizeof kq);
        if (!FileFindFirst(kq, FA_DIR | FA_HIDDEN | FA_SYSTEM, &f)) do {
            if (f.name[0] == '.' || (f.attr & FA_LABEL)) continue;
            kq[l] = '\\'; scpy(kq + l + 1, f.name, sizeof kq - l - 1);
            e = weg((BYTE)((f.attr & FA_DIR) != 0)); kq[l] = 0;
        } while (!e && !FileFindNext(&f));
        kq[l] = 0;
        if (!e && DirRemove(kq)) e = 1;
    } else { FileAttr(kq, 0, 1); if (FileDelete(kq)) e = 1; }
    if (e == 1) { statz(11, kq); e = 2; }        /* Meldung nur einmal */
    return e;
}
static void sammeln(WORD rb)                      /* Ordner kq (lokal) fuer den Server: Unterordner D, Dateien F, Pfade ab kq + rb */
{
    FINDDATA f; WORD l = slen(kq);
    scat(kq, "\\*.*", sizeof kq);
    if (!FileFindFirst(kq, FA_DIR | FA_HIDDEN | FA_SYSTEM, &f)) do {
        if (f.name[0] == '.' || (f.attr & FA_LABEL)) continue;
        kq[l] = '\\'; scpy(kq + l + 1, f.name, sizeof kq - l - 1);
        jadd((char)((f.attr & FA_DIR) ? 'D' : 'F'), kq + rb);
        if (f.attr & FA_DIR) sammeln(rb);
        kq[l] = 0;
    } while (!FileFindNext(&f));
    kq[l] = 0;
}
static void lname(char *d, LPCSTR p)              /* Serverpfad a/b/c -> pf[0] + A\B\C (8.3) */
{
    char t[RN], n[13]; WORD i;
    scpy(d, pf[0], 100);
    while (*p) {
        for (i = 0; *p && *p != '/' && i < RN - 1; ) t[i++] = *p++;
        t[i] = 0; if (*p == '/') p++;
        dosname(n, t); scat(d, n, 100); if (*p) scat(d, "\\", 100);
    }
}
static void jlos(void)                            /* naechster Auftrag; leer: fertig, beide Seiten neu */
{
    char p[100], t[100], c; WORD i; FINDDATA f;
    while (jauf && jp < jn) {
        c = jq[jp++]; for (i = 0; jq[jp] && i < sizeof p - 1; ) p[i++] = jq[jp++]; p[i] = 0; jp++;
        xgesamt = 0;
        if (jauf == 1) {                          /* senden: Pfad mit / zum Server */
            scpy(t, p, sizeof t); for (i = 0; t[i]; i++) if (t[i] == '\\') t[i] = '/';
            if (c == 'D') { phase = F_MKD; senden("MKD", t); knoepfe(); return; }
            scpy(dname, pf[0], sizeof dname); scat(dname, p, sizeof dname);
            if ((fh = FileOpen(dname, 0)) < 0) { statz(11, dname); continue; }
            if (!FileFindFirst(dname, FA_HIDDEN | FA_SYSTEM, &f)) xgesamt = f.groesse;
            pr("sende ", dname); op = O_STOR; xn = 0; xo = 0; ende_f = 0;
        } else {                                  /* laden: L Ordner anlegen und auflisten, F Datei */
            lname(dname, p);
            if (c == 'L') { DirMake(dname); pr("Ordner ", dname); scpy(jlp, p, sizeof jlp); jlist = 1; op = O_LIST; rzl = 0; phase = F_PASV; senden("PASV", 0); knoepfe(); return; }
            if (!ueber(dname)) continue;
            if ((fh = FileCreate(dname, 0)) < 0) { statz(11, dname); continue; }
            pr("lade ", dname); op = O_RETR; scpy(t, p, sizeof t);
        }
        scpy(rzeile, jauf == 1 ? t : p, sizeof rzeile); if (jauf == 1) for (i = 0; rzeile[i]; i++) if (rzeile[i] == '\\') rzeile[i] = '/';
        phase = F_PASV; senden("PASV", 0); knoepfe();
        return;
    }
    i = jauf; jauf = 0; stapel = 0;
    if (jvoll) { jvoll = 0; statz(28, ""); } else { char z[8]; zahl(z, jz); statz(27, z); scat(status, StrGet(hi, 29), sizeof status); DlgSetText(dl, ID_STAT, status); }
    c = (char)wahl(ID_LL); lokal(0); SendMessage(DlgItem(dl, ID_LL), L_SETSEL, (BYTE)c, 0);   /* Auswahl bleibt */
    if (i == 1) liste(); else { anzeigen(1, wahl(ID_LR)); knoepfe(); }   /* laden: Serverliste unveraendert */
}
static void kopieren(void)
{
    BYTE r = rechts_f(); WORD s, i, k = anzahl(r); char n[RN]; ENT FAR *v = e[r];
    if (phase != F_BEREIT && phase != F_AUS) { statz(17, ""); return; }
    s = wahl(LID(r));
    if (!k) { if (!mkbar(r, s)) { statz(18, ""); return; } v[s].mk = 1; }
    if (phase == F_AUS) {                         /* Platte zu Platte, Ordner samt Inhalt */
        if (!vgl(pf[0], pf[1])) { statz(26, ""); return; }
        CursorBusy(TRUE);
        for (i = 0; i < ne[r]; i++) if (v[i].mk) {
            scpy(n, v[i].n, sizeof n); scpy(kq, pf[r], sizeof kq); scat(kq, n, sizeof kq); scpy(kz, pf[!r], sizeof kz); scat(kz, n, sizeof kz);
            if (baum(v[i].dir)) break;
            statz(27, n);
        }
        CursorBusy(FALSE);
        for (i = 0; i < ne[r]; i++) v[i].mk = 0;
        lokal((BYTE)!r); anzeigen(r, s);
        return;
    }
    jn = jp = jz = 0; jvoll = 0;                       /* Server: Auftragsliste, dann nacheinander (Ordner samt Inhalt) */
    for (i = 0; i < ne[r]; i++) if (v[i].mk) {
        scpy(n, v[i].n, sizeof n);
        if (r) jadd((char)(v[i].dir ? 'L' : 'F'), n);
        else { jadd((char)(v[i].dir ? 'D' : 'F'), n); if (v[i].dir) { scpy(kq, pf[0], sizeof kq); scat(kq, n, sizeof kq); sammeln(slen(pf[0])); } }
    }
    for (i = 0; i < ne[r]; i++) v[i].mk = 0;
    jauf = (BYTE)(r ? 2 : 1); jlos();
}
static void loe_senden(void) { char n[RN]; scpy(n, e[1][qi].n, sizeof n); phase = F_EINZEL; senden(e[1][qi].dir ? "RMD" : "DELE", n); }
static BYTE weiter_loe(void)
{
    WORD s;
    e[1][qi].mk = 0;
    if ((s = naechst(1)) == 0xFFFF) return 0;
    qi = s; loe_senden();
    return 1;
}
static BYTE frage_loe(BYTE r, WORD s, WORD k)
{
    char t[100], n[RN];
    if (test) return 1;
    scpy(t, StrGet(hi, 20), sizeof t);
    if (k) zahl(n, k); else scpy(n, e[r][s].n, sizeof n);
    scat(t, n, sizeof t);
    return (BYTE)(MsgBox(dl, t, StrGet(hi, 1), MB_YESNO | MB_ASK | MB_DEFNO) == ID_YES);
}
static void loeschen(void)
{
    BYTE r = rechts_f(); WORD s = wahl(LID(r)), k = anzahl(r), i; char n[13];
    if (!k && !mkbar(r, s)) return;
    if (SRV(r)) {
        if (phase != F_BEREIT || !frage_loe(1, s, k)) return;
        if (!k) e[1][s].mk = 1;
        stapel = 3; qi = naechst(1); loe_senden();
        return;
    }
    if (!frage_loe(r, s, k)) return;
    if (!k) e[r][s].mk = 1;
    for (i = 0; i < ne[r]; i++) if (e[r][i].mk) {
        scpy(n, e[r][i].n, sizeof n); scpy(dname, pf[r], sizeof dname); scat(dname, n, sizeof dname);
        scpy(kq, dname, sizeof kq); CursorBusy(TRUE); k = weg(e[r][i].dir); CursorBusy(FALSE); if (k) break;   /* Ordner samt Inhalt (31.10.) */
    }
    beide(r);
}
static void neuordner(BYTE txt)                   /* txt: leere Textdatei statt Ordner */
{
    BYTE r = rechts_f(); char n[RN]; WORD i;
    n[0] = 0;
    if (!InputBox(dl, StrGet(hi, (WORD)(txt ? 25 : 21)), StrGet(hi, 1), n, SRV(r) ? RN : 13) || !n[0]) return;
    if (txt) { for (i = 0; n[i] && n[i] != '.'; i++) ; if (!n[i] && i <= 8) scat(n, ".TXT", sizeof n); }
    if (SRV(r)) {
        if (phase != F_BEREIT) { statz(17, ""); return; }
        if (!txt) { phase = F_EINZEL; senden("MKD", n); return; }
        fh = -1; op = O_STOR; xn = 0; xo = 0; ende_f = 1; xgesamt = 0; stapel = 0;   /* leere Datei senden */
        scpy(rzeile, n, sizeof rzeile); phase = F_PASV; senden("PASV", 0); knoepfe(); return;
    }
    scpy(dname, pf[r], sizeof dname); scat(dname, n, sizeof dname);
    if (txt) { short h = FileCreate(dname, 0); if (h < 0) statz(11, dname); else FileClose(h); }
    else if (DirMake(dname)) statz(11, dname);
    beide(r);
}
static void neu_lesen(void) { BYTE r = rechts_f(); if (SRV(r)) { if (phase == F_BEREIT) liste(); } else lokal(r); }
static void alle(BYTE r) { WORD i; BYTE an = (BYTE)(anzahl(r) == 0); for (i = 0; i < ne[r]; i++) markiere(r, i, an); anzeigen(r, wahl(LID(r))); }
static void befehl(WORD id)
{
    if (seite) SetFocus(seite);
    if (id == ID_KOPIE) kopieren(); else if (id == ID_LOESCH) loeschen(); else if (id == ID_ORDNER) neuordner(0);
    else if (id == ID_TEXT) neuordner(1); else if (id == ID_NEU) neu_lesen(); else if (id == ID_ALLE) alle(rechts_f());
    if (seite) SetFocus(seite);
}
/* ---------------- Fenster: Laufwerksleisten, Groesse ---------------- */
static void rect_von(WORD id, RECT *r) { POINT a, b; GetWindowRect(DlgItem(dl, id), (RECT FAR *)r); a.x = r->l; a.y = r->t; b.x = r->r; b.y = r->b; ScreenToClient(dl, &a); ScreenToClient(dl, &b); r->l = a.x; r->t = a.y; r->r = b.x; r->b = b.y; }
static void leiste(WORD idl, WORD basis)
{
    RECT r; WORD i, k = 0; char t[3]; DRIVEINFO di;
    rect_von(idl, &r); bw = (short)((r.b - r.t) * 2 + 4);
    t[1] = ':'; t[2] = 0;
    for (i = 1; i <= 26; i++)
        if (!DriveInfo(i | DI_NURTYP, &di) && di.typ != DT_NONE && !(di.flags & DI_PHANTOM)) {
            short x = (short)(r.l + k * bw);
            if (x + bw > r.r) break;
            t[0] = (char)('A' + i - 1);
            CreateWindow(CL_BUTTON, t, WS_CHILD | WS_VISIBLE, x, (short)r.t, (short)(bw - 2), (short)(r.b - r.t), dl, (WORD)(basis + i), hi);
            if (basis == ID_RW) bx0[i] = x;
            k++;
        }
}
static void groesse(void)
{
    RECT c, q; short dx, dy, hx; WORD i;
    GetClientRect(dl, (RECT FAR *)&c);
    dx = (short)(c.r - c.l - cw0); dy = (short)(c.b - c.t - ch0);
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    hx = (short)(dx / 2);
    for (i = 0; i < NLID; i++) {
        q = r0[i];
        switch (lid[i]) {
        case ID_HIER: case ID_LEISTE: q.r += hx; break;
        case ID_DORT: case ID_LEISTE2: q.l += hx; q.r += dx; break;
        case ID_LL: q.r += hx; q.b += dy; break;
        case ID_LR: q.l += hx; q.r += dx; q.b += dy; break;
        case ID_STAT: q.t += dy; q.b += dy; q.r += dx; break;
        case 2: q.l += dx; q.r += dx; q.t += dy; q.b += dy; break;
        default: q.t += dy; q.b += dy;
        }
        MoveWindow(DlgItem(dl, lid[i]), (short)q.l, (short)q.t, (short)(q.r - q.l), (short)(q.b - q.t), TRUE);
    }
    for (i = 1; i <= 26; i++) if (bx0[i]) MoveWindow(DlgItem(dl, (WORD)(ID_RW + i)), (short)(bx0[i] + hx), (short)r0[3].t, (short)(bw - 2), (short)(r0[3].b - r0[3].t), TRUE);
}
static void menue(void)
{
    POINT p; WORD id;
    if (!pm) pm = MenuLoad(hi, 1);
    if (!pm) return;
    CursorPos(&p); ScreenToClient(dl, &p);
    if ((id = MenuPopup(dl, pm, 0, (short)p.x, (short)p.y)) != 0) befehl(id);
}

static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_INITDIALOG:
        dl = d; lokal(0); lokal(1); knoepfe(); TimerSet(d, 2, 1000);
        { RECT c; WORD i; GetClientRect(d, (RECT FAR *)&c); cw0 = (WORD)(c.r - c.l); ch0 = (WORD)(c.b - c.t); for (i = 0; i < NLID; i++) rect_von(lid[i], &r0[i]); }
        leiste(ID_LEISTE, ID_LW); leiste(ID_LEISTE2, ID_RW); lauf = 1;
        seite = DlgItem(d, ID_LL); SetFocus(seite); statz(2, "");
        pr("bereit", "");
        if (autov) PostMessage(d, M_COMMAND, ID_VERB, MAKELONG(0, N_CLICKED));
        return FALSE;
    case M_SIZE:
        if (lauf) groesse();
        return FALSE;
    case M_KEY:
        { HWND f = GetFocus(); if (f == DlgItem(d, ID_LL) || f == DlgItem(d, ID_LR)) {
            BYTE r = (BYTE)(f == DlgItem(d, ID_LR)); WORD s = wahl(LID(r)), ks = HIWORD(p2);
            seite = f;
            if (p1 == ' ' || p1 == K_INS) { markiere(r, s, 2); anzeigen(r, (WORD)(s + 1 < ne[r] ? s + 1 : s)); return TRUE; }
            if (p1 == 1 && (ks & KS_CTRL)) { alle(r); return TRUE; }               /* Strg+A */
            if (p1 == 18 && (ks & KS_CTRL)) { neu_lesen(); return TRUE; }          /* Strg+R */
            if (p1 == 0x15D || (p1 == K_F10 && (ks & KS_SHIFT))) { menue(); return TRUE; }   /* Umschalt+F10 */
            if (p1 == K_F5) { befehl(ID_KOPIE); return TRUE; }
            if (p1 == K_F7) { befehl(ID_ORDNER); return TRUE; }
            if (p1 == K_F8 || p1 == K_DEL) { befehl(ID_LOESCH); return TRUE; }
            if (p1 == K_TAB && !(ks & KS_SHIFT)) { seite = DlgItem(d, r ? ID_LL : ID_LR); SetFocus(seite); return TRUE; }   /* Tab: andere Seite */
        } }
        if (p1 == K_ENTER) { HWND f = GetFocus(); if (f == DlgItem(d, ID_LL) || f == DlgItem(d, ID_LR)) { oeffnen(f == DlgItem(d, ID_LL) ? ID_LL : ID_LR); return TRUE; } }
        return FALSE;
    case M_TIMER:
        if (p1 == 1) { pumpe(); return TRUE; }
        if (phase == F_BEREIT && hc && ++leer >= 60) senden("NOOP", 0);    /* Server trennen sonst untaetige Verbindungen */
        if (phase != F_AUS && phase != F_BEREIT && ++nw > (phase == F_NETZ ? 30 : 20)) trennen(16, "");
        return TRUE;
    case M_NETSTATE:
        if (phase != F_NETZ) return TRUE;
        if (p1 != NS_OPEN) { trennen(16, ""); return TRUE; }
        phase = F_DNS; nw = 0; if (NetResolve(d, srv)) trennen(22, srv);
        return TRUE;
    case M_NETRESOLVED:
        if (phase != F_DNS) return TRUE;
        if (p1) { trennen(22, srv); return TRUE; }
        sip = p2; phase = F_VERB; nw = 0;
        if (!(hc = NetOpen(d, sip, sport, NO_TCP))) trennen(10, "");
        return TRUE;
    case M_NETCONNECT:
        if (p1 == hc && phase == F_VERB) { if (p2) { hc = 0; trennen(10, ""); } else { phase = F_GRUSS; nw = 0; } return TRUE; }
        if (p1 == hd && phase == F_DATEN) {
            if (p2) { hd = 0; phase = F_BEREIT; op = 0; if (fh >= 0) { FileClose(fh); fh = -1; } statz(10, ""); knoepfe(); return TRUE; }
            phase = F_XFER;
            if (op == O_LIST) { if (jlist) senden("LIST", jlp); else senden("LIST", 0); }   /* nicht "jlist ? jlp : 0": nahe 0 wird DS:0, nicht NULL */ else senden(op == O_RETR ? "RETR" : "STOR", rzeile);
        }
        return TRUE;
    case M_NETDATA:
        if (p1 == hc && hc) steuer_daten(); else if (p1 == hd && hd) daten_lesen();
        return TRUE;
    case M_NETCLOSED:
        if (p1 == hc && hc) { steuer_daten(); hc = 0; trennen(23, ""); return TRUE; }
        if (p1 == hd && hd) { daten_lesen(); if (op == O_LIST && rzl) { rzeile[rzl] = 0; listzeile(rzeile); rzl = 0; } NetClose(hd); hd = 0; dzu = 1; datenende(); }
        return TRUE;
    case M_COMMAND:
        if (p1 == ID_LL || p1 == ID_LR) {
            BYTE r = (BYTE)(p1 == ID_LR); WORD s = wahl(p1);
            seite = DlgItem(d, p1);
            if (HIWORD(p2) == N_DBLCLK) oeffnen(p1);
            else if (HIWORD(p2) == N_RCLICK) menue();
            else if (HIWORD(p2) == N_SELECT && KeyState(KS_CTRL)) { markiere(r, s, 2); anzeigen(r, s); }   /* Strg+Klick */
            return TRUE;
        }
        if ((p1 > ID_LW && p1 <= ID_LW + 26) || (p1 > ID_RW && p1 <= ID_RW + 26)) {   /* Laufwerksleiste: Ordner wechseln, Ansicht bleibt */
            BYTE r = (BYTE)(p1 > ID_RW); char *p = pf[r];
            if (SRV(r)) return TRUE;
            p[0] = (char)('A' + p1 - (r ? ID_RW : ID_LW) - 1); p[1] = ':'; p[2] = '\\'; p[3] = 0;
            lokal(r); seite = DlgItem(d, LID(r)); SetFocus(seite); return TRUE;
        }
        if (HIWORD(p2) != N_CLICKED) return FALSE;
        if (p1 == ID_VERB) { if (phase == F_AUS) verbinden(); else trennen(24, ""); seite = DlgItem(d, ID_LR); SetFocus(seite); return TRUE; }
        if (p1 == ID_KOPIE || p1 == ID_LOESCH || p1 == ID_ORDNER) { befehl(p1); return TRUE; }
        if (p1 == ID_CANCEL || p1 == ID_OK) { lauf = 0; TimerKill(d, 2); trennen(0, 0); pr("Ende", ""); DialogEnd(d, ID_OK); return TRUE; }
        return FALSE;
    }
    return FALSE;
}

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD k;
    (void)show; hi = inst;
    if (cmd && cmd[0] == 'V' && (BYTE)cmd[1] <= ' ') { autov = 1; cmd++; while (*cmd == ' ') cmd++; }
    if (cmd && cmd[0] == 'T') { test = 1; cmd++; while (*cmd == ' ') cmd++; }
    if (cmd && *cmd) scpy(pf[0], cmd, sizeof pf[0]);
    else {                         /* aktueller Ordner */
        char b[70]; WORD lw = 0; SYSINFO si; KernInfo(&si);
        b[0] = 0; DirGet(0, b);
        if (b[0] && b[1] == ':') scpy(pf[0], b, sizeof pf[0]); else { lw = si.bootlw ? si.bootlw : 3; pf[0][0] = (char)('@' + lw); pf[0][1] = ':'; pf[0][2] = '\\'; pf[0][3] = 0; }
    }
    k = slen(pf[0]); if (k && pf[0][k - 1] != '\\') scat(pf[0], "\\", sizeof pf[0]);
    scpy(pf[1], pf[0], sizeof pf[1]);
    if (!(hm = MemAlloc((DWORD)2 * NE * sizeof(ENT) + 1024 + JQ, MEM_MOVABLE)) || !(e[0] = (ENT FAR *)MemLock(hm))) return 1;
    e[1] = e[0] + NE; xb = (char FAR *)(e[1] + NE); jq = xb + 1024;
    DialogRun(inst, 10, 0, dproc, 0);
    if (pm) MenuFree(pm);
    MemUnlock(hm); MemFree(hm);
    return 0;
}
