/* RDNET.C - Netzwerk-Einstellungen (Checkliste Punkt 8, RD1 1.2 RDNET).
 * Vier Seiten, je ein Dialog mit Reitern "1 Karte", "2 Adresse", "3 Zeit", "4 Dienste" (Alt+1..4), unten Testen/OK/Abbruch.
 * Die Werte bleiben beim Seitenwechsel erhalten; geschrieben wird erst bei OK oder Testen.
 * 1 Karte:   Netzkarte (Paketreiber), Treiberdatei, INT/IRQ/E/A/Speicher, Treiber beim Booten laden (INSTALL= in CONFIG.SYS).
 * 2 Adresse: DHCP oder feste Adresse (IP, Maske, Gateway, DNS), Rechnername; zeigt die aktuelle Adresse.
 * 3 Zeit:    Zeitserver, Zeitzone, Sommerzeit, eigene Abweichung, Uhr beim Start von RD2 stellen (siehe NETZEIT.H).
 * 4 Dienste: FTP (Server, Port, Benutzer, Kennwort) und IRC (Server, Port, Name, Kanal, Kennwort).
 * Testen speichert und prueft der Reihe nach: Paketreiber, Netzstart (DHCP), FTP-Server, IRC-Server, Zeitserver.
 * RD2.INI: [Hardware] Netz=PAKET/KEIN, [Netz] Karte Treiber Paket IRQ EA Speicher DHCP IP Maske Gateway DNS Name
 * NTP ZoneName Zone Sommer Zeit, [FTP] Server Port Benutzer Kennwort, [IRC] Server Port Nick Kanal Kennwort.
 * Befehlszeile "T [Ordner]": Testmodus (DbgPrint, CONFIG.SYS aus dem Ordner; test/rdnet.sh). */
#include "RD2.H"
#include "NETZEIT.H"

#define ID_TEST 3
#define ID_S1   91
#define CMAX    8192
enum { K_EIGEN = 7, K_KEIN = 8, NK = 9 };
static const struct { const char *name, *datei, *par; } karten[NK] = {
    { "NE1000", "NE1000.COM", "%P %I %A" }, { "NE2000", "NE2000.COM", "%P %I %A" },
    { "3Com 3C501", "3C501.COM", "%P %I %A" }, { "3Com 3C503", "3C503.COM", "%P %I %A" },
    { "3Com 3C509", "3C5X9PD.COM", "%P" }, { "WD8003/SMC", "WD8003E.COM", "%P %I %A %M" },
    { "NI5210", "NI5210.COM", "%P %I %A %M" }, { "*", "", "%P" }, { "-", "", "" } };
static HINST hi; static BYTE test, boot, dhcp, uhr; static WORD seite, karte, zone, regel;
static char ini[80], ord[64], cs[80], home[66], zl[100];
static char treiber[66], pint[8], irq[4], ea[8], spm[8], ip[16], maske[16], gw[16], dns[16], name[16];
static char ntp[40], eigen[8], ftp[40], ftpport[6], ftpuser[24], ftppass[24], irc[40], ircport[6], nick[16], kanal[24], ircpass[24];
static char altzeile[100];                         /* INSTALL=-Zeile beim Laden (Vergleich beim Speichern) */

