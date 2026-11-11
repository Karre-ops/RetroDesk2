/* RDQR.C - Dateien vom Smartphone empfangen (11.11.2026): kleiner Webserver auf Port 80 und QR-Code mit der Adresse.
 * Das Smartphone scannt den Code, der Browser zeigt eine Seite "Datei waehlen / Senden" (ohne App). Die Seite schickt
 * jede Datei per POST /<schluessel>/u?n=NAME roh (XMLHttpRequest, mit Fortschritt). RDQR schreibt sie Stueck fuer
 * Stueck in den Zielordner (RD2.INI [QR] Ordner=, Vorgabe <Laufwerk von RD2>\EINGANG, Menue Datei > Zielordner).
 * Namen werden zu 8.3, vorhandene Dateien nicht ueberschrieben (NAME~1 ...). Der Schluessel (4 Zeichen) wird bei jedem
 * Start neu gewuerfelt, andere Adressen bekommen 404. Server laeuft nur, solange das Fenster offen ist.
 * Befehlszeile "T": Testmodus, Schluessel "test", Meldungen per DbgPrint (test/rdqr.sh). QR-Erzeuger: QR.C. */
#include "RD2.H"
#include "QR.H"

#define CM_ORDNER 101
#define CM_ENDE 109
#define NK 3                                      /* gleichzeitige Verbindungen */
#define HB 1024                                   /* Anfragekopf */
#define NL 6                                      /* Meldungszeilen */
#define LB 60
enum { K_FREI, K_KOPF, K_DATEN };
typedef struct { HSOCK s; BYTE st; WORD hl; short f; DWORD rest, ges; char name[13]; char voll[80]; char h[HB]; } VERB;

static HINST hi; static HWND fe; static HMENU mn; static BYTE test, bereit, neu_malen;
static HSOCK hl; static VERB k[NK];
static char ini[80], ordner[66], url[48], tok[6], lg[NL][LB], fort[LB];
static unsigned char qm[QRN][QRN];
static char io[1024];
static short zh, ax, ay;

/* ---------------- Hilfen ---------------- */
static WORD slen(LPCSTR s) { WORD n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, LPCSTR s, WORD n) { while (--n && *s) *d++ = *s++; *d = 0; }
static void scat(char *d, LPCSTR s, WORD n) { WORD l = slen(d); if (l < n) scpy(d + l, s, (WORD)(n - l)); }
static char gr(char c) { return (char)(c >= 'a' && c <= 'z' ? c - 32 : c); }
static void zahl(char *o, DWORD v) { char t[12]; WORD i = 0; do t[i++] = (char)('0' + v % 10); while (v /= 10); while (i) *o++ = t[--i]; *o = 0; }
static void zcat(char *d, DWORD v, WORD n) { char t[12]; zahl(t, v); scat(d, t, n); }
static void kb(char *d, DWORD v, WORD n) { zcat(d, (v + 1023) >> 10, n); scat(d, " KB", n); }
static void pr(LPCSTR a, LPCSTR b) { if (test) { char t[140]; scpy(t, "RDQR: ", sizeof t); scat(t, a, sizeof t); scat(t, b, sizeof t); DbgPrint(t); } }
static void melde(WORD id, LPCSTR b)              /* neue Meldungszeile unten */
{
    WORD i;
    for (i = 0; i < NL - 1; i++) scpy(lg[i], lg[i + 1], LB);
    scpy(lg[NL - 1], StrGet(hi, id), LB); scat(lg[NL - 1], b, LB);
    InvalidateRect(fe, 0, TRUE);
}

