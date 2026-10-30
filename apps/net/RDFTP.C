/* RDFTP.C - FTP als zweites Dateifenster (Punkt 8, wie RD1 RDFTP.OVL): links die eigene Platte, rechts der Server.
 * Server, Port, Benutzer, Kennwort aus RD2.INI [FTP] (RDNET, Seite Dienste). Ein Fenster (Dialog), nur RDTCP:
 * Steuerverbindung zeilenweise, Daten passiv (PASV, immer an die Adresse der Steuerverbindung, NAT/Tunnel).
 * Bedienung: Tab wechselt die Seite, Eingabe/Doppelklick oeffnet Ordner (".." hoch, "[-A-]" Laufwerk),
 * Alt+K kopiert die Auswahl auf die andere Seite, Alt+L loescht, Alt+N neuer Ordner, Alt+V verbinden/trennen.
 * Lange Servernamen: angezeigt gekuerzt, beim Herunterladen als DOS-Name (8.3, ungueltige Zeichen "_").
 * Befehlszeile "T [Ordner]": Testmodus (DbgPrint, Startordner), test/rdftp.sh.
 * 30.10.2026: Fenster mit Minimieren/Maximieren (Listen wachsen mit), Laufwerksleiste links oben, Mehrfachauswahl
 * (Leertaste/Einfg, Strg+Klick, Strg+A), F5 kopieren, F7 Ordner, F8/Entf loeschen, Strg+R neu lesen, Rechtsklick-Menue
 * mit Neuer Ordner/Neue Textdatei, NOOP nach 60 s Ruhe (Server trennen sonst untaetige Verbindungen). */
#include "RD2.H"

#define ID_HIER  30
#define ID_DORT  31
#define ID_STAT  32
#define ID_LL    40
#define ID_LR    41
#define ID_VERB  50
#define ID_KOPIE 51
#define ID_LOESCH 52
#define ID_ORDNER 53
#define ID_TEXT  54
#define ID_NEU   55
#define ID_ALLE  56
#define ID_LEISTE 33
#define ID_LW    60                                /* 60 + Laufwerk (1 = A:) */
#define NLW      12                                /* Knoepfe hoechstens */
#define NL 160                                    /* Eintraege links */
#define NR 160                                    /* Eintraege rechts */
#define RN 48                                     /* Servername hoechstens */
typedef struct { char n[13]; BYTE dir, mk; DWORD gr; } LENT;
typedef struct { char n[RN]; BYTE dir, mk; DWORD gr; } RENT;

enum { F_AUS, F_NETZ, F_DNS, F_VERB, F_GRUSS, F_USER, F_PASS, F_TYPE, F_PWD, F_BEREIT,
       F_PASV, F_DATEN, F_XFER, F_CWD, F_EINZEL };
enum { O_LIST = 1, O_RETR, O_STOR };

static HINST hi; static HWND dl; static BYTE test, phase, op, z226, dzu, pumpen, ende_f; static WORD nw;
static HSOCK hc, hd; static DWORD sip; static WORD sport;
static char srv[40], user[24], pass[24], lp[80], rp[128], zb[260], cmdb[160], dname[RN], status[80];
static WORD zl, nll, nrr, rzl; static short fh = -1; static DWORD xbytes, xgesamt;
static HMEM hm; static LENT FAR *le; static RENT FAR *re; static char FAR *xb; static WORD xn, xo;
static BYTE stapel, lauf; static WORD qi, leer, cw0, ch0; static HMENU pm;   /* stapel: 1 laden, 2 senden, 3 Server loeschen */
static const BYTE lid[11] = { ID_HIER, ID_DORT, ID_LEISTE, ID_LL, ID_LR, ID_STAT, ID_VERB, ID_KOPIE, ID_LOESCH, ID_ORDNER, 2 };
static RECT r0[11];
static char rzeile[160]; static HWND seite;   /* zuletzt bediente Liste (Alt+Buchstabe setzt den Fokus auf den Knopf) */

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
static int vgl(LPCSTR a, LPCSTR b) { while (*a && gr(*a) == gr(*b)) { a++; b++; } return gr(*a) - gr(*b); }
static void zeile_fmt(char *t, LPCSTR n, BYTE dir, DWORD g);
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