/* ---------------- Hilfen ---------------- */
static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static BYTE gr(BYTE c) { return (BYTE)(c >= 'a' && c <= 'z' ? c - 32 : c); }
static BYTE gleich(LPCSTR a, LPCSTR b) { while (*a && gr(*a) == gr(*b)) { a++; b++; } return gr(*a) == gr(*b); }
static void zahl(char *d, DWORD v, WORD n) { char t[12]; WORD i = 0; do t[i++] = (char)('0' + v % 10); while ((v /= 10) && i < 11); while (i && n > 1) { *d++ = t[--i]; n--; } *d = 0; }
static void zwei(char *d, WORD v) { d[0] = (char)('0' + v / 10 % 10); d[1] = (char)('0' + v % 10); d[2] = 0; }
static void hex2(char *d, BYTE v) { static const char h[] = "0123456789ABCDEF"; d[0] = h[v >> 4]; d[1] = h[v & 15]; d[2] = 0; }
static void ipstr(char *d, DWORD v) { WORD i; d[0] = 0; for (i = 0; i < 4; i++) { char t[4]; zahl(t, (v >> (8 * i)) & 255, 4); scat(d, t, 16); if (i < 3) scat(d, ".", 16); } }
static BYTE ip_ok(LPCSTR s)
{
    WORD i, v, k;
    for (i = 0; i < 4; i++) {
        for (v = 0, k = 0; *s >= '0' && *s <= '9'; s++, k++) v = v * 10 + *s - '0';
        if (!k || k > 3 || v > 255) return 0;
        if (i < 3 && *s++ != '.') return 0;
    }
    return *s == 0;
}
static short abw(LPCSTR s)                        /* "+05:30", "-3", "1:00" -> Minuten; 0x7FFF = ungueltig */
{
    short sg = 1, h = 0, m = 0; WORD k = 0;
    if (*s == '+' || *s == '-') sg = *s++ == '-' ? -1 : 1;
    while (*s >= '0' && *s <= '9') { h = (short)(h * 10 + *s++ - '0'); k++; }
    if (*s == ':') { s++; while (*s >= '0' && *s <= '9') m = (short)(m * 10 + *s++ - '0'); }
    if (!k || *s || h > 14 || m > 59) return 0x7FFF;
    return (short)(sg * (h * 60 + m));
}
static void abwstr(char *d, short v) { WORD a = (WORD)(v < 0 ? -v : v); d[0] = (char)(v < 0 ? '-' : '+'); zwei(d + 1, a / 60); d[3] = ':'; zwei(d + 4, a % 60); }
static WORD pkt(void)                             /* geladener Paketreiber: INT oder 0 */
{
    WORD i; const char FAR * FAR *v = (const char FAR * FAR *)0x00000000UL; const char FAR *p;
    for (i = 0x60; i <= 0x80; i++) {
        p = v[i]; if (!p) continue;
        if (p[3] == 'P' && p[4] == 'K' && p[5] == 'T' && p[6] == ' ' && p[7] == 'D' && p[8] == 'R' && p[9] == 'V' && p[10] == 'R') return i;
    }
    return 0;
}
static const char *basis(LPCSTR p) { LPCSTR s = p + slen(p); while (s > p && s[-1] != '\\' && s[-1] != ':') s--; return (const char *)s; }
static void param(char *d, WORD n)                /* Befehlszeile des Treibers aus der Vorlage */
{
    const char *t = karten[karte].par; char *v;
    scpy(d, treiber, n);
    while (*t) {
        if (*t == '%') { t++; v = *t == 'P' ? pint : *t == 'I' ? irq : *t == 'A' ? ea : spm; if (*v) { scat(d, " ", n); scat(d, v, n); } t++; }
        else t++;
    }
}
static void pr(LPCSTR a, LPCSTR b) { if (test) { scpy(zl, "RDNET: ", sizeof zl); scat(zl, a, sizeof zl); scat(zl, b, sizeof zl); DbgPrint(zl); } }