/* ---------------- Anzeige ---------------- */
static void malen(void)
{
    PAINT ps; HDC d; RECT r, c; short mh, mw, x0, y0, tx, y, i, j; char t[90];
    d = BeginPaint(fe, &ps); GetClientRect(fe, &c);
    mh = (short)((c.b - 8) / (QRN + 6)); if (mh < 1) mh = 1;
    mw = (short)((long)mh * ay / ax); if (mw < 1) mw = 1;
    while (mh > 1 && mw * (QRN + 6) > c.r / 2) { mh--; mw = (short)((long)mh * ay / ax); if (mw < 1) mw = 1; }
    x0 = 4; y0 = 4;
    SetBrush(d, C_WHITE, PAT_SOLID); r.l = x0; r.t = y0; r.r = (short)(x0 + mw * (QRN + 6)); r.b = (short)(y0 + mh * (QRN + 6)); FillRect(d, &r);
    SetPen(d, C_BLACK); Frame(d, &r);
    tx = (short)(r.r + 10);
    if (bereit) {
        SetBrush(d, C_BLACK, PAT_SOLID);
        for (j = 0; j < QRN; j++) for (i = 0; i < QRN; i++) if (qm[j][i]) {
            RECT m; short a = i;
            while (i + 1 < QRN && qm[j][i + 1]) i++;   /* Laeufe am Stueck fuellen */
            m.l = (short)(x0 + (a + 3) * mw); m.r = (short)(x0 + (i + 4) * mw); m.t = (short)(y0 + (j + 3) * mh); m.b = (short)(m.t + mh);
            FillRect(d, &m);
        }
    }
    SetBkMode(d, BK_TRANSPARENT); SetTextColor(d, C_BLACK);
    y = 4;
    TextOut(d, tx, y, StrGet(hi, 2), -1); y += zh;
    TextOut(d, tx, y, StrGet(hi, 3), -1); y += zh + zh / 2;
    if (bereit) { TextOut(d, tx, y, url, -1); } else TextOut(d, tx, y, StrGet(hi, 4), -1);
    y += zh + zh / 2;
    scpy(t, StrGet(hi, 5), sizeof t); scat(t, ordner, sizeof t); TextOut(d, tx, y, t, -1); y += zh + zh / 2;
    for (i = 0; i < NL; i++, y += zh) if (lg[i][0]) TextOut(d, tx, y, lg[i], -1);
    if (fort[0]) TextOut(d, tx, (short)(y + zh / 2), fort, -1);
    EndPaint(fe, &ps);
}

/* ---------------- Einstellungen ---------------- */
static void ordner_lesen(void)
{
    SYSINFO si; char v[66];
    KernInfo(&si); scpy(ini, si.home, sizeof ini); if (slen(ini) && ini[slen(ini) - 1] != '\\') scat(ini, "\\", sizeof ini); scat(ini, "RD2.INI", sizeof ini);
    v[0] = si.home[0] ? si.home[0] : 'C'; v[1] = ':'; v[2] = 0; scat(v, "\\EINGANG", sizeof v);
    ProfileGetStr(ini, "QR", "Ordner", v, ordner, sizeof ordner);
    if (!ordner[0]) scpy(ordner, v, sizeof ordner);
}
static void ordner_waehlen(void)
{
    FILEDLG f; char b[80]; WORD i;
    scpy(b, ordner, sizeof b); if (slen(b) && b[slen(b) - 1] != '\\') scat(b, "\\", sizeof b);
    DirMake(ordner);                              /* damit der Dialog dort beginnen kann */
    scat(b, StrGet(hi, 7), sizeof b);
    f.groesse = sizeof f; f.flags = FD_SAVE; f.titel = StrGet(hi, 6); f.filter = 0; f.datei = b; f.len = sizeof b;
    if (!FileDialog(fe, &f)) return;
    for (i = slen(b); i && b[i - 1] != '\\'; i--) ;   /* Dateiname weg, Ordner bleibt */
    if (i > 3) i--;                               /* "C:\" behaelt den Strich */
    b[i] = 0; if (!b[0]) return;
    scpy(ordner, b, sizeof ordner); ProfileSetStr(ini, "QR", "Ordner", ordner);
    pr("Ordner ", ordner); InvalidateRect(fe, 0, TRUE);
}

/* ---------------- HTTP ---------------- */
static void sende(VERB *v, LPCSTR s) { WORD n = slen(s); if (n) NetSend(v->s, s, n); }
static void zu(VERB *v) { if (v->f > 0) FileClose(v->f); v->f = 0; NetClose(v->s); v->st = K_FREI; v->s = 0; }
static void antwort(VERB *v, LPCSTR code, LPCSTR typ, LPCSTR text)
{
    static char t[160];
    scpy(t, "HTTP/1.0 ", sizeof t); scat(t, code, sizeof t);
    scat(t, "\r\nContent-Type: ", sizeof t); scat(t, typ, sizeof t);
    scat(t, "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", sizeof t);
    sende(v, t); sende(v, text); zu(v);
}
static const char seite1[] =
    "<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content=\"width=device-width\">"
    "<title>RetroDesk 2</title></head><body style=\"font:18px sans-serif;margin:1.5em\">"
    "<h3>RetroDesk 2</h3><p>Datei an den PC senden / Send a file to the PC</p>"
    "<p><input type=file id=f multiple></p><p><button onclick=s() style=\"font-size:20px\">Senden / Send</button></p>"
    "<p id=m></p><script>var u='/";