/* ---------------- links: eigene Platte ---------------- */
static void links_anzeigen(WORD sel)
{
    char t[34]; WORD i; HWND l = DlgItem(dl, ID_LL);
    SendMessage(l, L_CLEAR, 0, 0);
    for (i = 0; i < nll; i++) {
        if (le[i].dir == 2) { t[0] = '['; t[1] = '-'; t[2] = le[i].n[0]; t[3] = '-'; t[4] = ']'; t[5] = 0; }
        else { char n[13]; scpy(n, le[i].n, sizeof n); zeile_mk(t, n, le[i].dir, le[i].gr, le[i].mk); }
        SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)t);
    }
    SendMessage(l, L_SETSEL, sel, 0);
}
static void links(void)
{
    FINDDATA f; char m[90], t[34]; WORD i, j; LENT x; DRIVEINFO di;
    nll = 0;
    if (slen(lp) > 3) { fcpy(le[0].n, "..", 13); le[0].dir = 1; le[0].gr = 0; nll = 1; }
    scpy(m, lp, sizeof m); scat(m, "*.*", sizeof m);
    if (!FileFindFirst(m, FA_DIR, &f)) do {
        if (f.name[0] == '.' || nll >= NL - 26) continue;
        fcpy(le[nll].n, f.name, 13); le[nll].dir = (BYTE)((f.attr & FA_DIR) != 0); le[nll].gr = f.groesse; nll++;
    } while (!FileFindNext(&f));
    for (i = (WORD)(slen(lp) > 3); i < nll; i++)  /* Ordner zuerst, dann nach Namen */
        for (j = i; j > (WORD)(slen(lp) > 3) && (le[j].dir > le[j - 1].dir || (le[j].dir == le[j - 1].dir && vgl(le[j].n, le[j - 1].n) < 0)); j--) { x = le[j]; le[j] = le[j - 1]; le[j - 1] = x; }
    for (i = 1; i <= 26 && nll < NL; i++)
        if (!DriveInfo(i | DI_NURTYP, &di) && di.typ != DT_NONE && !(di.flags & DI_PHANTOM)) { le[nll].n[0] = (char)('A' + i - 1); le[nll].n[1] = 0; le[nll].dir = 2; le[nll].gr = 0; nll++; }
    for (i = 0; i < nll; i++) le[i].mk = 0;
    links_anzeigen(0);
    DlgSetText(dl, ID_HIER, lp);
    if (test) { zahl(t, nll); pr("links ", t); }
}

/* ---------------- rechts: Server ---------------- */
static void rechts_zeigen(WORD sel)
{
    WORD i; char t[34], n[RN]; HWND l = DlgItem(dl, ID_LR);
    SendMessage(l, L_CLEAR, 0, 0);
    for (i = 0; i < nrr; i++) { scpy(n, re[i].n, sizeof n); zeile_mk(t, n, re[i].dir, re[i].gr, re[i].mk); SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)t); if (test) pr("  ", t); }
    SendMessage(l, L_SETSEL, sel, 0);
}
static void rechts_anzeigen(void)
{
    WORD i, j, a = (WORD)(nrr && re[0].n[0] == '.' && re[0].n[1] == '.'); RENT x; char t[34];
    for (i = a + 1; i < nrr; i++)                 /* ".." bleibt oben, Ordner zuerst, dann nach Namen */
        for (j = i; j > a && (re[j].dir > re[j - 1].dir || (re[j].dir == re[j - 1].dir && vgl(re[j].n, re[j - 1].n) < 0)); j--) { x = re[j]; re[j] = re[j - 1]; re[j - 1] = x; }
    for (i = 0; i < nrr; i++) re[i].mk = 0;
    rechts_zeigen(0);
    DlgSetText(dl, ID_DORT, rp);
    if (test) { zahl(t, nrr); pr("rechts ", t); }
}
static void rechts_leer(void)
{
    nrr = 0; SendMessage(DlgItem(dl, ID_LR), L_CLEAR, 0, 0); DlgSetText(dl, ID_DORT, StrGet(hi, 3));
}
static char *weiter(char *p) { while (*p && *p != ' ') p++; while (*p == ' ') p++; return p; }
static DWORD zahlvon(const char *p) { DWORD g = 0; while (*p >= '0' && *p <= '9') g = g * 10 + (BYTE)(*p++ - '0'); return g; }
static void listzeile(char *s)                    /* Unix "drwxr-xr-x 2 u g 4096 Okt 26 10:00 Name" oder DOS "10-26-26 10:00AM <DIR> Name" */
{
    char *p = s, *g = 0, *q; BYTE dir; WORD i;
    if (nrr >= NR) return;
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
    for (i = 0; p[i] && i < RN - 1; i++) re[nrr].n[i] = p[i];
    if (p[i]) return;                             /* zu lang: weglassen */
    re[nrr].n[i] = 0; re[nrr].dir = dir; re[nrr].gr = dir ? 0 : zahlvon(g); nrr++;
}