/* ---------------- CONFIG.SYS ---------------- */
static HMEM hm; static char FAR *ein, FAR *aus;
static WORD zlen(const char FAR *a) { WORD n = 0; while (a[n] && a[n] != '\n') n++; return n; }
static const char FAR *sp(const char FAR *a) { while (*a == ' ' || *a == '\t') a++; return a; }
static BYTE beginnt(const char FAR *a, LPCSTR k) { while (*k) if (gr(*a++) != (BYTE)*k++) return 0; return 1; }
static BYTE unser(const char FAR *a, char *zeile)   /* INSTALL=-Zeile eines Paketreibers aus der Liste oder des eigenen */
{
    char n[14]; const char FAR *e, FAR *s; WORD i = 0, k;
    a = sp(a); if (!beginnt(a, "INSTALL")) return 0;
    a = sp(a + 7); if (*a != '=') return 0;
    a = sp(a + 1);
    for (e = a; *e && *e != ' ' && *e != '\r' && *e != '\n'; e++) ;
    for (s = e; s > a && s[-1] != '\\' && s[-1] != ':'; s--) ;
    while (s < e && i < 13) n[i++] = *s++;
    n[i] = 0;
    for (k = 0; k < NK; k++) if (karten[k].datei[0] && gleich(n, karten[k].datei)) break;
    if (k == NK && !(treiber[0] && gleich(n, basis(treiber)))) return 0;
    if (zeile) { for (i = 0; a[i] && a[i] != '\r' && a[i] != '\n' && i < 99; i++) zeile[i] = a[i]; zeile[i] = 0; }
    return 1;
}
static WORD lesen(LPCSTR n, char FAR *b)
{
    short f; WORD r = 0;
    if ((f = FileOpen(n, FO_READ)) >= 0) { r = FileRead(f, b, CMAX - 1); if (r == 0xFFFF) r = 0; FileClose(f); }
    b[r] = 0; return r;
}
static WORD cfg_schreiben(void)                   /* 0 ok, 1 Fehler; nur wenn sich die Zeile aendert */
{
    char neu[100]; const char FAR *a; WORD o = 0, l, i;
    neu[0] = 0;
    if (boot && karte != K_KEIN && treiber[0]) param(neu, sizeof neu);
    if (gleich(neu, altzeile)) return 0;
    if (!hm || !(ein = (char FAR *)MemLock(hm))) return 1;
    aus = ein + CMAX; lesen(cs, ein);
    for (a = ein; *a; a += l + (a[l] != 0)) {
        l = zlen(a);
        if (unser(a, 0)) continue;
        for (i = 0; i < l && o < CMAX - 120; i++) aus[o++] = a[i];
        if (a[l]) aus[o++] = '\n';
    }
    if (neu[0]) {
        if (o && aus[o - 1] != '\n') { aus[o++] = '\r'; aus[o++] = '\n'; }
        for (i = 0; i < 8; i++) aus[o++] = "INSTALL="[i];
        for (i = 0; neu[i]; i++) aus[o++] = neu[i];
        aus[o++] = '\r'; aus[o++] = '\n';
    }
    i = FileSaveSafe(cs, aus, o) ? 1 : 0;
    MemUnlock(hm);
    if (!i) scpy(altzeile, neu, sizeof altzeile);
    pr("CONFIG.SYS INSTALL=", neu);
    return i;
}

