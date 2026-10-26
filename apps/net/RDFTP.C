/* RDFTP.C - FTP als zweites Dateifenster (Punkt 8, wie RD1 RDFTP.OVL): links die eigene Platte, rechts der Server.
 * Server, Port, Benutzer, Kennwort aus RD2.INI [FTP] (RDNET, Seite Dienste). Ein Fenster (Dialog), nur RDTCP:
 * Steuerverbindung zeilenweise, Daten passiv (PASV, immer an die Adresse der Steuerverbindung, NAT/Tunnel).
 * Bedienung: Tab wechselt die Seite, Eingabe/Doppelklick oeffnet Ordner (".." hoch, "[-A-]" Laufwerk),
 * Alt+K kopiert die Auswahl auf die andere Seite, Alt+L loescht, Alt+N neuer Ordner, Alt+V verbinden/trennen.
 * Lange Servernamen: angezeigt gekuerzt, beim Herunterladen als DOS-Name (8.3, ungueltige Zeichen "_").
 * Befehlszeile "T [Ordner]": Testmodus (DbgPrint, Startordner), test/rdftp.sh. */
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
#define NL 160                                    /* Eintraege links */
#define NR 160                                    /* Eintraege rechts */
#define RN 48                                     /* Servername hoechstens */
typedef struct { char n[13]; BYTE dir; DWORD gr; } LENT;
typedef struct { char n[RN]; BYTE dir; DWORD gr; } RENT;

enum { F_AUS, F_NETZ, F_DNS, F_VERB, F_GRUSS, F_USER, F_PASS, F_TYPE, F_PWD, F_BEREIT,
       F_PASV, F_DATEN, F_XFER, F_CWD, F_EINZEL };
enum { O_LIST = 1, O_RETR, O_STOR };

static HINST hi; static HWND dl; static BYTE test, phase, op, z226, dzu, pumpen, ende_f; static WORD nw;
static HSOCK hc, hd; static DWORD sip; static WORD sport;
static char srv[40], user[24], pass[24], lp[80], rp[128], zb[260], cmdb[160], dname[RN], status[80];
static WORD zl, nll, nrr, rzl; static short fh = -1; static DWORD xbytes, xgesamt;
static HMEM hm; static LENT FAR *le; static RENT FAR *re; static char FAR *xb; static WORD xn, xo;
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
static void links(void)
{
    FINDDATA f; char m[90], t[34]; WORD i, j; HWND l = DlgItem(dl, ID_LL); LENT x; DRIVEINFO di;
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
    SendMessage(l, L_CLEAR, 0, 0);
    for (i = 0; i < nll; i++) {
        if (le[i].dir == 2) { t[0] = '['; t[1] = '-'; t[2] = le[i].n[0]; t[3] = '-'; t[4] = ']'; t[5] = 0; }
        else { char n[13]; scpy(n, le[i].n, sizeof n); zeile_fmt(t, n, le[i].dir, le[i].gr); }
        SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)t);
    }
    SendMessage(l, L_SETSEL, 0, 0);
    DlgSetText(dl, ID_HIER, lp);
    if (test) { zahl(t, nll); pr("links ", t); }
}