static const char seite2[] =
    "/u?n=';function s(){var l=document.getElementById('f').files,m=document.getElementById('m'),i=0;"
    "function n(){if(i>=l.length){m.textContent=l.length?'Fertig / Done: '+l.length:'?';return}"
    "var d=l[i++],x=new XMLHttpRequest();x.open('POST',u+encodeURIComponent(d.name));"
    "x.upload.onprogress=function(e){if(e.total)m.textContent=d.name+': '+Math.round(e.loaded*100/e.total)+' %'};"
    "x.onload=function(){if(x.status==200)n();else m.textContent=x.status+' '+x.responseText};"
    "x.onerror=function(){m.textContent='Verbindung unterbrochen / Connection lost'};x.send(d)}n()}</script></body></html>";
static void seite(VERB *v)
{
    static char t[24];
    sende(v, "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n");
    sende(v, seite1); scpy(t, tok, sizeof t); sende(v, t); sende(v, seite2); zu(v);
    pr("Seite", "");
}
static BYTE erlaubt(char c) { return (BYTE)((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '!' || c == '#' || c == '$' || c == '&' || c == '(' || c == ')' || c == '@' || c == '~'); }
static WORD hexw(char c) { return (WORD)(c >= '0' && c <= '9' ? c - '0' : (gr(c) >= 'A' && gr(c) <= 'F' ? gr(c) - 'A' + 10 : 99)); }
static void dosname(const char *q, char *o)      /* "Foto 2026.jpeg" (URL-kodiert) -> "FOTO2026.JPE" */
{
    char t[80]; WORD n = 0, i, b = 0, e, p = 0xFFFF;
    while (*q && *q != ' ' && *q != '&' && n < sizeof t - 1) {   /* %XX und + aufloesen */
        char c = *q++;
        if (c == '%' && hexw(q[0]) < 16 && hexw(q[1]) < 16) { c = (char)(hexw(q[0]) * 16 + hexw(q[1])); q += 2; }
        else if (c == '+') c = ' ';
        if (c == '/' || c == '\\' || c == ':') { n = 0; continue; }   /* nur der letzte Teil */
        t[n++] = c;
    }
    t[n] = 0;
    for (i = 0; i < n; i++) if (t[i] == '.') p = i;
    for (i = 0; i < n && i != p && b < 8; i++) { char c = gr(t[i]); if ((BYTE)c >= 0x80 || c == ' ' || c == '.') continue; o[b++] = erlaubt(c) ? c : '_'; }
    if (!b) { scpy(o, "DATEI", 9); b = 5; }
    e = b;
    if (p != 0xFFFF) { o[e++] = '.'; for (i = p + 1; i < n && e < b + 4; i++) { char c = gr(t[i]); if ((BYTE)c >= 0x80 || c == ' ') continue; o[e++] = erlaubt(c) ? c : '_'; } if (e == b + 1) e = b; }
    o[e] = 0;
}
static void pfad(char *d, LPCSTR n) { scpy(d, ordner, 80); if (slen(d) && d[slen(d) - 1] != '\\') scat(d, "\\", 80); scat(d, n, 80); }
static BYTE gibts(LPCSTR p) { short f = FileOpen(p, FO_READ | FO_DENYNONE); if (f < 0) return 0; FileClose(f); return 1; }
static void eindeutig(VERB *v)                    /* NAME.EXT -> NAME~1.EXT ... NAME~9.EXT */
{
    char b[13], e[5]; WORD i, n; char *p;
    pfad(v->voll, v->name);
    if (!gibts(v->voll)) return;
    for (p = v->name; *p && *p != '.'; p++) ;
    scpy(e, p, sizeof e); *p = 0; scpy(b, v->name, 7);
    for (i = 1; i <= 9; i++) {
        n = slen(b); b[n] = '~'; b[n + 1] = (char)('0' + i); b[n + 2] = 0;
        scpy(v->name, b, 9); scat(v->name, e, 13); b[n] = 0;
        pfad(v->voll, v->name); if (!gibts(v->voll)) return;
    }
}
static void fortschritt(VERB *v)
{
    scpy(fort, v->name, LB); scat(fort, ": ", LB); kb(fort, v->ges - v->rest, LB); scat(fort, " / ", LB); kb(fort, v->ges, LB);
    neu_malen = 1;
}
static void fertig(VERB *v)
{
    char t[40];
    FileClose(v->f); v->f = 0;
    scpy(t, v->name, sizeof t); scat(t, " (", sizeof t); kb(t, v->ges, sizeof t); scat(t, ")", sizeof t);
    fort[0] = 0; melde(9, t);
    { char z[40]; scpy(z, v->name, sizeof z); scat(z, " ", sizeof z); zcat(z, v->ges, sizeof z); pr("empfangen ", z); }
    antwort(v, "200 OK", "text/plain", "OK");
}
static void schreiben(VERB *v, const char *b, WORD n)
{
    if (n > v->rest) n = (WORD)v->rest;
    if (n && FileWrite(v->f, b, n) != n) {
        FileClose(v->f); v->f = 0; FileDelete(v->voll); fort[0] = 0; melde(11, v->name); pr("voll ", v->name);
        antwort(v, "507 Insufficient Storage", "text/plain", "Kein Platz / Disk full");
        return;
    }
    v->rest -= n;
    if (!v->rest) fertig(v); else fortschritt(v);
}
static BYTE kopf_gleich(const char *z, const char *s) { while (*s && gr(*z) == gr(*s)) { z++; s++; } return (BYTE)!*s; }
static void anfrage(VERB *v, WORD ende)           /* Kopf h[0..ende) ist da, ab ende+4 schon Daten */
{
    char *p = v->h, *pf; WORD i, post; DWORD len = 0xFFFFFFFFUL; char t[12];
    v->h[ende] = 0;
    post = (WORD)kopf_gleich(p, "POST ");
    if (!post && !kopf_gleich(p, "GET ")) { antwort(v, "405 Method Not Allowed", "text/plain", "405"); return; }
    pf = p + (post ? 5 : 4);
    for (i = 0; i < ende; i++) if (kopf_gleich(v->h + i, "\ncontent-length:")) {
        char *q = v->h + i + 16; while (*q == ' ') q++;
        for (len = 0; *q >= '0' && *q <= '9'; q++) len = len * 10 + (*q - '0');
    }
    scpy(t, "/", sizeof t); scat(t, tok, sizeof t);
    if (!kopf_gleich(pf, t) || (pf[slen(t)] != '/' && pf[slen(t)] != ' ')) { pr("404", ""); antwort(v, "404 Not Found", "text/plain", "404"); return; }
    pf += slen(t);
    if (!post) { if (pf[0] == ' ' || (pf[0] == '/' && pf[1] == ' ')) seite(v); else antwort(v, "404 Not Found", "text/plain", "404"); return; }
    if (!kopf_gleich(pf, "/u?n=")) { antwort(v, "404 Not Found", "text/plain", "404"); return; }
    if (len == 0xFFFFFFFFUL) { antwort(v, "411 Length Required", "text/plain", "411"); return; }
    dosname(pf + 5, v->name);
    DirMake(ordner);
    eindeutig(v);
    if ((v->f = FileCreate(v->voll, 0)) < 0) { v->f = 0; melde(10, v->voll); pr("Fehler ", v->voll); antwort(v, "500 Internal Server Error", "text/plain", "Datei / File?"); return; }
    v->st = K_DATEN; v->rest = v->ges = len;
    melde(8, v->name); pr("Datei ", v->name);
    if (!len) { fertig(v); return; }
    if (v->hl > ende + 4) schreiben(v, v->h + ende + 4, (WORD)(v->hl - ende - 4));
}
static void lesen(VERB *v)
{
    WORD n, i, a;
    while (v->st != K_FREI) {
        if (v->st == K_KOPF) {
            a = v->hl;
            if (a >= HB - 1) { antwort(v, "431 Request Header Fields Too Large", "text/plain", "431"); return; }
            if (!(n = NetRecv(v->s, v->h + a, (WORD)(HB - 1 - a)))) return;
            v->hl += n;
            for (i = a > 3 ? a - 3 : 0; i + 3 < v->hl; i++)
                if (v->h[i] == '\r' && v->h[i + 1] == '\n' && v->h[i + 2] == '\r' && v->h[i + 3] == '\n') { anfrage(v, i); break; }
        } else {
            if (!(n = NetRecv(v->s, io, sizeof io))) return;
            schreiben(v, io, n);
        }
    }
}
static VERB *finde(HSOCK s) { WORD i; for (i = 0; i < NK; i++) if (k[i].st != K_FREI && k[i].s == s) return &k[i]; return 0; }

/* ---------------- Start ---------------- */
static void starten(void)
{
    DWORD ip = NetLocalIP(); BYTE *b = (BYTE *)&ip; WORD i; DWORD z; RDTIME tm;
    if (!(hl = NetListen(fe, 80, NO_TCP))) { melde(12, ""); pr("kein Lauscher", ""); return; }
    if (test) scpy(tok, "test", sizeof tok);
    else {
        TimeGet(&tm); z = TickCount() ^ ((DWORD)tm.hsek << 24) ^ ((DWORD)tm.sek << 16) ^ ((DWORD)tm.min << 8) ^ ip;
        for (i = 0; i < 4; i++) { z = z * 1103515245UL + 12345; tok[i] = "abcdefghjkmnpqrstuvwxyz23456789"[(WORD)(z >> 16) % 31]; }
        tok[4] = 0;
    }
    scpy(url, "http://", sizeof url);
    for (i = 0; i < 4; i++) { zcat(url, b[i], sizeof url); if (i < 3) scat(url, ".", sizeof url); }
    scat(url, "/", sizeof url); scat(url, tok, sizeof url); scat(url, "/", sizeof url);
    qr_make(url, qm); bereit = 1;
    pr("bereit ", url);
    InvalidateRect(fe, 0, TRUE);
}
static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    VERB *v; WORD i;
    if (!fe) fe = w;
    switch (msg) {
    case M_PAINT: malen(); return 0;
    case M_SIZE: InvalidateRect(w, 0, TRUE); return 0;
    case M_KEY: if (p1 == K_ESC) PostMessage(w, M_CLOSE, 0, 0); return 0;
    case M_NETSTATE:
        if (bereit || hl) return 0;
        if (p1 != NS_OPEN) { melde(4, ""); pr("kein Netz", ""); return 0; }
        starten(); return 0;
    case M_NETACCEPT:
        for (i = 0; i < NK && k[i].st != K_FREI; i++) ;
        if (i == NK) { NetClose(p1); return 0; }
        v = &k[i]; v->s = p1; v->st = K_KOPF; v->hl = 0; v->f = 0;
        lesen(v); return 0;
    case M_NETDATA: if ((v = finde(p1)) != 0) lesen(v); return 0;
    case M_NETCLOSED:
        if ((v = finde(p1)) == 0) return 0;
        lesen(v);
        if (v->st == K_DATEN) { FileClose(v->f); v->f = 0; FileDelete(v->voll); fort[0] = 0; melde(13, v->name); pr("abgebrochen ", v->name); }
        if (v->st != K_FREI) zu(v);
        return 0;
    case M_TIMER: if (neu_malen) { neu_malen = 0; InvalidateRect(w, 0, TRUE); } return 0;
    case M_COMMAND:
        if (p1 == CM_ORDNER) ordner_waehlen(); else if (p1 == CM_ENDE) PostMessage(w, M_CLOSE, 0, 0);
        return 0;
    case M_CLOSE:
        TimerKill(w, 1);
        for (i = 0; i < NK; i++) if (k[i].st != K_FREI) { if (k[i].st == K_DATEN) { FileClose(k[i].f); k[i].f = 0; FileDelete(k[i].voll); } zu(&k[i]); }
        if (hl) { NetClose(hl); hl = 0; }
        pr("Ende", ""); DestroyWindow(w); return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HDC d; short W, H;
    (void)show; hi = inst;
    if (cmd && cmd[0] == 'T') test = 1;
    wc.name = "RdQr"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    d = GetDC(0); W = GetCaps(d, CAPS_W); H = GetCaps(d, CAPS_H); zh = GetCaps(d, CAPS_TEXTH); ax = GetCaps(d, CAPS_ASPX); ay = GetCaps(d, CAPS_ASPY); ReleaseDC(0, d);
    if (ax <= 0) ax = 100; if (ay <= 0) ay = 100;
    ordner_lesen();
    fe = CreateWindow("RdQr", StrGet(inst, 1), WS_NORMAL | WS_VISIBLE, (short)(W / 32), (short)(H / 12), (short)(W - W / 16), (short)(H - H / 12 - H / 20), 0, 1, inst);
    if (!fe) return 1;
    mn = MenuLoad(inst, 1); MenuSet(fe, mn);
    SetFocus(fe); TimerSet(fe, 1, 500);
    melde(14, "");
    if (NetStart(fe)) { melde(4, ""); pr("kein Netz", ""); }
    while (GetMessage(&m)) DispatchMessage(&m);
    MenuFree(mn);
    return 0;
}