/* ---------------- RD2.INI ---------------- */
static void gs(LPCSTR sec, LPCSTR k, LPCSTR def, char *b, WORD n) { ProfileGetStr(ini, sec, k, def, b, n); }
static void laden(void)
{
    char t[40]; const char FAR *a; short m;
    gs("Netz", "Karte", "NE2000", t, sizeof t);
    for (karte = 0; karte < K_EIGEN && !gleich(t, karten[karte].name); karte++) ;
    if (gleich(t, "Eigener")) karte = K_EIGEN;
    gs("Hardware", "Netz", "PAKET", t, sizeof t); if (gleich(t, "KEIN")) karte = K_KEIN;
    scpy(t, ord, sizeof t); scat(t, "NET\\", sizeof t); scat(t, karten[karte < K_EIGEN ? karte : 1].datei, sizeof t);
    gs("Netz", "Treiber", t, treiber, sizeof treiber);
    gs("Netz", "Paket", "0x60", pint, sizeof pint); gs("Netz", "IRQ", "3", irq, sizeof irq);
    gs("Netz", "EA", "0x300", ea, sizeof ea); gs("Netz", "Speicher", "", spm, sizeof spm);
    dhcp = ProfileGetInt(ini, "Netz", "DHCP", 1) != 0;
    gs("Netz", "IP", "", ip, sizeof ip); gs("Netz", "Maske", "255.255.255.0", maske, sizeof maske);
    gs("Netz", "Gateway", "", gw, sizeof gw); gs("Netz", "DNS", "", dns, sizeof dns); gs("Netz", "Name", "RD2", name, sizeof name);
    gs("Netz", "NTP", "", ntp, sizeof ntp);
    gs("Netz", "ZoneName", "Berlin, Paris, Rom", t, sizeof t);
    for (zone = 0; zone < ZONE_EIGEN && !gleich(t, zonen[zone].name); zone++) ;
    m = (short)ProfileGetInt(ini, "Netz", "Zone", zonen[zone].min); abwstr(eigen, m);
    regel = (WORD)ProfileGetInt(ini, "Netz", "Sommer", zonen[zone].regel); if (regel > 4) regel = 0;
    uhr = ProfileGetInt(ini, "Netz", "Zeit", 0) != 0;
    gs("FTP", "Server", "", ftp, sizeof ftp); gs("FTP", "Port", "21", ftpport, sizeof ftpport);
    gs("FTP", "Benutzer", "anonymous", ftpuser, sizeof ftpuser); gs("FTP", "Kennwort", "", ftppass, sizeof ftppass);
    gs("IRC", "Server", "", irc, sizeof irc); gs("IRC", "Port", "6667", ircport, sizeof ircport);
    gs("IRC", "Nick", "RD2", nick, sizeof nick); gs("IRC", "Kanal", "", kanal, sizeof kanal); gs("IRC", "Kennwort", "", ircpass, sizeof ircpass);
    boot = 0; altzeile[0] = 0;
    if (hm && (ein = (char FAR *)MemLock(hm)) != 0) {
        lesen(cs, ein);
        for (a = ein; *a; a += zlen(a) + (a[zlen(a)] != 0)) if (unser(a, altzeile)) { boot = 1; break; }
        MemUnlock(hm);
    }
}
static WORD speichern(void)
{
    char t[8];
    ProfileSetStr(ini, "Hardware", "Netz", karte == K_KEIN ? "KEIN" : "PAKET");
    ProfileSetStr(ini, "Netz", "Karte", karte == K_EIGEN ? "Eigener" : karte == K_KEIN ? "Kein" : karten[karte].name);
    ProfileSetStr(ini, "Netz", "Treiber", treiber); ProfileSetStr(ini, "Netz", "Paket", pint);
    ProfileSetStr(ini, "Netz", "IRQ", irq); ProfileSetStr(ini, "Netz", "EA", ea); ProfileSetStr(ini, "Netz", "Speicher", spm);
    ProfileSetStr(ini, "Netz", "DHCP", dhcp ? "1" : "0");
    ProfileSetStr(ini, "Netz", "IP", ip); ProfileSetStr(ini, "Netz", "Maske", maske);
    ProfileSetStr(ini, "Netz", "Gateway", gw); ProfileSetStr(ini, "Netz", "DNS", dns); ProfileSetStr(ini, "Netz", "Name", name);
    ProfileSetStr(ini, "Netz", "NTP", ntp);
    ProfileSetStr(ini, "Netz", "ZoneName", zone == ZONE_EIGEN ? "*" : zonen[zone].name);
    { short m = zone == ZONE_EIGEN ? abw(eigen) : zonen[zone].min; char z[8]; if (m == 0x7FFF) m = 0;
      if (m < 0) { z[0] = '-'; zahl(z + 1, (DWORD)-m, 7); } else zahl(z, (DWORD)m, 8); ProfileSetStr(ini, "Netz", "Zone", z); }
    zahl(t, regel, sizeof t); ProfileSetStr(ini, "Netz", "Sommer", t);
    ProfileSetStr(ini, "Netz", "Zeit", uhr ? "1" : "0");
    ProfileSetStr(ini, "FTP", "Server", ftp); ProfileSetStr(ini, "FTP", "Port", ftpport);
    ProfileSetStr(ini, "FTP", "Benutzer", ftpuser); ProfileSetStr(ini, "FTP", "Kennwort", ftppass);
    ProfileSetStr(ini, "IRC", "Server", irc); ProfileSetStr(ini, "IRC", "Port", ircport);
    ProfileSetStr(ini, "IRC", "Nick", nick); ProfileSetStr(ini, "IRC", "Kanal", kanal); ProfileSetStr(ini, "IRC", "Kennwort", ircpass);
    pr("gespeichert", "");
    return cfg_schreiben();
}