/* ---------------- rechts: Server ---------------- */
static void rechts_anzeigen(void)
{
    WORD i, j, a = (WORD)(nrr && re[0].n[0] == '.' && re[0].n[1] == '.'); RENT x; char t[34], n[RN]; HWND l = DlgItem(dl, ID_LR);
    for (i = a + 1; i < nrr; i++)                 /* ".." bleibt oben, Ordner zuerst, dann nach Namen */
        for (j = i; j > a && (re[j].dir > re[j - 1].dir || (re[j].dir == re[j - 1].dir && vgl(re[j].n, re[j - 1].n) < 0)); j--) { x = re[j]; re[j] = re[j - 1]; re[j - 1] = x; }
    SendMessage(l, L_CLEAR, 0, 0);
    for (i = 0; i < nrr; i++) { scpy(n, re[i].n, sizeof n); zeile_fmt(t, n, re[i].dir, re[i].gr); SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)t); if (test) pr("  ", t); }
    SendMessage(l, L_SETSEL, 0, 0);
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
    NetSend(hc, cmdb, slen(cmdb)); nw = 0;
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
    phase = F_AUS; op = 0; pumpen = 0; TimerKill(dl, 1);
    rechts_leer(); knoepfe();
    if (txt) statz(txt, b);
    pr("getrennt ", b ? b : "");
}
static void liste(void) { op = O_LIST; nrr = 0; rzl = 0; if (slen(rp) > 1) { fcpy(re[0].n, "..", RN); re[0].dir = 1; re[0].gr = 0; nrr = 1; } phase = F_PASV; senden("PASV", 0); }
static void fertig_op(void)                       /* Datenuebertragung vorbei (226 und Datenkanal zu) */
{
    char t[24];
    if (fh >= 0) { FileClose(fh); fh = -1; }
    phase = F_BEREIT; pumpen = 0; TimerKill(dl, 1);
    if (op == O_LIST) { op = 0; rechts_anzeigen(); zahl(t, nrr - (slen(rp) > 1)); statz(6, t); knoepfe(); return; }
    zahl(t, xbytes);
    if (op == O_RETR) { op = 0; pr("geladen ", t); statz(7, t); links(); knoepfe(); return; }
    if (op == O_STOR) { pr("gesendet ", t); statz(8, t); liste(); return; }
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
    case F_EINZEL: phase = F_BEREIT; if (code / 100 != 2) { statz(9, z); return; } liste(); return;
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
            phase = F_BEREIT; op = 0; pumpen = 0; TimerKill(dl, 1); statz(9, z); pr("Fehler ", z); knoepfe(); return;
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
static void kopieren(void)
{
    HWND f = GetFocus(); WORD s; char n[RN], t[100];
    if (phase != F_BEREIT) { statz(17, ""); return; }
    if (f == DlgItem(dl, ID_LR)) {                /* herunterladen */
        if ((s = wahl(ID_LR)) >= nrr || re[s].dir) { statz(18, ""); return; }
        scpy(n, re[s].n, sizeof n); scpy(dname, lp, sizeof dname); dosname(t, n); scat(dname, t, sizeof dname);
        if (!test) { FINDDATA fd; if (!FileFindFirst(dname, 0, &fd)) { scpy(t, StrGet(hi, 19), sizeof t); scat(t, dname, sizeof t); if (MsgBox(dl, t, StrGet(hi, 1), MB_YESNO | MB_ASK | MB_DEFNO) != ID_YES) return; } }
        if ((fh = FileCreate(dname, 0)) < 0) { statz(11, dname); return; }
        pr("lade ", dname); xgesamt = re[s].gr; op = O_RETR; scpy(cmdb, n, sizeof cmdb);
    } else {                                      /* hochladen */
        if ((s = wahl(ID_LL)) >= nll || le[s].dir) { statz(18, ""); return; }
        scpy(n, le[s].n, sizeof n); scpy(dname, lp, sizeof dname); scat(dname, n, sizeof dname);
        if ((fh = FileOpen(dname, 0)) < 0) { statz(11, dname); return; }
        pr("sende ", dname); xgesamt = le[s].gr; op = O_STOR; xn = 0; xo = 0; ende_f = 0;
    }
    scpy(rzeile, n, sizeof rzeile);               /* Name fuer RETR/STOR nach PASV */
    phase = F_PASV; senden("PASV", 0); knoepfe();
}
static void loeschen(void)
{
    HWND f = GetFocus(); WORD s; char n[RN], t[100];
    if (f == DlgItem(dl, ID_LR)) {
        if (phase != F_BEREIT || (s = wahl(ID_LR)) >= nrr || re[s].n[0] == '.') return;
        scpy(n, re[s].n, sizeof n); scpy(t, StrGet(hi, 20), sizeof t); scat(t, n, sizeof t);
        if (!test && MsgBox(dl, t, StrGet(hi, 1), MB_YESNO | MB_ASK | MB_DEFNO) != ID_YES) return;
        phase = F_EINZEL; senden(re[s].dir ? "RMD" : "DELE", n); return;
    }
    if ((s = wahl(ID_LL)) >= nll || le[s].dir == 2 || le[s].n[0] == '.') return;
    scpy(n, le[s].n, sizeof n); scpy(t, StrGet(hi, 20), sizeof t); scat(t, n, sizeof t);
    if (!test && MsgBox(dl, t, StrGet(hi, 1), MB_YESNO | MB_ASK | MB_DEFNO) != ID_YES) return;
    scpy(dname, lp, sizeof dname); scat(dname, n, sizeof dname);
    if (le[s].dir ? DirRemove(dname) : FileDelete(dname)) statz(11, dname);
    links();
}
static void neuordner(void)
{
    HWND f = GetFocus(); char n[RN];
    n[0] = 0;
    if (!InputBox(dl, StrGet(hi, 21), StrGet(hi, 1), n, f == DlgItem(dl, ID_LR) ? RN : 13) || !n[0]) return;
    if (f == DlgItem(dl, ID_LR)) { if (phase != F_BEREIT) return; phase = F_EINZEL; senden("MKD", n); return; }
    scpy(dname, lp, sizeof dname); scat(dname, n, sizeof dname);
    if (DirMake(dname)) statz(11, dname);
    links();
}

static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_INITDIALOG:
        dl = d; rechts_leer(); links(); knoepfe(); TimerSet(d, 2, 1000);
        seite = DlgItem(d, ID_LL); SetFocus(seite); statz(2, "");
        pr("bereit", "");
        return FALSE;
    case M_KEY:
        { HWND f = GetFocus(); if (f == DlgItem(d, ID_LL) || f == DlgItem(d, ID_LR)) {
            seite = f;
            if (p1 == K_TAB && !(HIWORD(p2) & KS_SHIFT)) { seite = DlgItem(d, f == DlgItem(d, ID_LL) ? ID_LR : ID_LL); SetFocus(seite); return TRUE; }   /* Tab: andere Seite */
        } }
        if (p1 == K_ENTER) { HWND f = GetFocus(); if (f == DlgItem(d, ID_LL) || f == DlgItem(d, ID_LR)) { oeffnen(f == DlgItem(d, ID_LL) ? ID_LL : ID_LR); return TRUE; } }
        return FALSE;
    case M_TIMER:
        if (p1 == 1) { pumpe(); return TRUE; }
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
        if (p1 == ID_LL || p1 == ID_LR) { seite = DlgItem(d, p1); if (HIWORD(p2) == N_DBLCLK) oeffnen(p1); return TRUE; }
        if (HIWORD(p2) != N_CLICKED) return FALSE;
        if (p1 == ID_VERB) { if (phase == F_AUS) verbinden(); else trennen(24, ""); seite = DlgItem(d, ID_LR); SetFocus(seite); return TRUE; }
        if (p1 == ID_KOPIE || p1 == ID_LOESCH || p1 == ID_ORDNER) {
            if (seite) SetFocus(seite);
            if (p1 == ID_KOPIE) kopieren(); else if (p1 == ID_LOESCH) loeschen(); else neuordner();
            if (seite) SetFocus(seite);
            return TRUE;
        }
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
    MemUnlock(hm); MemFree(hm);
    return 0;
}