/* ---------------- Steuerverbindung ---------------- */
static void senden(LPCSTR a, LPCSTR b)
{
    scpy(cmdb, a, sizeof cmdb); if (b) { scat(cmdb, " ", sizeof cmdb); scat(cmdb, b, sizeof cmdb); }
    if (test && !(a[0] == 'P' && a[1] == 'A' && a[2] == 'S' && a[3] == 'S')) pr("> ", cmdb);
    scat(cmdb, "\r\n", sizeof cmdb);
    NetSend(hc, cmdb, slen(cmdb)); nw = 0; leer = 0;
}
static void knoepfe(void)
{
    BYTE da = (BYTE)(phase == F_BEREIT);
    SetWindowText(DlgItem(dl, ID_VERB), StrGet(hi, phase == F_AUS ? 4 : 5));
    EnableWindow(DlgItem(dl, ID_KOPIE), da || phase == F_AUS);
    EnableWindow(DlgItem(dl, ID_LOESCH), da || phase == F_AUS);
    EnableWindow(DlgItem(dl, ID_ORDNER), da || phase == F_AUS);
}
static void trennen(WORD txt, LPCSTR b)
{
    if (hd) { NetClose(hd); hd = 0; }
    if (hc) { if (phase >= F_BEREIT) NetSend(hc, "QUIT\r\n", 6); NetClose(hc); hc = 0; }
    if (fh >= 0) { FileClose(fh); fh = -1; }
    phase = F_AUS; op = 0; pumpen = 0; stapel = 0; TimerKill(dl, 1);
    rechts_leer(); knoepfe();
    if (txt) statz(txt, b);
    pr("getrennt ", b ? b : "");
}
static void liste(void) { op = O_LIST; nrr = 0; rzl = 0; if (slen(rp) > 1) { fcpy(re[0].n, "..", RN); re[0].dir = 1; re[0].gr = 0; nrr = 1; } phase = F_PASV; senden("PASV", 0); }
static BYTE weiter_kop(void);
static BYTE weiter_loe(void);
static void fertig_op(void)                       /* Datenuebertragung vorbei (226 und Datenkanal zu) */
{
    char t[24];
    if (fh >= 0) { FileClose(fh); fh = -1; }
    phase = F_BEREIT; pumpen = 0; TimerKill(dl, 1);
    if (op == O_LIST) { op = 0; rechts_anzeigen(); zahl(t, nrr - (slen(rp) > 1)); statz(6, t); knoepfe(); return; }
    zahl(t, xbytes);
    if (op == O_RETR) { op = 0; pr("geladen ", t); statz(7, t); if (stapel == 1 && weiter_kop()) return; stapel = 0; links(); knoepfe(); return; }
    if (op == O_STOR) { pr("gesendet ", t); statz(8, t); if (stapel == 2 && weiter_kop()) return; if (stapel) { stapel = 0; links(); } liste(); return; }
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
    case F_PASV:
        if (code != 227) break;
        for (p = z + 4; *p && (*p < '0' || *p > '9'); p++) ;
        for (i = 0; i < 6; i++) { v[i] = 0; while (*p >= '0' && *p <= '9') v[i] = (WORD)(v[i] * 10 + *p++ - '0'); if (*p == ',') p++; }
        z226 = 0; dzu = 0; xbytes = 0; rzl = 0;
        if (!(hd = NetOpen(dl, sip, (WORD)((v[4] << 8) | v[5]), NO_TCP))) { phase = F_BEREIT; statz(10, ""); return; }
        phase = F_DATEN; return;
    case F_DATEN: case F_XFER:
        if (code == 150 || code == 125) { if (op == O_STOR) { pumpen = 1; TimerSet(dl, 1, 55); } return; }
        if (code == 226 || code == 250) { z226 = 1; datenende(); return; }
        if (code / 100 == 4 || code / 100 == 5) {
            if (hd) { NetClose(hd); hd = 0; }
            if (fh >= 0) { FileClose(fh); fh = -1; if (op == O_RETR) FileDelete(dname); }
            phase = F_BEREIT; op = 0; pumpen = 0; stapel = 0; TimerKill(dl, 1); statz(9, z); pr("Fehler ", z); knoepfe(); return;
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
    if (NetStart(dl)) trennen(16, "");
}
static WORD wahl(WORD id) { return (WORD)SendMessage(DlgItem(dl, id), L_GETSEL, 0, 0); }
static void oeffnen(WORD id)
{
    WORD s = wahl(id), k;
    if (id == ID_LL) {
        if (s >= nll) return;
        if (le[s].dir == 2) { lp[0] = le[s].n[0]; lp[1] = ':'; lp[2] = '\\'; lp[3] = 0; }
        else if (!le[s].dir) return;
        else if (le[s].n[0] == '.') { k = slen(lp) - 1; while (k > 2 && lp[k - 1] != '\\') k--; lp[k] = 0; }
        else { char n[13]; scpy(n, le[s].n, sizeof n); scat(lp, n, sizeof lp); scat(lp, "\\", sizeof lp); }
        links(); return;
    }
    if (phase != F_BEREIT || s >= nrr || !re[s].dir) return;
    phase = F_CWD;
    if (re[s].n[0] == '.' && re[s].n[1] == '.') senden("CDUP", 0);
    else { char n[RN]; scpy(n, re[s].n, sizeof n); senden("CWD", n); }
}
static BYTE rechts_f(void) { return (BYTE)(seite == DlgItem(dl, ID_LR)); }
static BYTE mkbar(BYTE r, WORD i) { return r ? (BYTE)(i < nrr && re[i].n[0] != '.') : (BYTE)(i < nll && le[i].dir != 2 && le[i].n[0] != '.'); }
static WORD naechst(BYTE r)                       /* erster markierter Eintrag, sonst 0xFFFF */
{
    WORD i, n = r ? nrr : nll;
    for (i = 0; i < n; i++) if (r ? re[i].mk : le[i].mk) return i;
    return 0xFFFF;
}
static WORD anzahl(BYTE r) { WORD i, k = 0, n = r ? nrr : nll; for (i = 0; i < n; i++) if (r ? re[i].mk : le[i].mk) k++; return k; }
static void markiere(BYTE r, WORD i, BYTE wie)    /* wie: 0 aus, 1 an, 2 umschalten */
{
    BYTE FAR *m;
    if (!mkbar(r, i)) return;
    m = r ? &re[i].mk : &le[i].mk;
    *m = (BYTE)(wie == 2 ? !*m : wie);
}
static void zeigen(BYTE r, WORD sel) { if (r) rechts_zeigen(sel); else links_anzeigen(sel); }
static BYTE eins(BYTE r, WORD s)                  /* ein Eintrag auf die andere Seite; 1 = laeuft */
{
    char n[RN], t[100];
    qi = s;
    if (r) {                                      /* herunterladen */
        if (s >= nrr || re[s].dir) { statz(18, ""); return 0; }
        scpy(n, re[s].n, sizeof n); scpy(dname, lp, sizeof dname); dosname(t, n); scat(dname, t, sizeof dname);
        if (!test) { FINDDATA fd; if (!FileFindFirst(dname, 0, &fd)) { scpy(t, StrGet(hi, 19), sizeof t); scat(t, dname, sizeof t); if (MsgBox(dl, t, StrGet(hi, 1), MB_YESNO | MB_ASK | MB_DEFNO) != ID_YES) return 0; } }
        if ((fh = FileCreate(dname, 0)) < 0) { statz(11, dname); return 0; }
        pr("lade ", dname); xgesamt = re[s].gr; op = O_RETR; scpy(cmdb, n, sizeof cmdb);
    } else {                                      /* hochladen */
        if (s >= nll || le[s].dir) { statz(18, ""); return 0; }
        scpy(n, le[s].n, sizeof n); scpy(dname, lp, sizeof dname); scat(dname, n, sizeof dname);
        if ((fh = FileOpen(dname, 0)) < 0) { statz(11, dname); return 0; }
        pr("sende ", dname); xgesamt = le[s].gr; op = O_STOR; xn = 0; xo = 0; ende_f = 0;
    }
    scpy(rzeile, n, sizeof rzeile);               /* Name fuer RETR/STOR nach PASV */
    phase = F_PASV; senden("PASV", 0); knoepfe();
    return 1;
}
static BYTE weiter_kop(void)                      /* naechster markierter Eintrag des Stapels */
{
    BYTE r = (BYTE)(stapel == 1); WORD s;
    if (r) re[qi].mk = 0; else le[qi].mk = 0;
    while ((s = naechst(r)) != 0xFFFF) { if (r) re[s].mk = 0; else le[s].mk = 0; if (eins(r, s)) return 1; }
    return 0;
}
static void kopieren(void)
{
    BYTE r = rechts_f(); WORD s;
    if (phase != F_BEREIT) { statz(17, ""); return; }
    if ((s = naechst(r)) == 0xFFFF) { stapel = 0; eins(r, wahl(r ? ID_LR : ID_LL)); return; }
    stapel = (BYTE)(r ? 1 : 2);
    if (r) re[s].mk = 0; else le[s].mk = 0;
    if (!eins(r, s) && !weiter_kop()) { stapel = 0; zeigen(r, wahl(r ? ID_LR : ID_LL)); }
}
static BYTE weiter_loe(void)
{
    WORD s; char n[RN];
    re[qi].mk = 0;
    if ((s = naechst(1)) == 0xFFFF) return 0;
    qi = s; scpy(n, re[s].n, sizeof n); phase = F_EINZEL; senden(re[s].dir ? "RMD" : "DELE", n);
    return 1;
}
static BYTE frage_loe(BYTE r, WORD s, WORD k)
{
    char t[100], n[RN];
    if (test) return 1;
    scpy(t, StrGet(hi, 20), sizeof t);
    if (k) zahl(n, k); else scpy(n, r ? re[s].n : le[s].n, sizeof n);
    scat(t, n, sizeof t);
    return (BYTE)(MsgBox(dl, t, StrGet(hi, 1), MB_YESNO | MB_ASK | MB_DEFNO) == ID_YES);
}
static void loeschen(void)
{
    BYTE r = rechts_f(); WORD s = wahl(r ? ID_LR : ID_LL), k = anzahl(r), i; char n[13];
    if (!k && !mkbar(r, s)) return;
    if (r) {
        if (phase != F_BEREIT || !frage_loe(1, s, k)) return;
        if (!k) re[s].mk = 1;
        stapel = 3; qi = naechst(1); { char m[RN]; scpy(m, re[qi].n, sizeof m); phase = F_EINZEL; senden(re[qi].dir ? "RMD" : "DELE", m); }
        return;
    }
    if (!frage_loe(0, s, k)) return;
    if (!k) le[s].mk = 1;
    for (i = 0; i < nll; i++) if (le[i].mk) {
        scpy(n, le[i].n, sizeof n); scpy(dname, lp, sizeof dname); scat(dname, n, sizeof dname);
        if (le[i].dir ? DirRemove(dname) : FileDelete(dname)) statz(11, dname);
    }
    links();
}
static void neuordner(BYTE txt)                   /* txt: leere Textdatei statt Ordner */
{
    BYTE r = rechts_f(); char n[RN]; WORD i;
    n[0] = 0;
    if (!InputBox(dl, StrGet(hi, (WORD)(txt ? 25 : 21)), StrGet(hi, 1), n, r ? RN : 13) || !n[0]) return;
    if (txt) { for (i = 0; n[i] && n[i] != '.'; i++) ; if (!n[i] && i <= 8) scat(n, ".TXT", sizeof n); }
    if (r) {
        if (phase != F_BEREIT) { statz(17, ""); return; }
        if (!txt) { phase = F_EINZEL; senden("MKD", n); return; }
        fh = -1; op = O_STOR; xn = 0; xo = 0; ende_f = 1; xgesamt = 0; stapel = 0;   /* leere Datei senden */
        scpy(rzeile, n, sizeof rzeile); phase = F_PASV; senden("PASV", 0); knoepfe(); return;
    }
    scpy(dname, lp, sizeof dname); scat(dname, n, sizeof dname);
    if (txt) { short h = FileCreate(dname, 0); if (h < 0) statz(11, dname); else FileClose(h); }
    else if (DirMake(dname)) statz(11, dname);
    links();
}
static void neu_lesen(void) { if (rechts_f()) { if (phase == F_BEREIT) liste(); } else links(); }
static void alle(BYTE r) { WORD i, n = r ? nrr : nll; BYTE an = (BYTE)(anzahl(r) == 0); for (i = 0; i < n; i++) markiere(r, i, an); zeigen(r, wahl(r ? ID_LR : ID_LL)); }
static void befehl(WORD id)
{
    if (seite) SetFocus(seite);
    if (id == ID_KOPIE) kopieren(); else if (id == ID_LOESCH) loeschen(); else if (id == ID_ORDNER) neuordner(0);
    else if (id == ID_TEXT) neuordner(1); else if (id == ID_NEU) neu_lesen(); else if (id == ID_ALLE) alle(rechts_f());
    if (seite) SetFocus(seite);
}
/* ---------------- Fenster: Laufwerksleiste, Groesse ---------------- */
static void rect_von(WORD id, RECT *r) { POINT a, b; GetWindowRect(DlgItem(dl, id), (RECT FAR *)r); a.x = r->l; a.y = r->t; b.x = r->r; b.y = r->b; ScreenToClient(dl, &a); ScreenToClient(dl, &b); r->l = a.x; r->t = a.y; r->r = b.x; r->b = b.y; }
static void leiste(void)
{
    RECT r; WORD i, k = 0; short bw; char t[3]; DRIVEINFO di;
    rect_von(ID_LEISTE, &r); bw = (short)((r.b - r.t) * 2 + 4);
    t[1] = ':'; t[2] = 0;
    for (i = 1; i <= 26 && k < NLW; i++)
        if (!DriveInfo(i | DI_NURTYP, &di) && di.typ != DT_NONE && !(di.flags & DI_PHANTOM)) {
            t[0] = (char)('A' + i - 1);
            if (r.l + (k + 1) * bw > r.r) break;
            CreateWindow(CL_BUTTON, t, WS_CHILD | WS_VISIBLE, (short)(r.l + k * bw), (short)r.t, (short)(bw - 2), (short)(r.b - r.t), dl, (WORD)(ID_LW + i), hi);
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
    for (i = 0; i < 11; i++) {
        q = r0[i];
        switch (lid[i]) {
        case ID_HIER: case ID_LEISTE: q.r += hx; break;
        case ID_DORT: q.l += hx; q.r += dx; break;
        case ID_LL: q.r += hx; q.b += dy; break;
        case ID_LR: q.l += hx; q.r += dx; q.b += dy; break;
        case ID_STAT: q.t += dy; q.b += dy; q.r += dx; break;
        case 2: q.l += dx; q.r += dx; q.t += dy; q.b += dy; break;
        default: q.t += dy; q.b += dy;
        }
        MoveWindow(DlgItem(dl, lid[i]), (short)q.l, (short)q.t, (short)(q.r - q.l), (short)(q.b - q.t), TRUE);
    }
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
        dl = d; rechts_leer(); links(); knoepfe(); TimerSet(d, 2, 1000);
        { RECT c; WORD i; GetClientRect(d, (RECT FAR *)&c); cw0 = (WORD)(c.r - c.l); ch0 = (WORD)(c.b - c.t); for (i = 0; i < 11; i++) rect_von(lid[i], &r0[i]); }
        leiste(); lauf = 1;
        seite = DlgItem(d, ID_LL); SetFocus(seite); statz(2, "");
        pr("bereit", "");
        return FALSE;
    case M_SIZE:
        if (lauf) groesse();
        return FALSE;
    case M_KEY:
        { HWND f = GetFocus(); if (f == DlgItem(d, ID_LL) || f == DlgItem(d, ID_LR)) {
            BYTE r = (BYTE)(f == DlgItem(d, ID_LR)); WORD s = wahl(r ? ID_LR : ID_LL), ks = HIWORD(p2);
            seite = f;
            if (p1 == ' ' || p1 == K_INS) { markiere(r, s, 2); zeigen(r, (WORD)(s + 1 < (r ? nrr : nll) ? s + 1 : s)); return TRUE; }
            if (p1 == 1 && (ks & KS_CTRL)) { alle(r); return TRUE; }               /* Strg+A */
            if (p1 == 18 && (ks & KS_CTRL)) { neu_lesen(); return TRUE; }          /* Strg+R */
            if (p1 == K_F10 && (ks & KS_SHIFT)) { menue(); return TRUE; }
            if (p1 == K_F5) { befehl(ID_KOPIE); return TRUE; }
            if (p1 == K_F7) { befehl(ID_ORDNER); return TRUE; }
            if (p1 == K_F8 || p1 == K_DEL) { befehl(ID_LOESCH); return TRUE; }
            if (p1 == K_TAB && !(HIWORD(p2) & KS_SHIFT)) { seite = DlgItem(d, f == DlgItem(d, ID_LL) ? ID_LR : ID_LL); SetFocus(seite); return TRUE; }   /* Tab: andere Seite */
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
            if (op == O_LIST) senden("LIST", 0); else senden(op == O_RETR ? "RETR" : "STOR", rzeile);
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
            else if (HIWORD(p2) == N_SELECT && KeyState(KS_CTRL)) { markiere(r, s, 2); zeigen(r, s); }   /* Strg+Klick */
            return TRUE;
        }
        if (p1 > ID_LW && p1 <= ID_LW + 26) { lp[0] = (char)('A' + p1 - ID_LW - 1); lp[1] = ':'; lp[2] = '\\'; lp[3] = 0; links(); seite = DlgItem(d, ID_LL); SetFocus(seite); return TRUE; }
        if (HIWORD(p2) != N_CLICKED) return FALSE;
        if (p1 == ID_VERB) { if (phase == F_AUS) verbinden(); else trennen(24, ""); seite = DlgItem(d, ID_LR); SetFocus(seite); return TRUE; }
        if (p1 == ID_KOPIE || p1 == ID_LOESCH || p1 == ID_ORDNER) { befehl(p1); return TRUE; }
        if (p1 == ID_CANCEL || p1 == ID_OK) { TimerKill(d, 2); trennen(0, 0); pr("Ende", ""); DialogEnd(d, ID_OK); return TRUE; }
        return FALSE;
    }
    return FALSE;
}

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD k;
    (void)show; hi = inst;
    if (cmd && cmd[0] == 'T') { test = 1; cmd++; while (*cmd == ' ') cmd++; }
    if (cmd && *cmd) scpy(lp, cmd, sizeof lp);
    else {                         /* aktueller Ordner */
        char b[70]; WORD lw = 0; SYSINFO si; KernInfo(&si);
        b[0] = 0; DirGet(0, b);
        if (b[0] && b[1] == ':') scpy(lp, b, sizeof lp); else { lw = si.bootlw ? si.bootlw : 3; lp[0] = (char)('@' + lw); lp[1] = ':'; lp[2] = '\\'; lp[3] = 0; }
    }
    k = slen(lp); if (k && lp[k - 1] != '\\') scat(lp, "\\", sizeof lp);
    if (!(hm = MemAlloc((DWORD)NL * sizeof(LENT) + (DWORD)NR * sizeof(RENT) + 1024, MEM_MOVABLE)) || !(le = (LENT FAR *)MemLock(hm))) return 1;
    re = (RENT FAR *)(le + NL); xb = (char FAR *)(re + NR);
    DialogRun(inst, 10, 0, dproc, 0);
    if (pm) MenuFree(pm);
    MemUnlock(hm); MemFree(hm);
    return 0;
}