/* ---------------- Seiten ---------------- */
static void en(HWND d, WORD id, BYTE an) { EnableWindow(DlgItem(d, id), an); }
static void setz(HWND d, WORD id, LPCSTR s) { DlgSetText(d, id, s); }
static void hol(HWND d, WORD id, char *b, WORD n) { DlgGetText(d, id, b, n); }
static void karte_felder(HWND d)
{
    const char *t = karten[karte].par, *q; BYTE k = karte != K_KEIN, hat[3] = { 0, 0, 0 };
    WORD i;
    for (q = t; *q; q++) if (*q == '%') hat[q[1] == 'I' ? 0 : q[1] == 'A' ? 1 : q[1] == 'M' ? 2 : 0] |= q[1] != 'P';
    for (i = 21; i <= 27; i++) en(d, i, k);
    en(d, 24, k && hat[0]); en(d, 25, k && hat[1]); en(d, 26, k && hat[2]);
}
static void karte_status(HWND d)
{
    WORD v = pkt(); char t[60], h[4];
    if (v) { scpy(t, StrGet(hi, 2), sizeof t); hex2(h, (BYTE)v); scat(t, h, sizeof t); scat(t, "h.", sizeof t); }
    else scpy(t, StrGet(hi, boot ? 10 : 3), sizeof t);
    setz(d, 28, t);
}
static void adr_felder(HWND d) { WORD i; for (i = 32; i <= 35; i++) en(d, i, !dhcp); }
static void adr_status(HWND d)
{
    NETINFO ni; char t[72], h[20]; WORD i;
    if (NetCaps(&ni) == 0) {
        scpy(t, StrGet(hi, 4), sizeof t); ipstr(h, ni.ip); scat(t, h, sizeof t); scat(t, StrGet(hi, (ni.flags & NI_DHCP) ? 5 : 6), sizeof t);
        scat(t, ", MAC ", sizeof t);
        for (i = 0; i < 6; i++) { hex2(h, ni.mac[i]); scat(t, h, sizeof t); if (i < 5) scat(t, ":", sizeof t); }
        setz(d, 38, t);
    } else setz(d, 38, StrGet(hi, 7));
}
static void zeit_felder(HWND d) { en(d, 43, zone == ZONE_EIGEN); }
static void fuellen(HWND d)
{
    HWND c; WORD i;
    switch (seite) {
    case 0:
        c = DlgItem(d, 20);
        for (i = 0; i < NK; i++) SendMessage(c, L_ADD, 0, (DWORD)(LPCSTR)(i == K_EIGEN ? StrGet(hi, 48) : i == K_KEIN ? StrGet(hi, 49) : karten[i].name));
        SendMessage(c, L_SETSEL, karte, 0);
        setz(d, 21, treiber); setz(d, 23, pint); setz(d, 24, irq); setz(d, 25, ea); setz(d, 26, spm); DlgCheck(d, 27, boot);
        karte_felder(d); karte_status(d); break;
    case 1:
        DlgCheck(d, 30, dhcp); DlgCheck(d, 31, !dhcp);
        setz(d, 32, ip); setz(d, 33, maske); setz(d, 34, gw); setz(d, 35, dns); setz(d, 36, name);
        adr_felder(d); adr_status(d); break;
    case 2:
        setz(d, 40, ntp);
        c = DlgItem(d, 41);
        for (i = 0; i < NZONEN; i++) SendMessage(c, L_ADD, 0, (DWORD)(LPCSTR)(i == ZONE_EIGEN ? StrGet(hi, 42) : zonen[i].name));
        SendMessage(c, L_SETSEL, zone, 0);
        c = DlgItem(d, 42);
        for (i = 0; i < 5; i++) SendMessage(c, L_ADD, 0, (DWORD)(LPCSTR)StrGet(hi, 43 + i));
        SendMessage(c, L_SETSEL, regel, 0);
        setz(d, 43, eigen); DlgCheck(d, 44, uhr); zeit_felder(d); break;
    case 3:
        setz(d, 50, ftp); setz(d, 51, ftpport); setz(d, 52, ftpuser); setz(d, 53, ftppass);
        setz(d, 54, irc); setz(d, 55, ircport); setz(d, 56, nick); setz(d, 57, kanal); setz(d, 58, ircpass); break;
    }
}
static void holen(HWND d)
{
    switch (seite) {
    case 0:
        hol(d, 21, treiber, sizeof treiber); hol(d, 23, pint, sizeof pint); hol(d, 24, irq, sizeof irq);
        hol(d, 25, ea, sizeof ea); hol(d, 26, spm, sizeof spm); boot = (BYTE)DlgChecked(d, 27); break;
    case 1:
        dhcp = (BYTE)DlgChecked(d, 30);
        hol(d, 32, ip, sizeof ip); hol(d, 33, maske, sizeof maske); hol(d, 34, gw, sizeof gw); hol(d, 35, dns, sizeof dns);
        hol(d, 36, name, sizeof name); break;
    case 2:
        hol(d, 40, ntp, sizeof ntp); hol(d, 43, eigen, sizeof eigen); uhr = (BYTE)DlgChecked(d, 44); break;
    case 3:
        hol(d, 50, ftp, sizeof ftp); hol(d, 51, ftpport, sizeof ftpport); hol(d, 52, ftpuser, sizeof ftpuser); hol(d, 53, ftppass, sizeof ftppass);
        hol(d, 54, irc, sizeof irc); hol(d, 55, ircport, sizeof ircport); hol(d, 56, nick, sizeof nick); hol(d, 57, kanal, sizeof kanal);
        hol(d, 58, ircpass, sizeof ircpass); break;
    }
}
static WORD pruefen(HWND d)                       /* 0 = ok; sonst Seite mit dem Fehler (1..4) */
{
    if (!dhcp && karte != K_KEIN && (!ip_ok(ip) || !ip_ok(maske) || (gw[0] && !ip_ok(gw)) || (dns[0] && !ip_ok(dns)))) {
        MsgBox(d, StrGet(hi, 9), StrGet(hi, 1), MB_OK | MB_WARN); return 2;
    }
    if (zone == ZONE_EIGEN && abw(eigen) == 0x7FFF) { MsgBox(d, StrGet(hi, 11), StrGet(hi, 1), MB_OK | MB_WARN); return 3; }
    return 0;
}
static BOOL RDCALLBACK sproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD e;
    if (msg == M_INITDIALOG) {
        en(d, ID_S1 + seite, 0); fuellen(d);
        if (test) { char t[2]; t[0] = (char)('1' + seite); t[1] = 0; pr("Seite ", t); }
        { static const BYTE erst[4] = { 20, 30, 40, 50 }; SetFocus(DlgItem(d, (WORD)(erst[seite] + (seite == 1 && !dhcp)))); }   /* Fokus aufs erste Feld, nicht auf die Reiter */
        return TRUE;
    }
    if (msg != M_COMMAND) return FALSE;
    if (p1 >= ID_S1 && p1 < ID_S1 + 4) { holen(d); DialogEnd(d, (short)p1); return TRUE; }
    if (p1 == ID_OK || p1 == ID_TEST) {
        holen(d);
        if ((e = pruefen(d)) != 0) { if (e - 1 != seite) DialogEnd(d, (short)(ID_S1 + e - 1)); return TRUE; }
        DialogEnd(d, (short)p1); return TRUE;
    }
    if (p1 == ID_CANCEL) { DialogEnd(d, ID_CANCEL); return TRUE; }
    if (seite == 0 && p1 == 20 && HIWORD(p2) == N_SELECT) {
        WORD n = (WORD)SendMessage(DlgItem(d, 20), L_GETSEL, 0, 0);
        hol(d, 21, treiber, sizeof treiber);
        if (n < NK && n != karte) {
            if (karte < K_EIGEN && n < K_EIGEN && gleich(basis(treiber), karten[karte].datei)) {   /* Vorgabedatei der neuen Karte */
                char *b = (char *)basis(treiber); *b = 0; scat(treiber, karten[n].datei, sizeof treiber); setz(d, 21, treiber);
            }
            karte = n; karte_felder(d);
        }
        return TRUE;
    }
    if (seite == 0 && p1 == 22) {                 /* Treiber suchen */
        FILEDLG f; char b[66];
        hol(d, 21, b, sizeof b);
        f.groesse = sizeof f; f.flags = FD_OPEN | FD_MUSTEXIST; f.titel = StrGet(hi, 12); f.filter = "*.COM|*.COM|*.EXE|*.EXE|"; f.datei = b; f.len = sizeof b;
        if (FileDialog(d, &f)) setz(d, 21, b);
        return TRUE;
    }
    if (seite == 1 && (p1 == 30 || p1 == 31)) { dhcp = (BYTE)DlgChecked(d, 30); adr_felder(d); return TRUE; }
    if (seite == 2 && p1 == 41 && HIWORD(p2) == N_SELECT) {
        WORD n = (WORD)SendMessage(DlgItem(d, 41), L_GETSEL, 0, 0);
        if (n < NZONEN && n != zone) {
            zone = n;
            if (zone != ZONE_EIGEN) { abwstr(eigen, zonen[zone].min); setz(d, 43, eigen); regel = zonen[zone].regel; SendMessage(DlgItem(d, 42), L_SETSEL, regel, 0); }
            zeit_felder(d);
        }
        return TRUE;
    }
    if (seite == 2 && p1 == 42 && HIWORD(p2) == N_SELECT) { WORD n = (WORD)SendMessage(DlgItem(d, 42), L_GETSEL, 0, 0); if (n < 5) regel = n; return TRUE; }
    return FALSE;
}

/* ---------------- Netztest ---------------- */
static WORD tz, tw, tzeile; static HSOCK hs; static BYTE tfertig; static char tb[64];
static WORD ports(LPCSTR s) { WORD v = 0; while (*s >= '0' && *s <= '9') v = (WORD)(v * 10 + *s++ - '0'); return v; }
static void zeile(HWND d, LPCSTR a, LPCSTR b)
{
    char t[80];
    scpy(t, a, sizeof t); scat(t, b, sizeof t);
    if (tzeile < 9) setz(d, (WORD)(61 + tzeile++), t);
    pr("Test: ", t);
}
static const char *dienst(void) { return tz == 3 ? ftp : tz == 4 ? irc : ntp; }
static void tschritt(HWND d);
static void tweiter(HWND d) { if (hs) { NetClose(hs); hs = 0; } tz++; tw = 0; tschritt(d); }
static void tschritt(HWND d)                      /* 1 Netz starten, 3 FTP, 4 IRC, 5 Zeitserver, 6 fertig */
{
    WORD e;
    switch (tz) {
    case 1:
        zeile(d, StrGet(hi, 22), "");
        if ((e = NetStart(d)) != 0) { zahl(tb, e, sizeof tb); zeile(d, StrGet(hi, 26), e == RD_ERR_FORMAT ? StrGet(hi, 28) : e == RD_ERR_BUSY ? StrGet(hi, 37) : tb); tz = 6; tschritt(d); }
        return;
    case 3: case 4: case 5:
        if (tz == 5 && !ntp[0]) { gs("Netz", "NTPDHCP", "pool.ntp.org", ntp, sizeof ntp); }
        if (!dienst()[0]) { zeile(d, StrGet(hi, tz == 3 ? 39 : 40), ""); tweiter(d); return; }
        if (NetResolve(d, dienst())) { zeile(d, StrGet(hi, 26 + tz), StrGet(hi, 32)); tweiter(d); }
        return;
    case 6:
        tfertig = 1; zeile(d, StrGet(hi, 36), "");
        SetWindowText(DlgItem(d, ID_OK), "OK");
        return;
    }
}
static BOOL RDCALLBACK tproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD v, n; char t[40];
    switch (msg) {
    case M_INITDIALOG:
        tz = 0; tw = 0; tzeile = 0; hs = 0; tfertig = 0;
        TimerSet(d, 1, 1000);
        if ((v = pkt()) != 0) { hex2(t, (BYTE)v); scat(t, "h", sizeof t); zeile(d, StrGet(hi, 20), t); tz = 1; tschritt(d); }
        else { zeile(d, StrGet(hi, 21), ""); tz = 6; tschritt(d); }
        return FALSE;
    case M_TIMER:
        if (tfertig) return TRUE;
        if (++tw > (tz == 1 ? 25 : 12)) {         /* Zeitueberschreitung */
            zeile(d, tz == 1 ? StrGet(hi, 26) : StrGet(hi, 26 + tz), tz == 1 ? StrGet(hi, 27) : StrGet(hi, 33));
            if (tz == 1) { tz = 6; tschritt(d); } else tweiter(d);
        }
        return TRUE;
    case M_NETSTATE:
        if (tz != 1) return TRUE;
        if (p1 == NS_OPEN) {
            NETINFO ni; char h[20];
            NetCaps(&ni);
            ipstr(h, ni.ip); scpy(t, h, sizeof t); scat(t, StrGet(hi, (ni.flags & NI_DHCP) ? 5 : 6), sizeof t); zeile(d, StrGet(hi, 23), t);
            ipstr(h, ni.gateway); scpy(t, h, sizeof t); scat(t, ", DNS ", sizeof t); ipstr(h, ni.dns); scat(t, h, sizeof t); zeile(d, StrGet(hi, 24), t);
            tz = 3; tw = 0; tschritt(d);
        } else { zahl(t, p2, sizeof t); zeile(d, StrGet(hi, 26), p2 == RD_ERR_TIMEOUT ? StrGet(hi, 27) : t); tz = 6; tschritt(d); }
        return TRUE;
    case M_NETRESOLVED:
        if (tz < 3 || tz > 5 || hs) return TRUE;
        if (p1) { zeile(d, StrGet(hi, 26 + tz), StrGet(hi, 32)); tweiter(d); return TRUE; }
        n = tz == 3 ? ports(ftpport) : tz == 4 ? ports(ircport) : 123;
        hs = NetOpen(d, p2, n, tz == 5 ? NO_UDP : NO_TCP);
        if (!hs) { zeile(d, StrGet(hi, 26 + tz), StrGet(hi, 33)); tweiter(d); }
        return TRUE;
    case M_NETCONNECT:
        if (p1 != hs) return TRUE;
        if (p2) { zeile(d, StrGet(hi, 26 + tz), StrGet(hi, 33)); tweiter(d); return TRUE; }
        if (tz == 5) { BYTE q[48]; WORD i; for (i = 0; i < 48; i++) q[i] = 0; q[0] = 0x1B; NetSend(hs, q, 48); }
        else if (tz == 4) { zeile(d, StrGet(hi, 30), StrGet(hi, 34)); tweiter(d); }
        return TRUE;                              /* FTP: auf die Begruessung warten */
    case M_NETDATA:
        if (p1 != hs) return TRUE;
        n = NetRecv(hs, tb, sizeof tb - 1); tb[n] = 0;
        if (tz == 3) {
            for (v = 0; v < n && tb[v] != '\r' && tb[v] != '\n'; v++) ;
            tb[v < 30 ? v : 30] = 0; zeile(d, "FTP: ", tb); tweiter(d);
        } else if (tz == 5 && n >= 44) {
            RDTIME z; DWORD s = ((DWORD)(BYTE)tb[40] << 24) | ((DWORD)(BYTE)tb[41] << 16) | ((DWORD)(BYTE)tb[42] << 8) | (BYTE)tb[43];
            short m = zone == ZONE_EIGEN ? abw(eigen) : zonen[zone].min; if (m == 0x7FFF) m = 0;
            nz_ortszeit(s, m, (BYTE)regel, &z);
            zwei(t, z.tag); scat(t, ".", sizeof t); zwei(t + 3, z.monat); scat(t, ".", sizeof t); zahl(t + 6, z.jahr, 5);
            scat(t, " ", sizeof t); zwei(t + 11, z.std); scat(t, ":", sizeof t); zwei(t + 14, z.min); scat(t, ":", sizeof t); zwei(t + 17, z.sek);
            zeile(d, StrGet(hi, 35), t); tweiter(d);
        }
        return TRUE;
    case M_NETCLOSED:
        if (p1 == hs && tz == 3) { zeile(d, StrGet(hi, 29), StrGet(hi, 33)); tweiter(d); }
        return TRUE;
    case M_COMMAND:
        if (p1 == ID_OK || p1 == ID_CANCEL) { TimerKill(d, 1); if (hs) { NetClose(hs); hs = 0; } DialogEnd(d, ID_OK); return TRUE; }
        return FALSE;
    }
    return FALSE;
}

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    SYSINFO si; short r;
    (void)show; hi = inst;
    KernInfo(&si); scpy(home, si.home, sizeof home); if (slen(home) && home[slen(home) - 1] != '\\') scat(home, "\\", sizeof home);
    scpy(ini, home, sizeof ini); scat(ini, "RD2.INI", sizeof ini);
    ord[0] = (char)('@' + (si.bootlw ? si.bootlw : 3)); ord[1] = ':'; ord[2] = '\\'; ord[3] = 0;
    if (cmd && cmd[0] == 'T') { test = 1; cmd++; while (*cmd == ' ') cmd++; if (*cmd) scpy(ord, cmd, sizeof ord); }
    scpy(cs, ord, sizeof cs); scat(cs, "CONFIG.SYS", sizeof cs);
    hm = MemAlloc(2L * CMAX, MEM_MOVABLE);
    laden();
    pr("bereit", "");
    for (;;) {
        r = DialogRun(inst, (WORD)(11 + seite), 0, sproc, 0);
        if (r >= ID_S1 && r < ID_S1 + 4) { seite = (WORD)(r - ID_S1); continue; }
        if (r == ID_TEST || r == ID_OK) {
            if (speichern()) { MsgBox(0, StrGet(hi, 8), StrGet(hi, 1), MB_OK | MB_WARN); if (r == ID_OK) continue; }
            if (r == ID_OK) break;
            DialogRun(inst, 15, 0, tproc, 0);
            pr("Test zu", "");
            continue;
        }
        break;
    }
    if (hm) MemFree(hm);
    pr("Ende", "");
    return 0;
}
